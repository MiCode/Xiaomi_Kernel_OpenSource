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

ccflags-y  += -I$(srctree)/drivers/thermal/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/ppm_v3/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/fakeHeader/
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/gpu/hal/
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/include/gpufreq_v1/
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/v2/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/common/inc/
ccflags-y  += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2

ccflags-y  += -I$(srctree)/drivers/misc/mediatek/vpu/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/mdla/1.0

ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/

include $(srctree)/drivers/misc/mediatek/thermal/common/thermal_zones/Makefile

ccflags-y  += -I$(srctree)/drivers/misc/mediatek/cpufreq_v2/inc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/ppm_v3/inc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

obj-$(CONFIG_MTK_LEGACY_THERMAL) += thermal_monitor.o

thermal_monitor-y += common/mtk_thermal_platform.o
thermal_monitor-y += common/ap_thermal_limit.o
thermal_monitor-y += common/mtk_thermal_timer.o
thermal_monitor-y += mtk_cooler_shutdown.o
ifneq ($(strip $(CONFIG_LEDS_MTK)),)
thermal_monitor-y += mtk_cooler_backlight.o
endif
thermal_monitor-y += mtk_cooler_kshutdown.o
thermal_monitor-y += mtk_cooler_cam.o

#thermal zones
thermal_monitor-y += common/thermal_zones/mtk_ts_battery.o

ifneq ($(CONFIG_POWER_EXT),y)
thermal_monitor-y += common/thermal_zones/mtk_ts_bts.o
thermal_monitor-y += common/thermal_zones/mtk_ts_btsmdpa.o
endif

thermal_monitor-y += common/thermal_zones/mtk_ts_pa.o
thermal_monitor-y += common/thermal_zones/mtk_ts_pa_thput.o
thermal_monitor-y += common/thermal_zones/mtk_ts_charger.o



thermal_monitor-y += common/thermal_zones/mtk_ts_wmt.o

thermal_monitor-y += common/thermal_zones/mtk_tsAll.o

thermal_monitor-y += common/thermal_zones/mtk_ts_pmic.o

#thermal cooler

thermal_monitor-y += common/coolers/mtk_cooler_atm.o
thermal_monitor-y += common/coolers/mtk_cooler_dtm.o
thermal_monitor-y += common/coolers/mtk_cooler_sysrst.o
thermal_monitor-y += common/coolers/mtk_cooler_VR_FPS.o
thermal_monitor-y += common/coolers/mtk_ta.o


#thermal platform
ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6739)),)
include $(srctree)/drivers/misc/mediatek/thermal/v4/src/Makefile
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/v4/inc
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/v3/include/
thermal_monitor-y += v4/mtk_thermal_monitor.o
thermal_monitor-y += v4/src/mtk_tc.o
thermal_monitor-y += v4/src/mtk_pmic_efuse.o
thermal_monitor-y += v4/src/mtk_ts_6357buck1.o
thermal_monitor-y += v4/src/mtk_ts_6357buck2.o
thermal_monitor-y += common/thermal_zones/mtk_ts_cpu_noBank.o
thermal_monitor-y += common/thermal_zones/mtk_ts_imgsensor.o
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
thermal_monitor-y += common/coolers/mtk_cooler_mutt.o
endif
thermal_monitor-y += common/coolers/mtk_cooler_bcct_v2.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6761)),)
include $(srctree)/drivers/misc/mediatek/thermal/v3/src/Makefile
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/v3/inc
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/v3/include/
thermal_monitor-y += v3/mtk_thermal_monitor.o
thermal_monitor-y += v3/src/mtk_tc.o
thermal_monitor-y += v3/src/mtk_pmic_efuse.o
thermal_monitor-y += v3/src/mtk_ts_6357buck1.o
thermal_monitor-y += v3/src/mtk_ts_6357buck2.o
thermal_monitor-y += common/thermal_zones/mtk_ts_cpu_noBank.o
thermal_monitor-y += common/thermal_zones/mtk_ts_charger2.o
thermal_monitor-y += common/thermal_zones/mtk_ts_imgsensor.o
thermal_monitor-y += common/thermal_zones/mtk_ts_dctm.o
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
thermal_monitor-y += common/coolers/mtk_cooler_mutt.o
endif
thermal_monitor-y += common/coolers/mtk_cooler_bcct_v1.o
thermal_monitor-y += common/coolers/mtk_cooler_bcct_2nd.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6768)),)
include $(srctree)/drivers/misc/mediatek/thermal/src/Makefile
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/inc
thermal_monitor-y += common/thermal_zones/mtk_ts_dctm.o
thermal_monitor-y += common/thermal_zones/mtk_ts_imgsensor.o
thermal_monitor-y += mtk_thermal_monitor.o
thermal_monitor-y += src/mtk_tc.o
thermal_monitor-y += src/mtk_ts_6358buck1.o
thermal_monitor-y += src/mtk_ts_6358buck2.o
thermal_monitor-y += src/mtk_ts_6358buck3.o
thermal_monitor-y += common/thermal_zones/mtk_ts_cpu_noBank.o
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
thermal_monitor-y += common/coolers/mtk_cooler_mutt.o
endif
thermal_monitor-y += common/coolers/mtk_cooler_bcct_v1.o
thermal_monitor-y += src/mtk_pmic_efuse.o
thermal_monitor-y += src/mtk_thermal_ipi.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6765)),)
include $(srctree)/drivers/misc/mediatek/thermal/v2/src/Makefile
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/v2/inc
thermal_monitor-y += common/thermal_zones/mtk_ts_imgsensor.o
thermal_monitor-y += v2/mtk_thermal_monitor.o
thermal_monitor-y += v2/src/mtk_tc.o
thermal_monitor-y += v2/src/mtk_ts_6357buck1.o
thermal_monitor-y += v2/src/mtk_ts_6357buck2.o
thermal_monitor-y += common/thermal_zones/mtk_ts_cpu_noBank.o
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
thermal_monitor-y += common/coolers/mtk_cooler_mutt.o
endif
thermal_monitor-y += common/coolers/mtk_cooler_bcct_v1.o
thermal_monitor-y += v2/src/mtk_pmic_efuse.o
thermal_monitor-y += common/thermal_zones/mtk_ts_charger2.o
endif


ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6833)),)
include $(srctree)/drivers/misc/mediatek/thermal/v1/src/Makefile
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/v1/inc
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/v2/include/
thermal_monitor-y += common/thermal_zones/mtk_ts_imgsensor.o
thermal_monitor-y += v1/mtk_thermal_monitor.o
thermal_monitor-y += v1/src/mtk_pmic_efuse_mt6833.o
thermal_monitor-y += v1/src/mtk_thermal_ipi_mt6833.o
thermal_monitor-y += v1/src/mtk_lvts_tc_mt6833.o
thermal_monitor-y += v1/src/mtk_tc_wrapper_mt6833.o
thermal_monitor-y += v1/src/mtk_ts_6359dcxo.o
thermal_monitor-y += v1/src/mtk_ts_6359tsx.o
thermal_monitor-y += v1/src/mtk_ts_6359vcore.o
thermal_monitor-y += v1/src/mtk_ts_6359vgpu.o
thermal_monitor-y += v1/src/mtk_ts_6359vproc.o

thermal_monitor-y += common/thermal_zones/mtk_ts_btsnrpa.o
thermal_monitor-y += common/thermal_zones/mtk_ts_dctm.o
thermal_monitor-y += common/coolers/mtk_cooler_bcct_v2.o
thermal_monitor-y += common/coolers/mtk_cooler_mutt_gen97.o
thermal_monitor-y += common/thermal_zones/mtk_ts_cpu_noBankv2.o
endif
