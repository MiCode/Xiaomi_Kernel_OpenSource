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
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/video/include
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/gpu/ged/include
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/common/inc/
ccflags-y  += -I$(srctree)/drivers/power/mediatek/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/dfrc/
ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT),y)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_PLATFORM)
endif
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/vpu/$(CONFIG_MTK_PLATFORM)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/mdla/1.0
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/ppm_v3/src/mach/$(CONFIG_MTK_PLATFORM)

obj-y += mtk_ta.o

ifneq (,$(filter $(CONFIG_MTK_LEGACY_THERMAL_PLATFORM), "mt6779"))
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
#    obj-y += mtk_cooler_mutt.o
endif

#ifeq ($(CONFIG_MTK_PUMP_EXPRESS_PLUS_30_SUPPORT),y)
#	obj-y += mtk_cooler_bcct_v1.o
#else
#	ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
#		obj-y += mtk_cooler_bcct_v1.o
#		ifeq ($(CONFIG_MTK_DUAL_CHARGER_SUPPORT),y)
#			obj-y += mtk_cooler_bcct_2nd.o
#		endif
#	else
#		obj-y += mtk_cooler_bcct.o
#	endif
#endif
obj-y += mtk_cooler_atm.o
obj-y += mtk_cooler_dtm.o
obj-y += mtk_cooler_sysrst.o
#obj-y += mtk_cooler_NR.o
#obj-y += mtk_cooler_VR_FPS.o
#obj-y += mtk_cooler_ISP.o
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6761"))
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
    obj-$(CONFIG_THERMAL) += mtk_cooler_mutt.o
endif

obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_v1.o
#obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_2nd.o
obj-$(CONFIG_THERMAL) += mtk_cooler_atm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_dtm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_sysrst.o
obj-$(CONFIG_THERMAL) += mtk_cooler_VR_FPS.o
endif
