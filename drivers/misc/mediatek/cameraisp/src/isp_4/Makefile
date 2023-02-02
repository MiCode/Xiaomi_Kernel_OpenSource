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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(srctree)/drivers/iommu/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cameraisp/src/common/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cam_timesync/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccu/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccu/src/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mdp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/

# camera_isp_4_t.o for mt6768 series; camera_isp_4_c.o for mt6765 series.
ifeq ($(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT), m)
    obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT) += camera_isp_4_t.o
    obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT) += camera_isp_4_c.o
    obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT) += cam_qos_4.o

else ifeq ($(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT), y)
    ifeq ($(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP4_T), y)
        obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP4_T) += camera_isp_4_t.o
    else ifeq ($(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP4_C), y)
        obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP4_C) += camera_isp_4_c.o
    endif

    obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT) += cam_qos_4.o
endif
