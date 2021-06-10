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
# COMMON PART
# 1. xxx.h/c: Source files that is common for all chips
# 2. IPvx/: Common part for a Chipset Groups
#
# When one new chip coming, you can select an existing common part, ex. IPv1 or
# you can create a new group, ex. IPv2
################################################################################



#####################################
# MT67xx chipset group


#####################################
# Add new chipset groups here

# separate mt6833 from family mt6853
ifeq (y, $(filter y, $(CONFIG_MACH_MT6833)))
	MTK_PLATFORM := mt6833
endif

#####################################
# Common Source files for all chips
ifneq (y, $(filter y, $(CONFIG_MACH_MT6799) $(CONFIG_MACH_MT6759) $(CONFIG_MACH_MT6758) $(CONFIG_MACH_MT6763) $(CONFIG_MACH_MT6775) $(CONFIG_MACH_MT6771) $(CONFIG_MACH_MT6739) $(CONFIG_MACH_MT8168) $(CONFIG_MACH_MT6765) $(CONFIG_MACH_MT6768) $(CONFIG_MACH_MT6885) $(CONFIG_MACH_MT6785) $(CONFIG_MACH_MT8167) $(CONFIG_MACH_MT6873) $(CONFIG_MACH_MT6853) $(CONFIG_MACH_MT6893) $(CONFIG_MACH_MT6833)))

obj-$(CONFIG_MTK_FB) += 			\
			mtkfb_fence.o \
			fbconfig_kdebug.o
endif

obj-y += pwm10/
obj-y += corr10/
obj-y += color20/

obj-y += aal30/

ifeq (y, $(filter y, $(CONFIG_MACH_MT6799)  $(CONFIG_MACH_MT6757) $(CONFIG_MACH_KIBOPLUS)))
obj-y += od10/
endif

# Todo: newer chip
ifeq (y, $(filter y, $(CONFIG_MACH_MT6757) $(CONFIG_MACH_KIBOPLUS) $(CONFIG_MACH_MT6797) $(CONFIG_MACH_MT6795) $(CONFIG_MACH_MT8167)))
obj-y += rdma10/
obj-y += wdma10/
else
ifeq (y, $(filter y, $(CONFIG_MACH_MT6799)))
obj-y += rdma20/
obj-y += wdma20/
endif
endif

obj-y += layering_rule_base/

ccflags-y += \
				-I$(srctree)/drivers/misc/mediatek/video/include     \
				-I$(srctree)/drivers/misc/mediatek/video/common/IPv1     \
				-I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys \
				-I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox \
				-I$(srctree)/drivers/misc/mediatek/sync/      \

ifeq (y, $(filter y, $(CONFIG_MACH_MT6799) $(CONFIG_MACH_MT6759) $(CONFIG_MACH_MT6758) $(CONFIG_MACH_MT6763) $(CONFIG_MACH_MT6775) $(CONFIG_MACH_MT6771) $(CONFIG_MACH_MT6739) $(CONFIG_MACH_MT8168) $(CONFIG_MACH_MT6765) $(CONFIG_MACH_MT6768) $(CONFIG_MACH_MT6785) $(CONFIG_MACH_MT6885) $(CONFIG_MACH_MT6873) $(CONFIG_MACH_MT6853) $(CONFIG_MACH_MT6893)))

ccflags-y += \
				-I$(srctree)/drivers/misc/mediatek/cmdq/v3/ \
				-I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/
else
ifeq (y, $(filter y, $(CONFIG_MACH_MT6833)))
ccflags-y += \
                 -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/m4u/$(MTK_PLATFORM)/ \
                 -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/cmdq/v3/ \
                 -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/cmdq/v3/$(MTK_PLATFORM)/
else
ccflags-y += \
				-I$(srctree)/drivers/misc/mediatek/cmdq/v2/ \
				-I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(MTK_PLATFORM)/
endif
endif

ifneq ($(CONFIG_MTK_FB), y)
obj-y += mtkfb_dummy.o
endif

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM) \


ccflags-$(CONFIG_MTK_LCM) += -I$(srctree)/drivers/misc/mediatek/lcm/inc

ccflags-y += -I$(srctree)/drivers/staging/android/ion \
             -I$(srctree)/drivers/staging/android/ion/mtk

ifeq ($(CONFIG_MTK_INTERNAL_HDMI_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/
endif
ifeq ($(CONFIG_MTK_INTERNAL_MHL_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mhltx/
endif
ifeq ($(CONFIG_MTK_DITHERING_SUPPORT), y)
ccflags-y += -DDITHERING_SUPPORT
endif
