# SPDX-License-Identifier: GPL-2.0-or-later
#
# Makefile for the linux exFAT filesystem support.
#
ifneq ($(KERNELRELEASE),)
obj-$(CONFIG_EXFAT_FS) += exfat.o

exfat-y	:= inode.o namei.o dir.o super.o fatent.o cache.o nls.o misc.o \
	   file.o balloc.o
else
# Called from external kernel module build

KERNELRELEASE	?= $(shell uname -r)
KDIR	?= /lib/modules/${KERNELRELEASE}/build
MDIR	?= /lib/modules/${KERNELRELEASE}
PWD	:= $(shell pwd)

export CONFIG_EXFAT_FS := m

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

help:
	$(MAKE) -C $(KDIR) M=$(PWD) help

install: exfat.ko
	rm -f ${MDIR}/kernel/fs/exfat/exfat.ko
	install -m644 -b -D exfat.ko ${MDIR}/kernel/fs/exfat/exfat.ko
	depmod -aq

uninstall:
	rm -rf ${MDIR}/kernel/fs/exfat
	depmod -aq

endif

.PHONY : all clean install uninstall
