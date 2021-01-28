# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_ION) +=	ion.o ion-ioctl.o ion_heap.o
obj-$(CONFIG_MTK_ION_SEC_HEAP) += mtk_ion_secure_heap.o
obj-$(CONFIG_ION_SYSTEM_HEAP) += ion_system_heap.o ion_page_pool.o
obj-$(CONFIG_ION_CARVEOUT_HEAP) += ion_carveout_heap.o
obj-$(CONFIG_ION_CHUNK_HEAP) += ion_chunk_heap.o
obj-$(CONFIG_ION_CMA_HEAP) += ion_cma_heap.o
obj-$(CONFIG_MTK_ION_DEBUG) += mtk_ion.o

ifeq ($(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem/ssmr
include $(srctree)/drivers/misc/mediatek/trusted_mem/Makefile.include
endif

ccflags-$(CONFIG_MTK_AEE_FEATURE) += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-$(CONFIG_MTK_IOMMU_MISC_DBG) += -I$(srctree)/drivers/misc/mediatek/iommu/