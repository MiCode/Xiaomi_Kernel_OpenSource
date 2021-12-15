#
# Copyright (C) 2020 MediaTek Inc.
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

ccflags-y += -DMET_USER_EVENT_SUPPORT

ifeq ($(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES),"mediatek/mt6885_3gear")
MTK_PLATFORM := mt6893
else
ifeq ($(CONFIG_MACH_MT6893),y)
MTK_PLATFORM := mt6893
else
ifeq ($(CONFIG_MACH_MT6833),y)
MTK_PLATFORM := mt6833
else
ifeq ($(CONFIG_MACH_MT6877),y)
MTK_PLATFORM := mt6877
else
ifeq ($(CONFIG_MACH_MT6781),y)
MTK_PLATFORM := mt6781
else
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif
endif
endif
endif
endif

ccflags-y += -I$(CPUFREQ_ROOT_DIR)/src/mach/$(MTK_PLATFORM)/include


ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(CPUFREQ_ROOT_DIR)/src/mach/$(MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/include \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include	\
	-I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/base/power/include
#sspm

ifeq ($(CONFIG_MACH_MT6781),y)
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2/
else ifeq ($(CONFIG_MACH_MT6877),y)
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2/
else ifeq ($(CONFIG_MACH_MT6833),y)
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2/
else
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)
endif

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_VERSION), "v1")
ccflags-y += -D CONFIG_MTK_TINYSYS_SSPM_V1
$(warning "use CONFIG_MTK_TINYSYS_SSPM_V1")
endif

# main
# for mt6781,mt6877,mt6833,mt6768
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6785" "mt6768" "mt6853"))
obj-y := v2/mtk_cpufreq_main.o
obj-y += v2/mtk_cpufreq_interface.o
obj-y += v2/mtk_cpufreq_api.o
obj-y += v2/mtk_cpufreq_debug.o
obj-y += v2/mtk_cpufreq_hybrid.o
else
obj-y := mtk_cpufreq_main.o
obj-y += mtk_cpufreq_interface.o
obj-y += mtk_cpufreq_api.o
obj-y += mtk_cpufreq_debug.o
obj-y += mtk_cpufreq_hybrid.o
endif
# platform dependent files
obj-y += mach/

