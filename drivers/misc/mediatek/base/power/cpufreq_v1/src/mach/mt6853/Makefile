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
	-I$(CPUFREQ_ROOT_DIR)/src/mach/$(CONFIG_MTK_PLATFORM)	\
	-I$(srctree)/drivers/misc/mediatek/base/power/include	\
	-I$(srctree)/drivers/misc/mediatek/base/power/eem_v2/$(CONFIG_MTK_PLATFORM)/inc	\
	-I$(srctree)/drivers/misc/mediatek/freqhopping	\
	-I$(srctree)/drivers/misc/mediatek/freqhopping/$(CONFIG_MTK_PLATFORM)

ifeq ($(findstring _qea,$(PROJECT_DTB_NAMES)),_qea)
subdir-ccflags-y = -DQEA
endif

ifeq ($(findstring k6853tv1,$(PROJECT_DTB_NAMES)),k6853tv1)
subdir-ccflags-y = -DK6853TV1
endif

ifeq ($(findstring _turbo,$(PROJECT_DTB_NAMES)),_turbo)
subdir-ccflags-y = -DTURBO
endif

ifeq ($(findstring k6853v1_64_swrgo_5gc,$(PROJECT_DTB_NAMES)),k6853v1_64_swrgo_5gc)
subdir-ccflags-y = -DTURBO
endif

obj-y += mtk_cpufreq_platform.o
