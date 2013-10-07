Flipdot vlc video output plugin
===============================

Installation
------------

* Compile flipdot library
* Compile vlc with `--enable-run-as-root`
  * or `apt-get install vlc libvlc-dev` and
    [patch](http://www.linuxquestions.org/questions/linux-general-1/solved-vlc-running-under-root-without-compiling-748189/)
    the binary:  
    `sed -e 's/geteuid/getppid/' </usr/bin/vlc >./vlc`
  * or patch vlc to [call bcm2835_init in vlc-wrapper](patches/vlc-wrapper-bcm2835.diff)
* `make && sudo make install`
* `sudo ./vlc -V flipdot`
* adjust output with video filters:  
  `grain{variance=10}:adjust{brightness=1.23,brightness-threshold}`


netsync
-------

One Raspberry Pi should be able to drive at least 3x3 modules at 25fps. Distribute a stream to multiple Pis with `netsync` control.
Here's an example for 4 Pis with 120x96 pixels in total:

Optionally transcode a stream into dithered black-and-white scaled to the total display size and distributed via multicast:  

    rvlc $url --sout '#transcode{width=120,height=96,  
    vfilter={grain{variance=10}:adjust{brightness=1.23,brightness-threshold}},  
    deinterlace,fps=25,vcodec=h264,venc=x264{preset=faster,profile=main}}  
    :std{access=udp,dst=239.255.1.2,mux=ts{use-key-frames}}'

10.0.0.1: top left, netsync master and audio player  

    rvlc -V flipdot --control netsync --netsync-master  
    --video-filter "croppadd{cropbottom=48,cropright=60}"  
    udp://@239.255.1.2:1234

10.0.0.2: top right  

    rvlc -V flipdot --no-audio --control netsync --netsync-master-ip 10.0.0.1  
    --video-filter "croppadd{cropbottom=48,cropleft=60}"  
    udp://@239.255.1.2:1234

10.0.0.3: bottom left  

    rvlc -V flipdot --no-audio --control netsync --netsync-master-ip 10.0.0.1  
    --video-filter "croppadd{croptop=48,cropright=60}"  
    udp://@239.255.1.2:1234

10.0.0.4: bottom right  

    rvlc -V flipdot --no-audio --control netsync --netsync-master-ip 10.0.0.1  
    --video-filter "croppadd{croptop=48,cropleft=60}"  
    udp://@239.255.1.2:1234
