# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)

######################### fortsense driver config begin #########################

#just for mtk platform head file, kernel version less than kernle 4.0
#mtk平台，内核版本小于4.0参考头文件
ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)

#just for mtk platform head file, kernel version larger than kernle 4.0
#mtk平台，内核版本大于4.0参考头文件
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

#just for beanpod v2 compatible
#豆荚 V2 TEE平台，兼容头文件
ccflags-y += -I$(srctree)/drivers/misc/mediatek/teei/V1.0/tz_vfs

#甁砵 TEE 平台，兼容头文件
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/tkcore/core
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/tkcore/include/

# Android L
#include $(srctree)/drivers/misc/mediatek/Makefile.custom

#fortsense driver file
# 如果没有移植 Kconfig 导致驱动没编译到，请打开下面的 obj-y 并屏蔽 obj-$(CONFIG_FINGERPRINT_FORTSENSE)
obj-y                                     += fortsense_fp.o
#obj-$(CONFIG_FINGERPRINT_FORTSENSE)       += fortsense_fp.o
fortsense_fp-objs                         := sf_ctl.o sf_hw.o

########################## fortsense driver config end ##########################

else

KBUILDDIR := $(ANDROID_PRODUCT_OUT)/obj/KERNEL_OBJ
MODULEPWD := $(shell pwd)

.PHONY: all install clean

# $(MAKE) -C $(KBUILDDIR) ARCH=arm64 CROSS_COMPILE=aarch64-linux-android-
# $(MAKE) -C $(KBUILDDIR) ARCH=arm CROSS_COMPILE=arm-eabi-

all:
	$(MAKE) -C $(KBUILDDIR) ARCH=arm64 CROSS_COMPILE=aarch64-linux-android- \
		M=$(MODULEPWD) CONFIG_FINGERPRINT_FORTSENSE=m modules

install:
	adb remount
	adb push fortsense_fp.ko /data/
	adb shell sync

clean:
	rm -rf *.ko *.o *.mod.c .*.cmd .*tmp_versions *.symvers *.order 

endif # End of KERNELRELEASE.
