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
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(CPUFREQ_ROOT_DIR)/src/mach/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include	\
	-I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)
#Power MCU
ccflags-y += -I$(srctree)/drivers/misc/mediatek/spm2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/spm2/$(CONFIG_MTK_PLATFORM)
# main
obj-y := mt_cpufreq_main.o
obj-y += mt_cpufreq_interface.o
obj-y += mt_cpufreq_api.o
obj-y += mt_cpufreq_debug.o
obj-y += mt_cpufreq_hybrid.o

# platform dependent files
obj-y += mach/

