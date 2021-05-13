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

ifeq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6771"))
ifdef CONFIG_MTK_PLATFORM
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ifeq (,$(filter $(CONFIG_MACH_MT8168), y))
include $(srctree)/drivers/misc/mediatek/base/power/qos/$(MTK_PLATFORM)/Makefile
endif
endif
endif

ifeq ($(SSPM_VER),v2)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2
ccflags-y += -DUSE_SSMP_VER_V2
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1
ccflags-y += -DUSE_SSMP_VER_V1
endif

ifeq ($(CONFIG_MACH_MT6781),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2
ccflags-y += -DUSE_SSMP_VER_V2
endif

ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/met_drv/),)
ccflags-y += -DMET_SUPPORT
ccflags-y += -DMET_USER_EVENT_SUPPORT
endif

ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(CPUFREQ_ROOT_DIR)/src/mach/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/drivers/devfreq \
	-I$(srctree)/drivers/devfreq/helio-dvfsrc-v4/ \
	-I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/base/power/cm_mgr_v1/ \
	-I$(srctree)/drivers/misc/mediatek/base/power/cpufreq_v1/inc \
	-I$(srctree)/drivers/misc/mediatek/base/power/cpufreq_v1/src/mach/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/base/power/include \
	-I$(srctree)/drivers/misc/mediatek/base/power/qos/$(CONFIG_MTK_PLATFORM)/ \
	-I$(srctree)/drivers/misc/mediatek/dramc/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/emi/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/include \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/mach \
	-I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/include \
	-I$(srctree)/include/memory/mediatek/ \

ifeq ($(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES),"mediatek/mt6885_3gear")
ccflags-y += -DTRIGEAR_CM
endif
# main
ifeq (,$(filter $(CONFIG_MACH_MT8168), y))
obj-y += mtk_cm_mgr.o
else
obj-y += mtk_cm_mgr_mt8168.o
endif

ifeq ($(CONFIG_MACH_MT6893),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/cm_mgr_v1/mt6893
obj-y += mt6893/
else ifeq ($(CONFIG_MACH_MT6833),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/cm_mgr_v1/mt6833
obj-y += mt6833/
else ifeq ($(CONFIG_MACH_MT6877),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/cm_mgr_v1/mt6877
obj-y += mt6877/
else ifeq ($(CONFIG_MACH_MT6781),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/mt6781
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/cm_mgr_v1/mt6781
obj-y += mt6781/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/cm_mgr_v1/$(CONFIG_MTK_PLATFORM)
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

