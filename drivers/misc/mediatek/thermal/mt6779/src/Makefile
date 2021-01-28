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

THERMAL_CHIP_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/thermal/mt6779
ccflags-y  += -I$(THERMAL_CHIP_DRIVER_DIR)/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/gpu/hal/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/fakeHeader/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/sspm/mt6779
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/pmic/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/common/inc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y  += -I$(srctree)/include/dt-bindings/
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/

#obj-y += mtk_lvts_tc.o
obj-$(CONFIG_THERMAL) += mtk_tc.o
obj-$(CONFIG_THERMAL) += mtk_tc_wrapper.o
#obj-y += mtk_thermal_ipi.o
obj-$(CONFIG_THERMAL) += mtk_pmic_efuse.o
#obj-y += mtk_ts_6359vcore.o
#obj-y += mtk_ts_6359vproc.o
#obj-y += mtk_ts_6359vgpu.o
#obj-y += mtk_ts_6359tsx.o
#obj-y += mtk_ts_6359dcxo.o

