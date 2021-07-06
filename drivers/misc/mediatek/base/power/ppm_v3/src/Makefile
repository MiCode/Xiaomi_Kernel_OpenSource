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

ccflags-y += \
	-I$(PPM_ROOT_DIR)/inc \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include	\
	-I$(srctree)/drivers/misc/mediatek/base/power/include	\
	-I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)	\

ifneq (,$(findstring 3gear,$(PROJECT_DTB_NAMES)))
ccflags-y += -I$(PPM_ROOT_DIR)/src/mach/mt6893
else
ifeq ($(CONFIG_MACH_MT6893),y)
ccflags-y += -I$(PPM_ROOT_DIR)/src/mach/mt6893
else
ccflags-y += -I$(PPM_ROOT_DIR)/src/mach/$(CONFIG_MTK_PLATFORM)
endif
endif

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/sspm/	\
	-I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_PLATFORM)
endif

# only build-in dummy driver for MT6833
ifeq ($(CONFIG_MACH_MT6833),y)
obj-y := mtk_ppm_dummy.o
else

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
obj-y += mtk_ppm_cpi.o

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
endif
