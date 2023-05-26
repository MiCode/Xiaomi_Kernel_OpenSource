#
# Copyright (C) 2019 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

MT_GPUFREQ_BRINGUP ?= 0
MT_GPUFREQ_GED_READY ?= 1
MT_GPUFREQ_KICKER_PBM_READY ?= 1
MT_GPUFREQ_STATIC_PWR_READY2USE ?= 1
MT_GPUFREQ_LOW_BATT_VOLT_PROTECT ?= 1
MT_GPUFREQ_BATT_PERCENT_PROTECT ?= 1
MT_GPUFREQ_BATT_OC_PROTECT ?= 1
MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE ?= 1
MT_GPUFREQ_SETTLE_TIME_PROFILE ?= 0

ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/power/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/leakage_table_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/thermal/$(MTK_PLATFORM)/inc/mach/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lcm/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/clk/mediatek/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/ged/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/eem_v2/$(MTK_PLATFORM)/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/$(CONFIG_MTK_PLATFORM)

ccflags-y += -Wno-unused-variable
ccflags-y += -Wno-unused-function

ccflags-$(CONFIG_MTK_GPU_SPM_DVFS_SUPPORT) += -DMTK_GPU_SPM
ccflags-y += -DMT_GPUFREQ_BRINGUP=$(MT_GPUFREQ_BRINGUP)
ccflags-y += -DMT_GPUFREQ_GED_READY=$(MT_GPUFREQ_GED_READY)
ccflags-y += -DMT_GPUFREQ_KICKER_PBM_READY=$(MT_GPUFREQ_KICKER_PBM_READY)
ccflags-y += -DMT_GPUFREQ_STATIC_PWR_READY2USE=$(MT_GPUFREQ_STATIC_PWR_READY2USE)
ccflags-y += -DMT_GPUFREQ_LOW_BATT_VOLT_PROTECT=$(MT_GPUFREQ_LOW_BATT_VOLT_PROTECT)
ccflags-y += -DMT_GPUFREQ_BATT_PERCENT_PROTECT=$(MT_GPUFREQ_BATT_PERCENT_PROTECT)
ccflags-y += -DMT_GPUFREQ_BATT_OC_PROTECT=$(MT_GPUFREQ_BATT_OC_PROTECT)
ccflags-y += -DMT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE=$(MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE)
ccflags-y += -DMT_GPUFREQ_SETTLE_TIME_PROFILE=$(MT_GPUFREQ_SETTLE_TIME_PROFILE)

ifneq ($(CONFIG_FPGA_EARLY_PORTING), y)
obj-$(CONFIG_MACH_MT6785) += mtk_gpufreq_core.o
obj-$(CONFIG_MACH_MT6785) += mtk_picachu.o
endif

$(info *GPU CONFIG_MTK_FREQ_HOPPING = $(CONFIG_MTK_FREQ_HOPPING))
$(info *GPU MT_GPUFREQ_BRINGUP = $(MT_GPUFREQ_BRINGUP))
$(info *GPU MT_GPUFREQ_GED_READY = $(MT_GPUFREQ_GED_READY))
$(info *GPU MT_GPUFREQ_KICKER_PBM_READY = $(MT_GPUFREQ_KICKER_PBM_READY))
$(info *GPU MT_GPUFREQ_STATIC_PWR_READY2USE = $(MT_GPUFREQ_STATIC_PWR_READY2USE))
$(info *GPU MT_GPUFREQ_LOW_BATT_VOLT_PROTECT = $(MT_GPUFREQ_LOW_BATT_VOLT_PROTECT))
$(info *GPU MT_GPUFREQ_BATT_PERCENT_PROTECT = $(MT_GPUFREQ_BATT_PERCENT_PROTECT))
$(info *GPU MT_GPUFREQ_BATT_OC_PROTECT = $(MT_GPUFREQ_BATT_OC_PROTECT))
$(info *GPU MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE = $(MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE))
$(info *GPU MT_GPUFREQ_SETTLE_TIME_PROFILE = $(MT_GPUFREQ_SETTLE_TIME_PROFILE))
