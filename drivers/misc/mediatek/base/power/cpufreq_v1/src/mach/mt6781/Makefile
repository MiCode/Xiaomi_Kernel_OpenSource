#
# Copyright (C) 2016 MediaTek Inc.
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
ifeq ($(CONFIG_MACH_MT6781),y)
ccflags-y += \
        -I$(CPUFREQ_ROOT_DIR)/inc \
        -I$(CPUFREQ_ROOT_DIR)/src/mach/mt6781 \
        -I$(srctree)/drivers/misc/mediatek/base/power/include \
        -I$(srctree)/drivers/misc/mediatek/base/power/eem_v2/mt6781/inc \
        -I$(srctree)/drivers/misc/mediatek/freqhopping \
        -I$(srctree)/drivers/misc/mediatek/freqhopping/mt6781
else
ccflags-y += \
	-I$(CPUFREQ_ROOT_DIR)/inc \
	-I$(CPUFREQ_ROOT_DIR)/src/mach/$(MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/base/power/include \
	-I$(srctree)/drivers/misc/mediatek/base/power/eem_v2/$(MTK_PLATFORM)/inc \
	-I$(srctree)/drivers/misc/mediatek/freqhopping \
	-I$(srctree)/drivers/misc/mediatek/freqhopping/$(MTK_PLATFORM)
endif
SSPM_VER := v2
obj-y += mtk_cpufreq_platform.o
