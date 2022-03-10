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
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/platform
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/mt6873
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mt6873
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/conn_md/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/mt6873
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/


# hif objects
obj-$(CONFIG_MTK_ECCCI_DRIVER) := ccci_hif_all.o
ccci_hif_all-y := ccci_hif.o \
		ccci_cldma_plat.o \
		ccci_hif_cldma.o \
		ccci_hif_dpmaif.o dpmaif_drv.o \
		ccci_hif_dpmaif_comm.o ccci_hif_dpmaif_v3.o dpmaif_drv_v3.o \
		ccci_hif_dpmaif_v1.o dpmaif_drv_v1.o \
		dpmaif_bat.o dpmaif_bat_v3.o dpmaif_debug.o net_speed.o net_pool.o

ccci_hif_all-y += ccci_ringbuf.o ccci_hif_ccif.o

endif

