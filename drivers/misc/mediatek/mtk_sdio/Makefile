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

#add kernel source code path as head file search path
ccflags-y += -I$(srctree)/drivers/mmc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include

obj-$(CONFIG_MMC_MTK_SDIO) += msdc.o

msdc-y += sd.o
msdc-y += msdc_tune.o
msdc-y += autok.o
msdc-y += autok_dvfs.o

msdc-y += msdc_io.o



