#
# Copyright (C) 2020 MediaTek Inc.
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

obj-$(CONFIG_MTPROF) = bootprof.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/include/trace/events
ccflags-y += -I$(srctree)/include/kernel

obj-$(CONFIG_MTK_IRQ_MONITOR) += irq_monitor.o
irq_monitor-y += irq_monitor_main.o irq_monitor_test.o
irq_monitor-y += irq_count_tracer.o common.o
