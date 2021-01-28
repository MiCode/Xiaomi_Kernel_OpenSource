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

# In case the platform does NOT support this type of sensors
#obj-$(CONFIG_MTK_SCP_SENSORHUB) += SCP_sensorHub/
#obj-$(CONFIG_MTK_SENSOR_HUB_SUPPORT) += SCP_shf/
#obj-$(CONFIG_MTK_SCP_SENSORHUB_V1) += SCP_sensorHub_v1/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/sensorHub/inc_v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/sensorHub/nanohub
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp
#ccflags-y += -I$(srctree)/arch/arm/mach-mt2601/pike/dct/dct

ifeq ($(CONFIG_NANOHUB), y)
obj-$(CONFIG_MTK_SENSOR_SUPPORT) += sensorHub.o

sensorHub-$(CONFIG_NANOHUB_MTK_IPI) := nanohub/main.o \
            nanohub/bl.o nanohub/comms.o
sensorHub-$(CONFIG_NANOHUB_MTK_IPI) += nanohub/nanohub-mtk.o

sensorHub-$(CONFIG_NANOHUB_MTK_IPI) += SCP_nanoHub/SCP_nanoHub.o \
        SCP_power_monitor/SCP_power_monitor.o
endif

