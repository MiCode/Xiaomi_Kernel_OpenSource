# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

ifeq ($(CONFIG_MACH_MT6833),y)
CMDQ_PLATFORM := mt6833
else
CMDQ_PLATFORM := $(MTK_PLATFORM)
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(CMDQ_PLATFORM)/include \
             -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/mdp/$(CMDQ_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/mmp/

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

ccflags-y += -DCMDQ_SECURE_SUPPORT

ifeq ($(strip $(CONFIG_MTK_TEE_GP_SUPPORT)),y)
ccflags-y += -DCMDQ_GP_SUPPORT
obj-y += cmdq-sec-gp.o
endif
obj-y += cmdq-sec-helper.o
obj-y += cmdq-sec-mailbox.o

endif # $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) or $(CONFIG_MTK_CAM_SECURITY_SUPPORT)

obj-y += cmdq-test.o
obj-y += cmdq-util.o
obj-y += $(CMDQ_PLATFORM)/cmdq-platform.o
obj-y += cmdq-bw-mon.o
