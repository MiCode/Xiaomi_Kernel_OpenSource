# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox/
ccflags-y += -I$(srctree)/drivers/dma-buf/heaps/
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-videobuf/
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp/imgsys/
ccflags-y += -I$(srctree)/drivers/iommu
ccflags-y += -I$(srctree)/drivers/misc/mediatek/iommu

ifneq ($(wildcard $(srctree)/include/dt-bindings/memory/mt6983-larb-port.h),)
	subdir-ccflags-y += -DFDVT_TF_DUMP
endif

mtk_aie-objs := \
mem/aie_videobuf2-dma-contig.o\
mem/aie_videobuf2-memops.o\
mem/aie_videobuf2-vmalloc.o\
mtk_aie_53.o\
mtk_aie_drv_isp71.o \
mtk_aie_drv_isp7s.o

obj-$(CONFIG_MTK_CAMERA_FD_ISP7S) += mtk_aie.o
