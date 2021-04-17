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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpueb/common/v1/include/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpueb/platform/

#subdir-ccflags-y += -Wno-error

obj-m += gpueb_mt6893.o
gpueb_mt6893-y += \
    common/v1/gpueb_common_init.o \
    common/v1/gpueb_common_ipi_init.o \
    common/v1/gpueb_common_ipi_test.o \
    common/v1/gpueb_common_logger.o \
    common/v1/gpueb_common_plat_service_init.o \
    common/v1/gpueb_common_reserved_mem.o \
    platform/gpueb_mt6893_config.o

obj-m += gpueb_mt6983.o
gpueb_mt6983-y += \
    common/v1/gpueb_common_init.o \
    common/v1/gpueb_common_ipi_init.o \
    common/v1/gpueb_common_ipi_test.o \
    common/v1/gpueb_common_logger.o \
    common/v1/gpueb_common_plat_service_init.o \
    common/v1/gpueb_common_reserved_mem.o \
    platform/gpueb_mt6983_config.o