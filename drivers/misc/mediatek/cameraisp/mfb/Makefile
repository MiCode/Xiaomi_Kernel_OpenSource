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
ccflags-y += -I$(srctree)/drivers/staging/android/ion \
             -I$(srctree)/drivers/staging/android/ion/mtk
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/

obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += camera_mfb.o
camera_mfb-objs += isp_6s/engine_request.o
camera_mfb-objs += isp_6s/camera_mfb.o

MTK_MFB_REG_VERSION := 2
subdir-ccflags-y += -DMTK_MFB_REG_VERSION=$(MTK_MFB_REG_VERSION)
