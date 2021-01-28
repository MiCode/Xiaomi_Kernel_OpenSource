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

MTK_PLATFORM := $(CONFIG_MTK_PLATFORM)

ccflags-$(CONFIG_MACH_MT6768) += -DCMDQ_MDP_ENABLE_SPR
ccflags-$(CONFIG_MACH_MT6885) += -DCMDQ_MDP_ENABLE_SPR
ccflags-$(CONFIG_MACH_MT6885) += -DPMQOS_VERSION2
ccflags-$(CONFIG_MACH_MT6885) += -DMDP_MMPATH
ccflags-$(CONFIG_MACH_MT6873) += -DCMDQ_MDP_ENABLE_SPR
ccflags-$(CONFIG_MACH_MT6873) += -DPMQOS_VERSION2
ccflags-$(CONFIG_MACH_MT6873) += -DMDP_MMPATH

ifeq ($(CONFIG_MACH_MT6785),y)
ccflags-y += -DPMQOS_VERSION2
ccflags-y += -DMDP_MMPATH
ccflags-y += -DCMDQ_MDP_ENABLE_SPR
ccflags-$(CONFIG_MTK_DEVAPC) += -DMDP_DEVAPC
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/smi/$(MTK_PLATFORM)/ \
             -I$(srctree)/drivers/misc/mediatek/mmdvfs/ \
             -I$(srctree)/drivers/misc/mediatek/mmp/ \
             -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include

ccflags-$(CONFIG_MTK_CMDQ_MBOX_EXT) += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox/

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
endif

# secure path
# use CONFIG_MTK_CMDQ_MBOX_EXT support
# secure path
ifeq ($(or $(strip $(CONFIG_TRUSTONIC_TEE_SUPPORT)), $(strip $(CONFIG_MICROTRUST_TEE_SUPPORT))),y)
ifeq ($(or $(strip $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)), $(strip $(CONFIG_MTK_CAM_SECURITY_SUPPORT))),y)
ccflags-y += -DCMDQ_SECURE_PATH_SUPPORT
endif	# end of CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT or CONFIG_MTK_CAM_SECURITY_SUPPORT
endif	# end of CONFIG_TRUSTONIC_TEE_SUPPORT or CONFIG_MICROTRUST_TEE_SUPPORT

# platform dependent definition
ifneq ($(strip $(CONFIG_MTK_GMO_RAM_OPTIMIZE)),y)
ccflags-y += -DCMDQ_LARGE_MAX_FIRSTERROR_BUFFER
endif

# driver module
obj-y += cmdq_record.o
obj-y += cmdq_virtual.o
obj-y += cmdq_device.o
obj-y += mdp_driver.o
obj-y += mdp_common.o
#obj-y += cmdq_prof.o
#obj-y += cmdq_test.o
obj-y += mdp_event_common.o
obj-y += cmdq_subsys_common.o
obj-y += cmdq_helper_ext.o

ifeq ($(strip $(CONFIG_MMPROFILE)),y)
obj-y += cmdq_mmp.o
endif

ifeq ($(platformdir_exists), y)
obj-y += $(subst ",,$(MTK_PLATFORM))/
endif

# mdp_sw_sync
obj-y += sync/

