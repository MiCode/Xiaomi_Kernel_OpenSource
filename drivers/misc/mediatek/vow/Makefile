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
ifeq ($(CONFIG_MACH_MT6877),y)
MTK_PLATFORM := mt6853
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

endif

DSP_VERSION := $(subst ",,$(CONFIG_MTK_VOW_DSP_VERSION))

ifneq ($(DSP_VERSION), )

ccflags-y += -I$(srctree)/drivers/misc/mediatek/vow/$(DSP_VERSION)

obj-$(CONFIG_MTK_VOW_SUPPORT) += $(DSP_VERSION)/

endif