# SPDX-License-Identifier: GPL-2.0

ccflags-y := -I$(srctree)/drivers/usb/gadget/
ccflags-y += -I$(srctree)/drivers/usb/gadget/udc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/rps/

mtk_u_ether-y			:= u_ether.o
mtk_usb_f_rndis-y		:= f_rndis.o rndis.o

obj-$(CONFIG_MTK_USB_U_ETHER)	+= mtk_u_ether.o
obj-$(CONFIG_MTK_USB_F_RNDIS)	+= mtk_usb_f_rndis.o
