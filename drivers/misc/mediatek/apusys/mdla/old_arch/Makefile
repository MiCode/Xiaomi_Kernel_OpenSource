#
# Copyright (C) 2018 MediaTek Inc.
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



PDIR = $(MTK_PLATFORM)
VDIR = 2.1
PMU_DIR = ../../pmu
PMU_VDIR = 1.0
PMU_PDIR = $(MTK_PLATFORM)
UTIL_DIR = ../../util
POWER_DIR = ../../power
POWER_VDIR = 2.0
POWER_PDIR = $(MTK_PLATFORM)
MIDWARE_DIR = ../../midware
MIDWARE_VDIR = 1.1
M4U_PDIR = $(MTK_PLATFORM)

# mt6893 and mt6885 co-platform folder
ifeq ($(CONFIG_MACH_MT6893),y)
PDIR = mt6885
PMU_PDIR = mt6885
POWER_PDIR = mt6885
M4U_PDIR = mt6885
endif

ccflags-y += -I$(obj)/$(VDIR)
ccflags-y += -I$(obj)/$(PDIR)
ccflags-y += -I$(obj)/$(UTIL_DIR)

#APUSYS PMU
ccflags-y += -I$(obj)/$(PMU_DIR)/$(PMU_PDIR)
ccflags-y += -I$(obj)/$(PMU_DIR)/$(PMU_VDIR)
ccflags-y += -I$(obj)/$(MEM_DIR)/$(MEM_VDIR)

#Power
ccflags-y += -I$(obj)/$(POWER_DIR)/$(POWER_VDIR)
ccflags-y += -I$(obj)/$(POWER_DIR)/$(POWER_VDIR)/$(POWER_PDIR)

ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(M4U_PDIR)
ccflags-y += -I$(srctree)/arch/arm64/include/asm/

#APUSYS
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/include
ccflags-y += -I$(obj)/$(MIDWARE_DIR)/$(MIDWARE_VDIR)

obj-y += mdla.o
mdla-y += $(VDIR)/mdla_main.o $(VDIR)/mdla_debug.o $(VDIR)/mdla_util.o $(VDIR)/mdla_power_ctrl.o $(VDIR)/mdla_cmd_proc.o
mdla-y += $(VDIR)/mdla_debug.o
mdla-y += $(PDIR)/mdla_plat.o
mdla-y += $(PMU_DIR)/$(PMU_VDIR)/mdla_trace.o
mdla-y += $(PMU_DIR)/$(PMU_PDIR)/mdla_pmu.o

subdir-ccflags-y += -DMTK_MDLA_DVT

# FPGA environment
ifeq ($(CONFIG_MTK_FPGA),y)
subdir-ccflags-y += -DMTK_MDLA_FPGA_PORTING
else ifeq ($(CONFIG_FPGA_EARLY_PORTING),y)
subdir-ccflags-y += -DMTK_MDLA_FPGA_PORTING
endif

# SWPM support
ifeq ($(CONFIG_MTK_SWPM),y)
subdir-ccflags-y += -DMTK_SWPM
endif

# PER_OBSERVER support
ifeq ($(CONFIG_MTK_PERF_OBSERVER),y)
subdir-ccflags-y += -DMTK_PERF_OBSERVER
endif
