# SPDX-License-Identifier: GPL-2.0

ccflags-y := -I$(srctree)/drivers/usb/gadget/
ccflags-y += -I$(srctree)/drivers/usb/gadget/udc/

usb_f_mtp-y                      := f_mtp.o
obj-$(CONFIG_USB_CONFIGFS_F_MTP) += usb_f_mtp.o
usb_f_ptp-y                      := f_ptp.o
obj-$(CONFIG_USB_CONFIGFS_F_MTP) += usb_f_ptp.o
