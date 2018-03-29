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

obj-$(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT) += mtk_pep_intf.o
obj-$(CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT) += mtk_pep20_intf.o

ifeq ($(CONFIG_MTK_HAFG_20),y)
	obj-$(CONFIG_MTK_SMART_BATTERY) += battery_meter_fg_20.o battery_common_fg_20.o
else
obj-$(CONFIG_MTK_SMART_BATTERY) += battery_meter.o battery_common.o
endif


ifeq ($(CONFIG_MTK_FAN5402_SUPPORT),y)
	obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
else ifeq ($(CONFIG_MTK_FAN5405_SUPPORT),y)
	obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
else
	ifeq ($(CONFIG_MTK_SWCHR_SUPPORT),y)
			obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
	else
		ifeq ($(CONFIG_MTK_BQ24158_SUPPORT),y)
			obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
		else
			ifeq ($(CONFIG_MTK_BQ24196_SUPPORT),y)
				obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
			else
				ifeq ($(CONFIG_MTK_BQ24296_SUPPORT),y)
					obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
				else
					ifeq ($(CONFIG_MTK_NCP1851_SUPPORT),y)
						obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
					else
						ifeq ($(CONFIG_MTK_NCP1854_SUPPORT),y)
							obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
						else
							ifeq ($(CONFIG_MTK_BQ24160_SUPPORT),y)
								obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
							else
								ifeq ($(CONFIG_MTK_BQ24157_SUPPORT),y)
									obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
								else
									ifeq ($(CONFIG_MTK_BQ24250_SUPPORT),y)
										obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
									else
										ifeq ($(CONFIG_MTK_BQ24261_SUPPORT),y)
											obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
										else
											ifeq ($(CONFIG_MTK_RT9536_SUPPORT),y)
												obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
											else
												ifeq ($(CONFIG_MTK_BQ25896_SUPPORT),y)
													obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
												else
													ifeq ($(CONFIG_MTK_BQ25898_DUAL_SUPPORT),y)
														obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
													else
														ifeq ($(CONFIG_MTK_RT9466_SUPPORT),y)
															obj-$(CONFIG_MTK_SMART_BATTERY) += switch_charging.o
														else
															obj-$(CONFIG_MTK_SMART_BATTERY) += linear_charging.o
														endif
													endif
												endif
											endif
										endif
									endif
								endif
							endif
						endif
					endif
				endif
			endif
		endif
	endif
endif
