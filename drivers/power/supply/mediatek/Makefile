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
ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
ifeq ($(CONFIG_MTK_CHARGER_BOMB),y)
	obj-y += battery_bomb/
else ifeq ($(CONFIG_MTK_CHARGER_CEZANNE),y)
	obj-y += battery_cezanne/
else ifeq ($(CONFIG_MTK_CHARGER_CANNON),y)
	obj-y += battery_cannon/
else ifeq ($(CONFIG_MTK_CHARGER_BEGONIA),y)
	obj-y += battery_begonia/
else
	obj-y += battery/
endif
endif
endif

ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
ifeq ($(CONFIG_MTK_CHARGER_BOMB),y)
	obj-$(CONFIG_MTK_CHARGER) += charger_bomb/
else ifeq ($(CONFIG_MTK_CHARGER_CEZANNE),y)
	obj-$(CONFIG_MTK_CHARGER) += charger_cezanne/
else ifeq ($(CONFIG_MTK_CHARGER_CANNON),y)
	obj-$(CONFIG_MTK_CHARGER) += charger_cannon/
else ifeq ($(CONFIG_MTK_CHARGER_BEGONIA),y)
	obj-$(CONFIG_MTK_CHARGER) += charger_begonia/
else
	obj-$(CONFIG_MTK_CHARGER) += charger/
endif
else ifeq ($(CONFIG_MTK_GAUGE_VERSION),20)
	obj-$(CONFIG_MTK_SMART_BATTERY) += battery_meter_fg_20.o battery_common_fg_20.o
	obj-$(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT) += mtk_pep_intf.o
	obj-$(CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT) += mtk_pep20_intf.o
else
	obj-$(CONFIG_MTK_SMART_BATTERY) += battery_meter.o battery_common.o
	obj-$(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT) += mtk_pep_intf.o
	obj-$(CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT) += mtk_pep20_intf.o
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

ifneq ($(CONFIG_MTK_GAUGE_VERSION),10)
obj-y += misc/
endif

