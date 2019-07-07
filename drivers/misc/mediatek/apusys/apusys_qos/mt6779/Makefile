#
# Copyright (C) 2018 MediaTek Inc.
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

#ccflags-y += -I$(srctree)/drivers/misc/mediatek/mdla/$(MTK_PLATFORM)
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/mdla/1.0
ccflags-y += -I$(obj)/../../mdla/2.0/
ccflags-y += -I$(obj)/../../include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/qos/$(MTK_PLATFORM)/

obj-m += apusys_qos.o
apusys_qos-objs := mdla_qos.o
