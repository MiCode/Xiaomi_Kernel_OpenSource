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
ccflags-y += -I$(srctree)/drivers/staging/android/ion/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cam_timesync/
ccflags-y += -I$(srctree)/include/dt-bindings/memory/
ccflags-y += -I$(srctree)/drivers/iommu/
ccflags-y += -I$(srctree)/drivers/clk/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cameraisp/src/common/

obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT) += camera_isp.o
obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT) += cam_qos.o
