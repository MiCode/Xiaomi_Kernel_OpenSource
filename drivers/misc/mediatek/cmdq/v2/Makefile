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

# common driver code for CMDQ


ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/mmp/ \
             -I$(srctree)/drivers/misc/mediatek/mdp/ \
             -I$(srctree)/drivers/iommu/ \
             -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include

ccflags-y += -I$(srctree)/kernel

# Enable AEE
ccflags-y += -DCMDQ_AEE_READY

# Enable MET
ccflags-y += -DMET_USER_EVENT_SUPPORT

ifeq ($(CONFIG_MTK_ENG_BUILD),y)
ccflags-y += -D_CMDQ_TEST_PROC_
ccflags-y += -D_CMDQ_DEBUG_
endif

# secure path
ifeq ($(strip $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)),y)
ifeq ($(strip $(CONFIG_TRUSTONIC_TEE_SUPPORT)),y)
include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v2/inc/cmdq_sec_dr \
             -I$(srctree)/drivers/misc/mediatek/cmdq/v2/inc/cmdq_sec_tl

ccflags-y += -DCMDQ_SECURE_PATH_SUPPORT
ifneq ($(CONFIG_MTK_CMDQ_TAB),y)
ccflags-y += -DCMDQ_SECURE_PATH_NORMAL_IRQ
endif
ccflags-y += -DCMDQ_EVENT_SVP_BACKUP
endif
endif


# platform dependent definition
ccflags-$(CONFIG_MTK_SMI_EXT) += -DCMDQ_CONFIG_SMI
# ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6755"))
# ccflags-$(CONFIG_MTK_MET) += -DCMDQ_MDP_MET_STATUS
# endif
ccflags-y += -DCMDQ_LARGE_MAX_FIRSTERROR_BUFFER
ccflags-y += -DCMDQ_SPECIAL_ESD_PRIORITY
ccflags-y += -D_CMDQ_DISABLE_MARKER_
#ccflags-y += -DCMDQ_PROFILE_MMP

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt8167" "mt8173"))
#ccflags-y += -DCMDQ_USE_LEGACY
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v2/inc/cmdq_sec_dr \
             -I$(srctree)/drivers/misc/mediatek/cmdq/v2/inc/cmdq_sec_tl \
             -I$(srctree)/drivers/misc/mediatek/include/mt-plat/trustzone

ifeq ($(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT),y)
ccflags-y += -DCMDQ_SECURE_PATH_SUPPORT
endif
endif

# driver module
obj-y += cmdq_record.o
obj-y += cmdq_core.o
obj-y += cmdq_virtual.o
obj-y += cmdq_device.o
obj-y += cmdq_driver.o
obj-y += cmdq_mmp.o
obj-y += cmdq_mdp_common.o
obj-y += cmdq_prof.o
obj-y += cmdq_test.o
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt8167" "mt8173"))
obj-y += cmdq_sec.o
else
obj-y += mtee/
endif

obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
