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

ifeq ($(CONFIG_MACH_MT6833),y)
	PDIR = mt6833
else ifeq ($(CONFIG_MACH_MT6893),y)
	PDIR = mt6893
else ifeq ($(CONFIG_MACH_MT6781),y)
	PDIR = mt6781
else ifeq ($(CONFIG_MACH_MT6768),y)
	PDIR = mt6768
else ifeq ($(CONFIG_MACH_MT6877),y)
	PDIR = mt6877
else ifeq ($(CONFIG_MACH_MT6739),y)
	PDIR = mt6739
else
	PDIR = $(CONFIG_MTK_PLATFORM)
endif

THERMAL_CHIP_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/thermal/$(PDIR)
ccflags-y  += -I$(THERMAL_CHIP_DRIVER_DIR)/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/$(PDIR)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/power/$(PDIR)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/dramc/$(PDIR)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/gpu/hal/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/fakeHeader/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/auxadc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/common/inc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/eem_v1/inc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/imgsensor/inc/
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/mt-plat/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/mcupm/$(PDIR)

ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)$(CONFIG_MTK_TINYSYS_SSPM_PLT_SUPPORT)),)
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V2)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2/
else ifneq ($(CONFIG_MTK_TINYSYS_SSPM_VERSION),"")
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
endif
endif


ifeq ($(CONFIG_MACH_MT6781),y)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/$(PDIR)/inc/
else ifeq ($(CONFIG_MACH_MT6877),y)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/mcupm/$(MTK_PLATFORM)
else
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/mcupm/$(PDIR)
endif

ifeq ($(CONFIG_MACH_MT6877),y)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/mcupm/mt6853/
endif

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

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6739"))
#obj-$(CONFIG_THERMAL) += mtk_ts_abb.o
obj-$(CONFIG_THERMAL) += mtk_ts_battery.o

ifneq ($(CONFIG_POWER_EXT),y)
obj-$(CONFIG_THERMAL) += mtk_ts_bts.o
obj-$(CONFIG_THERMAL) += mtk_ts_btsmdpa.o
endif

obj-$(CONFIG_THERMAL) += mtk_ts_cpu_noBank.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa_thput.o
obj-$(CONFIG_THERMAL) += mtk_ts_pmic.o
#obj-$(CONFIG_THERMAL) += mtk_ts_pmic1.o
obj-$(CONFIG_THERMAL) += mtk_ts_charger.o

ifneq ($(CONFIG_MTK_COMBO_WIFI),)
    obj-$(CONFIG_THERMAL) += mtk_ts_wmt.o
endif

obj-$(CONFIG_THERMAL) += mtk_tsAll.o
#obj-$(CONFIG_THERMAL) += mtk_ts_Abts.o
obj-$(CONFIG_THERMAL) += mtk_ts_imgsensor.o

ifneq ($(CONFIG_MTK_BIF_SUPPORT),)
obj-$(CONFIG_THERMAL) += mtk_ts_bif.o
endif
endif



ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6785"))

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
ifneq ($(filter "MT76%", $(CONFIG_MTK_COMBO_CHIP)),)
    obj-$(CONFIG_THERMAL) += mtk_ts_wmt_766x.o
else
    obj-$(CONFIG_THERMAL) += mtk_ts_wmt.o
endif
endif

obj-$(CONFIG_THERMAL) += mtk_tsAll.o
obj-$(CONFIG_THERMAL) += mtk_ts_imgsensor.o
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

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6768"))
obj-$(CONFIG_THERMAL) += mtk_ts_battery.o

obj-$(CONFIG_THERMAL) += mtk_ts_bts.o
obj-$(CONFIG_THERMAL) += mtk_ts_btsmdpa.o

obj-$(CONFIG_THERMAL) += mtk_ts_cpu_noBank.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa_thput.o

obj-$(CONFIG_THERMAL) += mtk_ts_charger.o
#obj-$(CONFIG_THERMAL) += mtk_ts_charger2.o

ifneq ($(CONFIG_MTK_COMBO_WIFI),)
    obj-$(CONFIG_THERMAL) += mtk_ts_wmt.o
endif

obj-$(CONFIG_THERMAL) += mtk_tsAll.o
obj-$(CONFIG_THERMAL) += mtk_ts_imgsensor.o
obj-$(CONFIG_THERMAL) += mtk_ts_dctm.o
obj-$(CONFIG_THERMAL) += mtk_ts_pmic.o

ifneq ($(CONFIG_MTK_BIF_SUPPORT),)
#obj-$(CONFIG_THERMAL) += mtk_ts_bif.o
endif
endif
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6853"))

obj-$(CONFIG_THERMAL) += mtk_ts_battery.o

obj-$(CONFIG_THERMAL) += mtk_ts_bts.o
obj-$(CONFIG_THERMAL) += mtk_ts_btsmdpa.o
obj-$(CONFIG_THERMAL) += mtk_ts_btsnrpa.o

obj-$(CONFIG_THERMAL) += mtk_ts_cpu_noBankv2.o
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
endif


ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6885"))

obj-$(CONFIG_THERMAL) += mtk_ts_battery.o

obj-$(CONFIG_THERMAL) += mtk_ts_bts.o
obj-$(CONFIG_THERMAL) += mtk_ts_btsmdpa.o
obj-$(CONFIG_THERMAL) += mtk_ts_btsnrpa.o

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
endif
