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
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include

obj-y += fusion.o

obj-$(CONFIG_MTK_ROTATVECHUB)		+=	rotatvechub/
obj-$(CONFIG_MTK_GAMEROTVECHUB)		+=	gamerotvechub/
obj-$(CONFIG_MTK_GMAGROTVECHUB)		+=	gmagrotvechub/
obj-$(CONFIG_MTK_GRAVITYHUB)		+=	gravityhub/
obj-$(CONFIG_MTK_LINEARACCHUB)		+=	linearacchub/
obj-$(CONFIG_MTK_ORIENTHUB)			+=	orienthub/
obj-$(CONFIG_MTK_UNCALI_ACCHUB)		+=	uncali_acchub/
obj-$(CONFIG_MTK_UNCALI_GYROHUB)	+=	uncali_gyrohub/
obj-$(CONFIG_MTK_UNCALI_MAGHUB)		+=	uncali_maghub/