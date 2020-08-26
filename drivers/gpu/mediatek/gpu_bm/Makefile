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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/qos/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq_v1/include/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/

ifneq ($(CONFIG_MTK_GPU_SUPPORT),)
ccflags-y += -DMTK_QOS_FRAMEWORK
$(info *GPU BM QoS 2.0 QOS Framework defined)
endif

obj-$(CONFIG_MTK_GPU_SUPPORT) += mtk_gpu_qos.o
mtk_gpu_qos-y := gpu_bm.o
