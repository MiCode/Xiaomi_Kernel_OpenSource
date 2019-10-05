# Copyright (C) 2018 MediaTek Inc.
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
# Makefile for the GenieZone KREE driver
#
GZ_SEC_STORAGE_UT := y

obj-y += gz_main.o gz_ut.o
ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public

obj-y += mtee-kree/

ifeq ($(GZ_SEC_STORAGE_UT),y)
obj-y += gz_sec_storage_ut.o
ccflags-y += -DGZ_SEC_STORAGE_UT
endif
