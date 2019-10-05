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

PDIR = $(MTK_PLATFORM)

ccflags-y += -I$(srctree)/drivers/devfreq
ccflags-y += -I$(srctree)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(PDIR)/include/mach/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/test/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/$(PDIR)

# FPGA environment
ifeq ($(CONFIG_MTK_FPGA),y)
subdir-ccflags-y += -DMTK_FPGA_PORTING
else ifeq ($(CONFIG_FPGA_EARLY_PORTING),y)
subdir-ccflags-y += -DMTK_FPGA_PORTING
endif

obj-m += apu_power.o
apu_power-objs:= apusys_power_driver.o apusys_power_ctl.o apusys_power_debug.o \
		$(PDIR)/hal_config_power.o $(PDIR)/apu_power_regulator.o \
		$(PDIR)/apu_power_clock.o $(PDIR)/apusys_power_cust.o

#obj-m += vpu_dummy_driver.o
#vpu_dummy_driver-objs:= $(PDIR)/dummy_vpu_driver.o

#obj-m += mdla_dummy_driver.o
#mdla_dummy_driver-objs:= $(PDIR)/dummy_mdla_driver.o
