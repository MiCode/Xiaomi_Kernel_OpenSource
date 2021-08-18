#
# Copyright (C) 2018 MediaTek Inc.
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

subdir-ccflags-y += -Werror
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/2.4
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp/imgsys/

ifeq ($(CONFIG_MTK_ION),y)
    ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion/
    ccflags-y  += -I$(srctree)/drivers/staging/android/mtk_ion/mtk/
else
    ccflags-$(CONFIG_ION) += -I$(srctree)/drivers/staging/android/aosp_ion/
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/ \
             -I$(srctree)/drivers/misc/mediatek/include/mt-plat/ \
             -I$(srctree)/drivers/misc/mediatek/include/ \
             -I$(srctree)/drivers/misc/mediatek/slbc/ \
             -I$(srctree)/drivers/dma-buf/heaps/ \
             -I$(srctree)/drivers/iommu
mtk_hcp-objs := \
mtk-hcp.o \
mtk-hcp_isp71.o \
mem/hcp_videobuf2-memops.o \
mem/hcp_videobuf2-vmalloc.o \
mem/hcp_videobuf2-dma-contig.o

obj-${CONFIG_VIDEO_MTK_ISP_HCP} += mtk_hcp.o
