# ARM DX910
1. Goal of this project
Providing all drivers for ARM DX910 powered devices. For example if you want to install the drivers for Hardkernels Odroid U3, you'll need to install `arm-dx910-odroidu3` and dependencies like the packaged libMali blob and the needed kernel drivers will be added as DKMS packages. So in case you want to upgrade your kernel, dkms will manage to build the mali kernel driver for you on kernel installations.

2. TODO:
  * Well, getting the r5p0 driver working with a correct devicetree entry. At the moment devicetree entries are missing in mainline and Hardkernels 3.8.y kernel does not use the devicetree method at all.
  * Since the driver is working, I'll start to package alternative drivers, just in case of interest like fbturbo, and maybe provide (correct!) linux packages. At the moment the devicetree binaries (*.dtb) are stored at /lib/firmware, which is not correct. Hopefully it will change in Ubuntu Xenial (current development release)

3. Tested devices/environments
  2. Hardkernel Odroid U3:
  OS: Ubuntu Wily Werewolf (15.10)
  DDX driver: xf86-video-armsoc (http://cgit.freedesktop.org/xorg/driver/xf86-video-armsoc/)
  Kernel: 4.3.0 (mainline)
  Mali400-MP4 with r5p0 driver: DT-entry still missing (https://community.arm.com/thread/9145)

4. Useful resources
  * https://github.com/ssvb/xf86-video-fbturbo/wiki
