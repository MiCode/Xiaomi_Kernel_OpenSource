#
# Copyright (C) 2016 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

ifeq ($(CONFIG_MTK_ECCCI_DRIVER), y)

ifndef MTK_PLATFORM
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif

ccflags-y += -I$(srctree)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/port
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/conn_md/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccmni
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/udc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/udc/udc_lib_inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/clkbuf_v1

# port objects
obj-$(CONFIG_MTK_ECCCI_DRIVER) := ccci_port.o
ccci_port-y := port_cfg.o \
		port_char.o \
		port_ctlmsg.o \
		port_udc.o \
		port_misc.o \
		port_ipc.o \
		port_net.o \
		port_poller.o \
		port_proxy.o \
		port_rpc.o \
		port_smem.o \
		port_sysmsg.o \
		ccci_swtp.o

endif

