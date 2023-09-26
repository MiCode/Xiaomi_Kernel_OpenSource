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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs/
obj-y += mmdvfs_plat.o

QOS_BOUND_DETECT := true
ifeq ($(CONFIG_MTK_QOS_V2),y)
ifeq (true, $(QOS_BOUND_DETECT))
ccflags-y += -DQOS_BOUND_DETECT
endif
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/qos/$(MTK_PLATFORM)/

ifeq ($(CONFIG_MEDIATEK_EMI),y)
ccflags-y += -DUSE_MEDIATEK_EMI
endif
