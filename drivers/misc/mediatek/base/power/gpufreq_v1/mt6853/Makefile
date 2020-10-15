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
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/gpufreq_v1/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/leakage_table_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/thermal/$(MTK_PLATFORM)/inc/mach/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/$(CONFIG_MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/gpu/ged/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dbgtop/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dbgtop/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/clk/mediatek/

ccflags-y += -Wno-unused-variable
ccflags-y += -Wno-unused-function

obj-y += mtk_gpufreq_plat.o

ifeq ($(findstring k6853tv1,$(PROJECT_DTB_NAMES)),k6853tv1)
	subdir-ccflags-y = -DK6853TV1
endif

ifeq ($(findstring aging,$(PROJECT_DTB_NAMES)),aging)
	subdir-ccflags-y += -DAGING_LOAD
endif

ifeq ($(findstring k6853v1_64_swrgo,$(PROJECT_DTB_NAMES)),k6853v1_64_swrgo)
	subdir-ccflags-y += -DK6853V1_64_SWRGO
endif

ifeq ($(findstring k6853v1_64_swrgo_5gc,$(PROJECT_DTB_NAMES)),k6853v1_64_swrgo_5gc)
	subdir-ccflags-y += -DK6853V1_64_SWRGO_5GC
endif
