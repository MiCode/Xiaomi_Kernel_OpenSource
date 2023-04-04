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

#
# Makefile for misc devices that really don't fit anywhere else.
#

ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/mmdvfs/ \
             -I$(srctree)/drivers/misc/mediatek/include/ \
             -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/ \
             -I$(srctree)/drivers/misc/mediatek/cameraisp/src/common/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mdp/

ccflags-$(CONFIG_IOMMU_API) += -I$(srctree)/drivers/iommu


#ifeq ($(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT), m)
#    obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT) += camera_isp_3_m.o
#else ifeq ($(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT), y)
#    ifeq ($(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP3_M), y)
#        obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP3_M) += camera_isp_3_m.o
#    else ifeq ($(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP3_Z), y)
#        obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP3_Z) += camera_isp_3_z.o
#    endif
#endif
obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP3_M) += camera_isp_3_m.o
obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP3_Z) += camera_isp_3_z.o
obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP3_M) += cam_qos_3.o

$(info cameraisp drv by platform $(platform_drv))
