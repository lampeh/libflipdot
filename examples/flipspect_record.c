// sndfile-tools-1.03/src/sndfile-spectrogram.c
// http://stackoverflow.com/questions/6666807/how-to-scale-fft-output-of-wave-file
// http://www.linuxjournal.com/article/6735?page=0,2


#define ALSA_PCM_NEW_HW_PARAMS_API

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <alsa/asoundlib.h>
#include <fftw3.h>

#include <bcm2835.h>
#include <flipdot.h>


// disable flipdot output for debugging
#define NOFLIP
// display state on stderr for every frame
#define VERBOSE

#define FFT_WIDTH DISP_COLS
#define FFT_HEIGHT DISP_ROWS

#if (FFT_HEIGHT != 16)
#error "lazy fail: FFT columns use uint16_t. FFT_HEIGHT must be 16"
#endif

// used in FFT size calculation from sndfile-spectrogram
//#define FREQ_MIN 20
#define FREQ_MIN 40

//#define FREQ_MAX 8000
#define FREQ_MAX 10000
//#define FREQ_MAX 16000
//#define FREQ_MAX 20000

#define SCALE 10


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
const static double minmag = 0.01;
const static double maxmag = 200.0;
static double mindB;


int main(void) {
	int rc;
	unsigned int val;
	unsigned int fft_len;
	int dir = 0;
	int size;
	int i;
	int last;

	/* Open PCM device for recording (capture). */
	if ((rc = snd_pcm_open(&handle, "hw:1", SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
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
	snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
	fprintf (stderr, "samplerate set to: %d (requested: %d)\n", val, FREQ_MAX * 2);

	// from sndfile-spectrogram:
	/*
	**      Choose a speclen value that is long enough to represent frequencies down
	**      to FREQ_MIN, and then increase it slightly so it is a multiple of 0x40 so that
	**      FFTW calculations will be quicker.
	*/
	fft_len = FFT_WIDTH * ((val / FREQ_MIN / FFT_WIDTH) + 1);
//	fft_len = fft_len * 2;
	fprintf(stderr, "minimum FFT size: %d samples\n", fft_len);

	fft_len += 0x40 - (fft_len & 0x3f);
	fprintf(stderr, "aligned FFT size: %d samples\n", fft_len);

	if ((fft_len-2) % FFT_WIDTH != 0) {
		fprintf(stderr, "warning: (fft_len-2) is not a multiple of FFT_WIDTH (%d %% %d = %d)\n", fft_len, FFT_WIDTH, fft_len % FFT_WIDTH);
	}

	/* Set period size */
	frames = fft_len;
	snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

	/* Write the parameters to the driver */
	if ((rc = snd_pcm_hw_params(handle, params)) < 0) {
		fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
		exit(1);
	}

	/* Use a buffer large enough to hold one period */
	snd_pcm_hw_params_get_period_size(params, &frames, &dir);
	size = frames * sizeof(int16_t);
	if ((buffer = malloc(size)) == NULL) {
		fprintf (stderr, "malloc failed.\n");
		exit (1);
	}

	if (frames != fft_len) {
		fprintf(stderr, "warning: alsa period size not aligned with FFT size (%d != %d)\n", frames, fft_len);
	}
	fprintf(stderr, "alsa period size set to %d samples = %d bytes/period\n", frames, size);

	time_domain = fftw_malloc(fft_len * sizeof(double));
	freq_domain = fftw_malloc(fft_len * sizeof(double));

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
	if (!bcm2835_init()) {
		fprintf(stderr, "bcm2835_init failed\n");
		exit(1);
	}

	flipdot_init();
	flipdot_clear_to_0();
#endif

	memset(rows, 0, sizeof(rows));

	mindB = SCALE * log10(minmag);
//	minmag = pow(10.0, mindB / SCALE);

#ifdef VERBOSE
	fprintf(stderr, "\e[H\e[2J");
	int max_changes = 0;
#endif

	while (1) {
		rc = snd_pcm_readi(handle, buffer, frames);

		if (rc == -EPIPE) {
			/* EPIPE means overrun */
			fprintf(stderr, "overrun occurred\n");
			snd_pcm_prepare(handle);
			continue;
		} else if (rc < 0) {
			fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
			continue;
		} else if (rc != (int)frames) {
			fprintf(stderr, "short read, read %d frames\n", rc);
			memset(time_domain, 0, sizeof(time_domain));
		}

#ifdef VERBOSE
		fprintf(stderr, "\e[H");
		int rows_changed_0 = 0;
		int rows_changed_1 = 0;
#endif

		for (i = 0; i < rc; i++) {
			time_domain[i] = (double)buffer[i]/32768;
//			fprintf(stderr, "%d:\t%d\t%f\n", i, buffer[i], time_domain[i]);
		}

		fftw_execute (plan) ;

		last = 1;
		for (i = 0; i < FFT_WIDTH; i++) {
			double sum = 0.0;
			int count = 0;
			uint16_t rows_new;
			uint16_t rows_to_0;
			uint16_t rows_to_1;

			// FFTW "halfcomplex" format
			// real values in freq_domain[0] ... freq_domain[fft_len/2]
			// imaginary values in freq_domain[fft_len-1] ... freq_domain[(fft_len/2)+1]
			// values in freq_domain[0] and freq_domain[fft_len/2] have no imaginary parts
			// http://www.fftw.org/fftw3_doc/The-Halfcomplex_002dformat-DFT.html

			// scale FFT bins to FFT_WIDTH
			do {
				// magnitude = sqrt(r^2 + i^2)
				sum += sqrt((freq_domain[last] * freq_domain[last]) +
						(freq_domain[fft_len - last] * freq_domain[fft_len - last]));
				last++;
				count++;
//				fprintf(stderr, "i = %d (%d), last = %d, count = %d\n", i, ((i+1) * (fft_len/2)) / FFT_WIDTH, last, count);
			} while (last <= ((i+1) * (fft_len/2)) / FFT_WIDTH && last < (fft_len/2));

			// calculate dB and scale to FFT_HEIGHT
			double mag = MAX(minmag, MIN(maxmag, sum / count));
			double ydB = SCALE * log10(mag / maxmag);
			int bar = MAX(0, MIN(FFT_HEIGHT, (int)round(((ydB / -mindB) * FFT_HEIGHT) + FFT_HEIGHT)));

			// calculate difference pattern for flipdot row register
			rows_new = ~(0xFFFF >> bar);
			rows_to_0 = ((rows[i]) & ~(rows_new));
			rows_to_1 = (~(rows[i]) & (rows_new));
			rows[i] = rows_new;

#ifdef VERBOSE
			fprintf(stderr, "%2d: mag = %3.4f  ydB = %3.4f   \tbar = %2d  rows_new = %5u  rows[i] = %5u  rows_to_0 = %5u  rows_to_1 = %5u  %c%c\e[K\n",
				i, mag, ydB, bar, rows_new, rows[i], rows_to_0, rows_to_1, (rows_to_0)?('X'):(' '), (rows_to_1)?('X'):(' '));

			if (rows_to_0) {
				rows_changed_0++;
			}
			if (rows_to_1) {
				rows_changed_1++;
			}
#endif

#ifndef NOFLIP
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
#endif
		}

#ifdef VERBOSE
		max_changes = MAX(max_changes, rows_changed_0 + rows_changed_1);
		fprintf(stderr, "flipdot changes: %2d + %2d = %3d (max: %3d)\n", rows_changed_0, rows_changed_1, rows_changed_0 + rows_changed_1, max_changes);

		if (last != (fft_len/2)) {
			fprintf(stderr, "bug: last != (fft_len/2)-1: %d, %d\n", last, (fft_len/2));
		}
#endif

/*
		if (rc = write(1, buffer, size) != size) {
			fprintf(stderr, "short write: wrote %d bytes\n", rc);
		}
*/
	}

#ifndef NOFLIP
	flipdot_shutdown();
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
