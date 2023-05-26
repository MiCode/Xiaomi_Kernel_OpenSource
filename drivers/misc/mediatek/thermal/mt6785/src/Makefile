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

THERMAL_CHIP_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/thermal/$(MTK_PLATFORM)
ccflags-y  += -I$(THERMAL_CHIP_DRIVER_DIR)/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/gpu/hal/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/fakeHeader/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/sspm/v1
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/sspm
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/include/spm/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/eem_v2/mt3967/inc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/pmic/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/common/inc/
ccflags-y  += -I$(srctree)/include/dt-bindings/
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/v1/include
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/mt-plat

obj-$(CONFIG_THERMAL) += mtk_lvts_tc.o
obj-$(CONFIG_THERMAL) += mtk_tc.o
obj-$(CONFIG_THERMAL) += mtk_tc_wrapper.o
obj-$(CONFIG_THERMAL) += mtk_thermal_ipi.o
obj-$(CONFIG_THERMAL) += mtk_pmic_efuse.o
obj-$(CONFIG_THERMAL) += mtk_ts_6359vcore.o
obj-$(CONFIG_THERMAL) += mtk_ts_6359vproc.o
obj-$(CONFIG_THERMAL) += mtk_ts_6359vgpu.o
obj-$(CONFIG_THERMAL) += mtk_ts_6359tsx.o
obj-$(CONFIG_THERMAL) += mtk_ts_6359dcxo.o

