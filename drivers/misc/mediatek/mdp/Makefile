#
# Copyright (C) 2019 MediaTek Inc.
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

# driver code for MDP

ifeq ($(CONFIG_MACH_MT6833),y)
MTK_PLATFORM := "mt6833"
else
MTK_PLATFORM := $(CONFIG_MTK_PLATFORM)
endif

ccflags-$(CONFIG_MACH_MT6885) += -DCMDQ_MDP_ENABLE_SPR
ccflags-$(CONFIG_MACH_MT6885) += -DMDP_MMPATH
ccflags-$(CONFIG_MACH_MT6893) += -DCMDQ_MDP_ENABLE_SPR
ccflags-$(CONFIG_MACH_MT6893) += -DMDP_MMPATH
ccflags-$(CONFIG_MACH_MT6873) += -DCMDQ_MDP_ENABLE_SPR
ccflags-$(CONFIG_MACH_MT6873) += -DMDP_MMPATH
ccflags-$(CONFIG_MACH_MT6853) += -DCMDQ_MDP_ENABLE_SPR
ccflags-$(CONFIG_MACH_MT6853) += -DMDP_MMPATH
ccflags-$(CONFIG_MACH_MT6893) += -DCMDQ_MDP_ENABLE_SPR
ccflags-$(CONFIG_MACH_MT6893) += -DPMQOS_VERSION2
ccflags-$(CONFIG_MACH_MT6893) += -DMDP_MMPATH
ccflags-$(CONFIG_MACH_MT6833) += -DCMDQ_MDP_ENABLE_SPR
ccflags-$(CONFIG_MACH_MT6833) += -DMDP_MMPATH

ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/ \
	     -I$(srctree)/drivers/misc/mediatek/smi/$(MTK_PLATFORM)/ \
	     -I$(srctree)/drivers/misc/mediatek/mmdvfs/ \
	     -I$(srctree)/drivers/misc/mediatek/mmp/ \
	     -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include \
	     -I$(srctree)/drivers/iommu/ \
	     -I$(srctree)/drivers/staging/android/ion \
	     -I$(srctree)/drivers/staging/android/ion/mtk

ccflags-$(CONFIG_MTK_CMDQ_MBOX_EXT) += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox/

ifeq ($(CONFIG_MTK_CMDQ_V3),y)
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6739" "mt6768" "mt6771" "mt8168" "mt6785"))
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/ \
		-I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)
endif
else ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt8167"))
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v2/ \
		-I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(MTK_PLATFORM)
endif


# mdp platform code
MDP_PLATFORM_DIR=$(srctree)/drivers/misc/mediatek/mdp/$(MTK_PLATFORM)

platformdir_exists=$(shell if [ -d ${MDP_PLATFORM_DIR} ]; then echo "y"; else echo "n"; fi;)
ifeq ($(platformdir_exists), y)
ccflags-y += -I$(MDP_PLATFORM_DIR)/
else
ccflags-y += -DMDP_COMMON_ENG_SUPPORT
endif

ifeq ($(CONFIG_MTK_ENG_BUILD),y)
ccflags-y += -DCMDQ_TEST_PROC
ccflags-y += -DCMDQ_PROFILE_LOCK
ccflags-y += -DCMDQ_DEBUG_LOOP_IRQ
ccflags-y += -DMDP_COMMAND_SIMULATE
endif

# secure path
# use CONFIG_MTK_CMDQ_MBOX_EXT support
# secure path
ifeq ($(or $(strip $(CONFIG_TRUSTONIC_TEE_SUPPORT)), $(strip $(CONFIG_MICROTRUST_TEE_SUPPORT)), \
	$(strip $(CONFIG_MTK_ENABLE_GENIEZONE))),y)
ifeq ($(or $(strip $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)), $(strip $(CONFIG_MTK_CAM_SECURITY_SUPPORT))),y)
ccflags-y += -DCMDQ_SECURE_PATH_SUPPORT
endif	# end of CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT or CONFIG_MTK_CAM_SECURITY_SUPPORT
endif	# end of CONFIG_TRUSTONIC_TEE_SUPPORT or CONFIG_MICROTRUST_TEE_SUPPORT

ifeq ($(or $(strip $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)), $(strip $(CONFIG_MTK_CAM_SECURITY_SUPPORT))),y)
ifeq ($(strip $(CONFIG_MTK_ENABLE_GENIEZONE)),y)
ifeq ($(strip $(CONFIG_MTK_CAM_GENIEZONE_SUPPORT)),y)
ccflags-y += -DCMDQ_SECURE_MTEE_SUPPORT
endif
endif
endif

# platform dependent definition
ifneq ($(strip $(CONFIG_MTK_GMO_RAM_OPTIMIZE)),y)
ccflags-y += -DCMDQ_LARGE_MAX_FIRSTERROR_BUFFER
endif

# driver module
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq_record.o
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq_virtual.o
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq_device.o
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += mdp_driver.o
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += mdp_common.o
#obj-y += cmdq_prof.o
#obj-y += cmdq_test.o
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += mdp_event_common.o
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq_subsys_common.o
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq_helper_ext.o
obj-y += mdp_ioctl_ex.o
obj-y += mdp_m4u.o

ifeq ($(strip $(CONFIG_MMPROFILE)),y)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq_mmp.o
endif

ifeq ($(platformdir_exists), y)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += $(subst ",,$(MTK_PLATFORM))/
endif

# mdp_sw_sync
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += sync/

