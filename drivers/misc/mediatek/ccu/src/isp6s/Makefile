# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2016 MediaTek Inc.
#

$(info $(MTK_PLATFORM))
$(info $(platform))
$(info $(srctree))

#ion
ccflags-y += -I$(srctree)/drivers/iommu/
#for sensorType in ccu_set_sensor_info
ccflags-y += -I$(srctree)/drivers/misc/mediatek/imgsensor/inc
#
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccu/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccu/src/isp6s/ccu_ext_interface
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/
ccflags-y += -I$(srctree)/drivers/i2c/busses/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs/
ccflags-y += -I$(srctree)/include/dt-bindings/memory/

obj-$(CONFIG_MTK_CCU) += ccu_isp6s.o

ccu_isp6s-objs += ccu_hw.o
ccu_isp6s-objs += ccu_reg.o
ccu_isp6s-objs += ccu_kd_mailbox.o
ccu_isp6s-objs += ccu_imgsensor.o
ccu_isp6s-objs += ccu_qos.o
ccu_isp6s-objs += ccu_drv.o
ccu_isp6s-objs += ccu_ipc.o
ccu_isp6s-objs += ccu_mva.o

