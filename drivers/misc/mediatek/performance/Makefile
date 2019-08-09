# Copyright (C) 2017 MediaTek Inc.
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

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/performance/perf_ioctl/ \
		    -I$(srctree)/drivers/misc/mediatek/performance/include/ \
		    -I$(srctree)/drivers/misc/mediatek/base/power/include/ \


obj-y += perfmgr_main.o
obj-y += base/
obj-y += cpu_loading/
obj-$(CONFIG_MTK_PERF_OBSERVER) += observer/

obj-$(CONFIG_MTK_FPSGO_V3) += resym/

ifeq (,$(findstring mt8,$(CONFIG_MTK_PLATFORM)))
	obj-$(CONFIG_MTK_BASE_POWER) += boost_ctrl/
	obj-$(CONFIG_MTK_BASE_POWER) += tchbst/
endif

obj-y += perf_ioctl/
obj-$(CONFIG_MTK_FPSGO) += fpsgo/
obj-$(CONFIG_MTK_FPSGO_V3) += fpsgo_v3/
obj-$(CONFIG_MTK_FPSGO_V3) += eara_thermal/

obj-$(CONFIG_MTK_LOAD_TRACKER) += load_track/
