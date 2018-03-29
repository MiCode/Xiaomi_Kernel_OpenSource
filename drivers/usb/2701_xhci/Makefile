#
# Makefile for USB Host Controller Drivers
#

# tell define_trace.h where to find the xhci trace header
CFLAGS_xhci-trace.o := -I$(src)

xhci-hcd-mtk-y := xhci.o xhci-mem.o
xhci-hcd-mtk-y += xhci-ring.o xhci-hub.o xhci-dbg.o
xhci-hcd-mtk-y += xhci-trace.o
xhci-hcd-mtk-y += xhci-mtk-sch.o
xhci-hcd-mtk-y += xhci-mtk.o

obj-$(CONFIG_USB_XHCI_MTK_UPS)	+= xhci-hcd-mtk.o
