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
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwmon/include

obj-y   +=  barometer.o barometer_factory.o

ifeq ($(CONFIG_MTK_SCP_SENSORHUB_V1),y)
obj-$(CONFIG_MTK_BAROHUB)      +=  barohub/
else
obj-$(CONFIG_MTK_BMP180) += BMP180/
obj-$(CONFIG_MTK_BMP180_NEW) += BMP180-new/
obj-$(CONFIG_MTK_BMP280) += bmp280/
obj-$(CONFIG_MTK_BMP280_NEW) += BMP280-new/
endif
