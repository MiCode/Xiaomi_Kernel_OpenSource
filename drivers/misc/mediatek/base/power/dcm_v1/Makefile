# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/dcm_v1/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/mach

ifeq ($(CONFIG_MACH_MT6833),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/dcm_v1/mt6833/
obj-y += mt6833/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/dcm_v1/$(MTK_PLATFORM)/
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

obj-$(CONFIG_MACH_MT6739) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6768) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6885) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6873) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6853) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6765) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6893) += mtk_dcm.o
obj-$(CONFIG_MACH_MT8168) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6833) += mtk_dcm.o
