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
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/fakeHeader/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/video/include
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/gpu/ged/include
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/common/inc/

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
	obj-$(CONFIG_THERMAL)  += mtk_cooler_3Gmutt.o
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

obj-$(CONFIG_THERMAL) += mtk_ta.o
