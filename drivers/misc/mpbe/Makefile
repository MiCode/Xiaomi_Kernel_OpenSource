#
# Copyright (C) 2023 Xiaomi Inc.
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
ccflags-y += -I$(srctree)/drivers/ufs/host/
ccflags-y += -I$(srctree)/drivers/ufs/core/
ccflags-y += -I$(srctree)/drivers/mi_ufs/

obj-$(CONFIG_MI_MPBE_SUPPORT)	+= mpbe.o
mpbe-y				+= mpbe-core.o mpbe-mictx.o mpbe-earaio.o mpbe-ufs.o

