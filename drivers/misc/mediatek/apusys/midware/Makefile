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

APUSYS_VERSION = 1.0
MNOC_VERSION = 1.0

UTIL_DIR = ../util

ccflags-y += -I$(srctree)/drivers/staging/android/ion
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach
ccflags-y += -I$(srctree)/drivers/iommu/
ccflags-y += -I$(obj)/../include/
ccflags-y += -I$(obj)/../mnoc/$(MNOC_VERSION)/
ccflags-y += -I$(obj)/$(UTIL_DIR)/

obj-m += apusys.o
apusys-$(CONFIG_FTRACE) += $(UTIL_DIR)/apusys_trace.o
apusys-objs += $(APUSYS_VERSION)/apusys_drv.o
apusys-objs += $(APUSYS_VERSION)/apusys_user.o
apusys-objs += $(APUSYS_VERSION)/cmd_parser.o
apusys-objs += $(APUSYS_VERSION)/cmd_parser_mdla.o
apusys-objs += $(APUSYS_VERSION)/resource_mgt.o
apusys-objs += $(APUSYS_VERSION)/scheduler.o
apusys-objs += $(APUSYS_VERSION)/apusys_dbg.o
apusys-objs += $(APUSYS_VERSION)/memory_mgt.o
apusys-objs += $(APUSYS_VERSION)/midware_trace.o
apusys-objs += $(APUSYS_VERSION)/thread_pool.o
apusys-objs += $(APUSYS_VERSION)/memory_ion.o
apusys-objs += $(APUSYS_VERSION)/memory_dma.o
