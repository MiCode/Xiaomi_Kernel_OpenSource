#
# Makefile for the linux FAT12/16/32(VFAT)/64(exFAT) filesystem driver.
#

obj-$(CONFIG_SDFAT_FS) += sdfat_fs.o

sdfat_fs-objs	:= sdfat.o core.o core_fat.o core_exfat.o api.o blkdev.o \
		   fatent.o amap_smart.o cache.o dfr.o nls.o misc.o \
		   mpage.o extent.o

sdfat_fs-$(CONFIG_SDFAT_VIRTUAL_XATTR) += xattr.o
sdfat_fs-$(CONFIG_SDFAT_STATISTICS) += statistics.o


all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

cscope:
	rm -rf cscope.files cscope.files
	find $(PWD) \( -name '*.c' -o -name '*.cpp' -o -name '*.cc' -o -name '*.h' -o -name '*.s' -o -name '*.S' \) -print > cscope.files
	cscope
