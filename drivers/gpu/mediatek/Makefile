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

ifneq ($(CONFIG_MTK_GPU_VERSION),"")
MTK_GPU_VERSION = $(subst ",,$(CONFIG_MTK_GPU_VERSION))

$(info MTK_GPU_VERSION = $(MTK_GPU_VERSION))

ifneq ($(CONFIG_MTK_PLATFORM),)
export MTK_PLATFORM = $(subst ",,$(CONFIG_MTK_PLATFORM))
else
export MTK_PLATFORM = $(word 4,$(MTK_GPU_VERSION))
endif

ifeq ($(MTK_PLATFORM),mt6873)
subdir-ccflags-y += -DCONFIG_GPU_MT6873
else ifeq ($(MTK_PLATFORM),mt6893)
subdir-ccflags-y += -DCONFIG_GPU_MT6893
else ifeq ($(MTK_PLATFORM),mt6853)
subdir-ccflags-y += -DCONFIG_GPU_MT6853
endif
endif

ifeq ($(CONFIG_MTK_GPU_MT6768_SUPPORT)$(CONFIG_MTK_GPU_MT6765_SUPPORT),)
    obj-y += gpu_bm/
    obj-y += gpueb/
endif


ifneq ($(CONFIG_MTK_GPU_SUPPORT),)
    obj-y += ged/
    obj-y += hal/
    obj-y += gpufreq/


ifneq ($(CONFIG_MTK_GPU_VERSION),"")
# GPU R5P0
ifeq ($(CONFIG_MTK_GPU_R5P0),y)
    obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
else ifneq ($(word 1,$(MTK_GPU_VERSION)),)
    obj-y += gpu_$(word 1,$(MTK_GPU_VERSION))/
else
    obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif
endif
endif