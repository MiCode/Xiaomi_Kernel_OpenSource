#
# Copyright (C) 2015 MediaTek Inc.
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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/mu3d/drv \
             -I$(srctree)/drivers/misc/mediatek/mu3d/hal \
             -I$(srctree)/drivers/misc/mediatek/mu3phy \
             -I$(srctree)/drivers/misc/mediatek/mu3phy/$(CONFIG_MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc \
             -I$(srctree)/drivers/misc/mediatek/include/mt-plat \
             -I$(srctree)/drivers/misc/mediatek/base/power/include/

ifeq ($(CONFIG_TCPC_CLASS),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb_c/tcpc/inc
endif

ifeq ($(CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pkt_track/mod-pkt_track/include \
             -I$(srctree)/drivers/usb/gadget \
             -I$(srctree)/drivers/usb/gadget/function \
             -I$(srctree)/drivers/misc/mediatek/eccci \
             -I$(srctree)/drivers/misc/mediatek/eccci/$(MTK_PLATFORM)
endif

ccflags-y += -DUSE_SSUSB_QMU

obj-y := musb_hdrc.o
musb_hdrc-y := musb_core.o
musb_hdrc-y += musb_gadget_ep0.o musb_gadget.o
musb_hdrc-y += musb_init.o ssusb_qmu.o mtk_usb.o
musb_hdrc-${CONFIG_DUAL_ROLE_USB_INTF} += musb_dual_role.o
musb_hdrc-$(CONFIG_DEBUG_FS) += musb_debugfs.o
musb_hdrc-$(CONFIG_PHY_MTK_SSUSB) += mtk-ssusb-hal.o ssusb_phy_debugfs.o
