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

#
# for USB OTG silicon based on Mentor Graphics INVENTRA designs
#
ccflags-$(CONFIG_USB_MTK_HDRC) += -I$(srctree)/drivers/misc/mediatek/usb20

# for battery related
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

# for SPM control usage
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/

# for TYPEC connection management
ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/inc
ifeq ($(CONFIG_TCPC_CLASS),y)
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
endif
# for ep0 test
ccflags-y += -I$(srctree)/drivers/usb/core/

# Phy
obj-$(CONFIG_MTK_MUSB_PHY)		+= mtk_usb20_phy.o
mtk_usb20_phy-y				+= usb20_phy.o
mtk_usb20_phy-$(CONFIG_DEBUG_FS)	+= usb20_phy_debugfs.o
