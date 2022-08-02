# SPDX-License-Identifier: GPL-2.0
#
# Makefile for USB Host Controller Drivers
#

# tell define_trace.h where to find the xhci trace header
CFLAGS_xhci-trace.o := -I$(src)

ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb/usb_offload

xhci-mtk-hcd-v2-y := xhci.o xhci-mem.o xhci-ext-caps.o
xhci-mtk-hcd-v2-y += xhci-ring.o xhci-hub.o xhci-dbg.o
xhci-mtk-hcd-v2-y += xhci-trace.o
xhci-mtk-hcd-v2-y += xhci-mtk.o xhci-mtk-sch.o

ifneq ($(CONFIG_DEBUG_FS),)
	xhci-mtk-hcd-v2-y		+= xhci-debugfs.o
endif

obj-$(CONFIG_DEVICE_MODULES_USB_XHCI_MTK) += xhci-mtk-hcd-v2.o
