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

ifneq ($(CONFIG_MTK_SMI_VARIANT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/

BUILD_ALL_FILES := true
ccflags-y += -I$(srctree)/drivers/clk/mediatek
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/$(CONFIG_MTK_PLATFORM)/

obj-y += smi_legacy.o

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6758"))
ccflags-y += -DSMI_VIN
ccflags-y += -DMMDVFS_HOOK

obj-y += mmdvfs_mgr_v3.o
obj-y += mmdvfs_mgr_util.o
obj-y += mmdvfs_profile_adaptor.o
else ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6765"))
ccflags-y += -DSMI_CER
ccflags-y += -DMMDVFS_HOOK
#ccflags-y += -DVCORE_READY
#ccflags-y += -DPLL_HOPPING_READY
ccflags-y += -DMMDVFS_QOS_SUPPORT
obj-y += mmdvfs_mgr_v3.o
obj-y += mmdvfs_mgr_util.o
obj-y += mmdvfs_profile_adaptor.o
SUPPORT_USE_DDR_TYPE := true
SUPPORT_QOS := true
SUPPORT_MMP := true
endif


ifeq (true, $(SUPPORT_QOS))
obj-y += mmdvfs_pmqos.o
endif


ifeq ($(CONFIG_MMPROFILE),y)
ifeq (true, $(SUPPORT_MMP))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -DMMDVFS_MMP
endif
endif
obj-y += $(subst ",,met)/

ifeq (true, $(SUPPORT_USE_DDR_TYPE))
ccflags-y += -DUSE_DDR_TYPE
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule/
endif

else
obj-y += $(subst ",,variant)/
endif
