
# Copyright (C) 2016 MediaTek Inc.
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
export PTP3_ROOT_DIR ?= $(srctree)/drivers/misc/mediatek/base/power/ptp3_v1
ifeq ($(CONFIG_MACH_MT6893),y)
obj-y += mt6893/
else
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif