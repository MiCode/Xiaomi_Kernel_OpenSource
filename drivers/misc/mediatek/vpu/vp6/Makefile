#
# Copyright (C) 2015 MediaTek Inc.
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

ccflags-y += -I$(srctree)/drivers/staging/android/ion
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vpu/vp6/$(MTK_PLATFORM)/
ccflags-$(CONFIG_MTK_PSEUDO_M4U) += -I$(srctree)/drivers/misc/mediatek/pseudo_m4u/

ifeq ($(CONFIG_MTK_QOS_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/devfreq/
endif

obj-$(CONFIG_MTK_VPU_SUPPORT) += vpu_drv.o vpu_algo.o vpu_dbg.o vpu_profile.o vpu_cmn.o
obj-$(CONFIG_MTK_VPU_SUPPORT) += vpubuf-dma.o vpubuf-core.o vpubuf-dma-contig.o vpubuf-memops.o
obj-$(CONFIG_MTK_VPU_SUPPORT) += vpu_load_image.o vpu_utilization.o
