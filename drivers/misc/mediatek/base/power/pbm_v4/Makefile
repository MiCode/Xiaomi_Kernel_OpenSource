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

ifeq ($(CONFIG_MACH_MT6781),y)
MTK_PLATFORM = $(CONFIG_MTK_PLATFORM)
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/pbm_v4/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/pbm_v4/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/thermal/$(MTK_PLATFORM)/inc/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/power_throttling/

ifeq ($(CONFIG_MACH_MT6877),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/mach/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/pbm_v4/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/pbm_v4/$(CONFIG_MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/thermal/mt6877/inc/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/power_throttling/
obj-y += mtk_pbm.o

else ifeq ($(CONFIG_MACH_MT6833),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/mach/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/pbm_v4/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/pbm_v4/$(CONFIG_MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/thermal/mt6833/inc/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/power_throttling/
obj-y += mtk_pbm.o

else
obj-$(CONFIG_MTK_PBM) += mtk_pbm.o
endif
