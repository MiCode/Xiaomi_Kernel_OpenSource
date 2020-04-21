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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/performance/observer

ccflags-y += -DNR_FREQ_CPU=16

obj-y += rs_usage.o
obj-y += rs_state.o

obj-$(CONFIG_MACH_MT6768) += platform/mt6768/
obj-$(CONFIG_MACH_MT6785) += platform/mt6785/
obj-$(CONFIG_MACH_MT6885) += platform/mt6885/
obj-$(CONFIG_MACH_MT6873) += platform/mt6873/
obj-$(CONFIG_MACH_MT6853) += platform/mt6853/
