#
# Copyright (C) 2017 MediaTek Inc.
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

ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE := y
endif

ifeq ($(CONFIG_MACH_MT6833),y)
	PDIR = mt6833
else
	PDIR = $(MTK_PLATFORM)
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(PDIR)/include/mach/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/pbm_v4/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/pbm_v4/$(PDIR)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/thermal/$(PDIR)/inc/

obj-y += mtk_pbm.o
