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

# assign value
UTIL_DIR = util

# declare apusys version
APUSYS_VERSION_MIDWARE = 1.5
APUSYS_VERSION_SAMPLE = 1.5
APUSYS_VERSION_MNOC = 1.5

# target object
obj-$(CONFIG_MTK_APUSYS_SUPPORT) += apusys.o

# common include path
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/include/

# core makefile
apusys-objs += core/apusys_core.o

# middleware makefile
ccflags-y += -I$(srctree)/drivers/staging/android/ion
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/debug/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/midware/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/midware/$(APUSYS_VERSION_MIDWARE)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/$(UTIL_DIR)/

apusys-objs += $(UTIL_DIR)/apusys_trace.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/apusys_drv.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/apusys_user.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/cmd_parser.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/cmd_parser_mdla.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/resource_mgt.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/scheduler.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/apusys_dbg.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/memory_mgt.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/thread_pool.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_mem_dummy.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_tag.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/sched_deadline.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/sched_normal.o

ifeq ($(CONFIG_MTK_ION), y)
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_mem_ion.o
endif

ifeq ($(CONFIG_MTK_APUSYS_DEBUG),y)
apusys-objs += debug/apu_tags.o
apusys-objs += debug/apu_tp.o
endif

# sample makefile
apusys-objs += sample/$(APUSYS_VERSION_SAMPLE)/sample_drv.o
apusys-objs += sample/$(APUSYS_VERSION_SAMPLE)/sample_inf.o

# edma driver makefile
apusys-objs += edma/edma_driver.o
apusys-objs += edma/edma_dbgfs.o
apusys-objs += edma/edma_cmd_hnd.o
apusys-objs += edma/apusys_power_dummy.o
# mnoc makefile
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/mnoc/$(APUSYS_VERSION_MNOC)/

apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/mnoc_dbg.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/mnoc_drv.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/mnoc_hw.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/mnoc_pmu.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/mnoc_qos.o