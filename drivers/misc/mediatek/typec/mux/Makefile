# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/usb/typec
ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc

obj-$(CONFIG_MTK_USB_TYPEC_MUX) += mux_switch.o
obj-$(CONFIG_TYPEC_MUX_FUSB304)	+= fusb304.o
obj-$(CONFIG_TYPEC_MUX_PTN36241G) += ptn36241g.o
obj-$(CONFIG_TYPEC_MUX_PS5169) += ps5169.o
obj-$(CONFIG_MTK_USB_TYPEC_DP_MUX) += pi3dpx1205a.o
