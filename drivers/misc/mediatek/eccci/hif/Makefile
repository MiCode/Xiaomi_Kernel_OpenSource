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

ifeq ($(CONFIG_MTK_ECCCI_DRIVER), m)
ccflags-y += -DCCCI_KMODULE_ENABLE

#ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/platform
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mt6873
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

# ccci_ccif.ko

ccci_ccif-y := ccci_ringbuf.o ccci_hif_ccif.o
obj-$(CONFIG_MTK_ECCCI_DRIVER) += ccci_ccif.o

# ccci_dpmaif.ko

ccci_dpmaif-y := net_pool.o md_spd_dvfs_fn.o md_spd_dvfs_method.o net_speed.o \
	ccci_dpmaif_com.o ccci_dpmaif_bat.o ccci_dpmaif_debug.o ccci_dpmaif_resv_mem.o \
	ccci_dpmaif_drv_com.o ccci_dpmaif_drv_v1.o ccci_dpmaif_drv_v2.o ccci_dpmaif_drv_v3.o


obj-$(CONFIG_MTK_ECCCI_DRIVER) += ccci_dpmaif.o

# ccci_cldma.ko
ccci_cldma-y := ccci_cldma_plat.o ccci_hif_cldma.o
obj-$(CONFIG_MTK_ECCCI_DRIVER) += ccci_cldma.o

endif


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
		net_pool.o md_spd_dvfs_fn.o md_spd_dvfs_method.o net_speed.o \
		ccci_dpmaif_com.o ccci_dpmaif_bat.o ccci_dpmaif_debug.o ccci_dpmaif_resv_mem.o \
		ccci_dpmaif_drv_com.o ccci_dpmaif_drv_v1.o ccci_dpmaif_drv_v2.o ccci_dpmaif_drv_v3.o

ccci_hif_all-y += ccci_ringbuf.o ccci_hif_ccif.o

endif

