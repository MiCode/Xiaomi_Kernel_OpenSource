# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_DMABUF_HEAPS_DEFERRED_FREE) += deferred-free-helper.o
obj-$(CONFIG_DMABUF_HEAPS_PAGE_POOL)	+= page_pool.o
obj-$(CONFIG_DMABUF_HEAPS_SYSTEM)	+= system_heap.o
obj-$(CONFIG_DMABUF_HEAPS_CMA)		+= cma_heap.o
obj-$(CONFIG_DMABUF_HEAPS_MTK_MM)	+= mtk_mm_heap.o

ccflags-y += -I$(srctree)/drivers/iommu/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/iommu/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem/
obj-$(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM)	+= mtk_sec_heap.o
obj-$(CONFIG_DMABUF_HEAPS_DEBUG) += mtk_heap_debug.o
