# SPDX-License-Identifier: GPL-2.0
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



# ccflags-y += -DC2K_USB_UT

ccflags-y += -I$(srctree)/drivers/usb/gadget
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

obj-$(CONFIG_MTK_ECCCI_C2K_USB) := c2k_usb.o
c2k_usb-$(CONFIG_MTK_ECCCI_C2K_USB) += f_rawbulk.o rawbulk.o rawbulk_transfer.o

# C2K Functions
c2k_usb_f_via_modem-y		:= f_via_modem.o
obj-$(CONFIG_MTK_ECCCI_C2K_USB)	+= c2k_usb_f_via_modem.o
c2k_usb_f_via_ets-y		:= f_via_ets.o
obj-$(CONFIG_MTK_ECCCI_C2K_USB)	+= c2k_usb_f_via_ets.o
c2k_usb_f_via_atc-y		:= f_via_atc.o
obj-$(CONFIG_MTK_ECCCI_C2K_USB)	+= c2k_usb_f_via_atc.o
c2k_usb_f_via_pcv-y		:= f_via_pcv.o
obj-$(CONFIG_MTK_ECCCI_C2K_USB)	+= c2k_usb_f_via_pcv.o
c2k_usb_f_via_gps-y		:= f_via_gps.o
obj-$(CONFIG_MTK_ECCCI_C2K_USB)	+= c2k_usb_f_via_gps.o

