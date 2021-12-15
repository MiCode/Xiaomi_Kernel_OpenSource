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

usb20plat := $(subst ",,$(CONFIG_MTK_PLATFORM))

ifeq ($(CONFIG_MACH_MT6781),y)
usb20plat := mt6781
endif
ifeq ($(CONFIG_MACH_MT6761),y)
usb20plat := mt6765
endif
ifeq ($(CONFIG_MACH_MT6833),y)
usb20plat := mt6833
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

# for SPM control usage
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include

# for phy and vbus settings
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb20/$(usb20plat)

ifeq ($(CONFIG_TCPC_CLASS),y)
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
endif

#for 6739
ccflags-y += -I$(srctree)/drivers/usb/core/

# Driver
obj-$(CONFIG_USB_MTK_HDRC)		:= musb_hdrc.o
musb_hdrc-$(CONFIG_USB_MTK_HDRC)	+= musb_core.o
musb_hdrc-$(CONFIG_USB_MTK_HDRC)	+= musb_virthub.o musb_host.o
musb_hdrc-$(CONFIG_USB_MTK_HDRC)	+= musb_gadget_ep0.o musb_gadget.o
musb_hdrc-$(CONFIG_PROC_FS)		+= musb_debugfs.o
musb_hdrc-$(CONFIG_MTK_MUSB_DUAL_ROLE)	+= musb_dr.o

# obj-$(CONFIG_MTK_MUSB_PLAT)		+= mtk_musb.o
obj-$(CONFIG_USB_MTK_HDRC)		+= musbhsdma.o

#Platform
obj-y += $(usb20plat)/
obj-$(CONFIG_USB_MTK_HDRC)		+= $(usb20plat)/usb20.o
obj-$(CONFIG_USB_MTK_HDRC)		+= $(usb20plat)/usb20_host.o
obj-$(CONFIG_USBIF_COMPLIANCE)		+= $(usb20plat)/usb20_otg_if.o

# QMU Realted
musb_hdrc-$(CONFIG_MTK_MUSB_QMU_SUPPORT)+= mtk_qmu.o musb_qmu.o
ifeq ($(CONFIG_MTK_MUSB_QMU_SUPPORT),y)
subdir-ccflags-$(CONFIG_MACH_MT8163) += -DMUSB_QMU_LIMIT_SUPPORT -DMUSB_QMU_LIMIT_RXQ_NUM=4 -DMUSB_QMU_LIMIT_TXQ_NUM=4
endif
ifeq ($(CONFIG_MTK_MUSB_QMU_SUPPORT),y)
subdir-ccflags-$(CONFIG_MACH_MT8167) += -DMUSB_QMU_LIMIT_SUPPORT -DMUSB_QMU_LIMIT_RXQ_NUM=4 -DMUSB_QMU_LIMIT_TXQ_NUM=4
endif

subdir-ccflags-$(CONFIG_MACH_MT6781) += -DUSB2_PHY_V2
subdir-ccflags-$(CONFIG_MACH_MT6833) += -DUSB2_PHY_V2
