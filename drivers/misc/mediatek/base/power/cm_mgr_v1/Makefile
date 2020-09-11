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

ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE := y
endif

ifdef CONFIG_MTK_PLATFORM
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
include $(srctree)/drivers/misc/mediatek/base/power/qos/$(MTK_PLATFORM)/Makefile
endif

ifeq ($(SSPM_VER),v2)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2
ccflags-y += -DUSE_SSMP_VER_V2
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1
ccflags-y += -DUSE_SSMP_VER_V1
endif



ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/met_drv/),)
ccflags-y += -DMET_SUPPORT
ccflags-y += -DMET_USER_EVENT_SUPPORT
endif

ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(CPUFREQ_ROOT_DIR)/src/mach/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/drivers/devfreq \
	-I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/base/power/cm_mgr_v1/ \
	-I$(srctree)/drivers/misc/mediatek/base/power/cm_mgr_v1/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/base/power/cpufreq_v1/inc \
	-I$(srctree)/drivers/misc/mediatek/base/power/cpufreq_v1/src/mach/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/base/power/include \
	-I$(srctree)/drivers/misc/mediatek/dramc/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/include \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/mach \
	-I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/include \

# main
obj-y += mtk_cm_mgr.o
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/

