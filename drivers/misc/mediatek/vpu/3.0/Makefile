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
ifeq ($(CONFIG_MTK_IOMMU_V2), y)
ccflags-y += -I$(srctree)/drivers/iommu
endif
ifeq ($(CONFIG_MTK_M4U), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

ccflags-y += -I$(srctree)/drivers/misc/mediatek/vpu/$(MTK_PLATFORM)/

obj-$(CONFIG_MTK_VPU_SUPPORT) += vpu_drv.o vpu_algo.o vpu_dbg.o
