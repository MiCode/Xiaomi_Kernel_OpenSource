
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

ifdef CONFIG_MTK_PLATFORM
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif

ifeq ($(CONFIG_MACH_MT6833),y)
	PDIR = mt6833
else ifeq ($(CONFIG_MACH_MT6893),y)
	PDIR = mt6893
else
	PDIR = $(MTK_PLATFORM)
endif

ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/	\
	-I$(srctree)/drivers/misc/mediatek/base/power/include/	\
	-I$(srctree)/drivers/misc/mediatek/base/power/include/swpm_v1/	\
	-I$(srctree)/drivers/misc/mediatek/base/power/qos/$(MTK_PLATFORM)/	\
	-I$(srctree)/drivers/misc/mediatek/thermal/$(PDIR)/inc/mach/	\
	-I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/	\

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)
endif

obj-y += mtk_swpm_platform.o
obj-y += mtk_swpm_sp_platform.o
