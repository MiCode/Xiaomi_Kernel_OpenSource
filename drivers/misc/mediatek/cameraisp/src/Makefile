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

ifeq ($(CONFIG_MACH_MT6739),y)
  obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

ifeq ($(CONFIG_MACH_MT6761),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_CAMERA_SUPPORT) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

ifeq ($(CONFIG_MACH_MT6779),y)
  obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
# obj-y += fdvt/
endif

ifeq ($(CONFIG_MACH_MT6765),y)
  obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
#  obj-y += fdvt/
endif

ifeq ($(CONFIG_MACH_MT6768),y)
  obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
#  obj-y += fdvt/
endif

ifeq ($(CONFIG_MACH_MT6885),y)
  obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

ifeq ($(CONFIG_MACH_MT6893),y)
  obj-y += mt6885/
endif

ifeq ($(CONFIG_MACH_MT6873),y)
  obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

ifeq (y, $(filter y, $(CONFIG_MACH_MT6833) $(CONFIG_MACH_MT6853) $(CONFIG_MACH_MT6877)))
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

ifeq (y, $(filter y, $(CONFIG_MACH_MT6833) $(CONFIG_MACH_MT6853) $(CONFIG_MACH_MT6781)))
  subdir-ccflags-y += -DDISABLE_SV_TOP0
endif

ifeq ($(CONFIG_MACH_MT6781),y)
  obj-y += mt6853/
  subdir-ccflags-y += -DDISABLE_CCU
endif
