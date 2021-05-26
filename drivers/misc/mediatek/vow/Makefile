#
# Copyright (C) 2020 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#
ifeq ($(CONFIG_MACH_MT6893),y)
MTK_PLATFORM := mt6885
else
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif


ifeq ($(CONFIG_MTK_TINYSYS_SCP_SUPPORT),y)

PCMCALLBACK_TYPE := $(subst ",,$(CONFIG_MTK_VOW_PCMCALLBACK_SUPPORT))

ifeq ($(PCMCALLBACK_TYPE),1st_stage)
subdir-ccflags-y += -DCONFIG_MTK_VOW_1STSTAGE_PCMCALLBACK
endif

ifeq ($(PCMCALLBACK_TYPE),2nd_stage)
subdir-ccflags-y += -DCONFIG_MTK_VOW_2NDSTAGE_PCMCALLBACK
endif

ifneq (,$(filter $(MTK_PLATFORM), mt6785 mt6771 mt6768))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vow/v01
obj-y += v01/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vow/v02
obj-y += v02/
endif
endif

# MTK vow driver interface
obj-y += $(MTK_PLATFORM)/