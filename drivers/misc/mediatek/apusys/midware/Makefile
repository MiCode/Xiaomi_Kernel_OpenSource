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
APUSYS_VERSION_MIDWARE = 1.1
APUSYS_VERSION_REVISER = 1.0

# mt6893 and mt6885 co-platform folder
ifeq ($(CONFIG_MACH_MT6893),y)
PDIR = mt6885
else ifeq ($(CONFIG_MACH_MT6877),y)
PDIR = mt6877
else
PDIR = $(MTK_PLATFORM)
endif

UTIL_DIR = ../util

ccflags-y += -I$(srctree)/drivers/staging/android/ion
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(PDIR)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(PDIR)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(PDIR)/include/mach
ccflags-y += -I$(srctree)/drivers/iommu/
ccflags-y += -I$(obj)/../include/
ccflags-y += -I$(obj)/../mnoc/$(APUSYS_VERSION_MNOC)/
ccflags-y += -I$(obj)/../reviser/$(APUSYS_VERSION_REVISER)/
ccflags-y += -I$(obj)/../power/$(APUSYS_VERSION_POWER)/
ccflags-y += -I$(obj)/../power/$(APUSYS_VERSION_POWER)/$(PDIR)/
ccflags-y += -I$(obj)/../debug/
ccflags-y += -I$(obj)/../debug/$(PDIR)/
ccflags-y += -I$(obj)/$(PDIR)/
ccflags-y += -I$(obj)/$(APUSYS_VERSION_MIDWARE)/
ccflags-y += -I$(obj)/$(UTIL_DIR)/

obj-y += apusys.o
apusys-$(CONFIG_FTRACE) += $(UTIL_DIR)/apusys_trace.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/mdw_drv.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/mdw_usr.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/mdw_dbg.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/mdw_rsc.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/mdw_cmn.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/mdw_cmd.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/mdw_sched.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/mdw_queue.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/mdw_queue_norm.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/mdw_queue_deadline.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/mdw_dispr.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/mdw_mem.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/mdw_mem_ion.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/mdw_fence.o
apusys-objs += $(APUSYS_VERSION_MIDWARE)/mdw_sysfs.o
apusys-$(CONFIG_MTK_APUSYS_DEBUG) += $(APUSYS_VERSION_MIDWARE)/mdw_tag.o

