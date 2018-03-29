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

ccflags-y += \
	-I$(PPM_ROOT_DIR)/inc \
	-I$(PPM_ROOT_DIR)/src/mach/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include	\
	-I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)	\
	-I$(srctree)/drivers/misc/mediatek/sspm/	\
	-I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_PLATFORM)

ifeq (0,1)
#ifneq ($(CONFIG_FPGA_EARLY_PORTING),y)
# main
obj-y := mt_ppm_main.o
obj-y += mt_ppm_interface.o
obj-y += mt_ppm_api.o

# for profiling
obj-y += mt_ppm_profile.o

# for IPI to PMCU
obj-y += mt_ppm_ipi.o

# policies
obj-y += mt_ppm_policy_dlpt.o
obj-y += mt_ppm_policy_perfserv.o
obj-y += mt_ppm_policy_sys_boost.o
obj-y += mt_ppm_policy_ptpod.o
obj-y += mt_ppm_policy_pwr_thro.o
obj-y += mt_ppm_policy_thermal.o
obj-y += mt_ppm_policy_lcm_off.o
obj-y += mt_ppm_policy_hica.o
obj-y += mt_ppm_policy_user_limit.o
# for test purpose
obj-y += mt_ppm_policy_ut.o

# platform dependent files
obj-y += mach/

else
# dummy driver to avoid build error
obj-y := mt_ppm_dummy.o
endif

