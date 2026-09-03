# SPDX-License-Identifier: GPL-2.0-only
#
# drivers/char/rpmb/Makefile
#
# Makefile for the Linux rpmb char device drivers.
#

obj-$(CONFIG_RPMB_INTF_DEV) += rpmb.o
rpmb-objs += core.o cdev.o
obj-$(CONFIG_RPMB_SIM) += rpmb_sim.o

ccflags-y += -D__CHECK_ENDIAN__
