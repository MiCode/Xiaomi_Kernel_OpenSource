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

ifeq (y, $(filter y, $(CONFIG_MACH_MT6765) $(CONFIG_MACH_MT6761) $(CONFIG_MACH_MT6779)))

cflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/pbm_v4/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/pbm_v4/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/thermal/$(MTK_PLATFORM)/inc/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/power_throttling/
obj-$(CONFIG_MTK_PBM) += mtk_pbm.o

else

ifeq ($(CONFIG_MACH_MT6833),y)
	PDIR = mt6833
else ifeq ($(CONFIG_MACH_MT6893),y)
	PDIR = mt6893
else
	PDIR = $(MTK_PLATFORM)
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/pbm_v4/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/pbm_v4/$(PDIR)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/thermal/$(PDIR)/inc/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/power_throttling/
obj-y += mtk_pbm.o

endif
