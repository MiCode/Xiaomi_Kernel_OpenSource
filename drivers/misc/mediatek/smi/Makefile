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

ifneq ($(CONFIG_MTK_SMI_VARIANT),y)

ifeq ($(CONFIG_MACH_MT6833),y)
SMI_PLATFORM := "mt6833"
else ifeq ($(CONFIG_MACH_MT6877),y)
SMI_PLATFORM := "mt6877"
else ifeq ($(CONFIG_MACH_MT6781),y)
SMI_PLATFORM := "mt6781"
else
SMI_PLATFORM := $(CONFIG_MTK_PLATFORM)
endif

ifeq ($(CONFIG_MTK_SMI_EXT), y)
ccflags-y += -I$(srctree)/drivers/clk/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(SMI_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi_bwl/$(MTK_PLATFORM)/
ccflags-$(CONFIG_MTK_IOMMU_V2) += -I$(srctree)/drivers/iommu/
ccflags-$(CONFIG_MTK_M4U) += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/$(SMI_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/ # qos/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)/
ccflags-$(CONFIG_MTK_SMI_EXT) += -I$(srctree)/drivers/misc/mediatek/smi/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/$(CONFIG_MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/mt-plat

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6739"))
ccflags-y += -DSMI_ZIO
ccflags-y += -DMMDVFS_HOOK
ccflags-y += -DVCORE_READY
ccflags-y += -I$(srctree)/drivers/clk/mediatek
obj-y += mmdvfs_mgr_v3.o
obj-y += mmdvfs_profile_adaptor.o
obj-y += mmdvfs_mgr_util.o
else ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6761"))
ccflags-y += -DSMI_MER
ccflags-y += -DMMDVFS_HOOK
ccflags-y += -DMMDVFS_QOS_SUPPORT
ccflags-y += -I$(srctree)/drivers/clk/mediatek
obj-y += mmdvfs_mgr_v3.o
obj-y += mmdvfs_profile_adaptor.o
obj-y += mmdvfs_mgr_util.o
else ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6771"))
ccflags-y += -DSMI_SYL
ccflags-y += -DDYNAMIC_DISP_HRT
ccflags-y += -DMMDVFS_HOOK
ccflags-y += -DVCORE_READY
ccflags-y += -DMMDVFS_QOS_SUPPORT
ccflags-y += -I$(srctree)/drivers/clk/mediatek
obj-y += mmdvfs_mgr_v3.o
obj-y += mmdvfs_profile_adaptor.o
obj-y += mmdvfs_mgr_util.o
SUPPORT_USE_DDR_TYPE := true
endif

ifeq (true, $(SUPPORT_USE_DDR_TYPE))
ccflags-y += -DUSE_DDR_TYPE
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule/
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6873" "mt6885"))
ccflags-y += -DSMI_5G
endif

ifneq (,$(filter $(SMI_PLATFORM), "mt6873" "mt6885" "mt6853" "mt6833" "mt6877" "mt6781"))
ccflags-y += -DSMI_SSPM
ccflags-y += -DPMQOS_USE_IOMMU_PORT
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2/
else ifneq (,$(filter $(SMI_PLATFORM), "mt6768" "mt6785"))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1/
else ifeq ($(CONFIG_MACH_MT6785),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6739" "mt6771"))
ccflags-y += -DSMI_CCF_NO_DUMP
endif

obj-y += smi_drv.o
endif

else
obj-y += $(subst ",,variant)/
endif
