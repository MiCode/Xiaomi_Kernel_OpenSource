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
	-I$(CPUFREQ_ROOT_DIR)/src/common_v2	\
	-I$(srctree)/drivers/misc/mediatek/include	\
	-I$(srctree)/drivers/misc/mediatek/eem_v2/mt6765/inc	\
	-I$(srctree)/drivers/misc/mediatek/freqhopping	\
	-I$(srctree)/drivers/misc/mediatek/freqhopping/mt6765	\

ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V0)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v0/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/mt6765
ccflags-y += -DUSE_SSPM_VER_V1
endif

#obj-$(CONFIG_MEDIATEK_CPU_DVFS_MT6765) += mtk_cpufreq_platform.o
