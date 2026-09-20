# Copyright (C) 2018 XiaoMi, Inc.
# Makefile for the fingerprint device.
#
xiaomi_spi_tee-objs := fp_driver.o fp_netlink.o fp_platform.o
obj-$(CONFIG_XIAOMI_FINGERPRINT) := xiaomi_spi_tee.o

$(warning "gjx CONFIG_FP_MTK_PLATFORM is $(CONFIG_FP_MTK_PLATFORM)")
#add some mtk makefile
# Each configuration option enables a list of files.
ifeq ($(CONFIG_FP_MTK_PLATFORM),y)
include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
subdir-ccflags-y += -I$(srctree)/drivers/tee/teei/510/tz_driver/include/
subdir-ccflags-y += -I$(srctree)/drivers/tee/teei/510/common/include/

CDEFINES += -DCONFIG_FP_MTK_PLATFORM
KBUILD_CPPFLAGS += $(CDEFINES)

endif

