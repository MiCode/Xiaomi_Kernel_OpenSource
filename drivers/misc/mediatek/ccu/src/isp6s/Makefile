#
# Copyright (C) 2016 MediaTek Inc.
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

$(info $(MTK_PLATFORM))
$(info $(platform))
$(info $(srctree))

#ion
ccflags-y += -I$(srctree)/drivers/staging/android/ion \
             -I$(srctree)/drivers/staging/android/ion/mtk \
             -I$(srctree)/drivers/iommu/
#for sensorType in ccu_set_sensor_info
ccflags-y += -I$(srctree)/drivers/misc/mediatek/imgsensor/inc
#
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/mt6873/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/mt6873/include/mach
ccflags-y += -I$(srctree)/drivers/misc/mediatek/irq/mt6873/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/mt6873/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccu/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccu/src/isp6s/ccu_ext_interface
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/
ccflags-y += -I$(srctree)/drivers/i2c/busses/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs/
ccflags-y += -I$(srctree)/include/dt-bindings/memory/

obj-$(CONFIG_MTK_CCU) += ccu.o


ccu-objs += ccu_hw.o
ccu-objs += ccu_reg.o
ccu-objs += ccu_kd_mailbox.o
ccu-objs += ccu_imgsensor.o
ccu-objs += ccu_qos.o
ccu-objs += ccu_drv.o
ccu-objs += ccu_ipc.o
ccu-objs += ccu_mva.o

