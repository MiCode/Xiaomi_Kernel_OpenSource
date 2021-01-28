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

################################################################################
# MT67xx Chip Set Group
#
#
################################################################################

ifeq ($(CONFIG_MTK_PLATFORM), "mt6761")
	MTK_PLATFORM := mt6765
else
	MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif

# remove this after k79 config CONFIG_MACH_MT6779 & CONFIG_MTK_PLATFORM in defconfig
ifeq ($(CONFIG_MTK_DISP_PLATFORM), "mt6779")
	CONFIG_MACH_MT6779 := y
	MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_DISP_PLATFORM))
endif

obj-$(CONFIG_MTK_FB) +=


ccflags-y += \
				-I$(srctree)/drivers/misc/mediatek/video/include     \
				-I$(srctree)/drivers/misc/mediatek/video/common     \
				-I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM) \
				-I$(srctree)/drivers/misc/mediatek/sync/      \
				-I$(srctree)/drivers/misc/mediatek/cmdq/v2/ \
				-I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(MTK_PLATFORM)/ \
				-I$(srctree)/drivers/misc/mediatek/ext_disp/$(MTK_PLATFORM) \
				-I$(srctree)/drivers/staging/android/ion

ifeq ($(CONFIG_MTK_PLATFORM), "mt6761")
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/mt6761
else
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)

ifeq ($(CONFIG_MTK_INTERNAL_HDMI_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/
endif
ifeq ($(CONFIG_MTK_INTERNAL_MHL_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mhltx/
endif
ifeq ($(CONFIG_MTK_DITHERING_SUPPORT), y)
ccflags-y += -DDITHERING_SUPPORT
endif

ifeq ($(CONFIG_MTK_GPU_SUPPORT), y)
ccflags-y += -DHWGPU_SUPPORT
endif

