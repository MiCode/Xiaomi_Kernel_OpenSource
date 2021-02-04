#
# Copyright (C) 2015 MediaTek Inc.
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


MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/pmic/include

ifneq ($(CONFIG_FPGA_EARLY_PORTING),y)
ifeq ($(CONFIG_MTK_PMIC_COMMON),y)
ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
	obj-y += battery/
endif
endif
endif

ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
	obj-$(CONFIG_MTK_CHARGER) += charger/
	obj-y += misc/
else ifeq ($(CONFIG_MTK_GAUGE_VERSION),20)
	obj-$(CONFIG_MTK_SMART_BATTERY) += battery_meter.o battery_common.o
endif

ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
	#nothing
else
	ifeq ($(CONFIG_MTK_SWCHR_SUPPORT),y)
		obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
	else ifeq ($(CONFIG_MTK_BQ24196_SUPPORT),y)
		obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
	else ifeq ($(CONFIG_MTK_BQ24296_SUPPORT),y)
		obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
	else
		obj-$(CONFIG_MTK_SMART_BATTERY) += linear_charging.o
	endif
endif

