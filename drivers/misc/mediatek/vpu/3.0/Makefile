# SPDX-License-Identifier: GPL-2.0 */
#
# Copyright (c) 2019 MediaTek Inc.
#

ccflags-y += -I$(srctree)/drivers/staging/android/ion
ifeq ($(CONFIG_MTK_IOMMU_V2), y)
ccflags-y += -I$(srctree)/drivers/iommu
endif
ifeq ($(CONFIG_MTK_M4U), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

ccflags-y += -I$(srctree)/drivers/misc/mediatek/vpu/$(MTK_PLATFORM)/

obj-$(CONFIG_MTK_VPU_SUPPORT) += vpu_drv.o vpu_algo.o vpu_dbg.o vpu_pool.o
