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
             -I$(srctree)/drivers/misc/mediatek/mu3phy/$(CONFIG_MTK_PLATFORM)

ifeq ($(CONFIG_USB_DEBUG),y)
	EXTRA_CFLAGS += -DDEBUG
endif

obj-$(CONFIG_MU3_PHY) := mu3phy.o
mu3phy-$(CONFIG_MU3_PHY) := mtk-phy.o
mu3phy-$(CONFIG_U3_PHY_GPIO_SUPPORT) += mtk-phy-gpio.o
mu3phy-$(CONFIG_U3_PHY_AHB_SUPPORT) += mtk-phy-ahb.o
mu3phy-$(CONFIG_C60802_SUPPORT) += mtk-phy-c60802.o
mu3phy-$(CONFIG_D60802_SUPPORT) += mtk-phy-d60802.o
mu3phy-$(CONFIG_E60802_SUPPORT) += mtk-phy-e60802.o
mu3phy-$(CONFIG_A60810_SUPPORT) += mtk-phy-a60810.o
ifeq (,$(filter $(CONFIG_MTK_PLATFORM) , "elbrus"))
mu3phy-$(CONFIG_DEBUG_FS) += usb20_phy_debugfs.o
else
ccflags-y += -DUSB_ELBRUS
endif
obj-$(CONFIG_PROJECT_PHY) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
