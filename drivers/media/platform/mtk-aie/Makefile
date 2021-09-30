# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox/
ccflags-y += -I$(srctree)/drivers/dma-buf/heaps/
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp/imgsys/

mtk_aie-objs := \
mtk_aie_53.o\
mtk_aie_drv.o

obj-$(CONFIG_MTK_CAMERA_FD_ISP7) += mtk_aie.o
