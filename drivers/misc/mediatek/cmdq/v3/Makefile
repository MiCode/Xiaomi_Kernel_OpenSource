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

#MTK_PLATFORM := $(CONFIG_MTK_PLATFORM)

ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/mmdvfs/ \
             -I$(srctree)/drivers/misc/mediatek/mmp/ \
             -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include/ \
             -I$(srctree)/drivers/iommu/

ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/tee/gud/$(CONFIG_TRUSTONIC_TEE_VERSION)/MobiCoreDriver/public/ \
             -I$(srctree)/drivers/tee/gud/$(CONFIG_TRUSTONIC_TEE_VERSION)/MobiCoreDriver/public/GP/
endif	# end pf CONFIG_TRUSTONIC_TEE_SUPPORT

ifeq ($(CONFIG_MICROTRUST_TEE_SUPPORT),y)
endif	# end of CONFIG_MICROTRUST_TEE_SUPPORT

# mdp platform code
#MDP_PLATFORM_DIR=$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)

#platformdir_exists=$(shell if [ -d ${MDP_PLATFORM_DIR} ]; then echo "y"; else echo "n"; fi;)
#ifeq ($(platformdir_exists), y)
#ccflags-y += -I$(MDP_PLATFORM_DIR)/
#else
#ccflags-y += -DCMDQ_COMMON_ENG_SUPPORT
#endif

#ccflags-y += -DCMDQ_CHECK_VALID
#ccflags-y += -DCMDQ_TRACE

# Enable AEE
#ccflags-y += -DCMDQ_AEE_READY

ccflags-y += -DCMDQ_TEST_PROC

ifeq ($(CONFIG_MTK_ENG_BUILD),y)
ccflags-y += -DCMDQ_PROFILE_LOCK
ccflags-y += -DCMDQ_DEBUG_LOOP_IRQ
endif

# secure path
ifeq ($(or $(strip $(CONFIG_TRUSTONIC_TEE_SUPPORT)), $(strip $(CONFIG_MICROTRUST_TEE_SUPPORT))),y)
ifeq ($(or $(strip $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)), $(strip $(CONFIG_MTK_CAM_SECURITY_SUPPORT))),y)

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/inc/cmdq_sec_dr \
             -I$(srctree)/drivers/misc/mediatek/cmdq/v3/inc/cmdq_sec_tl

ccflags-y += -DCMDQ_SECURE_PATH_SUPPORT

ccflags-y += -DCMDQ_EVENT_SVP_BACKUP
endif	# end of CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT or CONFIG_MTK_CAM_SECURITY_SUPPORT
endif	# end of CONFIG_TRUSTONIC_TEE_SUPPORT or CONFIG_MICROTRUST_TEE_SUPPORT

# platform dependent definition
ccflags-y += -DCMDQ_SPECIAL_ESD_PRIORITY
ccflags-y += -D_CMDQ_DISABLE_MARKER_

ifneq ($(strip $(CONFIG_MTK_GMO_RAM_OPTIMIZE)),y)
ccflags-y += -DCMDQ_LARGE_MAX_FIRSTERROR_BUFFER
endif

# driver module
obj-$(CONFIG_MTK_CMDQ)	+= cmdq_core.o
cmdq_core-y	:= cmdq_driver.o cmdq_device.o cmdq_helper_ext.o \
		cmdq_event_common.o cmdq_subsys_common.o cmdq_record.o \
		cmdq_virtual.o cmdq_mdp_common.o cmdq_mdp_mt6779.o
#obj-$(CONFIG_MTK_CMDQ) += cmdq_prof.o
obj-$(CONFIG_MTK_CMDQ) += cmdq_test.o

ifeq ($(strip $(CONFIG_MMPROFILE)),y)
obj-$(CONFIG_MTK_CMDQ) += cmdq_mmp.o
endif

#ifeq ($(platformdir_exists), y)
#obj-$(CONFIG_MTK_CMDQ) += $(subst ",,$(MTK_PLATFORM))/
#endif

ifeq ($(or $(strip $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)), $(strip $(CONFIG_MTK_CAM_SECURITY_SUPPORT))),y)
obj-$(CONFIG_MTK_CMDQ) += cmdq_sec.o
obj-$(CONFIG_MTK_CMDQ) += cmdq_sec_gp.o
endif	# end of CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT or CONFIG_MTK_CAM_SECURITY_SUPPORT

