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

obj-y += ged/
obj-y += hal/

MTK_GPU_VERSION = $(subst ",,$(CONFIG_MTK_GPU_VERSION))
$(info MTK_GPU_VERSION 1 = $(word 1,$(MTK_GPU_VERSION)))
# GPU R5P0
ifeq ($(CONFIG_MTK_GPU_R5P0),y)
    obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
else ifneq ($(word 1,$(MTK_GPU_VERSION)),)
    obj-y += gpu_$(word 1,$(MTK_GPU_VERSION))/
else
    obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6785"))
    obj-y += gpu_bm/
endif

obj-$(CONFIG_MACH_MT6885) += gpu_bm/
obj-$(CONFIG_MACH_MT6893) += gpu_bm/
obj-$(CONFIG_MACH_MT6873) += gpu_bm/
obj-$(CONFIG_MACH_MT6853) += gpu_bm/
