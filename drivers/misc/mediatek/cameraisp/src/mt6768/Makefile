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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion \
             -I$(srctree)/drivers/staging/android/mtk_ion/mtk
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/timer/timesync/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccu/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccu/src/

ifeq ($(CONFIG_MACH_MT6761),y)
    ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/mt6768/
else
    ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/
endif

obj-y += camera_isp.o
#obj-$(CONFIG_MTK_CAMERA_ISP_TSF_SUPPORT) += camera_tsf.o
#obj-$(CONFIG_MTK_CAMERA_ISP_DPE_SUPPORT) += camera_dpe.o
#obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += camera_rsc.o
#obj-$(CONFIG_MTK_CAMERA_ISP_GEPF_SUPPORT) += camera_gepf.o
#obj-$(CONFIG_MTK_CAMERA_ISP_FD_SUPPORT) += camera_fdvt.o
#obj-$(CONFIG_MTK_CAMERA_ISP_WPE_SUPPORT) += camera_wpe.o
#obj-$(CONFIG_MTK_CAMERA_ISP_EAF_SUPPORT) += camera_eaf.o

