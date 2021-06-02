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

ifneq (,$(filter $(strip $(MTK_PLATFORM)), mt6885 mt6873 mt6893 mt6853))

ifeq (y, $(filter y, $(CONFIG_MACH_MT6893)))
PDIR = mt6885
else
PDIR = $(MTK_PLATFORM)
endif

IPDIR = p1
VDIR = 4.0

# RemoteRPC
ccflags-y += -I$(srctree)/drivers/remoteproc/

# Memory
ccflags-y += -I$(srctree)/drivers/staging/android/ion
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(PDIR)/

# MTK platform
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(PDIR)/include

# APUSYS
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/debug
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/midware/1.1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/$(PDIR)

# VPU driver
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/vpu/$(VDIR)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/vpu/$(PDIR)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/vpu/$(IPDIR)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/util

# power related
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/qos/$(PDIR)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/devfreq

# SMI debug
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/

obj-y += vpu.o

vpu-$(CONFIG_MTK_APUSYS_VPU_DEBUG) += $(VDIR)/vpu_debug.o $(VDIR)/vpu_tag.o $(IPDIR)/vpu_dump.o $(IPDIR)/vpu_met.o
vpu-y += $(VDIR)/vpu_main.o $(VDIR)/vpu_algo.o $(VDIR)/vpu_cmd.o
vpu-y += $(IPDIR)/vpu_hw.o $(IPDIR)/vpu_mem.o $(IPDIR)/vpu_power.o
vpu-y += $(PDIR)/vpu_cfg.o

endif

