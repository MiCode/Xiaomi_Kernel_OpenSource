#
# Copyright (C) 2016 MediaTek Inc.
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

# mt6893&mt6885 co-platform config

ifeq (y, $(filter y, $(CONFIG_MACH_MT6877)))
MTK_PLATFORM := mt6853
subdir-ccflags-y += -DCCCI_PLATFORM_MT6877
else ifeq (y, $(filter y, $(CONFIG_MACH_MT6781)))
MTK_PLATFORM := mt6785
subdir-ccflags-y += -DCCCI_PLATFORM_MT6781
else ifeq (y, $(filter y, $(CONFIG_MACH_MT6893)))
MTK_PLATFORM := mt6885
else ifeq (y, $(filter y, $(CONFIG_MACH_MT6833)))
MTK_PLATFORM := mt6833
else
MTK_PLATFORM := $(CONFIG_MTK_PLATFORM)
endif

ifeq ($(CONFIG_MTK_ECCCI_DRIVER), y)

obj-$(CONFIG_MTK_ECCCI_DRIVER) := ccci.o
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccmni/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/conn_md/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(MTK_PLATFORM)

# common services
ccci-y := ccci_core.o ccci_bm.o ccci_modem.o modem_sys1.o

ifeq  ($(CONFIG_MEDIATEK_MT6577_AUXADC), y)
ccci-y += ccci_auxadc.o
endif

# sub modules
obj-y += fsm/
obj-y += hif/
obj-y += port/
obj-y += udc/
obj-$(CONFIG_MTK_MIX_DEVICES) +=  mixdev/

ifeq ($(CONFIG_MTK_ECCCI_C2K), y)
ccci-y += modem_sys3.o
endif

# platform code
obj-y += $(subst ",,$(MTK_PLATFORM))/

else
obj-  := dummy.o # avoid build error
endif
