#
# Copyright (C) 2018 MediaTek Inc.
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

#ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/performance/boost_ctrl/topo_ctrl/$(CONFIG_MTK_PLATFORM)/),)
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6739" "mt6768" "mt6771" "mt6885" "mt6893"))
	ccflags-y += \
		-I$(srctree)/drivers/misc/mediatek/performance/boost_ctrl/topo_ctrl/$(CONFIG_MTK_PLATFORM)
else
	ccflags-y += \
		-I$(srctree)/drivers/misc/mediatek/performance/boost_ctrl/topo_ctrl/common
endif

obj-y += topo_ctrl.o

