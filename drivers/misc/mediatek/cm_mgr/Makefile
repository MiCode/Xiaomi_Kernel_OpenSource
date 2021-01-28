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

ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/base/power/cpufreq_v1/inc \
	-I$(srctree)/drivers/misc/mediatek/base/power/cpufreq_v1/src/mach/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/cm_mgr/ \
	-I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat \
	-I$(srctree)/drivers/misc/mediatek/qos \
	-I$(srctree)/drivers/misc/mediatek/sspm \
	-I$(srctree)/include \

obj-${CONFIG_MTK_CM_MGR} += mtk_cm_mgr.o
mtk_cm_mgr-y = mtk_cm_mgr_common.o

mtk_cm_mgr-${CONFIG_MTK_CM_MGR_MT6779} += mtk_cm_mgr_mt6779.o
mtk_cm_mgr-${CONFIG_MACH_MT6761} += mtk_cm_mgr_mt6761.o
mtk_cm_mgr-${CONFIG_MACH_MT6765} += mtk_cm_mgr_mt6765.o

