# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc

ifeq ($(CONFIG_CHARGE_PROJECT),"PLATO")
obj-$(CONFIG_EXTCON_MTK_USB) += extcon-mtk-usb-new.o
else
obj-$(CONFIG_EXTCON_MTK_USB) += extcon-mtk-usb.o
endif
