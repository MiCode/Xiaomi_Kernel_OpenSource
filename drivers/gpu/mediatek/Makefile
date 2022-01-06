#
# Copyright (C) 2015 MediaTek Inc.
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

MTK_GPU_VERSION := $(subst ",,$(CONFIG_MTK_GPU_VERSION))
export MTK_GPU_VERSION
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
export MTK_PLATFORM

ifneq ($(CONFIG_MTK_GPU_SUPPORT),)
	obj-y += hal/
	obj-y += gpufreq/
	obj-y += ged/
ifneq (, $(findstring $(CONFIG_MTK_PLATFORM), "mt6779"))
	obj-y += gpu_bm/
$(info *GPU defined GPU BM QoS 2.0-419)
endif
ifneq (, $(findstring $(CONFIG_MTK_PLATFORM), "mt6885" "mt6873" "mt6853"))
	obj-y += gpu_bm_k414/
$(info *GPU defined GPU BM QoS 2.0-414)
endif

# GPU R5P0
ifeq ($(CONFIG_MTK_GPU_R5P0),y)
    obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
else ifeq ($(CONFIG_MACH_MT6781),y)
    $(info *direct build mali-r32p1 for CONFIG_MACH_MT6781)
    obj-y += gpu_mali/mali_valhall/mali-r32p1/drivers/gpu/arm/midgard/
else ifneq ($(word 1,$(MTK_GPU_VERSION)),)
    obj-y += gpu_$(word 1,$(MTK_GPU_VERSION))/
else
    obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif
endif

ifeq ($(CONFIG_GPU_VERSION_MALI_MIDGARD_R26P0),y)
ifneq ($(CONFIG_MTK_GPU_SUPPORT),m)
	obj-y += hal/
	obj-y += gpu_mali/
endif
endif
