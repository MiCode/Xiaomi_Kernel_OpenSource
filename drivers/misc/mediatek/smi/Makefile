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
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/qos/$(MTK_PLATFORM)/
ccflags-$(CONFIG_MACH_MT6779) += -I$(srctree)/drivers/misc/mediatek/emi/$(MTK_PLATFORM)/
ccflags-$(CONFIG_MACH_MT6779) += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-$(CONFIG_MACH_MT6779) += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi_bwl/$(MTK_PLATFORM)/


BUILD_ALL_FILES := true
ccflags-y += -I$(srctree)/drivers/clk/mediatek
ccflags-y += -I$(srctree)/drivers/devfreq
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/$(MTK_PLATFORM)/

ifeq ($(CONFIG_MACH_MT6761),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/mt6765/
else ifeq ($(CONFIG_MACH_MT6757),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(MTK_PLATFORM)/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/
endif

obj-y += smi_legacy.o
SUPPORT_QOS := true
FORCE_STEP0 := true
SKIP_SMI_CONFIG := false
SUPPORT_MET_SMI := true

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6757" "kiboplus"))
SUPPORT_QOS := false
SUPPORT_MET_SMI := false
ccflags-y += -DSMI_OLY
ccflags-y += -DSMI_MMU_V2
ccflags-y += -I$(srctree)/drivers/clk/mediatek
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/$(CONFIG_MTK_PLATFORM)/
obj-y += mmdvfs_mgr_v2.o
obj-y += mmdvfs_mgr_util.o
else ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6758"))
ccflags-y += -DSMI_VIN
ccflags-y += -DMMDVFS_HOOK

SUPPORT_QOS := false
obj-y += mmdvfs_mgr_v3.o
obj-y += mmdvfs_mgr_util.o
obj-y += mmdvfs_profile_adaptor.o
else ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6763"))
ccflags-y += -DSMI_BIA
ccflags-y += -DSPECIAL_BW_CONFIG_MM
ccflags-y += -DMMDVFS_HOOK
ccflags-y += -DVCORE_READY
SUPPORT_QOS := false
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
obj-y += mmdvfs_mgr_v3.o
obj-y += mmdvfs_mgr_util.o
obj-y += mmdvfs_profile_adaptor.o
SUPPORT_USE_DDR_TYPE := true
else ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6765"))
ccflags-y += -DSMI_CER
ccflags-y += -DMMDVFS_HOOK
obj-y += mmdvfs_mgr_v3.o
obj-y += mmdvfs_mgr_util.o
obj-y += mmdvfs_profile_adaptor.o
SUPPORT_USE_DDR_TYPE := true
SUPPORT_MMP := true
FORCE_STEP0 := false
else ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6761"))
ccflags-y += -DSMI_MER
ccflags-y += -DMMDVFS_HOOK
ccflags-y += -DPLL_HOPPING_READY
obj-y += mmdvfs_mgr_v3.o
obj-y += mmdvfs_mgr_util.o
obj-y += mmdvfs_profile_adaptor.o
SUPPORT_MMP := true
FORCE_STEP0 := false
else ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt3967"))
ccflags-y += -DSMI_EIG
ccflags-y += -DMMDVFS_HOOK
obj-y += mmdvfs_mgr_v3.o
obj-y += mmdvfs_mgr_util.o
obj-y += mmdvfs_profile_adaptor.o
SUPPORT_MMP := true
FORCE_STEP0 := false
else ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6779"))
ccflags-y += -DSMI_LAF
ccflags-y += -DMMDVFS_HOOK
#ccflags-y += -DDYNAMIC_DISP_HRT
#ccflags-y += -DSIMULATE_DVFSRC
ccflags-y += -DDVFSRC_HRT
ccflags-y += -DBLOCKING_MECHANISM
ccflags-y += -DMMDVFS_LIMIT
ccflags-y += -DCHECK_VOLTAGE
#ccflags-y += -DAEE_CAM_BW_MISMATCH
obj-y += mmdvfs_mgr_v3.o
obj-y += mmdvfs_mgr_util.o
obj-y += mmdvfs_profile_adaptor.o
SUPPORT_MMP := true
FORCE_STEP0 := false
SKIP_SMI_CONFIG := false
QOS_BOUND_DETECT := true
APPLY_CLK_LOG := false
endif

SMI_PLATFORM_DIR=$(srctree)/drivers/misc/mediatek/smi/$(MTK_PLATFORM)
platformdir_exists=$(shell if [ -d $(SMI_PLATFORM_DIR) ]; then echo "y"; else echo "n"; fi;)

ifeq (true, $(SUPPORT_QOS))
obj-y += mmdvfs_pmqos.o
ccflags-y += -DMMDVFS_QOS_SUPPORT
ifeq (true, $(FORCE_STEP0))
ccflags-y += -DMMDVFS_FORCE_STEP0
endif
ifeq (true, $(SKIP_SMI_CONFIG))
ccflags-y += -DMMDVFS_SKIP_SMI_CONFIG
endif
ifeq ($(platformdir_exists), y)
obj-y += $(subst ",,$(MTK_PLATFORM))/
else
obj-y += mmdvfs_plat_default.o
endif
ifeq (true, $(APPLY_CLK_LOG))
ccflags-y += -DAPPLY_CLK_LOG
ccflags-y += -I$(srctree)/drivers/clk/mediatek
endif
endif

ifeq (true, $(QOS_BOUND_DETECT))
ccflags-y += -DQOS_BOUND_DETECT
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/qos/$(MTK_PLATFORM)/
endif

ifeq ($(CONFIG_MMPROFILE),y)
ifeq (true, $(SUPPORT_MMP))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -DMMDVFS_MMP
endif
endif
ifeq (true, $(SUPPORT_MET_SMI))
obj-y += $(subst ",,met)/
endif
ifeq (true, $(SUPPORT_USE_DDR_TYPE))
ccflags-y += -DUSE_DDR_TYPE
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule/
endif

else
obj-y += $(subst ",,variant)/
endif
