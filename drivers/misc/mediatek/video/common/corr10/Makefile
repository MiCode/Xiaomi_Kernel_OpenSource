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

obj-y += ddp_dither.o
obj-y += ddp_gamma.o

ccflags-y += \
				-I$(srctree)/drivers/misc/mediatek/video/include     \
				-I$(srctree)/drivers/misc/mediatek/video/common/IPv1     \
				-I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys \
				-I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox \
				-I$(srctree)/drivers/misc/mediatek/sync/      \
				-I$(srctree)/drivers/misc/mediatek/lcm/inc/

ifeq (y, $(filter y, $(CONFIG_MACH_MT6763) $(CONFIG_MACH_MT6799) $(CONFIG_MACH_MT6759) \
					 $(CONFIG_MACH_MT6758) $(CONFIG_MACH_MT6739) $(CONFIG_MACH_MT6765) \
					 $(CONFIG_MACH_MT6761) $(CONFIG_MACH_MT3967) $(CONFIG_MACH_MT6779) \
			                 $(CONFIG_MACH_MT6768) $(CONFIG_MACH_MT6771)))
ccflags-y += \
                                -I$(srctree)/drivers/misc/mediatek/cmdq/v3/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(CMDQ_PLATFORM)/

else
ccflags-y += \
                                -I$(srctree)/drivers/misc/mediatek/cmdq/v2/ \
                                -I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(MTK_PLATFORM)/
endif

ifeq (y, $(filter y, $(CONFIG_MACH_MT6763) $(CONFIG_MACH_MT6739) $(CONFIG_MACH_MT6765) \
					 $(CONFIG_MACH_MT6761) $(CONFIG_MACH_MT3967) $(CONFIG_MACH_MT6779) \
					 $(CONFIG_MACH_MT6768) $(CONFIG_MACH_MT6771)))
ccflags-y += -DGAMMA_LIGHT
endif

ifeq (y, $(filter y, $(CONFIG_MACH_MT6765) $(CONFIG_MACH_MT6761) $(CONFIG_MACH_MT6768)))
ccflags-y += -DGAMMA_ROUND_CORNER
endif

ifneq ($(CONFIG_MTK_CMDQ_TAB), y)
ifeq (y, $(filter y, $(CONFIG_MACH_MT6758) $(CONFIG_MACH_MT6765) $(CONFIG_MACH_MT6761) $(CONFIG_MACH_MT6779) \
					$(CONFIG_MACH_MT6768) $(CONFIG_MACH_MT6771)))
ccflags-y += -DCCORR_TRANSITION
endif
endif

ifeq ($(CONFIG_MACH_MT6779), y)
    ccflags-y += -DCONFIG_MACH_MT6779
endif
