#
# Copyright (C) 2017 MediaTek Inc.
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
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#

MTK_TOP = $(srctree)/drivers/misc/mediatek/

ifeq ($(CONFIG_MACH_MT6893),y)
ccflags-y  += -I$(MTK_TOP)/performance/fpsgo_v3_topology/base/include/
ccflags-y  += -I$(MTK_TOP)/performance/fpsgo_v3_topology/fstb/
else ifeq ($(CONFIG_MACH_MT6877),y)
ccflags-y  += -I$(MTK_TOP)/performance/fpsgo_v3_topology/base/include/
ccflags-y  += -I$(MTK_TOP)/performance/fpsgo_v3_topology/fstb/
else ifeq ($(CONFIG_MACH_MT6781),y)
ccflags-y  += -I$(MTK_TOP)/performance/fpsgo_v3_topology/base/include/
ccflags-y  += -I$(MTK_TOP)/performance/fpsgo_v3_topology/fstb/
else ifeq ($(CONFIG_MACH_MT6739),y)
ccflags-y  += -I$(MTK_TOP)/performance/fpsgo_v3_topology/base/include/
ccflags-y  += -I$(MTK_TOP)/performance/fpsgo_v3_topology/fstb/
else ifeq ($(CONFIG_MACH_MT6833),y)
ccflags-y  += -I$(MTK_TOP)/performance/fpsgo_v3_topology/base/include/
ccflags-y  += -I$(MTK_TOP)/performance/fpsgo_v3_topology/fstb/
else ifeq ($(CONFIG_MACH_MT6768),y)
ccflags-y  += -I$(MTK_TOP)/performance/fpsgo_v3_topology/base/include/
ccflags-y  += -I$(MTK_TOP)/performance/fpsgo_v3_topology/fstb/
else
ccflags-y  += -I$(MTK_TOP)/performance/fpsgo_v3/base/include/
ccflags-y  += -I$(MTK_TOP)/performance/fpsgo_v3/fstb/
endif

obj-y += gbe1_main.o
obj-y += gbe2_main.o
obj-y += gbe_common.o
obj-y += gbe_sysfs.o
