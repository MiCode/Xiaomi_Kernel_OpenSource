exfat-nofuse
============

Linux non-fuse read/write kernel driver for the exFAT, FAT12, FAT16 and vfat (FAT32) file systems.<br />
Originally ported from Android kernel v3.0.

Kudos to ksv1986 for the mutex patch!<br />
Thanks to JackNorris for being awesome and providing the clear_inode() patch.<br />
<br />
Big thanks to lqs for completing the driver!<br />
Big thanks to benpicco for fixing 3.11.y compatibility!


Special thanks to github user AndreiLux for spreading the word about the leak!<br />


Installing as a stand-alone module:
====================================

    make
    sudo make install

To load the driver manually, run this as root:

    modprobe exfat

You may also specify custom toolchains by using CROSS_COMPILE flag, in my case:
>CROSS_COMPILE=../dorimanx-SG2-I9100-Kernel/android-toolchain/bin/arm-eabi-

Installing as a part of the kernel:
======================================

Let's take [linux] as the path to your kernel source dir...

	cd [linux]
	cp -rvf exfat-nofuse [linux]/fs/exfat

edit [linux]/fs/Kconfig
```
 menu "DOS/FAT/NT Filesystems"

  source "fs/fat/Kconfig"
 +source "fs/exfat/Kconfig"
  source "fs/ntfs/Kconfig"
  endmenu
```
  

edit [linux]/fs/Makefile
```
  obj-$(CONFIG_FAT_FS)    += fat/
 +obj-$(CONFIG_EXFAT_FS)  += exfat/
  obj-$(CONFIG_BFS_FS)    += bfs/
```

	cd [linux]
	make menuconfig

Go to:
> File systems > DOS/FAT/NT
>   check exfat as MODULE (M)
>   (437) Default codepage for exFAT
>   (utf8) Default iocharset for exFAT

> ESC to main menu
> Save an Alternate Configuration File
> ESC ESC

build your kernel

Have fun.


Installing as a DKMS module:
=================================

You can have even more fun with exfat-nofuse by installing it as a DKMS module has the main advantage of being auto-compiled (and thus, possibly surviving) between kernel upgrades.

First, get dkms. On Ubuntu this should be:

	sudo apt install dkms

Then copy the root of this repository to /usr/share:

	sudo cp -R . /usr/src/exfat-1.2.8 (or whatever version number declared on dkms.conf is)
	sudo dkms add -m exfat -v 1.2.8

Build and load the module:

	sudo dkms build -m exfat -v 1.2.8
	sudo dkms install -m exfat -v 1.2.8

Now you have a proper dkms module that will work for a long time... hopefully.



Free Software for the Free Minds!
=================================
