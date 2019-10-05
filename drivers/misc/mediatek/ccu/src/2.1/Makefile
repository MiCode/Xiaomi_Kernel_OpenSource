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

ccflags-y += -I$(srctree)/drivers/staging/android/ion \
             -I$(srctree)/drivers/staging/android/ion/mtk
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccu/src/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccu/src/$(MTK_PLATFORM)/ccu_ext_interface/
ccflags-y += -I$(srctree)/drivers/i2c/busses

ccflags-y += -I$(srctree)/drivers/iommu/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/imgsensor/inc

ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/

#$(warning $(subdir-ccflags-y))

#obj-$(CONFIG_MTK_VPU) += ccu_drv.o ccu_algo.o ccu_dbg.o
obj-y += ccu_drv.o ccu_n3d_a.o ccu_i2c.o
