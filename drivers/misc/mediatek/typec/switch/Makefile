# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/usb/typec

obj-y += mux_switch.o
obj-y += fusb304.o
obj-y += ptn36241g.o
obj-y += ps5169.o
ifeq ($(CONFIG_OCP96011_I2C), y)
obj-y += ocp96011-i2c.o
endif