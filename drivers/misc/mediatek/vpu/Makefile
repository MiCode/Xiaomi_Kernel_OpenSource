# SPDX-License-Identifier: GPL-2.0 */
#
# Copyright (c) 2019 MediaTek Inc.
#

#subdir-ccflags-y += -DMTK_VPU_EMULATOR

ifneq (,$(filter $(strip $(CONFIG_MTK_PLATFORM)), "mt6779"))
obj-y += $(CONFIG_MTK_PLATFORM)/
obj-y += 3.0/
MTK_VPU_CORE := 2
endif

subdir-ccflags-y += -DMTK_VPU_CORE=$(MTK_VPU_CORE)

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

