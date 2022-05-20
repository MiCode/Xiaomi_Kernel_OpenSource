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
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq/v2/include/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/ged/include/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpueb/include/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/clk/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/power_throttling/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pbm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/aee/aed/

ccflags-y += \
	-Wno-unused-variable \
	-Wno-unused-function

ifeq ($(findstring aging,$(PROJECT_DTB_NAMES)),aging)
	ccflags-y += -DGPUFREQ_AGING_LOAD
endif

mtk_gpufreq_wrapper-y := \
	gpufreq_v2.o \
	gpufreq_history.o \
	gpufreq_debug.o \
	gpu_misc.o

mtk_gpufreq_mt6886-y := \
	gpuppm.o \
	gpufreq_mt6886.o \

mtk_gpufreq_mt6983-y := \
	gpuppm.o \
	gpufreq_mt6983.o

mtk_gpufreq_mt6985-y := \
	gpuppm.o \
	gpufreq_mt6985.o

obj-$(CONFIG_MTK_GPU_SUPPORT) += mtk_gpufreq_wrapper.o
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6886.c),)
obj-$(CONFIG_MTK_GPU_MT6886_SUPPORT) += mtk_gpufreq_mt6886.o
endif
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6983.c),)
obj-$(CONFIG_MTK_GPU_MT6983_SUPPORT) += mtk_gpufreq_mt6983.o
endif
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6985.c),)
obj-$(CONFIG_MTK_GPU_MT6985_SUPPORT) += mtk_gpufreq_mt6985.o
endif
