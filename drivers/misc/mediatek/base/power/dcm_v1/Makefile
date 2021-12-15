
# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2016 MediaTek Inc.
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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/dcm_v1/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/dcm_v1/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/mach

ifeq ($(CONFIG_MACH_MT6781),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/dcm_v1/mt6781/
obj-y += mt6781/
else ifeq ($(CONFIG_MACH_MT6877),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/dcm_v1/mt6877/
obj-y += mt6877/
else ifeq ($(CONFIG_MACH_MT6833),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/dcm_v1/mt6833/
obj-y += mt6833/
else
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

obj-$(CONFIG_MACH_MT3967) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6758) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6759) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6761) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6763) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6799) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6765) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6779) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6739) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6781) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6768) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6877) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6833) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6885) += mtk_dcm.o
obj-$(CONFIG_MACH_MT6893) += mtk_dcm.o
