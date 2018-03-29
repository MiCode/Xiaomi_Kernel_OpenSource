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

THERMAL_CHIP_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/thermal/$(MTK_PLATFORM)
ccflags-y  += -I$(THERMAL_CHIP_DRIVER_DIR)/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/power/$(MTK_PLATFORM)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/gpu/hal/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/fakeHeader/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/auxadc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/common/inc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6735"))
ifeq ($(CONFIG_ARCH_MT6735),y)
ccflags-y  += -I$(THERMAL_CHIP_DRIVER_DIR)/inc/D1
endif

ifeq ($(CONFIG_ARCH_MT6735M),y)
ccflags-y  += -I$(THERMAL_CHIP_DRIVER_DIR)/inc/D2
endif

ifeq ($(CONFIG_ARCH_MT6753),y)
ccflags-y  += -I$(THERMAL_CHIP_DRIVER_DIR)/inc/D3
endif

ifeq ($(CONFIG_ARCH_MT6753),y)
obj-$(CONFIG_THERMAL) += mtk_ts_6311buck.o
endif

#obj-$(CONFIG_THERMAL) += mtk_ts_abb.o
obj-$(CONFIG_THERMAL) += mtk_ts_battery.o
obj-$(CONFIG_THERMAL) += mtk_ts_bts.o
obj-$(CONFIG_THERMAL) += mtk_ts_btsmdpa.o
#obj-$(CONFIG_THERMAL) += mtk_ts_buck.o
obj-$(CONFIG_THERMAL) += mtk_ts_cpu.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa_thput.o
obj-$(CONFIG_THERMAL) += mtk_ts_pmic.o

ifneq ($(CONFIG_MTK_COMBO_WIFI),)
    obj-$(CONFIG_THERMAL) += mtk_ts_wmt.o
endif

obj-$(CONFIG_THERMAL) += mtk_ts1.o
obj-$(CONFIG_THERMAL) += mtk_ts2.o
obj-$(CONFIG_THERMAL) += mtk_ts3.o
obj-$(CONFIG_THERMAL) += mtk_ts4.o
#obj-$(CONFIG_THERMAL) += mtk_ts5.o
endif



ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6580"))
#obj-$(CONFIG_THERMAL) += mtk_ts_6311buck.o
#obj-$(CONFIG_THERMAL) += mtk_ts_abb.o
obj-$(CONFIG_THERMAL) += mtk_ts_battery.o
obj-$(CONFIG_THERMAL) += mtk_ts_bts.o
#obj-$(CONFIG_THERMAL) += mtk_ts_btsmdpa.o
#obj-$(CONFIG_THERMAL) += mtk_ts_buck.o
obj-$(CONFIG_THERMAL) += mtk_ts_cpu.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa_thput.o
obj-$(CONFIG_THERMAL) += mtk_ts_pmic.o
obj-$(CONFIG_THERMAL) += mtk_ts_wmt.o

obj-$(CONFIG_THERMAL) += mtk_ts1.o
obj-$(CONFIG_THERMAL) += mtk_ts2.o
obj-$(CONFIG_THERMAL) += mtk_ts3.o
obj-$(CONFIG_THERMAL) += mtk_ts4.o
#obj-$(CONFIG_THERMAL) += mtk_ts5.o
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6755"))
ifneq ($(CONFIG_MTK_PMIC_CHIP_MT6353),y)
obj-$(CONFIG_THERMAL) += mtk_ts_6311buck.o
endif

#obj-$(CONFIG_THERMAL) += mtk_ts_abb.o
obj-$(CONFIG_THERMAL) += mtk_ts_battery.o
obj-$(CONFIG_THERMAL) += mtk_ts_bts.o
obj-$(CONFIG_THERMAL) += mtk_ts_btsmdpa.o
#obj-$(CONFIG_THERMAL) += mtk_ts_buck.o
obj-$(CONFIG_THERMAL) += mtk_ts_cpu.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa_thput.o
obj-$(CONFIG_THERMAL) += mtk_ts_pmic.o
ifneq ($(CONFIG_MTK_PMIC_CHIP_MT6353),y)
obj-$(CONFIG_THERMAL) += mtk_ts_tsx.o
endif

ifneq ($(CONFIG_MTK_COMBO_WIFI),)
    obj-$(CONFIG_THERMAL) += mtk_ts_wmt.o
endif

obj-$(CONFIG_THERMAL) += mtk_ts1.o
obj-$(CONFIG_THERMAL) += mtk_ts2.o
obj-$(CONFIG_THERMAL) += mtk_ts3.o
obj-$(CONFIG_THERMAL) += mtk_ts4.o
#obj-$(CONFIG_THERMAL) += mtk_ts5.o
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6797"))
#obj-$(CONFIG_THERMAL) += mtk_ts_6311buck.o
#obj-$(CONFIG_THERMAL) += mtk_ts_abb.o
obj-$(CONFIG_THERMAL) += mtk_ts_battery.o
obj-$(CONFIG_THERMAL) += mtk_ts_bts.o
obj-$(CONFIG_THERMAL) += mtk_ts_btsmdpa.o
#obj-$(CONFIG_THERMAL) += mtk_ts_buck.o
obj-$(CONFIG_THERMAL) += mtk_ts_cpu.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa_thput.o
obj-$(CONFIG_THERMAL) += mtk_ts_pmic.o
obj-$(CONFIG_THERMAL) += ts_da9214.o
obj-$(CONFIG_THERMAL) += mtk_ts_dram.o
obj-$(CONFIG_THERMAL) += mtk_ts_imgsensor.o

ifneq ($(CONFIG_MTK_COMBO_WIFI),)
    obj-$(CONFIG_THERMAL) += mtk_ts_wmt.o
endif

obj-$(CONFIG_THERMAL) += mtk_ts1.o
obj-$(CONFIG_THERMAL) += mtk_ts2.o
obj-$(CONFIG_THERMAL) += mtk_ts3.o
obj-$(CONFIG_THERMAL) += mtk_ts4.o
#obj-$(CONFIG_THERMAL) += mtk_ts5.o
endif


ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6757"))
ifneq ($(CONFIG_MTK_PMIC_CHIP_MT6353),y)
obj-$(CONFIG_THERMAL) += mtk_ts_6311buck.o
endif

#obj-$(CONFIG_THERMAL) += mtk_ts_abb.o
obj-$(CONFIG_THERMAL) += mtk_ts_battery.o
obj-$(CONFIG_THERMAL) += mtk_ts_bts.o
obj-$(CONFIG_THERMAL) += mtk_ts_btsmdpa.o
#obj-$(CONFIG_THERMAL) += mtk_ts_buck.o
obj-$(CONFIG_THERMAL) += mtk_ts_cpu.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa_thput.o
obj-$(CONFIG_THERMAL) += mtk_ts_pmic.o
obj-$(CONFIG_THERMAL) += mtk_ts_charger.o
ifneq ($(CONFIG_MTK_PMIC_CHIP_MT6353),y)
#obj-$(CONFIG_THERMAL) += mtk_ts_tsx.o
endif

ifneq ($(CONFIG_MTK_COMBO_WIFI),)
    obj-$(CONFIG_THERMAL) += mtk_ts_wmt.o
endif

obj-$(CONFIG_THERMAL) += mtk_ts_Abts.o

obj-$(CONFIG_THERMAL) += mtk_ts1.o
obj-$(CONFIG_THERMAL) += mtk_ts2.o
obj-$(CONFIG_THERMAL) += mtk_ts3.o
obj-$(CONFIG_THERMAL) += mtk_ts4.o
obj-$(CONFIG_THERMAL) += mtk_ts5.o
endif
