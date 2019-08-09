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

ifeq ($(CONFIG_TRUSTKERNEL_TEE_RPMB_SUPPORT),y)
ccflags-y += -DRPMB_SUPPORT
endif

tkcore_drv-objs:= \
		tee_tz_drv.o \
		tee_smc_xfer.o \
		tee_mem.o \
		handle.o

ifeq ($(CONFIG_ARM),y)
# "smc" assembly intruction requires dedicated "armv7 secure extension"
secext := $(call as-instr,.arch_extension sec,+sec)
AFLAGS_tee_smc-arm.o := -Wa,-march=armv7-a$(secext)
tkcore_drv-objs += \
	tee_smc-arm.o
endif

ifeq ($(CONFIG_ARM64),y)
tkcore_drv-objs += \
	tee_smc-arm64.o
endif

