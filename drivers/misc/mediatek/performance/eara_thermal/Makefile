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
EARA_THRM_TOP = $(srctree)/drivers/misc/mediatek/performance/eara_thermal
FPSGO_TOP = $(srctree)/drivers/misc/mediatek/performance/fpsgo_v3/

obj-y += thermal_budget.o

ccflags-y += -I$(MTK_TOP)/thermal/common/inc/
ccflags-y += -I$(MTK_TOP)/thermal/$(MTK_PLATFORM)/inc/
ccflags-y += -I$(MTK_TOP)/base/power/$(MTK_PLATFORM)/
ccflags-y += -I$(MTK_TOP)/base/power/include/
ccflags-y += -I$(MTK_TOP)/base/power/ppm_v3/src/mach/$(MTK_PLATFORM)/
ccflags-y += -I$(MTK_TOP)/base/power/ppm_v3/inc/
ccflags-y += -I$(FPSGO_TOP)/
ccflags-y += -I$(EARA_THRM_TOP)/platform/$(MTK_PLATFORM)/
ccflags-y += -I$(MTK_TOP)/vpu/$(MTK_PLATFORM)/
ccflags-y += -I$(MTK_TOP)/mdla/1.0/
ccflags-y += -I$(MTK_TOP)/performance/resym/

ccflags-y += \
        -I$(srctree)/include/ \
        -I$(MTK_TOP)/include/ \
        -I$(MTK_TOP)/include/mt-plat/$(MTK_PLATFORM)/include/mach/
