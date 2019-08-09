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

obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_MACH_MT6779) += 1.0/

ifneq (,$(filter $(strip $(MTK_PLATFORM)), mt6779))
endif
MTK_MDLA_CORE := 1
ifneq (,$(filter $(strip $(MTK_PLATFORM)), mt6779))
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
