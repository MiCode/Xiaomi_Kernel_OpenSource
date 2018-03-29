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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/include/ \
             -I$(srctree)/drivers/misc/mediatek/video/common/ \
             -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/ \
             -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox/ \
             -I$(srctree)/drivers/misc/mediatek/cmdq/v2/ \
             -I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(MTK_PLATFORM)/ \
             -I$(srctree)/drivers/misc/mediatek/base/power/mt6755/ \
             -I$(srctree)/drivers/misc/mediatek/lcm/inc \
             -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/ \
             -I$(srctree)/drivers/misc/mediatek/mmp/

ifeq ($(CONFIG_ARCH_MT6735),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys/mt6735/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox/mt6735/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/mt6735/
endif
ifeq ($(CONFIG_ARCH_MT6735M),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys/mt6735m/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox/mt6735m/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/mt6735m/
endif
ifeq ($(CONFIG_ARCH_MT6753),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys/mt6753/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox/mt6753/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/mt6753/
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys

obj-y += ddp_color.o

ifeq ($(strip $(CONFIG_MTK_PQ_COLOR_MODE)),"MDP")
    ccflags-y += -DDISP_COLOR_OFF
else ifeq ($(strip $(CONFIG_MTK_PQ_COLOR_MODE)),"OFF")
    ccflags-y += -DDISP_COLOR_OFF
endif

ifeq (y, $(filter y, $(CONFIG_ARCH_MT6755) $(CONFIG_ARCH_MT6797)))
    ccflags-y += -DCCORR_SUPPORT
endif

ifeq ($(strip $(CONFIG_MTK_PQ_COLOR_MODE)),"MDP")
    ccflags-y += -DMDP_COLOR_ON
else ifeq ($(strip $(CONFIG_MTK_PQ_COLOR_MODE)),"DISP_MDP")
    ccflags-y += -DDISP_MDP_COLOR_ON
else ifeq ($(strip $(CONFIG_MTK_PQ_COLOR_MODE)),"DISP")
    ccflags-y += -DDISP_COLOR_ON
endif

ifeq ($(strip $(CONFIG_MTK_LCM_PHYSICAL_ROTATION)),"180")
    ccflags-y += -DLCM_PHYSICAL_ROTATION_180
else ifeq ($(strip $(CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW)),y)
    ccflags-y += -DLCM_PHYSICAL_ROTATION_180
endif
