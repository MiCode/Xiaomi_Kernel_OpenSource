
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

ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/	\
	-I$(srctree)/drivers/misc/mediatek/base/power/include/	\
	-I$(srctree)/drivers/misc/mediatek/base/power/include/swpm_v1/			\
	-I$(srctree)/drivers/misc/mediatek/dramc/$(CONFIG_MTK_PLATFORM)/		\

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_PLATFORM)/
endif
ifeq ($(CONFIG_MTK_GPU_SWPM_SUPPORT), y)
MTK_GPU_VERSION = $(subst ",,$(CONFIG_MTK_GPU_VERSION))
$(info *MTK_GPU_VERSION 2 = $(word 2,$(MTK_GPU_VERSION)))
$(info *MTK_GPU_VERSION 3 = $(word 3,$(MTK_GPU_VERSION)))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/gpu/gpu_mali/mali_$(word 2,$(MTK_GPU_VERSION))/mali-$(word 3,$(MTK_GPU_VERSION))/drivers/gpu/arm/midgard/platform/mtk_platform_common/
endif

ifeq ($(CONFIG_MACH_MT6833), y)
ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/base/power/swpm_v1/mt6833/
	obj-y += $(subst ",,mt6833)/
else ifeq ($(CONFIG_MACH_MT6893), y)
ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/base/power/swpm_v1/mt6893/
	obj-y += $(subst ",,mt6893)/
else
ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/base/power/swpm_v1/$(CONFIG_MTK_PLATFORM)/
	obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

ifneq ($(CONFIG_FPGA_EARLY_PORTING),y)
obj-y += mtk_swpm.o
obj-y += mtk_swpm_common.o
endif
