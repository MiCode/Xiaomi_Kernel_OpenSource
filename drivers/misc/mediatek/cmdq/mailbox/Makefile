# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2021 MediaTek Inc.
#

#==========================================================================
#  Command Queue Common Headers
#==========================================================================

ccflags-y += -I$(srctree)/drivers/iommu \
             -I$(srctree)/drivers/misc/mediatek/iommu/ \
             -I$(srctree)/drivers/misc/mediatek/include \
             -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include \
             -I$(srctree)/drivers/misc/mediatek/mdp/$(MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/mmp/ \
             -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/vcp/rv/ \
             -I$(srctree)/drivers/misc/mediatek/vcp/include/

ccflags-$(CONFIG_DEBUG_FS) += -DCMDQ_MMPROFILE_SUPPORT

#==========================================================================
# Secure Command Queue driver
#==========================================================================

ifneq ($(filter y m, $(or $(CONFIG_MTK_TEE_GP_COORDINATOR), $(CONFIG_MTK_GZ_TZ_SYSTEM))),)
ccflags-y += -DCMDQ_SECURE_SUPPORT

ifneq ($(filter y m, $(CONFIG_MTK_TEE_GP_COORDINATOR)),)
include $(srctree)/drivers/tee/gpapi/Makefile.client.include
include $(srctree)/drivers/tee/teei/Makefile.include
include $(srctree)/drivers/tee/gud/Makefile.include
ccflags-y += -DCMDQ_GP_SUPPORT
else
ifneq ($(filter y m, $(CONFIG_MICROTRUST_TEE_SUPPORT)),)
include $(srctree)/drivers/tee/teei/Makefile.include
endif # !CONFIG_MICROTRUST_TEE_SUPPORT
ifneq ($(filter y m, $(CONFIG_TRUSTONIC_TEE_SUPPORT)),)
include $(srctree)/drivers/tee/gud/Makefile.include
endif # !CONFIG_TRUSTONIC_TEE_SUPPORT
endif # !CONFIG_MTK_TEE_GP_COORDINATOR

ifneq ($(filter y m, $(CONFIG_MTK_GZ_TZ_SYSTEM)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public
ccflags-y += -DCMDQ_SECURE_MTEE_SUPPORT
endif # CONFIG_MTK_GZ_TZ_SYSTEM

obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-sec-drv.o
ifneq ($(filter y m, $(CONFIG_MTK_TEE_GP_COORDINATOR)),)
cmdq-sec-drv-y += cmdq-sec-gp.o
endif # !CONFIG_MTK_TEE_GP_COORDINATOR
ifneq ($(filter y m, $(CONFIG_MTK_GZ_TZ_SYSTEM)),)
cmdq-sec-drv-y += cmdq_sec_mtee.o
endif # !CONFIG_MTK_GZ_TZ_SYSTEM
cmdq-sec-drv-y += cmdq-sec-mailbox.o
cmdq-sec-drv-y += cmdq-sec-helper.o
endif #!CONFIG_MTK_TEE_GP_COORDINATOR or CONFIG_MTK_GZ_TZ_SYSTEM

#==========================================================================
# Normal Command Queue driver
#==========================================================================

obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += mtk-cmdq-drv-ext.o
mtk-cmdq-drv-ext-y := mtk-cmdq-mailbox-ext.o mtk-cmdq-helper-ext.o cmdq-util.o

obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-test.o

ifeq ($(CONFIG_MTK_CMDQ_MBOX_EXT), y)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT_MT6768) += cmdq-platform-mt6768.o
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT_MT6765) += cmdq-platform-mt6765.o
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT_MT6761) += cmdq-platform-mt6761.o
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT_MT6739) += cmdq-platform-mt6739.o
else
ifneq ($(wildcard $(srctree)/$(src)/cmdq-platform-mt6833.c),)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-platform-mt6833.o
endif
ifneq ($(wildcard $(srctree)/$(src)/cmdq-platform-mt6853.c),)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-platform-mt6853.o
endif
ifneq ($(wildcard $(srctree)/$(src)/cmdq-platform-mt6873.c),)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-platform-mt6873.o
endif
ifneq ($(wildcard $(srctree)/$(src)/cmdq-platform-mt6885.c),)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-platform-mt6885.o
endif
ifneq ($(wildcard $(srctree)/$(src)/cmdq-platform-mt6893.c),)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-platform-mt6893.o
endif

ifneq ($(wildcard $(srctree)/$(src)/cmdq-platform-mt6983.c),)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-platform-mt6983.o
endif
ifneq ($(wildcard $(srctree)/$(src)/cmdq-platform-mt6879.c),)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-platform-mt6879.o
endif
ifneq ($(wildcard $(srctree)/$(src)/cmdq-platform-mt6895.c),)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-platform-mt6895.o
endif
ifneq ($(wildcard $(srctree)/$(src)/cmdq-platform-mt6855.c),)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-platform-mt6855.o
endif

ifneq ($(wildcard $(srctree)/$(src)/cmdq-platform-mt6789.c),)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-platform-mt6789.o
endif

ifneq ($(wildcard $(srctree)/$(src)/cmdq-platform-mt6768.c),)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-platform-mt6768.o
endif

ifneq ($(wildcard $(srctree)/$(src)/cmdq-platform-mt6765.c),)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-platform-mt6765.o
endif

ifneq ($(wildcard $(srctree)/$(src)/cmdq-platform-mt6761.c),)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-platform-mt6761.o
endif

ifneq ($(wildcard $(srctree)/$(src)/cmdq-platform-mt6739.c),)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-platform-mt6739.o
endif

endif

