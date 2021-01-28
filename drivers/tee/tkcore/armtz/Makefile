# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2015-2019 TrustKernel Incorporated

ifneq ($(strip $(M)),)
	obj-m += tkcore_drv.o
else
	obj-y += tkcore_drv.o
endif

M ?= $(srctree)/drivers/tee/tkcore

ccflags-y += -I$(M)/include/arm_common
ccflags-y += -I$(M)/include/linux
ccflags-y += -I$(M)/include
ccflags-y += -I$(M)/core

tkcore_drv-objs:= \
		tee_tz_drv.o \
		tee_smc_xfer.o \
		tee_mem.o \
		handle.o
