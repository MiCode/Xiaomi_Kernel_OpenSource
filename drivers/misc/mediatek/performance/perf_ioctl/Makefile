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

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6757"))
	ccflags-y += -DLEGACY_TOUCH_BOOST_SUPPORT
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/performance/tchbst/legacy/
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/

obj-y += perf_ioctl.o
