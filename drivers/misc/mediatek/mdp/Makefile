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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/mmdvfs/ \
             -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/ \
             -I$(srctree)/drivers/misc/mediatek/include/ \
             -I$(srctree)/drivers/misc/mediatek/mmp/ \
             -I$(srctree)/drivers/iommu \
             -I$(srctree)/drivers/misc/mediatek/iommu/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox/

ifeq ($(strip $(CONFIG_MTK_PQ_COLOR_MODE)),"MDP")
ccflags-y += -DCMDQ_MDP_COLOR
endif

# Enable MET
ccflags-y += -DMET_USER_EVENT_SUPPORT

# Enable AEE
ccflags-y += -DCMDQ_AEE_READY

# iommu callback support
ifeq ($(strip $(CONFIG_MTK_IOMMU)),y)
#ccflags-y += -DMDP_IOMMU_DEBUG
endif

# dump mdp command
#ccflags-y += -DMDP_COMMAND_SIMULATE

# Secure path for SVP/HDCP/SecCam
ifeq ($(CONFIG_MICROTRUST_TEE_SUPPORT),m)
ifeq ($(CONFIG_MTK_MDP_MTEE_SUPPORT),m)
ccflags-y += -DCMDQ_SECURE_PATH_SUPPORT
endif
endif

# platform dependent definition
ifneq ($(strip $(CONFIG_MTK_GMO_RAM_OPTIMIZE)),y)
ccflags-y += -DCMDQ_LARGE_MAX_FIRSTERROR_BUFFER
endif

# driver module
obj-$(CONFIG_MTK_MDP) += cmdq_helper_inf.o
ifneq ($(wildcard $(srctree)/$(src)/mdp_mt6983.c),)
obj-$(CONFIG_MTK_MDP) += mdp_drv_mt6983.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mdp_mt6985.c),)
obj-$(CONFIG_MTK_MDP) += mdp_drv_mt6985.o
endif

mdp_drv_mt6983-objs := mdp_mt6983.o \
	mdp_driver.o \
	mdp_ioctl_ex.o \
	mdp_common.o \
	cmdq_record.o \
	cmdq_virtual.o \
	cmdq_device.o \
	mdp_event_common.o \
	cmdq_subsys_common.o \
	cmdq_helper_ext.o \
	cmdq_mmp.o \
	mdp_fence.o

mdp_drv_mt6985-objs := mdp_mt6985.o \
	mdp_driver.o \
	mdp_ioctl_ex.o \
	mdp_common.o \
	cmdq_record.o \
	cmdq_virtual.o \
	cmdq_device.o \
	mdp_event_common.o \
	cmdq_subsys_common.o \
	cmdq_helper_ext.o \
	cmdq_mmp.o \
	mdp_fence.o

#obj-y += cmdq_prof.o
#obj-y += cmdq_test.o

#ifeq ($(strip $(CONFIG_MMPROFILE)),y)
#obj-y += cmdq_mmp.o
#endif

