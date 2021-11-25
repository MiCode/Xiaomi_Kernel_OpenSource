#
# Copyright (C) 2017 MediaTek Inc.
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
# for USB OTG silicon based on Mentor Graphics INVENTRA designs
#

ifeq ($(MTK_ALPS_BOX_SUPPORT),yes)
ccflags-y += -DMTK_ALPS_BOX_SUPPORT
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

# for SPM control usage
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include

# for typec event
ifeq ($(CONFIG_TCPC_CLASS),$(filter $(CONFIG_TCPC_CLASS),m y))
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
endif

# Driver
obj-$(CONFIG_USB_MTK_HDRC)		:= musb_hdrc.o
musb_hdrc-$(CONFIG_USB_MTK_HDRC)	+= musb_core.o
musb_hdrc-$(CONFIG_USB_MTK_HDRC)	+= musb_virthub.o musb_host.o
musb_hdrc-$(CONFIG_USB_MTK_HDRC)	+= musb_gadget_ep0.o musb_gadget.o
musb_hdrc-$(CONFIG_PROC_FS)		+= musb_debugfs.o
musb_hdrc-$(CONFIG_MTK_MUSB_DUAL_ROLE)	+= musb_dr.o

musb_hdrc-$(CONFIG_USB_MTK_HDRC)		+= usb20.o
musb_hdrc-$(CONFIG_USB_MTK_HDRC)		+= usb20_host.o

# obj-$(CONFIG_MTK_MUSB_PLAT)		+= mtk_musb.o
obj-$(CONFIG_USB_MTK_HDRC)		+= musbhsdma.o

# QMU Realted
musb_hdrc-$(CONFIG_MTK_MUSB_QMU_SUPPORT)+= mtk_qmu.o musb_qmu.o