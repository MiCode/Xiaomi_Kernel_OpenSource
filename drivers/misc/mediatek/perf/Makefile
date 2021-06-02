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

LINUXINCLUDE += -include $(srctree)/kernel/sched/sched.h

ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/vcorefs_v3/
ccflags-y += -I$(srctree)/drivers/devfreq/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/qos/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/net/ipv4/
ccflags-$(CONFIG_MTK_CORE_CTL) += -I$(srctree)/drivers/misc/mediatek/base/power/include/ppm_v3/
ccflags-$(CONFIG_MTK_CORE_CTL) += -I$(srctree)/drivers/misc/mediatek/sched/
ccflags-$(CONFIG_MTK_CORE_CTL) += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

#ccflags-y += \
#	-I$(srctree)/drivers/misc/mediatek/base/power/cm_mgr_v1/ \
#	-I$(srctree)/drivers/misc/mediatek/base/power/cm_mgr_v1/$(CONFIG_MTK_PLATFORM) \
#	-I$(srctree)/drivers/misc/mediatek/base/power/cpufreq_v1/inc \

# a tracker for performance index
obj-$(CONFIG_MTK_PERF_TRACKER) += perf_tracker.o
obj-y += perf_common.o
# a low memory hint sent to userspace
obj-$(CONFIG_MTK_LOWMEM_HINT) += lowmem_hint.o
obj-$(CONFIG_MTK_CORE_CTL) += core_ctl.o
