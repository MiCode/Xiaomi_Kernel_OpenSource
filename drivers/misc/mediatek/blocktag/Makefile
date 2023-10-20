#
# Copyright (C) 2020 MediaTek Inc.
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
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/aee/mrdump/
ccflags-y += -I$(srctree)/drivers/scsi/ufs/
ccflags-y += -I$(srctree)/drivers/mmc/core
ccflags-y += -I$(srctree)/drivers/mmc/host

obj-$(CONFIG_MTK_BLOCK_IO_TRACER)	+= blocktag.o
blocktag-y				+= blocktag-core.o blocktag-index.o blocktag-ufs.o blocktag-mmc.o
blocktag-$(CONFIG_MTK_BLOCK_IO_PM_DEBUG)+= blocktag-pm-trace.o
blocktag-$(CONFIG_MTK_FSCMD_TRACER)+= fscmd-trace.o

