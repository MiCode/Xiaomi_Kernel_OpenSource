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
ifeq ($(CONFIG_AGATE_CHARGER),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc_agate/
else ifeq ($(CONFIG_ARES_CHARGER),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc_ares/
else ifeq ($(CONFIG_PISSARRO_CHARGER),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc_pissarro/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/
endif
subdir-ccflags-$(CONFIG_MTK_USB_TYPEC_U3_MUX) += -I$(srctree)/drivers/misc/mediatek/typec/u3_switch/
subdir-ccflags-$(CONFIG_MTK_USB_TYPEC_U3_MUX) += -I$(srctree)/drivers/misc/mediatek/typec/inc
ifeq ($(CONFIG_AGATE_CHARGER),y)
obj-y += extcon_usb.o boost_manager_agate.o
else ifeq ($(CONFIG_PISSARRO_CHARGER),y)
obj-y += extcon_usb.o boost_manager_pissarro.o
else
obj-y += extcon_usb.o boost_manager.o
endif
obj-$(CONFIG_MTK_USB_IDDIG) += usb-iddig.o
obj-$(CONFIG_TCPC_CLASS) += usb-tcpc.o
obj-$(CONFIG_DUAL_ROLE_USB_INTF) += usb_dual_role.o
