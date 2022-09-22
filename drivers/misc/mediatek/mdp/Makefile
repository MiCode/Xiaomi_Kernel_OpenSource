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

ifeq ($(CONFIG_MICROTRUST_TEE_SUPPORT),m)
ifeq ($(CONFIG_MTK_MDP_MTEE_SUPPORT),m)
ccflags-y += -I$(srctree)/drivers/dma-buf/heaps
endif
endif

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
obj-$(CONFIG_MTK_MDP_MT6873) += mdp_drv_mt6873.o
obj-$(CONFIG_MTK_MDP_MT6853) += mdp_drv_mt6853.o
ifeq ($(CONFIG_MTK_MDP), y)
obj-$(CONFIG_MTK_MDP_MT6768) += mdp_drv_mt6768.o
else
ifneq ($(wildcard $(srctree)/$(src)/mdp_mt6893.c),)
obj-$(CONFIG_MTK_MDP) += mdp_drv_mt6893.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mdp_mt6983.c),)
obj-$(CONFIG_MTK_MDP) += mdp_drv_mt6983.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mdp_mt6879.c),)
obj-$(CONFIG_MTK_MDP) += mdp_drv_mt6879.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mdp_mt6895.c),)
obj-$(CONFIG_MTK_MDP) += mdp_drv_mt6895.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mdp_mt6855.c),)
obj-$(CONFIG_MTK_MDP) += mdp_drv_mt6855.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mdp_mt6833.c),)
obj-$(CONFIG_MTK_MDP) += mdp_drv_mt6833.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mdp_mt6789.c),)
obj-$(CONFIG_MTK_MDP) += mdp_drv_mt6789.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mdp_mt6768.c),)
obj-$(CONFIG_MTK_MDP) += mdp_drv_mt6768.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mdp_mt6765.c),)
obj-$(CONFIG_MTK_MDP) += mdp_drv_mt6765.o
endif
endif

mdp_drv_mt6873-objs := mdp_drv.o mdp_mt6873.o
mdp_drv_mt6853-objs := mdp_drv.o mdp_mt6853.o
mdp_drv_mt6833-objs := mdp_drv.o mdp_mt6833.o
mdp_drv_mt6893-objs := mdp_drv.o mdp_mt6893.o
mdp_drv_mt6983-objs := mdp_drv.o mdp_mt6983.o
mdp_drv_mt6879-objs := mdp_drv.o mdp_mt6879.o
mdp_drv_mt6895-objs := mdp_drv.o mdp_mt6895.o
mdp_drv_mt6855-objs := mdp_drv.o mdp_mt6855.o
mdp_drv_mt6789-objs := mdp_drv.o mdp_mt6789.o
mdp_drv_mt6768-objs := mdp_drv.o mdp_mt6768.o
mdp_drv_mt6765-objs := mdp_drv.o mdp_mt6765.o

mdp_drv-objs := mdp_driver.o \
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

