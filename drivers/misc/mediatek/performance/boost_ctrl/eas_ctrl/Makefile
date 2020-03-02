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

#EAS_CONTROLLER
MTK_TOP = $(srctree)/drivers/misc/mediatek/
ccflags-y += \
	-I$(MTK_TOP)/base/power/include/ \
	-I$(MTK_TOP)/base/power/$(MTK_PLATFORM)/ \
	-I$(MTK_TOP)/dramc/$(MTK_PLATFORM)/ \
	-I$(srctree)/drivers/misc/mediatek/performance/boost_ctrl/include/ \

obj-y += eas_ctrl.o

