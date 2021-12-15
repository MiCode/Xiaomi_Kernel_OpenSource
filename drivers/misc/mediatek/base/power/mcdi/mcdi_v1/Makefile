#
# Copyright (C) 2017 MediaTek Inc.
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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ifeq ($(filter $(CONFIG_MACH_MT6763) $(CONFIG_MACH_MT6758), y), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/cpuidle_v2/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/cpuidle_v3/
endif

ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)$(CONFIG_MTK_TINYSYS_SSPM_PLT_SUPPORT)),)

ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V2)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2/
else ifneq ($(CONFIG_MTK_TINYSYS_SSPM_VERSION),"")
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
endif
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/mcdi_v1/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/$(CONFIG_MTK_PLATFORM)/

obj-y += mtk_mcdi_main.o
obj-y += mtk_mcdi_util.o
obj-y += mtk_mcdi_profile.o
obj-y += mtk_mcdi_cpc.o
obj-y += mtk_mcdi_mcupm.o
