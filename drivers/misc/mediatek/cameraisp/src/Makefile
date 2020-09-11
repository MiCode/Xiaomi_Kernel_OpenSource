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

obj-y := cameraisp_dummy.o

ifeq ($(CONFIG_MACH_MT6775),y)
  obj-y += isp_50/
endif

ifeq ($(CONFIG_MACH_MT6739),y)
  obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

ifeq ($(CONFIG_MACH_MT6885),y)
  obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

ifeq ($(CONFIG_MACH_MT6873),y)
  obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

ifeq ($(CONFIG_MACH_MT6771),y)
  obj-y += isp_50/
endif
