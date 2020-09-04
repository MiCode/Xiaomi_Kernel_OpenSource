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


#ccflags-$(CONFIG_MACH_MT6768) += -DCMDQ_MDP_ENABLE_SPR
#ccflags-$(CONFIG_MACH_MT6885) += -DCMDQ_MDP_ENABLE_SPR
#ccflags-$(CONFIG_MACH_MT6885) += -DMDP_MMPATH
#ccflags-$(CONFIG_MACH_MT6893) += -DCMDQ_MDP_ENABLE_SPR
#ccflags-$(CONFIG_MACH_MT6893) += -DMDP_MMPATH
ccflags-$(CONFIG_MTK_MDP_MT6873) += -DCMDQ_MDP_ENABLE_SPR
ccflags-$(CONFIG_MTK_MDP_MT6873) += -DMDP_MMPATH
#ccflags-$(CONFIG_MACH_MT6853) += -DCMDQ_MDP_ENABLE_SPR
#ccflags-$(CONFIG_MACH_MT6853) += -DMDP_MMPATH

ifeq ($(CONFIG_MACH_MT6785),y)
ccflags-y += -DMDP_MMPATH
ccflags-y += -DCMDQ_MDP_ENABLE_SPR
ccflags-$(CONFIG_MTK_DEVAPC) += -DMDP_DEVAPC
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/mmdvfs/ \
             -I$(srctree)/drivers/misc/mediatek/include/ \
             -I$(srctree)/drivers/misc/mediatek/mmp/ \
             -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include \
             -I$(srctree)/drivers/iommu

ccflags-$(CONFIG_MTK_MDP_MT6873) += -I$(srctree)/drivers/misc/mediatek/mdp/mt6873

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox/

ifeq ($(strip $(CONFIG_MTK_PQ_COLOR_MODE)),"MDP")
ccflags-y += -DCMDQ_MDP_COLOR
endif

# Enable MET
ccflags-y += -DMET_USER_EVENT_SUPPORT

# Enable AEE
ccflags-y += -DCMDQ_AEE_READY

# secure path
ifeq ($(or $(strip $(CONFIG_TRUSTONIC_TEE_SUPPORT)), $(strip $(CONFIG_MICROTRUST_TEE_SUPPORT))),y)
ifeq ($(or $(strip $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)), $(strip $(CONFIG_MTK_CAM_SECURITY_SUPPORT))),y)
ccflags-y += -DCMDQ_SECURE_PATH_SUPPORT
endif	# end of CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT or CONFIG_MTK_CAM_SECURITY_SUPPORT
endif	# end of CONFIG_TRUSTONIC_TEE_SUPPORT or CONFIG_MICROTRUST_TEE_SUPPORT

# secure path
# use CONFIG_MTK_CMDQ_MBOX_EXT support
# secure path
ifeq ($(or $(strip $(CONFIG_TRUSTONIC_TEE_SUPPORT)), $(strip $(CONFIG_MICROTRUST_TEE_SUPPORT)), \
	$(strip $(CONFIG_MTK_ENABLE_GENIEZONE))),y)
ifeq ($(or $(strip $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)), $(strip $(CONFIG_MTK_CAM_SECURITY_SUPPORT))),y)
ccflags-y += -DCMDQ_SECURE_PATH_SUPPORT
endif	# end of CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT or CONFIG_MTK_CAM_SECURITY_SUPPORT
endif	# end of CONFIG_TRUSTONIC_TEE_SUPPORT or CONFIG_MICROTRUST_TEE_SUPPORT

# platform dependent definition
ifneq ($(strip $(CONFIG_MTK_GMO_RAM_OPTIMIZE)),y)
ccflags-y += -DCMDQ_LARGE_MAX_FIRSTERROR_BUFFER
endif

# driver module
obj-$(CONFIG_MTK_MDP) += mdp_drv.o
mdp_drv-y := mdp_driver.o \
				mdp_ioctl_ex.o

obj-$(CONFIG_MTK_MDP) += mdp_core.o
mdp_core-y := mdp_common.o \
				cmdq_record.o \
				cmdq_virtual.o \
				cmdq_device.o \
				mdp_event_common.o \
				cmdq_subsys_common.o \
				cmdq_helper_ext.o \
				cmdq_mmp.o

mdp_core-$(CONFIG_MTK_MDP_MT6873) += mdp_mt6873.o
#obj-y += cmdq_prof.o
#obj-y += cmdq_test.o

#ifeq ($(strip $(CONFIG_MMPROFILE)),y)
#obj-y += cmdq_mmp.o
#endif

#ifeq ($(platformdir_exists), y)
#obj-y += $(subst ",,$(MTK_PLATFORM))/
#endif

# mdp_sw_sync
obj-$(CONFIG_MTK_MDP) += sync/

