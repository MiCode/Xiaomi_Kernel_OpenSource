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

APUSYS_VERSION_POWER = 2.0
APUSYS_VERSION_MNOC = 1.0
APUSYS_VERSION_MIDWARE = 1.0
APUSYS_VERSION_REVISER = 1.0

UTIL_DIR = ../util

ccflags-y += -I$(srctree)/drivers/staging/android/ion
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach
ccflags-y += -I$(srctree)/drivers/iommu/
ccflags-y += -I$(obj)/../include/
ccflags-y += -I$(obj)/../mnoc/$(APUSYS_VERSION_MNOC)/
ccflags-y += -I$(obj)/../reviser/$(APUSYS_VERSION_REVISER)/
ccflags-y += -I$(obj)/../power/$(APUSYS_VERSION_POWER)/
ccflags-y += -I$(obj)/../power/$(APUSYS_VERSION_POWER)/$(MTK_PLATFORM)/
ccflags-y += -I$(obj)/../debug/
ccflags-y += -I$(obj)/$(MTK_PLATFORM)/
ccflags-y += -I$(obj)/1.0/
ccflags-y += -I$(obj)/$(UTIL_DIR)/


obj-y += apusys.o
apusys-$(CONFIG_FTRACE) += $(UTIL_DIR)/apusys_trace.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/apusys_drv.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/apusys_user.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/cmd_parser.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/cmd_parser_mdla.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/resource_mgt.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/scheduler.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/apusys_dbg.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/memory_mgt.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/thread_pool.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/memory_ion.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/memory_dma.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/sched_deadline.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/sched_normal.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/mdw_tag.o
apusys-objs += $(MTK_PLATFORM)/dump_reg.o
apusys-objs += $(MTK_PLATFORM)/secure_perf.o
