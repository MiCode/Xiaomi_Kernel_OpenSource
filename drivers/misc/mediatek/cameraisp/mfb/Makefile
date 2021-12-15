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

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))

$(info $(srctree))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/staging/android/ion \
             -I$(srctree)/drivers/staging/android/ion/mtk
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs/
ccflags-$(CONFIG_MTK_IOMMU_V2) += -I$(srctree)/drivers/iommu
ccflags-$(CONFIG_MTK_IOMMU_V2) += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-$(CONFIG_MTK_CMDQ_MBOX_EXT) += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox/

obj-y := camera_mfb_dummy.o

ifeq ($(CONFIG_MACH_MT6771),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += isp_50/camera_mfb.o
endif

ifeq ($(CONFIG_MACH_MT6775),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += isp_50/camera_mfb.o
endif

ifeq ($(CONFIG_MACH_MT6785),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += isp_50/camera_mfb.o
endif

ifeq ($(CONFIG_MACH_MT6779),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += mt6779/camera_mfb.o
endif

ifeq ($(CONFIG_MACH_MT6885),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += isp_6s/engine_request.o
  obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += isp_6s/camera_mfb.o
  MTK_MFB_REG_VERSION := 1
endif

ifeq ($(CONFIG_MACH_MT6873),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += isp_6s/engine_request.o
  obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += isp_6s/camera_mfb.o
  MTK_MFB_REG_VERSION := 2
endif

ifeq ($(CONFIG_MACH_MT6853),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += isp_6s/engine_request.o
  obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += isp_6s/camera_mfb.o
  MTK_MFB_REG_VERSION := 3
endif

ifeq ($(CONFIG_MACH_MT6893),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += isp_6s/engine_request.o
  obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += isp_6s/camera_mfb.o
  MTK_MFB_REG_VERSION := 1
endif

ifeq ($(CONFIG_MACH_MT6833),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += isp_6s/engine_request.o
  obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += isp_6s/camera_mfb.o
  MTK_MFB_REG_VERSION := 3
endif

ifeq ($(CONFIG_MACH_MT6781),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += isp_6s/engine_request.o
  obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += isp_6s/camera_mfb.o
  MTK_MFB_REG_VERSION := 3
endif

subdir-ccflags-y += -DMTK_MFB_REG_VERSION=$(MTK_MFB_REG_VERSION)
