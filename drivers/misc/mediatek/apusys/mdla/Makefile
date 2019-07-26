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
VDIR = 2.0
PMU_DIR = ../pmu
PMU_VDIR = 1.0
PMU_PDIR = mt6779
MEM_DIR = ../mem
MEM_VDIR = 1.0
UTIL_DIR = ../util
APUSYS_QOS_DIR = ../apusys_qos
APUSYS_QOS_PDIR = mt6779
POWER_DIR = ../power
POWER_VDIR = 2.0

ccflags-y += -I$(obj)/$(VDIR)
ccflags-y += -I$(obj)/$(PDIR)
ccflags-y += -I$(obj)/$(UTIL_DIR)
ccflags-y += -I$(obj)/$(PMU_DIR)/$(PMU_PDIR)
ccflags-y += -I$(obj)/$(PMU_DIR)/$(PMU_VDIR)
ccflags-y += -I$(obj)/$(APUSYS_QOS_DIR)/$(APUSYS_QOS_PDIR)
ccflags-y += -I$(obj)/$(MEM_DIR)/$(MEM_VDIR)
ccflags-y += -I$(obj)/$(POWER_DIR)/$(POWER_VDIR)
ccflags-y += -I$(obj)/$(POWER_DIR)/$(POWER_VDIR)/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/staging/android/ion
ccflags-y += -I$(srctree)/arch/arm64/include/asm/


obj-m += mdla.o
mdla-y += $(VDIR)/mdla_main.o $(VDIR)/mdla_debug.o $(VDIR)/mdla_util.o $(VDIR)/mdla_power_ctrl.o $(VDIR)/mdla_cmd_proc.o
mdla-y += $(VDIR)/mdla_debug.o
mdla-y += $(PDIR)/mdla_plat.o
mdla-y += $(PMU_DIR)/$(PMU_VDIR)/mdla_trace.o $(PMU_DIR)/$(PMU_VDIR)/mdla_decoder.o
mdla-y += $(PMU_DIR)/$(PMU_PDIR)/mdla_pmu.o
mdla-y += $(MEM_DIR)/$(MEM_VDIR)/gsm.o $(MEM_DIR)/$(MEM_VDIR)/mdla_ion.o

ifneq (,$(filter $(strip $(MTK_PLATFORM)), mt6885))
endif
MTK_MDLA_CORE := 1
ifneq (,$(filter $(strip $(MTK_PLATFORM)), mt6885))
MTK_MDLA_CORE := 1
else
endif
subdir-ccflags-y += -DMTK_MDLA_CORE=$(MTK_MDLA_CORE)
subdir-ccflags-y += -DMTK_MDLA_DVT

# FPGA environment
ifeq ($(CONFIG_MTK_FPGA),y)
subdir-ccflags-y += -DMTK_MDLA_FPGA_PORTING
else ifeq ($(CONFIG_FPGA_EARLY_PORTING),y)
subdir-ccflags-y += -DMTK_MDLA_FPGA_PORTING
endif

# VPU support
ifeq ($(CONFIG_MTK_VPU_SUPPORT),y)
subdir-ccflags-y += -DMTK_VPU_SUPPORT
endif

# SWPM support
ifeq ($(CONFIG_MTK_SWPM),y)
subdir-ccflags-y += -DMTK_SWPM
endif

# PER_OBSERVER support
ifeq ($(CONFIG_MTK_PERF_OBSERVER),y)
subdir-ccflags-y += -DMTK_PERF_OBSERVER
endif
# HQA load
ifeq ($(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES),"mediatek/evb6779_64_hqact")
subdir-ccflags-y += -DHQA_LOAD
endif

# HQA load
ifeq ($(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES),"mediatek/k79v1_64_ctighten")
subdir-ccflags-y += -DHQA_LOAD
endif
