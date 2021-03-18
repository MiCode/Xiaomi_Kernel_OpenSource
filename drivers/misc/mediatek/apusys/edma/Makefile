#
# Copyright (C) 2019 MediaTek Inc.
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

PDIR = $(MTK_PLATFORM)

ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/$(PDIR)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/midware/1.1/

obj-y += edma.o

edma-objs += edma_driver.o edma_dbgfs.o edma_plat_device.o
edma-objs += edma_cmd_hnd.o edma_v20.o edma_v30.o
