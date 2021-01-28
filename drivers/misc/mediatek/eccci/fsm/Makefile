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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/port
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

# fsm objects
obj-$(CONFIG_MTK_ECCCI_DRIVER) := ccci_fsm_all.o

ccci_fsm_all-y := ccci_fsm.o \
			ccci_fsm_ioctl.o \
			ccci_fsm_monitor.o \
			ccci_fsm_poller.o \
			ccci_fsm_scp.o \
			ccci_fsm_ee.o \
			ccci_fsm_sys.o \
			mdee_dumper_v1.o \
			mdee_dumper_v2.o \
			mdee_dumper_v3.o

endif