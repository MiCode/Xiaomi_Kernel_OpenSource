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
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#
MTK_TOP = $(srctree)/drivers/misc/mediatek/
EARA_THRM_TOP = $(srctree)/drivers/misc/mediatek/performance/eara_thermal/
FPSGO_TOP = $(srctree)/drivers/misc/mediatek/performance/fpsgo_v3/

ifeq ($(CONFIG_MACH_MT6877),y)
obj-y += platform/mt6877/thermal_budget_platform.o
else ifeq ($(CONFIG_MACH_MT6781),y)
obj-y += platform/mt6781/thermal_budget_platform.o
else
obj-y += platform/$(MTK_PLATFORM)/thermal_budget_platform.o
endif
obj-y += src/thermal_budget.o
obj-y += src/thermal_base.o
obj-y += src/thermal_prethrottle.o

ccflags-y += -I$(MTK_TOP)/thermal/common/inc/
ccflags-$(CONFIG_MACH_MT6833) += -I$(MTK_TOP)/thermal/mt6833/inc/
ccflags-$(CONFIG_MACH_MT6877) += -I$(MTK_TOP)/thermal/mt6877/inc/
ccflags-$(CONFIG_MACH_MT6781) += -I$(MTK_TOP)/thermal/mt6781/inc/
ccflags-y += -I$(MTK_TOP)/thermal/$(MTK_PLATFORM)/inc/
ccflags-y += -I$(MTK_TOP)/base/power/$(MTK_PLATFORM)/
ccflags-y += -I$(MTK_TOP)/base/power/include/
ccflags-$(CONFIG_MACH_MT6833) += -I$(MTK_TOP)/base/power/ppm_v3/src/mach/mt6833/
ccflags-$(CONFIG_MACH_MT6877) += -I$(MTK_TOP)/base/power/ppm_v3/src/mach/mt6877/
ccflags-$(CONFIG_MACH_MT6781) += -I$(MTK_TOP)/base/power/ppm_v3/src/mach/mt6781/
ccflags-y += -I$(MTK_TOP)/base/power/ppm_v3/src/mach/$(MTK_PLATFORM)/
ccflags-y += -I$(MTK_TOP)/base/power/ppm_v3/inc/
ccflags-y += -I$(FPSGO_TOP)/
ccflags-y += -I$(EARA_THRM_TOP)/include/
ccflags-y += -I$(MTK_TOP)/thermal/$(MTK_PLATFORM)/inc/mach/
ccflags-y += -I$(FPSGO_TOP)/fstb/

ifeq ($(CONFIG_MTK_APUSYS_SUPPORT),y)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/$(MTK_PLATFORM)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/
else
ccflags-y += -I$(MTK_TOP)/vpu/$(MTK_PLATFORM)/
ccflags-y += -I$(MTK_TOP)/mdla/1.0/
endif

ccflags-y += \
        -I$(srctree)/include/ \
        -I$(MTK_TOP)/include/ \
        -I$(MTK_TOP)/include/mt-plat/$(MTK_PLATFORM)/include/mach/ \
        -I$(MTK_TOP)/include/mt-plat/
