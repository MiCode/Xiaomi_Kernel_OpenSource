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

export MTK_GPU_VERSION := mali valhall r30p0 mt6893
$(info MTK_GPU_VERSION = $(MTK_GPU_VERSION))

$(shell ln -snf $(srctree)/$(src)/../gpu_mali/mali_$(word 2,$(MTK_GPU_VERSION)) $(srctree)/$(src)/mali_$(word 2,$(MTK_GPU_VERSION)))
$(info link $(srctree)/$(src)/../gpu_mali/mali_$(word 2,$(MTK_GPU_VERSION)) to $(srctree)/$(src)/mali_$(word 2,$(MTK_GPU_VERSION)))

export MTK_PLATFORM := $(word 4,$(MTK_GPU_VERSION))
subdir-ccflags-y += -DCONFIG_GPU_MT6893

obj-y += mali_$(word 2,$(MTK_GPU_VERSION))/
