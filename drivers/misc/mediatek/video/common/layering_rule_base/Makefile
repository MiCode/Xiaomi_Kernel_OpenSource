#
# Copyright (C) 2017 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#


# remove this after k79 config CONFIG_MACH_MT6779 & CONFIG_MTK_PLATFORM in defconfig
ifeq ($(CONFIG_MTK_DISP_PLATFORM), "mt6779")
	CONFIG_MACH_MT6779 := y
	MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_DISP_PLATFORM))
endif

# for legacy chip, use v1
ifeq (y, $(filter y, $(CONFIG_MACH_MT6763) $(CONFIG_MACH_MT6799) $(CONFIG_MACH_MT6758)))
	obj-y += v1/
else
ifeq (y, $(filter y, $(CONFIG_MACH_MT6779)))
	obj-y += v1.2/
else
	obj-y += v1.1/
endif
endif
