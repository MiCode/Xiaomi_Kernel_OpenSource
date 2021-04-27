#
# Copyright (C) 2019 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

ccflags-y += -I$(srctree)/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/gpufreq_v1/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/aee/aed/

obj-y += mtk_gpufreq_common.o

# SW ReadyGo, CONFIG_MTK_PLATFORM is family ID
ifeq ($(CONFIG_MACH_MT6893),y)
obj-y += mt6893/
else ifeq ($(CONFIG_MACH_MT6833),y)
obj-y += mt6833/
else ifeq ($(CONFIG_MACH_MT6877),y)
obj-y += mt6877/
else ifeq ($(CONFIG_MACH_MT6781),y)
obj-y += mt6781/
else
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif
