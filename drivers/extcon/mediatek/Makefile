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
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/misc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/
subdir-ccflags-$(CONFIG_MTK_USB_TYPEC_U3_MUX) += -I$(srctree)/drivers/misc/mediatek/typec/u3_switch/
subdir-ccflags-$(CONFIG_MTK_USB_TYPEC_U3_MUX) += -I$(srctree)/drivers/misc/mediatek/typec/inc
ifeq ($(CONFIG_MTK_CHARGER_CANNON),y)
obj-y += extcon_usb.o boost_manager_cannon.o
else
obj-y += extcon_usb.o boost_manager.o
endif
obj-$(CONFIG_MTK_USB_IDDIG) += usb-iddig.o
obj-$(CONFIG_TCPC_CLASS) += usb-tcpc.o
obj-$(CONFIG_DUAL_ROLE_USB_INTF) += usb_dual_role.o
