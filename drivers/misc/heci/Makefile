#
# Makefile - Intel HECI and ISS implementation
# Copyright (c) 2010-2015, Intel Corporation.
#
obj-m += heci.o
heci-objs := init.o
heci-objs += hbm.o
heci-objs += client.o
heci-objs += heci-api.o
heci-objs += bus.o
heci-$(CONFIG_DEBUG_FS) += debugfs.o

obj-m += heci-ish.o
heci-ish-objs := hw-ish.o
heci-ish-objs += pci-ish.o

obj-m += hid-heci-ish.o
hid-heci-ish-objs := heci-hid.o
hid-heci-ish-objs += heci-hid-client.o

obj-m += sens-col-core.o
sens-col-core-objs := senscol-core.o

