# SPDX-License-Identifier: GPL-2.0
obj-y					+= heap-helpers.o
obj-$(CONFIG_DMABUF_HEAPS_SYSTEM)	+= system_heap.o
obj-$(CONFIG_DMABUF_HEAPS_CMA)		+= cma_heap.o
obj-$(CONFIG_QCOM_DMABUF_HEAPS)		+= qcom_dma_heaps.o
qcom_dma_heaps-y			:= qcom_dma_heap.o qcom_dt_parser.o
