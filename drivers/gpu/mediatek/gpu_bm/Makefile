#
# Copyright (C) 2018 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/qos/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/

ifeq ($(CONFIG_MTK_GPU_SUPPORT),y)
ccflags-$(CONFIG_MTK_QOS_FRAMEWORK) += -DMTK_QOS_FRAMEWORK
$(info *GPU BM QoS 2.0 QOS Framework defined)
endif

ifeq ($(CONFIG_MTK_QOS_MT6893),y)
ccflags-y += -DMTK_SCMI
$(info *GPU BM QoS 2.0 SCMI defined)
endif


obj-$(CONFIG_MTK_GPU_SUPPORT) += mtk_gpu_qos.o
mtk_gpu_qos-y := gpu_bm.o
