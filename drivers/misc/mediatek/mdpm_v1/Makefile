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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pbm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/spm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mdpm_v1/inc/

obj-$(CONFIG_MTK_MDPM_LEGACY) += mtk_mdpm_v1.o
mtk_mdpm_v1-y += mtk_mdpm_common.o

ifneq ($(strip $(CONFIG_MTK_PLAT_POWER_MT6765)),)
mtk_mdpm_v1-y += mtk_mdpm_platform_6765.o
endif


