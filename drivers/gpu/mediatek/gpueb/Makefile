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
MT_GPUEB_BRINGUP ?= 0
MT_GPUEB_IPI_TEST ?= 1

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpueb/common/v01/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpueb/$(MTK_PLATFORM)/

#subdir-ccflags-y += -Wno-error

ccflags-y += -DMT_GPUEB_BRINGUP=$(MT_GPUEB_BRINGUP)

obj-$(CONFIG_MTK_TINYSYS_GPUEB_SUPPORT) += gpueb.o

gpueb-y += \
    common/v01/gpueb_common_init.o \
    common/v01/gpueb_common_ipi.o \
    $(MTK_PLATFORM)/gpueb_plat_ipi_setting.o

ifeq ($(MT_GPUEB_IPI_TEST),1)
    ccflags-y += -DMT_GPUEB_IPI_TEST=$(MT_GPUEB_IPI_TEST)
    gpueb-y += \
        $(MTK_PLATFORM)/gpueb_plat_ipi_test.o
endif