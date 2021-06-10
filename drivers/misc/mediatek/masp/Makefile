# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2021 MediaTek Inc.

MASP_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/masp
ASF_VER := asfv3

subdir-ccflags-y += \
             -I$(MASP_DRIVER_DIR)/$(ASF_VER)/inc \
             -I$(srctree)/drivers/misc/mediatek/include

include $(MASP_DRIVER_DIR)/$(ASF_VER)/Makefile.sec
include $(MASP_DRIVER_DIR)/$(ASF_VER)/Makefile.hacc

sec-y += $(sec_obj) $(hacc_obj)

obj-$(CONFIG_MTK_SECURITY_SW_SUPPORT) += sec.o

