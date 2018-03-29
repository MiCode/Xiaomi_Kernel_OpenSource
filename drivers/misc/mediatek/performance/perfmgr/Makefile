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

obj-y += perfmgr_main.o

ifeq ($(CONFIG_MTK_PERFMGR_TOUCH_BOOST),y)
ccflags-y += -DMTK_TOUCH_BOOST

obj-y += perfmgr_touch.o

ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/performance/perfmgr/$(MTK_PLATFORM)/),)
ccflags-y += -DMTK_BOOST_SUPPORT
obj-y += $(MTK_PLATFORM)/
endif

endif

#ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/

