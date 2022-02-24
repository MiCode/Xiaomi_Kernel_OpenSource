# SPDX-License-Identifier: GPL-2.0

ccflags-$(CONFIG_TCPC_CLASS) += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
ccflags-$(CONFIG_MTK_USB_TYPEC_U3_MUX) += -I$(srctree)/drivers/misc/mediatek/typec/switch

obj-$(CONFIG_EXTCON_MTK_USB) += extcon-mtk-usb.o

#ADAPT_CHARGER_V1/ADAPT_PSY_V1 used to adapt charger/psy v1
ifeq (y, $(filter y, $(CONFIG_MACH_MT6877) $(CONFIG_MACH_MT6768) $(CONFIG_MACH_MT6781) $(CONFIG_MACH_MT6739) \
 $(CONFIG_MACH_MT6833) $(CONFIG_MACH_MT6893) $(CONFIG_MACH_MT6885)))
ccflags-y += -DADAPT_CHARGER_V1
endif
ccflags-y += -DADAPT_PSY_V1
