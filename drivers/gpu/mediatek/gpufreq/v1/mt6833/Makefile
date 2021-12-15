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

# SW ReadyGo, need to include mt6833/ prior to mt6853/ if mt6833/ exists
ifeq ($(CONFIG_MACH_MT6833),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/thermal/mt6833/inc/mach/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/mt6833/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dbgtop/mt6833/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/mt6833/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/mt6833/include/mach/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/thermal/mt6833/
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq/v1/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/leakage_table_v2/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/$(CONFIG_MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/ged/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dbgtop/
ccflags-y += -I$(srctree)/drivers/clk/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/ged/include

ccflags-y += -Wno-unused-variable
ccflags-y += -Wno-unused-function

obj-y += mtk_gpufreq_plat.o

ifeq ($(findstring aging,$(PROJECT_DTB_NAMES)),aging)
	subdir-ccflags-y += -DAGING_LOAD
endif

ifeq ($(findstring k6833v1_64_33m,$(PROJECT_DTB_NAMES)),k6833v1_64_33m)
	subdir-ccflags-y += -DK6833V1_64_33M
endif
