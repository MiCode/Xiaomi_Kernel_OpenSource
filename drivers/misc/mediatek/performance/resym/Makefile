#
# Copyright (C) 2019 MediaTek Inc.
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

obj-y += rs_trace.o
obj-y += rs_base.o
obj-y += rs_main.o

#ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/performance/observer
ccflags-y += -I$(srctree)/kernel/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/perf_common/

ccflags-y += -DNR_FREQ_CPU=16

obj-y += rs_usage.o
obj-y += rs_state.o
obj-y += rs_index.o

obj-y += platform/$(MTK_PLATFORM)/
