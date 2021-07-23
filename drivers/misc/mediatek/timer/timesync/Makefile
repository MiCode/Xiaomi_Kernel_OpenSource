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



ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
ifeq (y, $(filter y, $(CONFIG_MACH_MT6853) $(CONFIG_MACH_MT6885) $(CONFIG_MACH_MT6873) $(CONFIG_MACH_MT6893)))
ccflags-y += -DSSPM_V2
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)/
endif

ifeq ($(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcupm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcupm/$(MTK_PLATFORM)/
endif

ifeq ($(CONFIG_MTK_AUDIODSP_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/$(MTK_PLATFORM)/
endif




obj-$(CONFIG_MTK_TIMER_TIMESYNC)	+= mtk_sys_timer.o
obj-$(CONFIG_MTK_ARCHCOUNTER_TIMESYNC)	+= archcounter_timesync.o
