# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ifneq ($(CONFIG_DEVICE_MODULES_USB_XHCI_MTK), )
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb/usb_xhci
else
ccflags-y += -I$(srctree)/drivers/usb/host
endif
ccflags-y += -I$(srctree)/drivers/usb/mtu3

obj-$(CONFIG_MTK_USB_BOOST) += usb_boost.o

usb_boost-y := usb_boost_common.o usb_boost_plat.o