# SPDX-License-Identifier: GPL-2.0

ccflags-$(CONFIG_TCPC_CLASS) += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc

obj-$(CONFIG_EXTCON_MTK_USB) += extcon-mtk-usb.o

#ADAPT_CHARGER_V1/ADAPT_PSY_V1 used to adapt charger/psy v1
ccflags-y += -DADAPT_CHARGER_V1
ccflags-y += -DADAPT_PSY_V1
