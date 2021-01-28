# SPDX-License-Identifier: GPL-2.0 */
#
# Copyright (c) 2019 MediaTek Inc.
#

obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_MACH_MT6758) += 1.0/
obj-$(CONFIG_MACH_MT6779) += 3.0/
#subdir-ccflags-y += -DMTK_VPU_EMULATOR

MTK_VPU_CORE := 1
ifneq (,$(filter $(strip $(MTK_PLATFORM)), mt6779))
MTK_VPU_CORE := 2
else
endif
subdir-ccflags-y += -DMTK_VPU_CORE=$(MTK_VPU_CORE)

subdir-ccflags-y += -DMTK_VPU_DVT


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

