#
# Copyright (C) 2018 MediaTek Inc.
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

#DRAM_CONTROLLER

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6739" "mt6763"))
	ccflags-y += \
		-DVCORE_DVFS_OPP_SUPPORT
else
	ccflags-y += \
		-DMTK_QOS_SUPPORT

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6771"))
	ccflags-y += \
		-DMTK_QOS_EMI_OPP
endif # ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6771"))
endif # ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6739" "mt6763"))

ifeq ($(CONFIG_MACH_MT6893),y)
ccflags-y += \
		-DMTK_K14_DRM_BOOST
else ifeq ($(CONFIG_MACH_MT6885),y)
ccflags-y += \
		-DMTK_K14_DRM_BOOST
else ifeq ($(CONFIG_MACH_MT6877),y)
ccflags-y += \
		-DMTK_K14_DRM_BOOST
else ifeq ($(CONFIG_MACH_MT6873),y)
ccflags-y += \
		-DMTK_K14_DRM_BOOST
else ifeq ($(CONFIG_MACH_MT6853),y)
ccflags-y += \
		-DMTK_K14_DRM_BOOST
else ifeq ($(CONFIG_MACH_MT6781),y)
ccflags-y += \
		-DMTK_K14_DRM_BOOST
else ifeq ($(CONFIG_MACH_MT6739),y)
ccflags-y += \
		-DMTK_K14_DRM_BOOST
else ifeq ($(CONFIG_MACH_MT6833),y)
ccflags-y += \
		-DMTK_K14_DRM_BOOST
else ifeq ($(CONFIG_MACH_MT6768),y)
ccflags-y += \
		-DMTK_K14_DRM_BOOST
endif

obj-y += dram_ctrl.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/include/memory/mediatek/

#if PM_DEVFREQ
ccflags-y += -I$(srctree)/drivers/devfreq/
#endif # PM_DEVFREQ


