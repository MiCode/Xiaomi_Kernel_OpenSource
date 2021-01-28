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

obj-y += ddp_aal.o

ccflags-y += \
				-I$(srctree)/drivers/misc/mediatek/video/include     \
				-I$(srctree)/drivers/misc/mediatek/video/common/IPv1     \
				-I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys \
				-I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox \
				-I$(srctree)/drivers/misc/mediatek/sync/      \
				-I$(srctree)/drivers/misc/mediatek/smi/ \
				-I$(srctree)/drivers/misc/mediatek/lcm/inc/ \
				-I$(srctree)/drivers/misc/mediatek/leds/

ifeq (y, $(filter y, $(CONFIG_MACH_MT6765) $(CONFIG_MACH_MT6761)))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/leds/mt6765
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/leds/$(MTK_PLATFORM)
endif


ifeq (y, $(filter y, $(CONFIG_MACH_MT6799) $(CONFIG_MACH_MT6759) $(CONFIG_MACH_MT6758) \
					 $(CONFIG_MACH_MT6763) $(CONFIG_MACH_MT6739) $(CONFIG_MACH_MT6765) \
					 $(CONFIG_MACH_MT6761) $(CONFIG_MACH_MT6771) $(CONFIG_MACH_MT3967) \
					 $(CONFIG_MACH_MT8168) $(CONFIG_MACH_MT6768) $(CONFIG_MACH_MT6885) \
					 $(CONFIG_MACH_MT6785) $(CONFIG_MACH_MT6873) $(CONFIG_MACH_MT6853) \
					 $(CONFIG_MACH_MT6893)))

ccflags-y += \
				-I$(srctree)/drivers/misc/mediatek/cmdq/v3/

ifeq ($(CONFIG_MACH_MT6761),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/mt6765/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/
endif
else
ifeq (y, $(filter y, $(CONFIG_MACH_MT6833)))
ccflags-y += \
                 -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/cmdq/v3/ \
                 -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/cmdq/v3/$(MTK_PLATFORM)/
else
ccflags-y += \
				-I$(srctree)/drivers/misc/mediatek/cmdq/v2/ \
				-I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(MTK_PLATFORM)/
endif
endif

ifeq (y, $(filter y, $(CONFIG_MACH_MT6763) $(CONFIG_MACH_MT6758) $(CONFIG_MACH_MT6739) \
					 $(CONFIG_MACH_MT6765) $(CONFIG_MACH_MT6761) $(CONFIG_MACH_MT6771) \
					 $(CONFIG_MACH_MT3967) $(CONFIG_MACH_MT8168) $(CONFIG_MACH_MT6768) \
					 $(CONFIG_MACH_MT6785) $(CONFIG_MACH_MT6885) $(CONFIG_MACH_MT6873) \
					 $(CONFIG_MACH_MT6853) $(CONFIG_MACH_MT6893) $(CONFIG_MACH_MT6833)))
    ccflags-y += -DNOT_SUPPORT_CABC_HW
endif

ifeq (y, $(filter y, $(CONFIG_MACH_MT6757) $(CONFIG_MACH_MT6771)))
    ccflags-y += -DLOAD_AAL_SUPPORT_FROM_DTS
endif

ifeq (y, $(filter y, $(CONFIG_MACH_MT6765) $(CONFIG_MACH_MT6761) $(CONFIG_MACH_MT8168)))
    ccflags-y += -DAMS_AAL_SUPPORT_KERNEL_API
endif

ifeq (y, $(filter y, $(CONFIG_MACH_MT6779) $(CONFIG_MACH_MT6785)))
    ccflags-y += -DAAL_HAS_YHIST
endif

ifeq (y, $(filter y, $(CONFIG_MACH_MT6779) $(CONFIG_MACH_MT6785)))
    ccflags-y += -DAAL_HAS_DRE3
endif

ifneq ($(CONFIG_MACH_MT8167),y)
ccflags-y += -DAAL_SUPPORT_KERNEL_API
endif
