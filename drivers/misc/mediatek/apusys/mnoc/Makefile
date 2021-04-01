#
# Copyright (C) 2019 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

ifeq (y, $(filter y, $(CONFIG_MACH_MT6877)))
PDIR = mt6877
else
PDIR = $(MTK_PLATFORM)
endif

MNOC_VER = 1.0
POWER_VER = 2.0
MIDWARE_VER = 1.1

# APUSYS
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/$(POWER_VER)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/$(POWER_VER)/$(PDIR)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/midware/$(MIDWARE_VER)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/debug

# MTK platform
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/qos/$(PDIR)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

# MNoC
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/mnoc/$(MNOC_VER)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/mnoc/$(PDIR)

obj-y += apusys_mnoc.o

apusys_mnoc-y += $(MNOC_VER)/mnoc_drv.o $(MNOC_VER)/mnoc_dbg.o
apusys_mnoc-y += $(MNOC_VER)/mnoc_qos.o $(MNOC_VER)/mnoc_pmu.o $(MNOC_VER)/mnoc_qos_sys.o
apusys_mnoc-y += $(PDIR)/mnoc_hw.o
