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

ifeq ($(CONFIG_MACH_MT6833),y)
MTK_PLATFORM := mt6833
else
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif

# mt6761 mt6762 mt6768 without RSC
$(info $(srctree))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/staging/android/ion \
             -I$(srctree)/drivers/staging/android/ion/mtk
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-y += -I$(srctree)/drivers/iommu

obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += camera_rsc.o
camera_rsc-objs += engine_request.o

ifeq ($(CONFIG_MACH_MT6758),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += $(MTK_PLATFORM)/camera_rsc.o
endif

ifeq ($(CONFIG_MACH_MT6771),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += isp_50/camera_rsc.o
endif

ifeq ($(CONFIG_MACH_MT6779),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += mt6779/camera_rsc.o
endif

ifeq ($(CONFIG_MACH_MT6785),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += mt6785/camera_rsc.o
endif

ifeq ($(CONFIG_MACH_MT6885),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += isp_6s/camera_rsc.o
endif

ifeq ($(CONFIG_MACH_MT6873),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += isp_6s/camera_rsc.o
endif

ifeq ($(CONFIG_MACH_MT6853),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += isp_6s/camera_rsc.o
endif

ifeq ($(CONFIG_MACH_MT6893),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += isp_6s/camera_rsc.o
endif

ifeq ($(CONFIG_MACH_MT6833),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += isp_6s/camera_rsc.o
endif

ifeq ($(CONFIG_MACH_MT6877),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += isp_6s/camera_rsc.o
endif

ifeq ($(CONFIG_MACH_MT6781),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += isp_6s/camera_rsc.o
endif

