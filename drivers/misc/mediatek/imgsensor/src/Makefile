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

include $(srctree)/drivers/misc/mediatek/imgsensor/src/Makefile.custom

obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM_SENSOR))/

ifeq ($(CONFIG_MTK_CAM_MAX_NUMBER_OF_CAMERA),4)
subdir-ccflags-y += -DMTK_SUB2_IMGSENSOR
endif

