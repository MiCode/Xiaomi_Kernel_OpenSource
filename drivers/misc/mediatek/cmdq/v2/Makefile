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
# 72/82/92 does not use this common code

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/mmp/ \
             -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include

# Enable AEE
ccflags-y += -DCMDQ_AEE_READY

# Enable MET
ccflags-y += -DMET_USER_EVENT_SUPPORT

ifneq ($(TARGET_BUILD_VARIANT),eng)
ccflags-y += -D_MTK_USER_
endif

ifneq ($(TARGET_BUILD_VARIANT),user)
ccflags-y += -D_CMDQ_TEST_PROC_
endif

# Common Clock Framework
ifneq ($(CONFIG_MTK_CLKMGR),y)

ifeq ($(CONFIG_ARCH_MT6580),y)
else ifeq ($(CONFIG_ARCH_MT6735M),y)
else ifeq ($(CONFIG_ARCH_MT6753),y)
else
ccflags-y += -DCMDQ_USE_CCF
endif

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

ifeq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6735"))
ccflags-y += -DCMDQ_EVENT_SVP_BACKUP
endif

endif
endif

# platform dependent definition
ccflags-$(CONFIG_MTK_SMI_EXT) += -DCMDQ_CONFIG_SMI

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6570" "mt6580"))
ccflags-y += -DCMDQ_DUMP_IMG_CLOCK_STATE
endif

# ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6755"))
# ccflags-$(CONFIG_MTK_MET) += -DCMDQ_MDP_MET_STATUS
# endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6735" "mt6570" "mt6580"))
ccflags-y += -DCMDQ_USE_LARGE_MAX_COOKIE
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6797" "mt6757"))
ccflags-y += -DCMDQ_LARGE_MAX_FIRSTERROR_BUFFER
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6735" "mt6570" "mt6580" "mt6755" "mt6797"))
ccflags-y += -DCMDQ_DISP_LEGACY_SUB_SCENARIO
endif

ifeq ($(CONFIG_MTK_CMDQ_TAB), y)
ccflags-y += -DCMDQ_USE_LEGACY
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v2/inc/cmdq_sec_dr \
             -I$(srctree)/drivers/misc/mediatek/cmdq/v2/inc/cmdq_sec_tl \
             -I$(srctree)/include/trustzone

ifeq ($(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT),y)
ccflags-y += -DCMDQ_SECURE_PATH_SUPPORT
endif
endif

ccflags-y += -D_CMDQ_DISABLE_MARKER_
ifeq (,$(filter $(CONFIG_MTK_PLATFORM), "elbrus"))
ccflags-y += -DCMDQ_PROFILE_MMP
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
ifneq ($(CONFIG_MTK_CMDQ_TAB),y)
obj-y += cmdq_sec.o
else
obj-y += mtee/
endif

obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
