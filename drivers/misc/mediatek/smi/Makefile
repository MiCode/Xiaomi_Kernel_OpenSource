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

ifeq ($(CONFIG_MTK_SMI_EXT), y)
ccflags-y += -I$(srctree)/drivers/clk/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi_bwl/$(MTK_PLATFORM)/
ccflags-$(CONFIG_MTK_IOMMU_V2) += -I$(srctree)/drivers/iommu/
ccflags-$(CONFIG_MTK_M4U) += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/$(MTK_PLATFORM)/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/ # qos/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/$(CONFIG_MTK_PLATFORM)

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6739"))
ccflags-y += -DSMI_ZIO
ccflags-y += -DMMDVFS_HOOK
ccflags-y += -I$(srctree)/drivers/clk/mediatek
obj-y += mmdvfs_mgr_v3.o
obj-y += mmdvfs_profile_adaptor.o
obj-y += mmdvfs_mgr_util.o
endif

obj-y += smi_drv.o
endif
