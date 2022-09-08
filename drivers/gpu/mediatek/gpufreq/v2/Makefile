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
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

ccflags-y += \
	-Wno-unused-variable \
	-Wno-unused-function

ifeq ($(findstring aging,$(PROJECT_DTB_NAMES)),aging)
	ccflags-y += -DGPUFREQ_AGING_LOAD
endif

ifeq ($(CONFIG_MTK_GPU_MT6768_SUPPORT)$(CONFIG_MTK_GPU_MT6765_SUPPORT),)
ccflags-y += -DMTK_GPU_EB_SUPPORT
endif


mtk_gpufreq_wrapper-y := \
	gpufreq_v2.o \
	gpufreq_debug.o \
	gpu_misc.o

mtk_gpufreq_mt6789-y := \
	gpufreq_history_common.o \
	gpufreq_history_mt6789.o \
	gpuppm.o \
	gpufreq_mt6789.o \
	gpudfd_mt6789.o

mtk_gpufreq_mt6833-y := \
	gpufreq_history_common.o \
	gpuppm.o \
	gpufreq_mt6833.o \
	gpudfd_mt6833.o

mtk_gpufreq_mt6855-y := \
	gpufreq_history_common.o \
	gpuppm.o \
	gpufreq_mt6855.o \
	gpudfd_mt6855.o

mtk_gpufreq_mt6879-y := \
	gpufreq_history_common.o \
	gpuppm.o \
	gpufreq_mt6879.o \
	gpudfd_mt6879.o

mtk_gpufreq_mt6893-y := \
	gpufreq_history_common.o \
	gpuppm.o \
	gpufreq_mt6893.o \
	gpudfd_mt6893.o

mtk_gpufreq_mt6895-y := \
	gpufreq_history_common.o \
	gpuppm.o \
	gpufreq_mt6895.o \
	gpudfd_mt6895.o

mtk_gpufreq_mt6983-y := \
	gpufreq_history_common.o \
	gpuppm.o \
	gpufreq_mt6983.o \
	gpudfd_mt6983.o

mtk_gpufreq_mt6768-y := \
	gpufreq_history_common.o \
	gpuppm.o \
	gpufreq_mt6768.o

mtk_gpufreq_mt6765-y := \
	gpufreq_history_common.o \
	gpuppm.o \
	gpufreq_mt6765.o

obj-$(CONFIG_MTK_GPU_SUPPORT) += mtk_gpufreq_wrapper.o
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6789.c),)
ifneq ($(wildcard $(srctree)/$(src)/gpudfd_mt6789.c),)
obj-$(CONFIG_MTK_GPU_MT6789_SUPPORT) += mtk_gpufreq_mt6789.o
endif
endif
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6833.c),)
ifneq ($(wildcard $(srctree)/$(src)/gpudfd_mt6833.c),)
obj-$(CONFIG_MTK_GPU_MT6833_SUPPORT) += mtk_gpufreq_mt6833.o
endif
endif
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6855.c),)
ifneq ($(wildcard $(srctree)/$(src)/gpudfd_mt6855.c),)
obj-$(CONFIG_MTK_GPU_MT6855_SUPPORT) += mtk_gpufreq_mt6855.o
endif
endif
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6879.c),)
ifneq ($(wildcard $(srctree)/$(src)/gpudfd_mt6879.c),)
obj-$(CONFIG_MTK_GPU_MT6879_SUPPORT) += mtk_gpufreq_mt6879.o
endif
endif
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6893.c),)
ifneq ($(wildcard $(srctree)/$(src)/gpudfd_mt6893.c),)
obj-$(CONFIG_MTK_GPU_MT6893_SUPPORT) += mtk_gpufreq_mt6893.o
endif
endif
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6895.c),)
ifneq ($(wildcard $(srctree)/$(src)/gpudfd_mt6895.c),)
obj-$(CONFIG_MTK_GPU_MT6895_SUPPORT) += mtk_gpufreq_mt6895.o
endif
endif
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6983.c),)
ifneq ($(wildcard $(srctree)/$(src)/gpudfd_mt6983.c),)
obj-$(CONFIG_MTK_GPU_MT6983_SUPPORT) += mtk_gpufreq_mt6983.o
endif
endif
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6768.c),)
obj-$(CONFIG_MTK_GPU_MT6768_SUPPORT) += mtk_gpufreq_mt6768.o
endif
ifneq ($(wildcard $(srctree)/$(src)/gpufreq_mt6765.c),)
obj-$(CONFIG_MTK_GPU_MT6765_SUPPORT) += mtk_gpufreq_mt6765.o
endif
