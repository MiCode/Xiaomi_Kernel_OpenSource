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

ccflags-y += -I$(srctree)/kernel/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcdi/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcdi/include/mcdi_v1/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/cpuidle/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/spm/
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)$(CONFIG_MTK_TINYSYS_SSPM_PLT_SUPPORT)),)
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V0)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v0/
else ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V1)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1/
else ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V2)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v3/
endif
endif

obj-$(CONFIG_MEDIATEK_MCDI) += mcdi.o
mcdi-y += mcdi_v1/mtk_mcdi_main.o
mcdi-y += mcdi_v1/mtk_mcdi_util.o
mcdi-y += mcdi_v1/mtk_mcdi_profile.o
mcdi-y += mcdi_v1/mtk_mcdi_cpc.o
mcdi-y += mcdi_v1/mtk_mcdi_mcupm.o
mcdi-y += mtk_mcdi_governor.o
mcdi-y += mtk_mcdi_governor_lib.o
mcdi-y += mtk_mcdi_governor_hint.o
mcdi-y += mtk_mcdi_api.o

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6768)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcdi/mt6768/
mcdi-y += mt6768/mtk_mcdi_plat.o
mcdi-y += mt6768/mtk_mcdi_state.o

else ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6765)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcdi/common_v2/
mcdi-y += common_v2/mtk_mcdi_plat.o
mcdi-y += common_v2/mtk_mcdi_state.o
endif
