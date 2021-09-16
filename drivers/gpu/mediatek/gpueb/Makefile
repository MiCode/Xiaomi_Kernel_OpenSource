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
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpueb/include/

#subdir-ccflags-y += -Wno-error

obj-m += gpueb.o
gpueb-y += \
    gpueb_init.o \
    gpueb_ipi.o \
    gpueb_logger.o \
    gpueb_plat_service.o \
    gpueb_reserved_mem.o \
    gpueb_gpumpu.o \
    gpueb_hwvoter_dbg.o \
    gpueb_debug.o \
    gpueb_timesync.o
