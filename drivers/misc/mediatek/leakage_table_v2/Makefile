#
# Copyright (C) 2017 MediaTek Inc.
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

ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/include/ \
	-I$(srctree)/drivers/misc/mediatek/include/leakage_table_v2/ \

obj-$(CONFIG_MTK_STATIC_POWER_LEGACY) += mediatek_static_power.o
mediatek_static_power-y += mtk_static_power.o

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6768)),)
mediatek_static_power-y += mtk_static_power_plat.o
else ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6765)),)
mediatek_static_power-y += mtk_static_power_6765.o
endif


