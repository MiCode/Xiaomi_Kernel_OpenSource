# Copyright (c) 2015-2018 TrustKernel Incorporated
# All Rights Reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# version 2 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.

ifneq ($(strip $(M)),)
	obj-m += tkcore_drv.o
else
	obj-y += tkcore_drv.o
endif

M ?= $(srctree)/drivers/misc/mediatek/tkcore

ccflags-y += -I$(M)/include/arm_common
ccflags-y += -I$(M)/include/linux
ccflags-y += -I$(M)/include
ccflags-y += -I$(M)/core

tkcore_drv-objs:= \
		tee_tz_drv.o \
		tee_smc_xfer.o \
		tee_mem.o \
		handle.o
