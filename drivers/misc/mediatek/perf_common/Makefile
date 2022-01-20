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
LINUXINCLUDE += -include $(srctree)/kernel/sched/sched.h
ccflags-y += -I$(src)              # needed for trace events
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/vcorefs_v3/
ccflags-y += -I$(srctree)/drivers/devfreq/

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6768" "mt6781" "mt6833" "mt6853" "mt6877" "mt6885" "mt6893"))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/qos/$(MTK_PLATFORM)/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/qos/
endif

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-$(CONFIG_MTK_CORE_CTL) += -I$(srctree)/drivers/misc/mediatek/base/power/include/ppm_v3/
ccflags-$(CONFIG_MTK_CORE_CTL) += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

obj-y += perf_common.o
# a tracker for performance index
obj-$(CONFIG_MTK_PERF_TRACKER) += perf_tracker.o
# RQ stats for TLP estimation
obj-$(CONFIG_MTK_CORE_CTL) += rq_stats.o
obj-$(CONFIG_MTK_CORE_CTL) += sched_avg.o
obj-$(CONFIG_MTK_CORE_CTL) += core_ctl.o
