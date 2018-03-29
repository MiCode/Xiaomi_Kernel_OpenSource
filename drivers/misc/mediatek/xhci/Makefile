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
# Makefile for USB Host Lib
#

ccflags-y += -I$(srctree)/drivers/usb/host
ccflags-y += -I$(srctree)/drivers/usb/core
ccflags-y += -I$(srctree)/drivers/misc/mediatek/xhci
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mu3phy
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mu3phy/$(CONFIG_MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mu3d/drv
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mu3d/hal
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb_c/inc

ifeq ($(CONFIG_USB_DEBUG),y)
	EXTRA_CFLAGS		+= -DDEBUG
endif

obj-$(CONFIG_USB_XHCI_MTK) += xhci-mtk-power.o xhci-mtk-driver.o xhci-mtk-attrs.o
