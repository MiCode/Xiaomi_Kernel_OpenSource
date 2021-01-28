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

obj-y += layering_rule_base.o

ccflags-y += \
                                -I$(srctree)/drivers/misc/mediatek/video/include     \
                                -I$(srctree)/drivers/misc/mediatek/video/common/IPv1     \
                                -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys \
                                -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox \
                                -I$(srctree)/drivers/misc/mediatek/sync/      \
                                -I$(srctree)/drivers/misc/mediatek/lcm/inc/ \

ccflags-y += \
                                -I$(srctree)/drivers/misc/mediatek/cmdq/v3/ \
                                -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/

ifeq ($(CONFIG_MACH_MT6779), y)
    ccflags-y += -DCONFIG_MACH_MT6779
endif
