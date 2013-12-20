// Flipdot Spectrum Analyzer
// reads input from alsa capture device
// calculates FFT with fftw3
// displays spectrum graph on flipdot display
// (-DNOFLIP if you don't have a flipdot device and want only the debug ouput)

// To compile without bcm2835 and libflipdot:
// gcc -o flipspect_record flipspect_record.c -O3 -g -DNOFLIP -lasound -lfftw3 -lm

// Initially copy & pasted from these sources:
// sndfile-tools-1.03/src/sndfile-spectrogram.c
// http://stackoverflow.com/questions/6666807/how-to-scale-fft-output-of-wave-file
// http://www.linuxjournal.com/article/6735?page=0,2


#define ALSA_PCM_NEW_HW_PARAMS_API

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <getopt.h>

#include <alsa/asoundlib.h>
#include <fftw3.h>


// used in FFT size calculation from sndfile-spectrogram
#define FREQ_MIN 25
//#define FREQ_MIN 40
//#define FREQ_MIN 1000

// requested samplerate = FREQ_MAX * 2
//#define FREQ_MAX 8000
//#define FREQ_MAX 12000
//#define FREQ_MAX 16000
#define FREQ_MAX 24000

#define LOG_SCALE 10
#define FFT_SCALE1 1

// disable flipdot output for debugging
#define NOFLIP

#ifndef NOFLIP
#include <bcm2835.h>
#include <flipdot.h>
#else
// flipdot.h would define these constants:
#define DISP_COLS 40
#define DISP_ROWS 16
#define REGISTER_COL_BYTE_COUNT 6
#endif

#define FFT_WIDTH DISP_COLS
#define FFT_HEIGHT DISP_ROWS

#if (FFT_HEIGHT != 16)
#error "lazy fail: FFT columns use uint16_t. FFT_HEIGHT must be 16"
#endif


#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define SETBIT(b,i) ((((uint8_t *)(b))[(i) >> 3]) |= (1 << ((i) & 7)))
#define CLEARBIT(b,i) ((((uint8_t *)(b))[(i) >> 3]) &= ((1 << ((i) & 7)))^0xFF)


static snd_pcm_t *handle;
static snd_pcm_hw_params_t *params;
static snd_pcm_uframes_t frames;

static int16_t *buffer;

static fftw_plan plan;

static double *time_domain;
static double *freq_domain;

// FFT columns are displayed by setting bits in the flipdot row register
// and selecting only a single column in the col register.
static uint8_t cols[REGISTER_COL_BYTE_COUNT];
// keep the last values to calculate the difference
static uint16_t rows[FFT_WIDTH];

// limit the range of magnitude to display
static double minmag = 0.01;
static double maxmag = 200.0;
static double mindB;

// divide the FFT range by this factor
static unsigned int fft_scale2 = 2;

static char bargraph[9][9] = {
	"        ",
	"X       ",
	"XX      ",
	"XXX     ",
	"XXXX    ",
	"XXXXX   ",
	"XXXXXX  ",
	"XXXXXXX ",
	"XXXXXXXX"
};
		
static unsigned int verbose = 0;
static unsigned int verbose_n = 1;
static unsigned int noflip = 0;
static unsigned int max_changes = 0;
static unsigned int rows_changed_0;
static unsigned int rows_changed_1;
static struct timeval tv0, tv1, tv2, tv4;
static double cur_usec1, cur_usec2;
static double cur_usec3, cur_usec4;
static double max_usec1 = 0, max_usec2 = 0;
static double max_usec3 = 0, max_usec4 = 0;


void usage(void) {
	fprintf(stderr, "Flipdot Spectrum Analyzer\n"
			"Usage:\n"
			"-h       | --help               This text\n"
			"-v       | --verbose            Display timing on stderr for every frame\n"
			"                                Use twice for debug output\n"
			"-e <n>   | --verbose-every <n>  Display debug output every n frames\n"
			"-d <dev> | --device <dev>       ALSA input device name (e.g.: \"hw:1\")\n"
			"-m <val> | --maxmag <val>       Set upper magnitude limit\n"
			"-i <val> | --minmag <val>       Set lower magnitude limit\n"
			"-s <val> | --scale <val>        Divide FFT frequency range\n"
			"                                (e.g: 2 = display half frequency range)\n"
			"-n       | --no-flip            Don't update flipdot display\n"
			"           --dry-run\n"
			"\n");
}

int main(int argc, char **argv) {
	int rc;
	int size;
	int last;
	unsigned int val;
	unsigned int fft_len;
	unsigned int i, vi;
	char *device = NULL;

           
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"verbose", no_argument, NULL, 'v'},
		{"verbose-every", required_argument, NULL, 'e'},
		{"device", required_argument, 0, 'd'},
		{"maxmag", required_argument, 0, 'm'},
		{"minmag", required_argument, 0, 'i'},
		{"scale", required_argument, 0, 's'},
		{"no-flip", no_argument, 0, 'n'},
		{"dry-run", no_argument, 0, 'n'},
		{0, 0, 0, 0}
	};
	int options_index = 0;

	while ((rc = getopt_long(argc, argv, "hve:d:m:i:s:n", long_options, &options_index)) != -1) {
		switch(rc) {
			case 'v':
				verbose++;
				break;
			case 'e':
				verbose_n = atoi(optarg);
				break;
			case 'd':
				device = optarg;
				break;
			case 'm':
				maxmag = atof(optarg);
				break;
			case 'i':
				minmag = atof(optarg);
				break;
			case 's':
				fft_scale2 = atoi(optarg);
				break;
			case 'n':
				noflip = 1;
				break;
			case 'h':
			case '?':
				usage();
				return 2;
			default:
				break;
		}
	}

	if (device == NULL) {
		device = "default";
	}


	/* Open PCM device for recording (capture). */
	if ((rc = snd_pcm_open(&handle, device, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf(stderr, "unable to open pcm device \"%s\": %s\n", device, snd_strerror(rc));
		exit(1);
	}

	/* Allocate a hardware parameters object. */
	snd_pcm_hw_params_alloca(&params);

	/* Fill it in with default values. */
	snd_pcm_hw_params_any(handle, params);

	/* Set the desired hardware parameters. */

	/* Interleaved mode */
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

	/* Format */
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);

	/* Channels */
	snd_pcm_hw_params_set_channels(handle, params, 1);

	/* Sampling rate */
	val = FREQ_MAX * 2;
	snd_pcm_hw_params_set_rate_near(handle, params, &val, NULL);
	fprintf (stderr, "samplerate set to: %d (requested: %d)\n", val, FREQ_MAX * 2);

	// from sndfile-spectrogram:
	/*
	**      Choose a speclen value that is long enough to represent frequencies down
	**      to FREQ_MIN, and then increase it slightly so it is a multiple of 64 so that
	**      FFTW calculations will be quicker.
	*/
	// FFT resolution = samplerate / fft_len
	// align to FFT_WIDTH (two elements ignored)
	//fft_len = 2 + (FFT_WIDTH * ((val / FREQ_MIN / FFT_WIDTH) + 1));
	fft_len = val / FREQ_MIN;
	fft_len = fft_len * FFT_SCALE1;
	double df = (double)val/fft_len;
	fprintf(stderr, "minimum FFT size: %d samples (df = %.2f Hz)\n", fft_len, df);

	if (fft_len & 0x3f) {
		fft_len += 0x40 - (fft_len & 0x3f);
		df = (double)val/fft_len;
		fprintf(stderr, "aligned FFT size: %d samples (df = %.2f Hz)\n", fft_len, df);
	}

	if ((fft_len-2) % FFT_WIDTH != 0) {
		fprintf(stderr, "warning: (fft_len-2) is not a multiple of FFT_WIDTH (%d %% %d = %d)\n", fft_len-2, FFT_WIDTH, (fft_len-2) % FFT_WIDTH);
	}

	/* Set period size */
	frames = fft_len;
	snd_pcm_hw_params_set_period_size_near(handle, params, &frames, NULL);

	/* Write the parameters to the driver */
	if ((rc = snd_pcm_hw_params(handle, params)) < 0) {
		fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
		exit(1);
	}

	/* Use a buffer large enough to hold one period */
	snd_pcm_hw_params_get_period_size(params, &frames, NULL);
	size = frames * sizeof(int16_t);
	if ((buffer = malloc(size)) == NULL) {
		fprintf (stderr, "malloc failed.\n");
		exit (1);
	}

	if (frames != fft_len) {
		fprintf(stderr, "warning: alsa period size not aligned with FFT size (%d != %d)\n", (unsigned int)frames, fft_len);
	}
	fprintf(stderr, "alsa period size set to %d samples = %d bytes/period\n", (unsigned int)frames, size);

	fprintf(stderr, "frame window: %.2fms (%.2f fps)\n", ((double)frames / (double)val)*1000, 1/((double)frames / (double)val));


	time_domain = fftw_alloc_real(fft_len);
	freq_domain = fftw_alloc_real(fft_len);

	if (time_domain == NULL || freq_domain == NULL) {
		fprintf(stderr, "fftw_malloc failed\n");
		exit(1);
	}

	plan = fftw_plan_r2r_1d(fft_len, time_domain, freq_domain, FFTW_R2HC, FFTW_PATIENT | FFTW_DESTROY_INPUT);
	if (plan == NULL) {
		fprintf (stderr, "fftw_plan failed.\n");
		exit (1);
	}

	fputs("FFTW plan:\n", stderr);
	fftw_fprint_plan(plan, stderr);
	fputc('\n', stderr);


#ifndef NOFLIP
	if (!noflip) {
		if (!bcm2835_init()) {
			fprintf(stderr, "bcm2835_init failed\n");
			exit(1);
		}

		flipdot_init();
		flipdot_clear_to_0();
	}
#endif


	if (verbose) {
		if (verbose > 1) {
			fprintf(stderr, "\e[H\e[2J");
			vi = 0;
		}
		tv1.tv_sec = 0;
	}

	memset(rows, 0, sizeof(rows));

	mindB = LOG_SCALE * log10(minmag);
//	minmag = pow(10.0, mindB / SCALE);


	while (1) {

		rc = snd_pcm_readi(handle, buffer, frames);

		if (rc == -EPIPE) {
			/* EPIPE means overrun */
			fprintf(stderr, "overrun occurred\n");
			snd_pcm_prepare(handle);
			continue;
		} else if (rc < 0) {
			fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
			break;
		} else if (rc != (int)frames) {
			fprintf(stderr, "short read, read %d frames\n", rc);
			memset(time_domain, 0, sizeof(time_domain));
		}

		if (verbose) {
			gettimeofday(&tv2, NULL);
		}


		// convert S16_LE integer samples to floating point
		for (i = 0; i < rc; i++) {
			time_domain[i] = (double)buffer[i]/32768;
//			fprintf(stderr, "%d:\t%d\t%f\n", i, buffer[i], time_domain[i]);
		}

		// execute transformation from time_domain to freq_domain
		fftw_execute (plan) ;


		if (verbose) {
			if (verbose > 1 && (vi % verbose_n) == 0) {
				fprintf(stderr, "\e[H");
			}
			rows_changed_0 = 0;
			rows_changed_1 = 0;
			cur_usec4 = 0;
		}

		// skip first element in freq_domain
		last = 1;


		for (i = 0; i < FFT_WIDTH; i++) {
			double sum = 0.0;
			int count = 0;
			uint16_t rows_new;
			uint16_t rows_to_0;
			uint16_t rows_to_1;
			double freq_min, freq_max;

			if (verbose > 1) {
				freq_min = last * df - df/2;
			}


			// FFTW "halfcomplex" format
			// non-redundant half of the complex output
			// real values in freq_domain[0] ... freq_domain[fft_len/2]
			// imaginary values in freq_domain[fft_len-1] ... freq_domain[(fft_len/2)+1]
			// values in freq_domain[0] and freq_domain[fft_len/2] have no imaginary parts
			// http://www.fftw.org/fftw3_doc/The-Halfcomplex_002dformat-DFT.html

			// scale FFT bins to FFT_WIDTH
			while (last <= ((i+1) * (fft_len/2/fft_scale2)) / FFT_WIDTH && last < (fft_len/2/fft_scale2)) {
				// magnitude = sqrt(r^2 + i^2)
				sum += sqrt((freq_domain[last] * freq_domain[last]) +
						(freq_domain[fft_len - last] * freq_domain[fft_len - last]));
				last++;
				count++;
//				fprintf(stderr, "i = %d (%d), last = %d, count = %d\n", i, ((i+1) * (fft_len/2/fft_scale2)) / FFT_WIDTH, last, count);
			}

			if (count == 0) {
				if (verbose) {
					fprintf(stderr, "bug: no data for index %d\n", i);
				}
				continue;
			}


			// calculate dB and scale to FFT_HEIGHT
			double mag = MAX(minmag, MIN(maxmag, sum / count));
			double ydB = LOG_SCALE * log10(mag / maxmag);
			int bar = MAX(0, MIN(FFT_HEIGHT, (int)round(((ydB / -mindB) * FFT_HEIGHT) + FFT_HEIGHT)));

			// calculate difference pattern for flipdot row register
			rows_new = ~(0xFFFF >> bar);
			rows_to_0 = ((rows[i]) & ~(rows_new));
			rows_to_1 = (~(rows[i]) & (rows_new));
			rows[i] = rows_new;


			if (verbose) {
				if (verbose > 1 && (vi % verbose_n) == 0) {
					freq_max = last * df - df/2;

					fprintf(stderr, "%2d: mag = %3.2f  ydB = %3.2f  \t"
						"bar = %2d  %8s  rows_new = %5u  rows_to_0 = %5u  rows_to_1 = %5u  %c%c  %5d - %5d Hz\e[K\n",
						i, mag, ydB, bar, bargraph[bar/2], rows_new, rows_to_0, rows_to_1,
						(rows_to_0)?('X'):(' '), (rows_to_1)?('X'):(' '), (int)round(freq_min), (int)round(freq_max));
				}

				if (rows_to_0) {
					rows_changed_0++;
				}
				if (rows_to_1) {
					rows_changed_1++;
				}

				gettimeofday(&tv4, NULL);
			}


#ifndef NOFLIP
			if (!noflip) {
				// https://wiki.attraktor.org/FlipdotDisplay#Logisch

				// black -> white (OE0)
				if (rows_to_0) {
					memset(cols, 0xFF, sizeof(cols));
					CLEARBIT(cols, i + ((i / MODULE_COLS) * COL_GAP));
					flipdot_display_row_single((uint8_t *)&rows_to_0, cols, 0);
				}

				// white -> black (OE1)
				if (rows_to_1) {
					memset(cols, 0x00, sizeof(cols));
					SETBIT(cols, i + ((i / MODULE_COLS) * COL_GAP));
					flipdot_display_row_single((uint8_t *)&rows_to_1, cols, 1);
				}
			}
#endif


			if (verbose) {
				gettimeofday(&tv0, NULL);
				cur_usec4 += ((tv0.tv_sec*1000000) + tv0.tv_usec) - ((tv4.tv_sec*1000000) + tv4.tv_usec);
			}
		}


		if (verbose) {
			max_changes = MAX(max_changes, rows_changed_0 + rows_changed_1);

			gettimeofday(&tv0, NULL);
			cur_usec2 = ((tv0.tv_sec*1000000) + tv0.tv_usec) - ((tv2.tv_sec*1000000) + tv2.tv_usec);
			cur_usec3 = cur_usec2 - cur_usec4;

			if (tv1.tv_sec != 0) {
				cur_usec1 = ((tv0.tv_sec*1000000) + tv0.tv_usec) - ((tv1.tv_sec*1000000) + tv1.tv_usec);

				max_usec1 = MAX(max_usec1, cur_usec1); 
				max_usec2 = MAX(max_usec2, cur_usec2); 
				max_usec3 = MAX(max_usec3, cur_usec3); 
				max_usec4 = MAX(max_usec4, cur_usec4); 
			}

			fprintf(stderr, "\n"
					"flipdot changes: %2d + %2d = %3d (max: %3d)\n"
					"frame processed in \t%.2fms   \t(max: %.2fms)\n"
					"frame displayed in \t%.2fms   \t(max: %.2fms)\n"
					"total frame time: \t%.2fms   \t(max: %.2fms)\n"
					"time incl. read: \t%.2fms   \t(max: %.2fms)\n",
					rows_changed_0, rows_changed_1, rows_changed_0 + rows_changed_1, max_changes,
					cur_usec3/1000, max_usec3/1000, cur_usec4/1000, max_usec4/1000,
					cur_usec2/1000, max_usec2/1000, cur_usec1/1000, max_usec1/1000);

			tv1 = tv0;

			if (last != (fft_len/2/fft_scale2)) {
				fprintf(stderr, "bug: last != (fft_len/2/fft_scale2)-1: %d, %d\n", last, (fft_len/2/fft_scale2));
				fprintf(stderr, "\e[1A");
			}

			fprintf(stderr, "\e[6A");

			if (verbose > 1 && (vi++ % verbose_n) == 0) {
				vi = 1;
			}
		}


/*
		if (rc = write(1, buffer, size) != size) {
			fprintf(stderr, "short write: wrote %d bytes\n", rc);
		}
*/
	}


#ifndef NOFLIP
	if (!noflip) {
		flipdot_shutdown();
	}
#endif

	snd_pcm_drain(handle);
	snd_pcm_close(handle);
	free(buffer);

	fftw_free(time_domain);
	fftw_free(freq_domain);
	fftw_destroy_plan(plan);
	fftw_cleanup();

	return 0;
}
