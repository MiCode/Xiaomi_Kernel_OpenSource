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

ifeq ($(CONFIG_DUM_CHAR_V2),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/nand/v2/
obj-y += v2/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/nand/v1/
obj-y += v1/
endif