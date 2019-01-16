#
# Makefile for USB Host Controller Drivers
#


# ccflags-y += -I$(src)/../../../../mediatek/kernel/include
include $(srctree)/drivers/misc/mediatek/Makefile.custom

ccflags-$(CONFIG_USB_DEBUG) += -DDEBUG

xhci-hcd-test-y := xhci.o xhci-mem.o xhci-ring.o xhci-hub.o xhci-dbg.o xhci-platform.o 
xhci-hcd-test-y += mtk-protocol.o mtk-test.o mtk-test-lib.o mtk-usb-hcd.o xhci-mtk-scheduler.o xhci-mtk-power.o

obj-$(CONFIG_MTK_TEST_XHCI)	+= xhci-hcd-test.o

