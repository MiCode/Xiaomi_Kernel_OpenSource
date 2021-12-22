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
else ifeq ($(CONFIG_MACH_MT6877),y)
	PDIR = mt6877
else ifeq ($(CONFIG_MACH_MT6739),y)
	PDIR = mt6739
else ifeq ($(CONFIG_MACH_MT6768),y)
	PDIR = mt6768
else
	PDIR = $(CONFIG_MTK_PLATFORM)
endif

THERMAL_CHIP_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/thermal/$(PDIR)
ccflags-y  += -I$(THERMAL_CHIP_DRIVER_DIR)/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/$(PDIR)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/fakeHeader/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/ppm_v3/src/mach/$(PDIR)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/video/include
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/gpu/ged/include
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/common/inc/
ccflags-y  += -I$(srctree)/drivers/power/mediatek/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/eem_v1/inc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/dfrc/

ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)$(CONFIG_MTK_TINYSYS_SSPM_PLT_SUPPORT)),)
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V2)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2/
else ifneq ($(CONFIG_MTK_TINYSYS_SSPM_VERSION),"")
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
endif
endif

ifeq ($(CONFIG_MACH_MT6877),y)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/mcupm/$(MTK_PLATFORM)
else
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/mcupm/$(PDIR)
endif
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/vpu/$(PDIR)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/mdla/1.0
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/

ifeq (y, $(filter y, $(CONFIG_MACH_MT6781) $(CONFIG_MACH_MT6893)))
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/ppm_v3/src/mach/$(PDIR)
else
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/ppm_v3/src/mach/$(PDIR)
endif

obj-y += mtk_ta.o

ifneq (,$(filter $(CONFIG_MTK_LEGACY_THERMAL_PLATFORM), "mt6779"))
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/mmdvfs/

ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
    obj-$(CONFIG_THERMAL) += mtk_cooler_mutt.o
endif
obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_v1.o
ifeq ($(CONFIG_MTK_DUAL_CHARGER_SUPPORT),y)
	obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_2nd.o
endif
obj-$(CONFIG_THERMAL) += mtk_cooler_atm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_dtm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_sysrst.o
obj-$(CONFIG_THERMAL) += mtk_cooler_NR.o
obj-$(CONFIG_THERMAL) += mtk_cooler_VR_FPS.o
obj-$(CONFIG_THERMAL) += mtk_cooler_ISP.o
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6761" "mt6785"))
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
    obj-$(CONFIG_THERMAL) += mtk_cooler_mutt.o
endif

ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/mt-plat/

obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_v1.o
obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_2nd.o
obj-$(CONFIG_THERMAL) += mtk_cooler_atm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_dtm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_sysrst.o
obj-$(CONFIG_THERMAL) += mtk_cooler_VR_FPS.o
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6765"))
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
    obj-$(CONFIG_THERMAL) += mtk_cooler_mutt.o
endif

obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_v1.o
obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_2nd.o


obj-$(CONFIG_THERMAL) += mtk_cooler_atm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_dtm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_sysrst.o
obj-$(CONFIG_THERMAL) += mtk_cooler_VR_FPS.o
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6739" "mt6768"))
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
    obj-$(CONFIG_THERMAL) += mtk_cooler_mutt.o
endif
ifeq ($(CONFIG_MTK_PUMP_EXPRESS_PLUS_30_SUPPORT),y)
	obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_v1.o
else
	ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
		obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_v1.o
		ifeq ($(CONFIG_MTK_DUAL_CHARGER_SUPPORT),y)
			obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_2nd.o
		endif
	else
		obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o
	endif
endif

obj-$(CONFIG_THERMAL) += mtk_cooler_atm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_dtm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_sysrst.o
obj-$(CONFIG_THERMAL) += mtk_cooler_VR_FPS.o
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6853"))
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
    obj-$(CONFIG_THERMAL) += mtk_cooler_mutt_gen97.o
endif

ifeq ($(CONFIG_MTK_PUMP_EXPRESS_PLUS_30_SUPPORT),y)
	obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_v1.o
else
	ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
		obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_v1.o
		ifeq ($(CONFIG_MTK_DUAL_CHARGER_SUPPORT),y)
			obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_2nd.o
		endif
	else
		obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o
	endif
endif
obj-$(CONFIG_THERMAL) += mtk_cooler_atm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_dtm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_sysrst.o
obj-$(CONFIG_THERMAL) += mtk_cooler_VR_FPS.o
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6885"))
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
    obj-$(CONFIG_THERMAL) += mtk_cooler_mutt_gen97.o
endif

ifeq ($(CONFIG_MTK_PUMP_EXPRESS_PLUS_30_SUPPORT),y)
	obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_v1.o
else
	ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
		obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_v1.o
		ifeq ($(CONFIG_MTK_DUAL_CHARGER_SUPPORT),y)
			obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_2nd.o
		endif
	else
		obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o
	endif
endif
obj-$(CONFIG_THERMAL) += mtk_cooler_atm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_dtm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_sysrst.o
endif
