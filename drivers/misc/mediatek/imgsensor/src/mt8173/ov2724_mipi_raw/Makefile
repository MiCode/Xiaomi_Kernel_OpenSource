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

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))

MTK_PLATFORM_PATH_ROOT := $(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)
MTK_PROJECT_PATH_ROOT := $(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)

ccflags-y += -I$(MTK_PROJECT_PATH_ROOT)/camera/camera \

MTK_CUSTOM_PATH  := $(srctree)/drivers/misc/mediatek

ccflags-y += -I$(MTK_CUSTOM_PATH)/camera/$(MTK_PLATFORM) \
             -I$(MTK_CUSTOM_PATH)/i2c/$(MTK_PLATFORM) \
             -I$(MTK_CUSTOM_PATH)/imgsensor/inc \
             -I$(MTK_CUSTOM_PATH)/imgsensor/src/$(MTK_PLATFORM)/camera_hw \

obj-y += ov2724mipiraw_Sensor.o


