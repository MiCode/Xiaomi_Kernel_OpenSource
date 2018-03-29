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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/mu3d/drv \
             -I$(srctree)/drivers/misc/mediatek/mu3d/hal \
             -I$(srctree)/drivers/misc/mediatek/mu3phy
ifeq (,$(filter $(CONFIG_MTK_PLATFORM) , "elbrus"))
ccflags-y += -DUSE_SSUSB_QMU
else
ccflags-y += -DUSB_ELBRUS
endif

obj-y := mu3d_hal.o

mu3d_hal-y := mu3d_hal_osal.o \
              mu3d_hal_qmu_drv.o \
              mu3d_hal_usb_drv.o \
              mu3d_hal_phy.o
