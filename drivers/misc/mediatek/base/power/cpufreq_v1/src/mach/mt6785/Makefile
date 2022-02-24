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

ccflags-y +=	\
	-I$(CPUFREQ_ROOT_DIR)/inc	\
	-I$(CPUFREQ_ROOT_DIR)/src/mach/mt6785	\
	-I$(srctree)/drivers/misc/mediatek/base/power/include	\
	-I$(srctree)/drivers/misc/mediatek/base/power/eem_v2/mt6785/inc	\
	-I$(srctree)/drivers/misc/mediatek/freqhopping	\
	-I$(srctree)/drivers/misc/mediatek/freqhopping/mt6785

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/mt6785
ccflags-y += -DUSE_SSPM_VER_V1
endif

obj-y += mtk_cpufreq_platform.o
