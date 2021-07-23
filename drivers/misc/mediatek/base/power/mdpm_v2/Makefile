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

ifeq ($(CONFIG_MACH_MT6833),y)
	PDIR = mt6833
else
	PDIR = $(MTK_PLATFORM)
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/spm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/mdpm_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/mdpm_v2/$(PDIR)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(PDIR)/include/mach/

obj-y += mtk_mdpm_common.o
obj-y += $(subst ",,$(PDIR))/
