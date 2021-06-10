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

MTK_GPU_VERSION = $(subst ",,$(CONFIG_MTK_GPU_VERSION))
$(info *MTK_GPU_VERSION = $(MTK_GPU_VERSION))

ifeq ($(word 2,$(MTK_GPU_VERSION)),clark)
	obj-y += m1.13ED5776728/
else ifeq ($(word 2,$(MTK_GPU_VERSION)),auckland)
	obj-y += m1.13ED5776728/
endif
