
# Copyright (C) 2016 MediaTek Inc.
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

ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq_v1/include/$(word 4,$(subst ",,$(CONFIG_MTK_GPU_VERSION)))/
ccflags-y += -I$(srctree)/include/linux/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eemgpu/common/inc/

obj-${CONFIG_MTK_PTPOD_GPU} += mtk_eem.o
mtk_eem-y += \
	mtk_eemgpu.o \
	mtk_eemgpu_platform.o \
	mtk_eemgpu_internal.o
