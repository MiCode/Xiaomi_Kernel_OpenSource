#
# Makefile for IP over TTY driver
#
MODULE_NAME = ip-over-tty
EXTRA_CFLAGS = -Wall -Werror
# TODO: get rid of this before release
#EXTRA_CFLAGS += -Wno-unused-variable -Wno-unused-function -Wno-unused-label

obj-$(CONFIG_IP_OVER_TTY)	+= $(MODULE_NAME).o

$(MODULE_NAME)-objs := ip_over_tty.o \
			ip_over_tty_ldisc.o \
			ip_over_tty_net.o
