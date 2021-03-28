
# Copyright (C) 2016 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#-I$(srctree)/drivers/misc/mediatek/base/power/eem_v2/$(CONFIG_MTK_PLATFORM)/inc/	\
#-I$(srctree)/drivers/misc/mediatek/pmic/include/	\
#

ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/base/power/include/spm_v3/	\
	-I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)\
	-I$(srctree)/drivers/misc/mediatek/base/power/include/	\
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/	\
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/mach	\
	-I$(srctree)/drivers/misc/mediatek/pmic/	\
	-I$(srctree)/drivers/misc/mediatek/pmic/include	\
	-I$(srctree)/drivers/misc/mediatek/pmic/extbuck \
	-I$(srctree)/drivers/misc/mediatek/pmic/include/mt6356	\
	-I$(srctree)/drivers/misc/mediatek/pmic/include/mt6358	\
	-I$(srctree)/drivers/misc/mediatek/thermal/$(CONFIG_MTK_PLATFORM)/inc/mach	\
	-I$(srctree)/include/linux/	\
	-I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)	\
	-I$(EEMGPU_ROOT_DIR)/mt6833/inc	\
	-I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/	\


ifeq ($(PROJECT_DTB_NAMES),mediatek/k6833v1_64_alpha_mcl)
	subdir-ccflags-y += -DMC50_LOAD
endif

ifeq ($(PROJECT_DTB_NAMES),mediatek/k6833v1_64_mcl)
	subdir-ccflags-y += -DMC50_LOAD
endif

ifeq ($(PROJECT_DTB_NAMES),mediatek/k6833v1_64_33m_mcl)
	subdir-ccflags-y += -DMC50_LOAD
endif


obj-y += mtk_eemgpu.o
obj-y += mtk_eemgpu_platform.o
obj-y += mtk_eemgpu_internal.o
