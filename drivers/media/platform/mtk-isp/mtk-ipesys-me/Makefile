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

ifneq ($(wildcard $(srctree)/include/dt-bindings/memory/mt6983-larb-port.h),)
	subdir-ccflags-y += -DTF_DUMP_71_1
else ifneq ($(wildcard $(srctree)/include/dt-bindings/memory/mt6879-larb-port.h),)
	subdir-ccflags-y += -DTF_DUMP_71_2
endif
$(info "imgsys, subdir-ccflags-y = $(subdir-ccflags-y)")

mtk-ipesys-me-objs :=\
mtk-ipesys-me.o

obj-${CONFIG_VIDEO_MTK_ISP_IPESYS} += mtk-ipesys-me.o

