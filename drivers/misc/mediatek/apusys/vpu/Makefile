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

ifneq (,$(filter $(strip $(PDIR)), mt6885))

PDIR = $(MTK_PLATFORM)
VDIR = 4.0
MTK_VPU_CORE = 3

# RemoteRPC
ccflags-y += -I$(srctree)/drivers/remoteproc/

# Memory
ccflags-y += -I$(srctree)/drivers/staging/android/ion
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(PDIR)/

# MTK platform
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(PDIR)/include

# APUSYS
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/$(PDIR)

# VPU driver
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/vpu/$(VDIR)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/vpu/$(PDIR)

# power related
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/qos/$(PDIR)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/devfreq

# SMI debug
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/

obj-m += vpu.o

vpu-$(CONFIG_MTK_APUSYS_VPU_DEBUG) += $(VDIR)/vpu_debug.o
vpu-y += $(VDIR)/vpu_main.o $(VDIR)/vpu_algo.o
vpu-y += $(PDIR)/vpu_hw.o $(PDIR)/vpu_reg.o $(PDIR)/vpu_mem.o

else

endif

subdir-ccflags-y += -DMTK_VPU_CORE=$(MTK_VPU_CORE)
