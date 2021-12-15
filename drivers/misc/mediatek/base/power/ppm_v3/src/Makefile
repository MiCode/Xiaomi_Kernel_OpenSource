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
ifeq ($(CONFIG_MACH_MT6877),y)
	USE_PPM_CPI = "true"
else ifeq ($(CONFIG_MACH_MT6833),y)
	USE_PPM_CPI = "true"
else ifeq ($(CONFIG_MACH_MT6781),y)
	USE_PPM_CPI = "true"
else ifeq ($(CONFIG_MACH_MT6739),y)
	USE_PPM_CPI = "true"
else ifeq ($(CONFIG_MACH_MT6768),y)
	USE_PPM_CPI = "true"
else
	USE_PPM_CPI = "false"
endif


ccflags-y += \
	-I$(PPM_ROOT_DIR)/inc \
	-I$(PPM_ROOT_DIR)/src/mach/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include	\
	-I$(srctree)/drivers/misc/mediatek/base/power/include	\
	-I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)	\
	-I$(srctree)/drivers/gpu/mediatek/gpufreq/include	\
	-I$(srctree)/drivers/misc/mediatek/power_throttling       \

ifeq ($(CONFIG_MACH_MT6877), y)
ccflags-y += \
	-I$(PPM_ROOT_DIR)/src/mach/mt6877\
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/mt6877/include	\
	-I$(srctree)/drivers/misc/mediatek/base/power/mt6877
endif

ifeq ($(CONFIG_MACH_MT6833),y)
ccflags-y += \
	-I$(PPM_ROOT_DIR)/src/mach/mt6833\
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/mt6833/include	\
	-I$(srctree)/drivers/misc/mediatek/base/power/mt6833\
	-I$(srctree)/drivers/misc/mediatek/thermal/mt6833/inc/mach/
endif

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/sspm/	\
	-I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_PLATFORM)
endif

ifneq ($(CONFIG_FPGA_EARLY_PORTING),y)
# main
obj-y := mtk_ppm_main.o
obj-y += mtk_ppm_interface.o
obj-y += mtk_ppm_api.o

# for profiling
obj-y += mtk_ppm_profile.o

# for IPI to SSPM
obj-y += mtk_ppm_ipi.o

# for CPI
ifeq ($(USE_PPM_CPI), "true")
$(warning USE_PPM_CPI)
obj-y += mtk_ppm_cpi.o
endif

# policies
obj-y += mtk_ppm_policy_dlpt.o
obj-y += mtk_ppm_policy_sys_boost.o
obj-y += mtk_ppm_policy_ptpod.o
obj-y += mtk_ppm_policy_pwr_thro.o
obj-y += mtk_ppm_policy_thermal.o
obj-y += mtk_ppm_policy_lcm_off.o
obj-y += mtk_ppm_policy_hard_user_limit.o
obj-y += mtk_ppm_policy_user_limit.o
obj-y += mtk_ppm_policy_force_limit.o
# for test purpose
obj-y += mtk_ppm_policy_ut.o

# platform dependent files
obj-y += mach/
else
# dummy driver to avoid build error
obj-y := mtk_ppm_dummy.o
endif
