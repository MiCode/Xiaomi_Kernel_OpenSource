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

ccflags-y += -I$(srctree)/drivers/watchdog/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include

obj-$(CONFIG_MTK_WD_KICKER) += wd_common_drv.o
obj-y += wd_api.o

# for WDT Test
# obj-y += wdt_test.o
