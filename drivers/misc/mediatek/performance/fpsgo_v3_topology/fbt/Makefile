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

FPSGO_TOP = $(srctree)/drivers/misc/mediatek/performance/fpsgo_v3_topology
FBT_TOP = $(FPSGO_TOP)/fbt
COMP_TOP = $(FPSGO_TOP)/composer
MTK_TOP = $(srctree)/drivers/misc/mediatek
BASE_TOP = $(FPSGO_TOP)/base
FSTB_TOP = $(FPSGO_TOP)/fstb
EARA_TOP = $(FPSGO_TOP)/eara_job

ccflags-y += -DNR_FREQ_CPU=16

ifeq ($(CONFIG_MACH_MT6893),y)
obj-y += platform/mt6893/fbt_cpu_platform.o
else ifeq ($(CONFIG_MACH_MT6877),y)
obj-y += platform/mt6877/fbt_cpu_platform.o
else ifeq ($(CONFIG_MACH_MT6781),y)
obj-y += platform/mt6781/fbt_cpu_platform.o
else
obj-y += platform/$(MTK_PLATFORM)/fbt_cpu_platform.o
endif

ifeq ($(CONFIG_MACH_MT6893),y)
CONFIG_FPSGO_THERMAL_SUPPORT := y
ccflags-y += -DCONFIG_FPSGO_THERMAL_SUPPORT
else
CONFIG_FPSGO_THERMAL_SUPPORT := n
endif

obj-y += \
	src/fbt_cpu.o \
	src/xgf.o \
	src/mini_top.o \
	src/uboost.o \

obj-$(CONFIG_FPSGO_THERMAL_SUPPORT) += src/thermal_aware.o

ccflags-y += \
	-I$(srctree)/include/ \
	-I$(MTK_TOP)/include/ \
	-I$(FPSGO_TOP)/base/include/ \
	-I$(COMP_TOP)/include/ \
	-I$(FBT_TOP)/include/ \
	-I$(BASE_TOP)/include/ \
	-I$(MTK_TOP)/base/power/include/ \
	-I$(MTK_TOP)/base/power/$(MTK_PLATFORM)/ \
	-I$(FSTB_TOP)/ \
	-I$(EARA_TOP)/include/ \
	-I$(srctree)/kernel/ \
	-I$(MTK_TOP)/thermal/$(MTK_PLATFORM)/inc/ \
	-I$(MTK_TOP)/thermal/$(MTK_PLATFORM)/inc/mach \
	-I$(MTK_TOP)/include/mt-plat \
	-I$(MTK_TOP)/include/mt-plat/$(MTK_PLATFORM)/include/mach \
	-I$(srctree)/drivers/gpu/mediatek/gpufreq/ \
