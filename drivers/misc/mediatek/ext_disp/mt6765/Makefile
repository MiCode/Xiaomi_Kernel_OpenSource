#
# Copyright (C) 2017 MediaTek Inc.
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
# Makefile for external display driver.
#
CMDQ_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/ \
             -I$(srctree)/drivers/misc/mediatek/ext_disp/  \
             -I$(srctree)/drivers/misc/mediatek/ext_disp/include/  \
             -I$(srctree)/drivers/misc/mediatek/ext_disp/common/test/ \
             -I$(srctree)/drivers/misc/mediatek/gpu/ged/include \
             -I$(srctree)/drivers/misc/mediatek/video/common \
             -I$(srctree)/drivers/misc/mediatek/video/include/ \
             -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys/ \
             -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox/ \
             -I$(srctree)/drivers/misc/mediatek/sync/ \
             -I$(srctree)/drivers/misc/mediatek/hdmi/$(CONFIG_CUSTOM_KERNEL_HDMI)/ \
             -I$(srctree)/drivers/misc/mediatek/cmdq/v3/ \
             -I$(srctree)/drivers/misc/mediatek/hdmi/inc/ \
             -I$(srctree)/drivers/misc/mediatek/smi \
             -I$(srctree)/drivers/staging/android/ion \
             -I$(srctree)/drivers/staging/android/ion/mtk \
             -I$(srctree)/drivers/misc/mediatek/lcm/inc

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(CMDQ_PLATFORM)/

ifeq ($(CONFIG_MTK_PLATFORM), "mt6761")
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/mt6761 \
                    -I$(srctree)/drivers/misc/mediatek/m4u/2.0
else
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM) \
                    -I$(srctree)/drivers/misc/mediatek/m4u/2.0
endif

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/

ifeq ($(CONFIG_CUSTOM_KERNEL_HDMI), "MT8193")
ccflags-y +=	-DHDMI_MT8193_SUPPORT
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/mt8193/
endif

ifeq ($(CONFIG_CUSTOM_KERNEL_HDMI), "Sii8348")
ccflags-y +=	-DSII8348_SUPPORT
endif
ifeq ($(CONFIG_CUSTOM_KERNEL_HDMI), "ANX7805")
ccflags-y +=	-DANX7805_SUPPORT
endif

obj-y :=  \
    external_display.o \
    mtk_extd_mgr.o   \
    extd_debug.o \
    extd_utils.o \
    extd_hdmi.o \
    extd_epd.o \
    extd_lcm.o \
    extd_multi_control.o \
    extd_factory.o
