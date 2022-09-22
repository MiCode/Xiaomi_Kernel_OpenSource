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

ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/fakeHeader/
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/gpu/hal/
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/include/gpufreq_v1/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/common/inc/

ccflags-y  += -I$(srctree)/drivers/misc/mediatek/vpu/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/mdla/1.0

ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/

include $(srctree)/drivers/misc/mediatek/thermal/common/thermal_zones/Makefile
include $(srctree)/drivers/misc/mediatek/thermal/src/Makefile
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/cpufreq_v2/inc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/ppm_v3/inc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

obj-$(CONFIG_MTK_LEGACY_THERMAL) += thermal_monitor.o
thermal_monitor-y += mtk_thermal_monitor.o
thermal_monitor-y += common/mtk_thermal_platform.o
thermal_monitor-y += common/ap_thermal_limit.o
thermal_monitor-y += common/mtk_thermal_timer.o
thermal_monitor-y += mtk_cooler_shutdown.o
thermal_monitor-y += mtk_cooler_kshutdown.o
thermal_monitor-y += mtk_cooler_cam.o

#thermal zones
thermal_monitor-y += common/thermal_zones/mtk_ts_battery.o
thermal_monitor-y += common/thermal_zones/mtk_ts_bts.o
thermal_monitor-y += common/thermal_zones/mtk_ts_btsmdpa.o
thermal_monitor-y += common/thermal_zones/mtk_ts_cpu_noBank.o
thermal_monitor-y += common/thermal_zones/mtk_ts_pa.o
thermal_monitor-y += common/thermal_zones/mtk_ts_pa_thput.o
thermal_monitor-y += common/thermal_zones/mtk_ts_charger.o

thermal_monitor-y += common/thermal_zones/mtk_ts_wmt.o

thermal_monitor-y += common/thermal_zones/mtk_tsAll.o
thermal_monitor-y += common/thermal_zones/mtk_ts_imgsensor.o
thermal_monitor-y += common/thermal_zones/mtk_ts_dctm.o
thermal_monitor-y += common/thermal_zones/mtk_ts_pmic.o

#thermal cooler
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
thermal_monitor-y += common/coolers/mtk_cooler_mutt.o
endif

thermal_monitor-y += common/coolers/mtk_cooler_bcct_v1.o

thermal_monitor-y += common/coolers/mtk_cooler_atm.o
thermal_monitor-y += common/coolers/mtk_cooler_dtm.o
thermal_monitor-y += common/coolers/mtk_cooler_sysrst.o
thermal_monitor-y += common/coolers/mtk_cooler_VR_FPS.o
thermal_monitor-y += common/coolers/mtk_ta.o


#thermal platform
thermal_monitor-y += src/mtk_tc.o
thermal_monitor-y += src/mtk_pmic_efuse.o
thermal_monitor-y += src/mtk_thermal_ipi.o
thermal_monitor-y += src/mtk_ts_6358buck1.o
thermal_monitor-y += src/mtk_ts_6358buck2.o
thermal_monitor-y += src/mtk_ts_6358buck3.o
