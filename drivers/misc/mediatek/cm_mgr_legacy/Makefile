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
	-I$(srctree)/drivers/misc/mediatek/cpufreq_v2/src/platform \
	-I$(srctree)/drivers/misc/mediatek/cpufreq_v2/inc \
	-I$(srctree)/drivers/misc/mediatek/cm_mgr \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat \
	-I$(srctree)/drivers/misc/mediatek/include \
	-I$(srctree)/drivers/misc/mediatek/qos \
	-I$(srctree)/drivers/misc/mediatek/sspm \
	-I$(srctree)/include \

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6761)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/cpufreq_v1/src/mach/mt6761
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/mt6761
endif

ifneq ($(filter y m,$(CONFIG_INTERCONNECT_MTK_EXTENSION)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/
endif

obj-${CONFIG_MTK_CM_MGR_LEGACY} += mtk_cm_mgr.o
mtk_cm_mgr-y = mtk_cm_mgr_common.o
ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6761)),)
mtk_cm_mgr-y += mtk_cm_mgr_mt6761.o
endif

