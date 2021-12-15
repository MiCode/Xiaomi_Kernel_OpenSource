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

subdir-ccflags-y += -Wformat
ifeq ($(CONFIG_MACH_MT6781), y)
MTK_PLATFORM := mt6781
obj-y += $(MTK_PLATFORM)/
else
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

ifeq ($(CONFIG_MACH_MT6785), y)
MTK_ADSP_HW_VER := 0
else ifeq ($(CONFIG_MACH_MT6781), y)
MTK_ADSP_HW_VER := 0
else
MTK_ADSP_HW_VER := 1
endif
subdir-ccflags-y += -DMTK_ADSP_HW_VER=$(MTK_ADSP_HW_VER)
