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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/power_gs_v1/

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6761)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/power_gs_v1/mt6761/
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6739)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/power_gs_v1/mt6739/
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

obj-$(CONFIG_MTK_POWER_GS_LEGACY) += mtk_power_gs_v1.o
mtk_power_gs_v1-y += mtk_power_gs.o
mtk_power_gs_v1-y += mtk_power_gs_api.o

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6768)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/power_gs_v1/mt6768/
mtk_power_gs_v1-y += mt6768/mtk_power_gs_6358_array.o
mtk_power_gs_v1-y += mt6768/mtk_cg_array.o
mtk_power_gs_v1-y += mt6768/mtk_dcm_array.o
mtk_power_gs_v1-y += mt6768/mtk_power_gs_sp.o
mtk_power_gs_v1-y += mt6768/mtk_power_gs_compare.o
mtk_power_gs_v1-y += mt6768/mtk_power_gs_dump_pmic.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6761)),)
mtk_power_gs_v1-y += mt6761/mtk_cg_array.o
mtk_power_gs_v1-y += mt6761/mtk_dcm_array.o
mtk_power_gs_v1-y += mt6761/mtk_power_gs_sp.o
mtk_power_gs_v1-y += mt6761/mtk_power_gs_compare.o
mtk_power_gs_v1-y += mt6761/mtk_power_gs_dump_pmic.o
mtk_power_gs_v1-y += mt6761/mtk_power_gs_6357_array.o
endif

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6739)),)
mtk_power_gs_v1-y += mt6739/mtk_cg_array.o
mtk_power_gs_v1-y += mt6739/mtk_dcm_array.o
mtk_power_gs_v1-y += mt6739/mtk_power_gs_sp.o
mtk_power_gs_v1-y += mt6739/mtk_power_gs_compare.o
mtk_power_gs_v1-y += mt6739/mtk_power_gs_dump_pmic.o
mtk_power_gs_v1-y += mt6739/mtk_power_gs_6357_array.o
endif

ifneq ($(strip $(CONFIG_MTK_POWER_GS_LEGACY_V2)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/power_gs_v1/common_v2/
obj-$(CONFIG_MTK_POWER_GS_LEGACY_V2) += mtk_power_gs_v2.o
mtk_power_gs_v2-y += mtk_power_gs.o
mtk_power_gs_v2-y += mtk_power_gs_api.o
mtk_power_gs_v2-y += common_v2/mtk_power_gs_6355_array.o
mtk_power_gs_v2-y += common_v2/mtk_cg_array.o
mtk_power_gs_v2-y += common_v2/mtk_dcm_array.o
mtk_power_gs_v2-y += common_v2/mtk_power_gs_sp.o
mtk_power_gs_v2-y += common_v2/mtk_power_gs_compare.o
mtk_power_gs_v2-y += common_v2/mtk_power_gs_dump_pmic.o
endif
