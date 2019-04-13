#
# Copyright (C) 2018 MediaTek Inc.
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
	-I$(srctree)/kernel/trace/ \

ccflags-$(CONFIG_MTK_FPSGO) += \
	-I$(srctree)/drivers/misc/mediatek/performance/fpsgo/fstb/ \

ccflags-$(CONFIG_MTK_FPSGO_V3) += \
	-I$(srctree)/drivers/misc/mediatek/performance/fpsgo_v3/fstb/ \

obj-y += utch.o
