# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

ifeq ($(CONFIG_MACH_MT6833),y)
	PDIR = mt6833
else
	PDIR = $(MTK_PLATFORM)
endif

THERMAL_CHIP_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/thermal/$(PDIR)
ccflags-y  += -I$(THERMAL_CHIP_DRIVER_DIR)/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/$(PDIR)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/fakeHeader/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ifneq (,$(findstring 3gear,$(PROJECT_DTB_NAMES)))
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/ppm_v3/src/mach/mt6893
else
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/ppm_v3/src/mach/$(PDIR)
endif
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/video/include
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/gpu/ged/include
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/common/inc/
ccflags-y  += -I$(srctree)/drivers/power/mediatek/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/eem_v1/inc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/dfrc/
ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT),y)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/sspm
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/sspm/v1
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/sspm/$(PDIR)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/mcupm/$(PDIR)
endif
ifeq ($(CONFIG_MTK_APUSYS_SUPPORT),y)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/$(PDIR)
else
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/vpu/$(PDIR)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/mdla/1.0
endif

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

ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
    #obj-$(CONFIG_THERMAL) += mtk_cooler_amddulthro.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_amdtxctrl.o
    obj-$(CONFIG_THERMAL) += mtk_cooler_amutt.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mddtxpwr.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mddulthro.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mdtxpwr.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mdulthro.o
    obj-$(CONFIG_THERMAL) += mtk_cooler_mutt.o
	#obj-$(CONFIG_THERMAL)  += mtk_cooler_3Gmutt.o
endif
obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o
obj-$(CONFIG_THERMAL) += mtk_cooler_atm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_dtm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_sysrst.o
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6580"))
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
    #obj-$(CONFIG_THERMAL) += mtk_cooler_amddulthro.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_amdtxctrl.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_amutt.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mddtxpwr.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mddulthro.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mdtxpwr.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mdulthro.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mutt.o
	#obj-$(CONFIG_THERMAL)  += mtk_cooler_3Gmutt.o
endif
obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o
obj-$(CONFIG_THERMAL) += mtk_cooler_atm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_dtm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_sysrst.o
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6755" "mt6797"))
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
    #obj-$(CONFIG_THERMAL) += mtk_cooler_amddulthro.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_amdtxctrl.o
    obj-$(CONFIG_THERMAL) += mtk_cooler_amutt.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mddtxpwr.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mddulthro.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mdtxpwr.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mdulthro.o
    obj-$(CONFIG_THERMAL) += mtk_cooler_mutt.o
endif
obj-$(CONFIG_THERMAL) += mtk_cooler_fps.o
obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o
obj-$(CONFIG_THERMAL) += mtk_cooler_atm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_dtm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_sysrst.o
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "elbrus"))
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
    #obj-$(CONFIG_THERMAL) += mtk_cooler_amddulthro.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_amdtxctrl.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_amutt.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mddtxpwr.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mddulthro.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mdtxpwr.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mdulthro.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mutt.o
endif
#obj-$(CONFIG_THERMAL) += mtk_cooler_fps.o
#obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o
obj-$(CONFIG_THERMAL) += mtk_cooler_atm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_dtm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_sysrst.o
endif

obj-$(CONFIG_THERMAL) += mtk_ta.o




ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6757" "kiboplus"))
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
    #obj-$(CONFIG_THERMAL) += mtk_cooler_amddulthro.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_amdtxctrl.o
    obj-$(CONFIG_THERMAL) += mtk_cooler_amutt.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mddtxpwr.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mddulthro.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mdtxpwr.o
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mdulthro.o
    obj-$(CONFIG_THERMAL) += mtk_cooler_mutt.o
endif
obj-$(CONFIG_THERMAL) += mtk_cooler_fps.o
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



ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6799" "mt6759" "mt6763" "mt6765" "mt6758" "mt8168"))
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
    obj-$(CONFIG_THERMAL) += mtk_cooler_mutt.o
endif
obj-$(CONFIG_THERMAL) += mtk_cooler_fps.o
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
#obj-$(CONFIG_THERMAL) += mtk_cooler_NR.o
obj-$(CONFIG_THERMAL) += mtk_cooler_VR_FPS.o
endif


ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6739" "mt6771" "mt6775" "mt6765" "mt6768" "mt6785"))
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
    obj-$(CONFIG_THERMAL) += mtk_cooler_mutt.o
endif
#obj-$(CONFIG_THERMAL) += mtk_cooler_fps.o
ifeq ($(CONFIG_MTK_PUMP_EXPRESS_PLUS_30_SUPPORT),y)
	#obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_v1.o
else
	ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
		#obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_v1.o
		ifeq ($(CONFIG_MTK_DUAL_CHARGER_SUPPORT),y)
			#obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_2nd.o
		endif
	else
		#obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o
	endif
endif
obj-$(CONFIG_THERMAL) += mtk_cooler_atm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_gpu_cus.o
ifeq ($(CONFIG_MTK_VPU_SUPPORT),y)
	obj-$(CONFIG_THERMAL) += mtk_cooler_vpu_cus.o
endif
obj-$(CONFIG_THERMAL) += mtk_cooler_char_curr_cus.o
obj-$(CONFIG_THERMAL) += mtk_cooler_dtm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_sysrst.o
#obj-$(CONFIG_THERMAL) += mtk_cooler_NR.o
#obj-$(CONFIG_THERMAL) += mtk_cooler_VR_FPS.o
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6885"))
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
    obj-$(CONFIG_THERMAL) += mtk_cooler_mutt_gen97.o
endif
#obj-$(CONFIG_THERMAL) += mtk_cooler_fps.o
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
#obj-$(CONFIG_THERMAL) += mtk_cooler_NR.o
#obj-$(CONFIG_THERMAL) += mtk_cooler_VR_FPS.o
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6873"))
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
    obj-$(CONFIG_THERMAL) += mtk_cooler_mutt_gen97.o
endif
#obj-$(CONFIG_THERMAL) += mtk_cooler_fps.o
ifeq ($(CONFIG_MTK_PUMP_EXPRESS_PLUS_30_SUPPORT),y)
	#obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_v1.o
else
	ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
		#obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_v1.o
		ifeq ($(CONFIG_MTK_DUAL_CHARGER_SUPPORT),y)
			#obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_2nd.o
		endif
	else
		#obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o
	endif
endif

obj-$(CONFIG_THERMAL) += mtk_cooler_atm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_gpu_cus.o
obj-$(CONFIG_THERMAL) += mtk_cooler_char_curr_cus.o
obj-$(CONFIG_THERMAL) += mtk_cooler_md_cus.o

obj-$(CONFIG_THERMAL) += mtk_cooler_dtm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_sysrst.o
#obj-$(CONFIG_THERMAL) += mtk_cooler_NR.o
#obj-$(CONFIG_THERMAL) += mtk_cooler_VR_FPS.o
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6853"))
ifeq ($(CONFIG_MTK_THERMAL_PA_VIA_ATCMD),y)
    #obj-$(CONFIG_THERMAL) += mtk_cooler_mutt_gen97.o
endif
#obj-$(CONFIG_THERMAL) += mtk_cooler_fps.o
ifeq ($(CONFIG_MTK_PUMP_EXPRESS_PLUS_30_SUPPORT),y)
	#obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_v1.o
else
	ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
		#obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_v1.o
		ifeq ($(CONFIG_MTK_DUAL_CHARGER_SUPPORT),y)
			#obj-$(CONFIG_THERMAL) += mtk_cooler_bcct_2nd.o
		endif
	else
		#obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o
	endif
endif
obj-$(CONFIG_THERMAL) += mtk_cooler_atm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_gpu_cus.o
obj-$(CONFIG_THERMAL) += mtk_cooler_char_curr_cus.o
obj-$(CONFIG_THERMAL) += mtk_cooler_mutt_gen97.o
obj-$(CONFIG_THERMAL) += mtk_cooler_md_cus.o

obj-$(CONFIG_THERMAL) += mtk_cooler_dtm.o
obj-$(CONFIG_THERMAL) += mtk_cooler_sysrst.o
#obj-$(CONFIG_THERMAL) += mtk_cooler_NR.o
#obj-$(CONFIG_THERMAL) += mtk_cooler_VR_FPS.o
endif

