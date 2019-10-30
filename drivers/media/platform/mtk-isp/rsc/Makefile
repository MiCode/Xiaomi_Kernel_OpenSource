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

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))

$(info "RSC: makefile start srctree: $(srctree)")
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp/rsc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/variant/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp/
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp/mtk-hcp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(CONFIG_MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/staging/android/ion \
             -I$(srctree)/drivers/staging/android/ion/mtk

ccflags-$(CONFIG_MTK_IOMMU_V2) += -I$(srctree)/drivers/iommu
ccflags-$(CONFIG_MTK_IOMMU_V2) += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-$(CONFIG_MTK_IOMMU_V2) += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/

obj-y += mtk_rsc.o
obj-y += mtk_rsc-v4l2.o
#obj-y += engine_request.o
# To provide alloc context managing memory shared
# between CPU and ISP coprocessor
#mtk_rsc_smem-objs := \
#mtk_rsc-smem-drv.o
#obj-y += mtk_rsc_smem.o

# Utilits to provide frame-based streaming model
# with v4l2 user interfaces
mtk_rsc_util-objs := \
mtk_rsc-dev.o \
mtk_rsc-v4l2-util.o \
mtk_rsc-dev-ctx-core.o

obj-y += mtk_rsc_util.o

$(info "RSC: makefile end")
