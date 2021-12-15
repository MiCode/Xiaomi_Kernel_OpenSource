# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

ifeq ($(CONFIG_MACH_MT6833),y)
CMDQ_PLATFORM := mt6833
else ifeq ($(CONFIG_MACH_MT6877),y)
CMDQ_PLATFORM := mt6877
else ifeq ($(CONFIG_MACH_MT6781),y)
CMDQ_PLATFORM := mt6781
else
CMDQ_PLATFORM := $(MTK_PLATFORM)
endif

ifeq ($(CONFIG_MACH_MT6833),y)
MDP_PLATFORM := "mt6833"
else ifeq ($(CONFIG_MACH_MT6781),y)
MDP_PLATFORM := mt6781
else
MDP_PLATFORM := $(CONFIG_MTK_PLATFORM)
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(CMDQ_PLATFORM)/include \
             -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/mdp/$(MDP_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/mmp/
ifeq ($(or $(strip $(CONFIG_MTK_SVP_ON_MTEE_SUPPORT)), $(strip $(CONFIG_MTK_CAM_GENIEZONE_SUPPORT))),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/m4u
ccflags-y += -I$(srctree)/drivers/iommu
endif


ifeq ($(or $(strip $(CONFIG_MTK_SVP_ON_MTEE_SUPPORT)), $(strip $(CONFIG_MTK_CAM_GENIEZONE_SUPPORT))),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/m4u
ccflags-y += -I$(srctree)/drivers/iommu
endif

ifeq ($(or $(strip $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)), $(strip $(CONFIG_MTK_CAM_SECURITY_SUPPORT))),y)

ifeq ($(CONFIG_MICROTRUST_TEE_SUPPORT),y)
include $(srctree)/drivers/misc/mediatek/teei/Makefile.include
endif

ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
endif

ifeq ($(strip $(CONFIG_MTK_ENABLE_GENIEZONE)),y)
ifeq ($(strip $(CONFIG_MTK_CAM_GENIEZONE_SUPPORT)),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public
ccflags-y += -DCMDQ_SECURE_MTEE_SUPPORT
obj-y += cmdq_sec_mtee.o
endif
endif

ifeq ($(strip $(CONFIG_MTK_SVP_ON_MTEE_SUPPORT)),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public
ccflags-y += -DCMDQ_SECURE_MTEE_SUPPORT
obj-y += cmdq_sec_mtee.o
endif

ccflags-y += -DCMDQ_SECURE_SUPPORT
obj-y += cmdq-sec-helper.o
obj-y += cmdq-sec-mailbox.o

endif # $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) or $(CONFIG_MTK_CAM_SECURITY_SUPPORT)

obj-y += cmdq-test.o
obj-y += cmdq-util.o
obj-y += $(CMDQ_PLATFORM)/cmdq-platform.o
obj-y += cmdq-bw-mon.o
