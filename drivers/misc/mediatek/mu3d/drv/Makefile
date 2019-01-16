include $(srctree)/drivers/misc/mediatek/Makefile.custom

EXTRA_CFLAGS += -DUSE_SSUSB_QMU

obj-y := musb_hdrc.o

musb_hdrc-y := musb_core.o
musb_hdrc-y += musb_gadget_ep0.o musb_gadget.o
musb_hdrc-y += musb_init.o ssusb_qmu.o mt_usb.o
musb_hdrc-$(CONFIG_DEBUG_FS) += musb_debugfs.o
