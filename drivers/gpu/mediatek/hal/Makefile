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

ccflags-y += \
	-I$(srctree)/drivers/gpu/mediatek/ged/include \
	-I$(srctree)/drivers/gpu/mediatek/ \

obj-$(CONFIG_MTK_GPU_SUPPORT) += mtk_gpu_hal.o
mtk_gpu_hal-y := mtk_gpu_utility.o
