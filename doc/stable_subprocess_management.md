# stable subprocess management

ffio provides simple Python interfaces to FFmpeg-C-API via `ctypes`.

Most other third python packages for wrapping FFmpeg only fork a subprocess using `subprocess` or `multiprocess` to initiate an `ffmpeg` command process by OS. The main problem of this method is the instability of the program. 

If processing the online video streams, you will find that many network blocking problems will have an influence on the `ffmpeg` command process, causing `defunct` which can not be detected by the main process.


WARNING: nvidia-installer was forced to guess the X library path '/usr/lib64' and X module path                          
   '/usr/lib64/xorg/modules'; these paths were not queryable from the system.  If X fails to find the NVIDIA X             
   driver module, please install the `pkg-config` utility and the X.Org SDK/development package for your 
   distribution and reinstall the driver.