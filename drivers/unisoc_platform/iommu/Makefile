#SPDX-FileCopyrightText: 2019 Unisoc (Shanghai) Technologies Co.Ltd
#SPDX-License-Identifier: GPL-2.0-only

ccflags-y := -I $(srctree)/$(src)

unisoc-iommu-y := \
	sprd_iommu.o \
	sprd_iommu_sysfs.o

unisoc-iommu-y += \
	drv/sprd_iommuex_ops.o \
	drv/sprd_iommuvau_ops.o

unisoc-iommu-y += drv/api/sprd_iommu_api.o

unisoc-iommu-y += \
	drv/cll/sprd_iommuex_cll.o \
	drv/cll/sprd_iommuvau_cll.o

unisoc-iommu-y += drv/com/sprd_com.o

unisoc-iommu-y += \
	drv/hal/sprd_iommuex_hal_register.o \
	drv/hal/sprd_iommuvau_hal_register.o

unisoc-iommu-y += drv/osal/sprd_osal.o

obj-$(CONFIG_UNISOC_IOMMU) += unisoc-iommu.o
