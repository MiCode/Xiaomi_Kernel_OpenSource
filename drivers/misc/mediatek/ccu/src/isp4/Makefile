# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2016 MediaTek Inc.
#

#ion
ccflags-y += -I$(srctree)/drivers/iommu/
#for sensorType in ccu_set_sensor_info
ccflags-y += -I$(srctree)/drivers/misc/mediatek/imgsensor/inc
#
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include/mach
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/irq/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccu/src/1.2
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccu/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccu/src/isp4/ccu_ext_interface/
ccflags-y += -I$(srctree)/drivers/i2c/busses
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs/
ccflags-y += -I$(srctree)/include/dt-bindings/memory/

obj-$(CONFIG_MTK_CCU) += ccu_isp4.o

ccu_isp4-objs += ccu_drv.o
ccu_isp4-objs += ccu_hw.o
#ccu_isp4-objs += ccu_i2c.o
#ccu_isp4-objs += ccu_i2c_hw.o
ccu_isp4-objs += ccu_kd_mailbox.o
#ccu_isp4-objs += ccu_n3d_a.o
ccu_isp4-objs += ccu_reg.o
ccu_isp4-objs += ccu_sw_ver.o

#obj-$(CONFIG_MTK_VPU) += ccu_hw.o
#obj-y += ccu_hw.o ccu_reg.o ccu_kd_mailbox.o ccu_i2c_hw.o ccu_sw_ver.o
