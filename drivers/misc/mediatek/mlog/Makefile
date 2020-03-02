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

ccflags-$(CONFIG_ZRAM) += -I$(srctree)/drivers/block/zram
ccflags-$(CONFIG_ZSMALLOC) += -I$(srctree)/drivers/staging/zsmalloc
ccflags-y += -I$(srctree)/drivers/staging/android/ion

obj-y += mlog.o
mlog-y := mlog_dump.o mlog_logger.o
