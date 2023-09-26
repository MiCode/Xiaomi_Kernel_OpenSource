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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public
ccflags-y += -I$(srctree)/drivers/misc/mediatek/memory-ssmr
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem/public

obj-$(CONFIG_MTK_GZ_KREE) += gz_main.o mtee_ut/gz_ut.o \
			     mtee_ut/gz_chmem_ut.o mtee_ut/gz_shmem_ut.o mtee_ut/gz_vreg_ut.o \
			     mtee-kree/

obj-$(CONFIG_MTK_ENABLE_GENIEZONE) += gz-trusty/
