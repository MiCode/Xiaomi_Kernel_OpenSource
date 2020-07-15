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
APUSYS_VERSION_POWER_PLATFORM = mt6873

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

apusys-$(CONFIG_FTRACE) += $(UTIL_DIR)/apusys_trace.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_drv.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_usr.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_dbg.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_rsc.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_cmn.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_cmd.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_sched.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_queue_norm.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_queue_deadline.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_pack.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_mem.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_mem_dmy.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_mem_aosp.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_import.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_fence.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_tag.o
ifeq ($(CONFIG_MTK_ION), y)
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_mem_ion.o
endif

# debug makefile
ifeq ($(CONFIG_MTK_APUSYS_DEBUG),y)
apusys-objs += debug/apu_tags.o
apusys-objs += debug/apu_tp.o
endif

# sample driver makefile
apusys-objs += sample/$(APUSYS_VERSION_SAMPLE)/sample_drv.o
apusys-objs += sample/$(APUSYS_VERSION_SAMPLE)/sample_inf.o

# edma driver makefile
apusys-objs += edma/edma_driver.o
apusys-objs += edma/edma_dbgfs.o
apusys-objs += edma/edma_cmd_hnd.o
#apusys-objs += edma/apusys_power_dummy.o
# mnoc makefile
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/mnoc/$(APUSYS_VERSION_MNOC)/

apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/mnoc_dbg.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/mnoc_drv.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/mnoc_hw.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/mnoc_pmu.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/mnoc_qos.o

# power driver makefile
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/debug/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/$(APUSYS_VERSION_POWER_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/midware/$(APUSYS_VERSION_MIDWARE)/
ifeq ($(APUSYS_VERSION_POWER_PLATFORM),mt6873)
subdir-ccflags-y += -DCONFIG_MACH_MT6873
endif

apusys-objs += power/2.0/apusys_power_driver.o
apusys-objs += power/2.0/apusys_power_ctl.o
apusys-objs += power/2.0/apusys_power_debug.o
apusys-objs += power/2.0/$(APUSYS_VERSION_POWER_PLATFORM)/hal_config_power.o
apusys-objs += power/2.0/$(APUSYS_VERSION_POWER_PLATFORM)/apu_power_regulator.o
apusys-objs += power/2.0/$(APUSYS_VERSION_POWER_PLATFORM)/apu_power_clock.o
apusys-objs += power/2.0/$(APUSYS_VERSION_POWER_PLATFORM)/apusys_power_cust.o
apusys-objs += power/2.0/$(APUSYS_VERSION_POWER_PLATFORM)/apu_power_table.o
apusys-objs += power/2.0/$(APUSYS_VERSION_POWER_PLATFORM)/apu_platform_resource.o
apusys-objs += power/2.0/$(APUSYS_VERSION_POWER_PLATFORM)/apu_platform_debug.o
apusys-objs += power/2.0/$(APUSYS_VERSION_POWER_PLATFORM)/apusys_power_rule_check.o
apusys-objs += power/2.0/$(APUSYS_VERSION_POWER_PLATFORM)/apu_power_tag.o
#apusys-objs += power/2.0/$(APUSYS_VERSION_POWER_PLATFORM)/dummy_vpu_driver.o
#apusys-objs += power/2.0/$(APUSYS_VERSION_POWER_PLATFORM)/dummy_mdla_driver.o

# apu_top target
obj-$(CONFIG_MTK_APUSYS_SUPPORT) += apu_top/

# mdla
src=$(srctree)/drivers/misc/mediatek/apusys
ccflags-y += -I$(src)/mdla/
apusys-objs += $(shell cd $(src) && find mdla -name "*.c" | sed 's/\.c/\.o/g')

