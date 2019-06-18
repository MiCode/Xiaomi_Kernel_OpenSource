# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)

MFD_TOP = $(srctree)/drivers/mfd
PLAT_TOP = $(srctree)/drivers/misc/mediatek/include/mt-plat

ifeq ($(CONFIG_MTK_CLKBUF_BBLPM),y)
    ccflags-y += -D BBLPM_SUPPORT=1
else
    ccflags-y += -D BBLPM_SUPPORT=0
endif

ifeq ($(CONFIG_MTK_CLK_BUFFER),y)
    ccflags-y += -I$(src)/src \
	     -I$(srctree)/include/ \
	     -I$(MFD_TOP)/ \
	     -I$(PLAT_TOP)/

    obj-$(CONFIG_MTK_CLK_BUFFER) += clkbuf.o

    clkbuf-y += mtk-clkbuf-bridge.o \
		src/mtk-clkbuf-ctl.o \
		src/mtk-clkbuf-hw.o
else
    ccflags-y += -I$(srctree)/include/ \
	     -I$(PLAT_TOP)/
    obj-y += mtk-clkbuf-bridge.o
endif


# Otherwise we were called directly from the command line;
# invoke the kernel build system.
else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD  := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif
