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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/mu3d/drv \
             -I$(srctree)/drivers/misc/mediatek/mu3d/hal \
             -I$(srctree)/drivers/misc/mediatek/mu3phy \
			 -I$(srctree)/drivers/misc/mediatek/base/power/include/

ifeq ($(CONFIG_USB_DEBUG),y)
	EXTRA_CFLAGS += -DDEBUG
endif

#For USB PHY TUNING
#EXTRA_CFLAGS += -DMTK_USB_PHY_TUNING

obj-$(CONFIG_PROJECT_PHY) += mtk-phy-asic.o
obj-$(CONFIG_DEBUG_FS) += usb20_phy_debugfs.o
