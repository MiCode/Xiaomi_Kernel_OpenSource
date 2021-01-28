#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

# CONN-MD Bridge Driver
# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)

    ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/mt6779
    ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/
    ccflags-y += -I$(srctree)/drivers/misc/mediatek/dual_ccci/

    ccflags-y += -I$(src)/include/

    obj-$(CONFIG_MTK_CONN_MD) += conn_md_drv.o
    conn_md_drv-objs := conn_md.o
    conn_md_drv-objs += conn_md_exp.o
    conn_md_drv-objs += conn_md_log.o
    conn_md_drv-objs += conn_md_dump.o
    conn_md_drv-objs += conn_md_dbg.o
    conn_md_drv-objs += conn_md_test.o

# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD  := $(shell pwd)

default:
	$(MAKE) -C

endif
