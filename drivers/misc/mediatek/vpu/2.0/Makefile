# SPDX-License-Identifier: GPL-2.0 */
#
# Copyright (c) 2019 MediaTek Inc.
#


ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

ccflags-$(CONFIG_MTK_M4U) += -I$(srctree)/drivers/misc/mediatek/m4u/$(CONFIG_MTK_PLATFORM)/
ccflags-$(CONFIG_IOMMU_API) += -I$(srctree)/drivers/iommu
ccflags-$(CONFIG_IOMMU_API) += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include
obj-$(CONFIG_MTK_VPU_SUPPORT) += vpu_drv.o vpu_algo.o vpu_dbg.o
