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

ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/include
ccflags-y += -I$(srctree)/drivers/dma-buf/heaps
ccflags-y += -I$(srctree)/drivers/misc/mediatek/slbc
ccflags-y += -I$(srctree)/include/linux/soc/mediatek

mtk_aov-objs := \
	mtk-aov-drv.o	\
	mtk-aov-core.o \
	mtk-aov-trace.o \
	mtk-aov-aee.o \
	mtk-aov-queue.o \
	mtk-aie-model.o \
	alloc/tlsf/tlsf_alloc.o

obj-${CONFIG_VIDEO_MTK_ISP_AOV} += mtk_aov.o
