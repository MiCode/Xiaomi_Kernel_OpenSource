#
# Copyright (C) 2019 MediaTek Inc.
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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/iommu
ccflags-y += -I$(srctree)/drivers/dma-buf/heaps
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem/

obj-$(CONFIG_MTK_IOMMU_MISC_DBG)    += iommu_debug.o
obj-$(CONFIG_MTK_IOMMU_MISC_SECURE) += iommu_secure.o
obj-$(CONFIG_MTK_IOMMU_MISC_TEST)   += iommu_test.o
obj-$(CONFIG_MTK_IOMMU_MISC)        += iommu_gz.o

ifneq ($(filter y m, $(CONFIG_MTK_ENABLE_GENIEZONE)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public

iommu_gz-objs := iommu_pseudo.o iommu_gz_sec.o
else
iommu_gz-objs := iommu_pseudo.o
endif
