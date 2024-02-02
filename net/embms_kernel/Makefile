#
# Makefile for Embms Kernel module.
#

KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

obj-m += embms_kernel.o

ccflags-y += -D__CHECK_ENDIAN__

CDEFINES += -D__CHECK_ENDIAN__

KBUILD_CPPFLAGS += $(CDEFINES)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(shell pwd) modules
modules_install:
	$(MAKE) INSTALL_MOD_STRIP=1 -C $(KERNEL_SRC) M=$(shell pwd) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) clean

