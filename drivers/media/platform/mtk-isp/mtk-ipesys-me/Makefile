# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2018 MediaTek Inc.
#
export MTK_IMGSYS_VER := $(subst ",,$(CONFIG_MTK_IMGSYS_VER))
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp/mtk-hcp
ccflags-$(CONFIG_MTK_CMDQ_MBOX_EXT) += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox/
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp/imgsys/
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp/imgsys/platforms/
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp/imgsys/platforms/isp71
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/
ccflags-y += -I$(srctree)/drivers/iommu
ccflags-y += -I$(srctree)/drivers/misc/mediatek/iommu

# Utilities to provide frame-based streaming model
# with v4l2 user interfaces and alloc context managing
# memory shared between ISP and coprocessor


mtk-ipesys-me-objs :=\
mtk-ipesys-me.o

obj-m += mtk-ipesys-me.o

