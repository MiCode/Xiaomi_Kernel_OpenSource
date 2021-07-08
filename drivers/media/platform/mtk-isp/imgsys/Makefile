# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2018 MediaTek Inc.
#
export MTK_IMGSYS_VER := $(subst ",,$(CONFIG_MTK_IMGSYS_VER))
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp/mtk-hcp
ccflags-$(CONFIG_MTK_CMDQ_MBOX_EXT) += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox/
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp/imgsys/platforms/
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp/imgsys/platforms/isp71
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/

# Utilities to provide frame-based streaming model
# with v4l2 user interfaces and alloc context managing
# memory shared between ISP and coprocessor
mtk_imgsys_util-objs := \
mtk_imgsys-v4l2.o \
mtk_imgsys-dev.o \
mtk_imgsys-of.o \
mtk_imgsys-sys.o \
mtk_imgsys-cmdq.o

mtk_imgsys_hw_isp71-objs := \
platforms/isp71/mtk_imgsys-debug.o \
platforms/isp71/modules/mtk_imgsys-dip.o \
platforms/isp71/modules/mtk_imgsys-traw.o \
platforms/isp71/modules/mtk_imgsys-pqdip.o \
platforms/isp71/modules/mtk_imgsys-wpe.o \
platforms/isp71/modules/mtk_imgsys-me.o \
platforms/isp71/modules/mtk_imgsys-adl.o

mtk_imgsys_v4l2-objs :=\
mtk_imgsys_util.o \
mtk_imgsys_hw_isp71.o

obj-m += mtk_imgsys_v4l2.o
obj-n += mtk_imgsys-cmdq.o
obj-n += mtk_imgsys-trace.o
