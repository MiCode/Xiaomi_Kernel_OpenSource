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

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
NUXINCLUDE += -include $(srctree)/kernel/sched/sched.h
ccflags-y += -I$(src)              # needed for trace events
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/qos/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

obj-y += perf_common.o
# a tracker for performance index
obj-$(CONFIG_MTK_PERF_TRACKER) += perf_tracker.o
