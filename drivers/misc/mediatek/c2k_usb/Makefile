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


ifeq ($(CONFIG_MTK_ECCCI_C2K), y)

#ccflags-y += -DC2K_USB_UT

ccflags-y += -I$(srctree)/drivers/usb/gadget

obj-$(CONFIG_MTK_ECCCI_C2K) += c2k_usb.o
c2k_usb-y := f_rawbulk.o rawbulk.o rawbulk_transfer.o

else

obj- :=dummy.o

endif
