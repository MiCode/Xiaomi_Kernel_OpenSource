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
ccflags-y += -I$(srctree)/drivers/misc/mediatek/thermal/mt6853/inc/mach/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq/v1/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/leakage_table_v2/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/mt6853/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/mt6853/include/mach/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/mt6853/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/gpu/ged/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dbgtop/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dbgtop/mt6853/
ccflags-y += -I$(srctree)/drivers/clk/mediatek/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/ged/include
ccflags-y += -Wno-unused-variable
ccflags-y += -Wno-unused-function

obj-y += mtk_gpufreq_plat.o

ifeq ($(findstring k6853tv1,$(PROJECT_DTB_NAMES)),k6853tv1)
	subdir-ccflags-y = -DK6853TV1
endif

ifeq ($(findstring aging,$(PROJECT_DTB_NAMES)),aging)
	subdir-ccflags-y += -DAGING_LOAD
endif
