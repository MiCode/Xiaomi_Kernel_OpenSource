# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

ifeq ($(CONFIG_MICROTRUST_TEE_SUPPORT),y)
include $(srctree)/drivers/misc/mediatek/teei/Makefile.include
endif

ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
endif

obj-y += cmdq-test.o

obj-$(CONFIG_MTK_TEE_GP_SUPPORT) += cmdq-sec-helper.o
obj-$(CONFIG_MTK_TEE_GP_SUPPORT) += cmdq-sec-mailbox.o
obj-$(CONFIG_MTK_TEE_GP_SUPPORT) += cmdq-sec-gp.o
