#
# Copyright (C) 2016 MediaTek Inc.
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

ifneq (,$(filter $(strip $(MTK_PLATFORM)), mt8168))
obj-y += vp6/
obj-y += vp6/$(subst ",,$(CONFIG_MTK_PLATFORM))/
MTK_VPU_CORE := 1
subdir-ccflags-y += -DMTK_VPU_SMI_DEBUG_ON
else
ifeq (y, $(filter y, $(CONFIG_MACH_MT6893)))
PDIR = mt6885
else
PDIR = $(CONFIG_MTK_PLATFORM)
endif
obj-y += $(subst ",,$(PDIR))/
obj-$(CONFIG_MACH_MT6771) += 2.0/
obj-$(CONFIG_MACH_MT6785) += 3.0/
#subdir-ccflags-y += -DMTK_VPU_EMULATOR

MTK_VPU_CORE := 2
endif

#subdir-ccflags-y += -DMTK_VPU_SMI_DEBUG_ON
subdir-ccflags-y += -DMTK_VPU_CORE=$(MTK_VPU_CORE)
subdir-ccflags-y += -DMTK_VPU_DVT

ifeq ($(CONFIG_MTK_QOS_V2),y)
subdir-ccflags-y += -DENABLE_EARA_QOS
endif

# FPGA environment
ifeq ($(CONFIG_MTK_FPGA),y)
subdir-ccflags-y += -DMTK_VPU_FPGA_PORTING
else ifeq ($(CONFIG_FPGA_EARLY_PORTING),y)
subdir-ccflags-y += -DMTK_VPU_FPGA_PORTING
endif

# MDLA config
ifeq ($(CONFIG_MTK_MDLA_SUPPORT),y)
subdir-ccflags-y += -DMTK_MDLA_SUPPORT
endif

# PER_OBSERVER support
ifeq ($(CONFIG_MTK_PERF_OBSERVER),y)
subdir-ccflags-y += -DMTK_PERF_OBSERVER
endif
# HQA load
ifeq ($(PROJECT_DTB_NAMES),"mediatek/evb6779_64_hqact")
subdir-ccflags-y += -DHQA_LOAD
endif

# HQA load
ifeq ($(PROJECT_DTB_NAMES),"mediatek/k79v1_64_ctighten")
subdir-ccflags-y += -DHQA_LOAD
endif

# AGING load
ifeq ($(PROJECT_DTB_NAMES),"mediatek/k85v1_64_aging")
subdir-ccflags-y += -DAGING_MARGIN
endif
