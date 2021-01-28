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

THERMAL_CHIP_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/thermal/$(CONFIG_MTK_PLATFORM)
ccflags-y  += -I$(THERMAL_CHIP_DRIVER_DIR)/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/power/$(CONFIG_MTK_PLATFORM)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/dramc/$(CONFIG_MTK_PLATFORM)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/gpu/hal/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/auxadc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/common/inc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/imgsensor/inc/
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/

ifneq (,$(filter $(CONFIG_MTK_LEGACY_THERMAL_PLATFORM), "mt6779"))
obj-$(CONFIG_THERMAL) += mtk_ts_battery.o
obj-$(CONFIG_THERMAL) += mtk_ts_bts.o
obj-$(CONFIG_THERMAL) += mtk_ts_btsmdpa.o

obj-$(CONFIG_THERMAL) += mtk_ts_cpu_noBank.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa_thput.o
obj-$(CONFIG_THERMAL) += mtk_ts_pmic.o
obj-$(CONFIG_THERMAL) += mtk_ts_charger.o
#obj-$(CONFIG_THERMAL) += mtk_ts_charger2.o

ifneq ($(CONFIG_MTK_COMBO_WIFI),)
    obj-$(CONFIG_THERMAL) += mtk_ts_wmt.o
endif

obj-$(CONFIG_THERMAL) += mtk_tsAll.o
obj-$(CONFIG_THERMAL) += mtk_ts_imgsensor.o
obj-$(CONFIG_THERMAL) += mtk_ts_dctm.o
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6761"))

obj-$(CONFIG_THERMAL) += mtk_ts_battery.o

obj-$(CONFIG_THERMAL) += mtk_ts_bts.o
obj-$(CONFIG_THERMAL) += mtk_ts_btsmdpa.o

obj-$(CONFIG_THERMAL) += mtk_ts_cpu_noBank.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa_thput.o
obj-$(CONFIG_THERMAL) += mtk_ts_pmic.o
ifneq (,$(findstring tb8766, $(CONFIG_ARCH_MTK_PROJECT)))
ifneq ($(CONFIG_CHARGER_BQ25601),)
obj-$(CONFIG_THERMAL) += mtk_ts_btscharger.o
else
obj-$(CONFIG_THERMAL) += mtk_ts_charger.o
endif
else
obj-$(CONFIG_THERMAL) += mtk_ts_charger.o
endif
obj-$(CONFIG_THERMAL) += mtk_ts_charger2.o

ifneq ($(CONFIG_MTK_COMBO_WIFI),)
    obj-$(CONFIG_THERMAL) += mtk_ts_wmt.o
endif

obj-$(CONFIG_THERMAL) += mtk_tsAll.o
obj-$(CONFIG_THERMAL) += mtk_ts_imgsensor.o
obj-$(CONFIG_THERMAL) += mtk_ts_dctm.o
endif
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6765"))

obj-$(CONFIG_THERMAL) += mtk_ts_battery.o

obj-$(CONFIG_THERMAL) += mtk_ts_bts.o
obj-$(CONFIG_THERMAL) += mtk_ts_btsmdpa.o

obj-$(CONFIG_THERMAL) += mtk_ts_cpu_noBank.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa_thput.o
obj-$(CONFIG_THERMAL) += mtk_ts_pmic.o
ifneq (,$(findstring tb8768, $(CONFIG_ARCH_MTK_PROJECT)))
ifneq ($(CONFIG_CHARGER_BQ25601),)
obj-$(CONFIG_THERMAL) += mtk_ts_btscharger.o
else
obj-$(CONFIG_THERMAL) += mtk_ts_charger.o
endif
else
obj-$(CONFIG_THERMAL) += mtk_ts_charger.o
endif
obj-$(CONFIG_THERMAL) += mtk_ts_charger2.o

ifneq ($(CONFIG_MTK_COMBO_WIFI),)
    obj-$(CONFIG_THERMAL) += mtk_ts_wmt.o
endif

obj-$(CONFIG_THERMAL) += mtk_tsAll.o
obj-$(CONFIG_THERMAL) += mtk_ts_imgsensor.o
endif
