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
             -I$(srctree)/drivers/misc/mediatek/usb_c/inc

ifeq (,$(filter $(CONFIG_MTK_PLATFORM) , "elbrus"))
ccflags-y += -DUSE_SSUSB_QMU
else
ccflags-y += -DUSB_ELBRUS
endif

obj-y := musb_hdrc.o
musb_hdrc-y := musb_core.o
musb_hdrc-y += musb_gadget_ep0.o musb_gadget.o
musb_hdrc-y += musb_init.o ssusb_qmu.o mt_usb.o
musb_hdrc-$(CONFIG_DEBUG_FS) += musb_debugfs.o
