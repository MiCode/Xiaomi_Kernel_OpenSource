# SPDX-License-Identifier: GPL-2.0
#
# USB peripheral controller drivers
#
subdir-ccflags-$(CONFIG_USB_GADGET_DEBUG)	:= -DDEBUG
subdir-ccflags-$(CONFIG_USB_GADGET_VERBOSE)	+= -DVERBOSE_DEBUG
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb_boost
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y				+= -I$(srctree)/drivers/usb/gadget/udc
ccflags-y				+= -I$(srctree)/drivers/usb/gadget/function
ifdef CONFIG_MTPROF
ccflags-y += -Idrivers/misc/mediatek/mtprof/
endif

obj-$(CONFIG_USB_LIBCOMPOSITE)	+= libcomposite.o
libcomposite-y			:= usbstring.o config.o epautoconf.o
libcomposite-y			+= composite.o functions.o configfs.o u_f.o
libcomposite-$(CONFIG_USB_CONFIGFS_UEVENT)          += meta.o

obj-$(CONFIG_USB_GADGET)	+= udc/ function/ legacy/
