
# Copyright (C) 2016 MediaTek Inc.
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
export EEM_ROOT_DIR ?= $(srctree)/drivers/misc/mediatek/base/power/eem_v2
ifeq ($(CONFIG_MACH_MT6739),y)
obj-y += mt6739/
else
ifeq ($(CONFIG_MACH_MT6768),y)
obj-y += mt6768/
else
ifeq ($(CONFIG_MACH_MT6833),y)
obj-y += mt6833/
else
ifeq ($(CONFIG_MACH_MT6877),y)
obj-y += mt6877/
else
ifeq ($(CONFIG_MACH_MT6781),y)
obj-y += mt6781/
else
ifeq ($(CONFIG_MACH_MT6885),y)
obj-y += mt6885/
else
ifeq ($(CONFIG_MACH_MT6893),y)
obj-y += mt6893/
else
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif
endif
endif
endif
endif
endif
endif
