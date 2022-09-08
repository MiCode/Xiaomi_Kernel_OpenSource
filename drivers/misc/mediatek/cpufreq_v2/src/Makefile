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
ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6768)),)
ccflags-y += -I$(CPUFREQ_ROOT_DIR)/src/mt6768
include $(srctree)/drivers/misc/mediatek/cpufreq_v2/src/mt6768/Makefile
ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(srctree)/drivers/misc/mediatek/include \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/mt6768/include	\
	-I$(srctree)/drivers/misc/mediatek/include      \
	-I$(srctree)/drivers/misc/mediatek/mcupm/mt6768/
endif

ifneq ($(strip $(CONFIG_MEDIATEK_CPU_DVFS_MT6765)),)
ccflags-y += -I$(CPUFREQ_ROOT_DIR)/src/common_v2
include $(srctree)/drivers/misc/mediatek/cpufreq_v2/src/common_v2/Makefile

ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(CPUFREQ_ROOT_DIR)/src/common_v2 \
	-I$(srctree)/drivers/misc/mediatek/include \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/common_v2/include	\
	-I$(srctree)/drivers/misc/mediatek/common_v2 \
	-I$(srctree)/drivers/misc/mediatek/include      \
	-I$(srctree)/drivers/misc/mediatek/mcupm/common_v2/
endif

#sspm
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V1)),)
	ccflags-y += -DUSE_SSPM_VER_V1
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1/
endif
#sspm for common_v2-6765
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V0)),)
ccflags-y += -DUSE_SSPM_VER_V1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v0/
endif

obj-$(CONFIG_MEDIATEK_CPU_DVFS) += CPU_DVFS.o

CPU_DVFS-y := mtk_cpufreq_main.o
CPU_DVFS-y += mtk_cpufreq_interface.o
CPU_DVFS-y += mtk_cpufreq_api.o
CPU_DVFS-y += mtk_cpufreq_debug.o
CPU_DVFS-y += mtk_cpufreq_hybrid.o
ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6768)),)
CPU_DVFS-y += mt6768/mtk_cpufreq_platform.o
endif
ifneq ($(strip $(CONFIG_MEDIATEK_CPU_DVFS_MT6765)),)
CPU_DVFS-y += common_v2/mtk_cpufreq_platform.o
endif
