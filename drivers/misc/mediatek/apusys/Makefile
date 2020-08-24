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
APUSYS_VERSION_DEBUG = 1.0
APUSYS_VERSION_MNOC = 1.5
APUSYS_VERSION_VPU = 4.5
APUSYS_VERSION_POWER_PLATFORM = mt6873
APUSYS_VERSION_REVISER = 1.5

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
apusys-objs += $(UTIL_DIR)/apu_bmap.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_drv.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_usr.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_dbg.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_rsc.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_cmn.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_cmd.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_sched.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_queue.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_queue_norm.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_queue_deadline.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_dispr.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_sysfs.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_mem.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_mem_dmy.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_mem_aosp.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_import.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_fence.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_tag.o
ifeq ($(CONFIG_MTK_ION), y)
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_mem_ion.o
endif

# sample driver makefile
apusys-objs += sample/$(APUSYS_VERSION_SAMPLE)/sample_drv.o
apusys-objs += sample/$(APUSYS_VERSION_SAMPLE)/sample_inf.o

# edma driver makefile
apusys-objs += edma/edma_driver.o
apusys-objs += edma/edma_dbgfs.o
apusys-objs += edma/edma_cmd_hnd.o
apusys-objs += edma/edma_plat_device.o
apusys-objs += edma/edma_v20.o
#apusys-objs += edma/apusys_power_dummy.o


# reviser driver makefile
REVISER=$(srctree)/drivers/misc/mediatek/apusys/
REVISER_PLAT=reviser/$(APUSYS_VERSION_REVISER)/platform
REVISER_COMN=reviser/$(APUSYS_VERSION_REVISER)/common

ccflags-y += -I$(REVISER)/$(REVISER_PLAT)
ccflags-y += -I$(REVISER)/$(REVISER_COMN)
ccflags-y += -I$(REVISER)/$(REVISER_PLAT)/v_1_0

apusys-objs += $(REVISER_COMN)/reviser_drv.o
apusys-objs += $(REVISER_COMN)/reviser_dbg.o
apusys-objs += $(REVISER_COMN)/reviser_export.o
apusys-objs += $(REVISER_COMN)/reviser_hw_mgt.o
apusys-objs += $(REVISER_COMN)/reviser_mem.o
apusys-objs += $(REVISER_COMN)/reviser_power.o
apusys-objs += $(REVISER_COMN)/reviser_table_mgt.o

apusys-objs += $(REVISER_PLAT)/reviser_device.o
apusys-objs += $(REVISER_PLAT)/reviser_plat.o

apusys-objs += $(REVISER_PLAT)/v_1_0/reviser_hw_v1_0.o
apusys-objs += $(REVISER_PLAT)/v_1_0/reviser_plat_v1_0.o
apusys-objs += $(REVISER_PLAT)/v_1_0/reviser_reg_v1_0.o



# mnoc makefile
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/mnoc/$(APUSYS_VERSION_MNOC)/common/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/mnoc/$(APUSYS_VERSION_MNOC)/platform/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/mnoc/$(APUSYS_VERSION_MNOC)/utilities/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/mnoc/$(APUSYS_VERSION_MNOC)/interface/

apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/common/mnoc_dbg.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/common/mnoc_drv.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/common/mnoc_pmu.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/common/mnoc_tag.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/platform/mnoc_plat_device.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/platform/v1_50/mnoc_hw_v1_50.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/platform/v1_51/mnoc_hw_v1_51.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/platform/v1_52/mnoc_hw_v1_52.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/utilities/mnoc_util.o
ifneq (, $(CONFIG_MTK_QOS_FRAMEWORK))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/qos/
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/common/mnoc_qos.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/common/mnoc_qos_sys.o
endif

# debug makefile
ifeq ($(CONFIG_MTK_APUSYS_DEBUG),y)
apusys-objs += debug/apu_tags.o
apusys-objs += debug/apu_tp.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/debug/$(APUSYS_VERSION_DEBUG)/common/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/debug/$(APUSYS_VERSION_DEBUG)/interface/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/debug/$(APUSYS_VERSION_DEBUG)/platform/

apusys-objs += debug/$(APUSYS_VERSION_DEBUG)/common/debug_drv.o
apusys-objs += debug/$(APUSYS_VERSION_DEBUG)/platform/debug_plat_device.o
apusys-objs += debug/$(APUSYS_VERSION_DEBUG)/platform/mt6853/debug_mt6853.o
apusys-objs += debug/$(APUSYS_VERSION_DEBUG)/platform/mt6873/debug_mt6873.o
apusys-objs += debug/$(APUSYS_VERSION_DEBUG)/platform/mt6885/debug_mt6885.o
endif

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

# vpu
VPU = vpu/$(APUSYS_VERSION_VPU)
ccflags-y += -I$(src)/$(VPU)
ifeq ($(CONFIG_MTK_APUSYS_VPU_DEBUG),y)
apusys-objs += $(VPU)/vpu_debug.o
apusys-objs += $(VPU)/vpu_tag.o
apusys-objs += $(VPU)/vpu_dump.o
apusys-objs += $(VPU)/vpu_met.o
endif
ifeq ($(CONFIG_MTK_APUSYS_VPU),y)
apusys-objs += $(VPU)/vpu_main.o
apusys-objs += $(VPU)/vpu_algo.o
apusys-objs += $(VPU)/vpu_cmd.o
apusys-objs += $(VPU)/vpu_hw.o
apusys-objs += $(VPU)/vpu_mem.o
apusys-objs += $(VPU)/vpu_power.o
apusys-objs += $(VPU)/vpu_cfg.o
endif

# devapc
apusys-objs += devapc/dapc.o
apusys-objs += devapc/dapc_cfg.o
