#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

#
# Makefile for misc devices that really don't fit anywhere else.
#

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))

MTK_CUSTOM_PATH  := $(srctree)/drivers/misc/mediatek

ifneq ($(CONFIG_ARM64), y)
# For arm32, mach folder
MTK_PLATFORM_PATH_ROOT := $(srctree)/arch/arm/mach-$(MTK_PLATFORM)
MTK_PROJECT_PATH_ROOT := $(srctree)/arch/arm/mach-$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)
  ifneq ($(filter y,$(CONFIG_ARCH_MT6752) $(CONFIG_ARCH_MT6732) $(CONFIG_ARCH_MT6595) $(CONFIG_ARCH_MT6795) $(CONFIG_ARCH_MT6735) $(CONFIG_ARCH_MT6735M) $(CONFIG_ARCH_MT6753) $(CONFIG_ARCH_MT6580)),)
    # For armv8 32bit
    MTK_PLATFORM_PATH_ROOT := $(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)
    MTK_PROJECT_PATH_ROOT := $(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)
  endif
else
# For arm64, no mach-xxx folder
MTK_PLATFORM_PATH_ROOT := $(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)
MTK_PROJECT_PATH_ROOT := $(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)
endif



subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/sub/inc

ifeq ($(CONFIG_MTK_LENS_USE_MODULE), y)

SUB_CFILES += sub_lens.c

ifeq ($(CONFIG_MTK_LENS_AD5820AF_SUPPORT),y)
SUB_CFILES += common/ad5820af/AD5820AF.c
endif

ifeq ($(CONFIG_MTK_LENS_DW9714AF_SUPPORT),y)
SUB_CFILES += common/dw9714af/DW9714AF.c
endif
ifeq ($(CONFIG_MTK_LENS_LC898212AF_SUPPORT),y)
SUB_CFILES += common/lc898212af/LC898212AF.c
endif

obj-m := subaf.o
subaf-objs := $(SUB_CFILES:.c=.o)

else

obj-y      += sub_lens.o
obj-$(CONFIG_MTK_LENS_FM50AF_SUPPORT)      += common/fm50af/
obj-$(CONFIG_MTK_LENS_BU6424AF_SUPPORT)    += common/bu6424af/
obj-$(CONFIG_MTK_LENS_BU6429AF_SUPPORT)    += common/bu6429af/
obj-$(CONFIG_MTK_LENS_DW9714AF_SUPPORT)    += common/dw9714af/
obj-$(CONFIG_MTK_LENS_DW9814AF_SUPPORT)    += common/dw9814af/
obj-$(CONFIG_MTK_LENS_DW9718AF_SUPPORT)    += common/dw9718af/
obj-$(CONFIG_MTK_LENS_LC898212AF_SUPPORT)  += common/lc898212af/
obj-$(CONFIG_MTK_LENS_LC898214AF_SUPPORT)  += common/lc898214af/
obj-$(CONFIG_MTK_LENS_AD5820AF_SUPPORT)    += common/ad5820af/
obj-$(CONFIG_MTK_LENS_WV511AAF_SUPPORT)    += common/wv511aaf/
obj-$(CONFIG_MTK_LENS_AK7371AF_SUPPORT)    += common/ak7371af/

endif