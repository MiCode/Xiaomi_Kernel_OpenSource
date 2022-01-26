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

ifeq ($(CONFIG_MACH_MT6893),y)
PDIR = mt6885
else ifeq ($(CONFIG_MACH_MT6877),y)
PDIR = mt6877
else
PDIR = $(MTK_PLATFORM)
endif

ccflags-y += -I$(srctree)/drivers/devfreq
ccflags-y += -I$(srctree)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(PDIR)/include/mach/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/debug/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/test/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/$(PDIR)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/midware/1.1/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/$(CONFIG_MTK_PLATFORM)/

# FPGA environment
ifeq ($(CONFIG_MTK_FPGA),y)
subdir-ccflags-y += -DMTK_FPGA_PORTING
else ifeq ($(CONFIG_FPGA_EARLY_PORTING),y)
subdir-ccflags-y += -DMTK_FPGA_PORTING
endif

obj-y:= apusys_power_driver.o apusys_power_ctl.o apusys_power_debug.o \
		$(PDIR)/hal_config_power.o $(PDIR)/apu_power_regulator.o \
		$(PDIR)/apu_power_clock.o $(PDIR)/apusys_power_cust.o \
		$(PDIR)/apu_power_table.o $(PDIR)/apu_platform_resource.o \
		$(PDIR)/apu_platform_debug.o $(PDIR)/apusys_power_rule_check.o

ifeq (,$(filter $(strip $(MTK_PLATFORM)), mt6885))
obj-y += $(PDIR)/apu_power_tag.o
endif


ifneq (,$(findstring aging,$(PROJECT_DTB_NAMES)))
subdir-ccflags-y += -DAGING_MARGIN
endif

#obj-y += vpu_dummy_driver.o
#vpu_dummy_driver-objs:= $(PDIR)/dummy_vpu_driver.o

#obj-y += mdla_dummy_driver.o
#mdla_dummy_driver-objs:= $(PDIR)/dummy_mdla_driver.o
