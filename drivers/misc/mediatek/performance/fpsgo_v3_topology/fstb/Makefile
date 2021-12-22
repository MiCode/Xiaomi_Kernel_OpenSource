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

obj-y += fstb.o

MTK_TOP = $(srctree)/drivers/misc/mediatek/

ccflags-y  += -I$(srctree)/drivers/misc/mediatek/video/include
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/ged/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/dfrc/
ccflags-y  += -I$(srctree)/kernel/trace/
ccflags-y  += -I$(MTK_TOP)/base/power/include/
ccflags-y  += -I$(MTK_TOP)/base/power/$(MTK_PLATFORM)/
ccflags-y  += -I$(MTK_TOP)/performance/fpsgo_v3_topology/base/include
ccflags-y  += -I$(MTK_TOP)/performance/fpsgo_v3_topology/eara_job/include
ccflags-y  += -I$(MTK_TOP)/performance/include
