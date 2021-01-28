# SPDX-License-Identifier: GPL-2.0

ifeq (y, $(filter y, $(CONFIG_MACH_MT6765) $(CONFIG_MACH_MT6761)))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/leds/mt6765
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/leds/$(MTK_PLATFORM)
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/include

obj-$(CONFIG_MTK_LEDS) += mtk_leds_drv.o

ifeq (y, $(filter y, $(CONFIG_MACH_MT6765) $(CONFIG_MACH_MT6761)))
obj-y += $(subst ",,mt6765)/
else
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

