
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
	-I$(EEM_ROOT_DIR)/$(CONFIG_MTK_PLATFORM)/inc	\
	-I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/	\

ifeq ($(findstring _aging,$(PROJECT_DTB_NAMES)),_aging)
subdir-ccflags-y += -DAGING_LOAD
endif

ifeq ($(PROJECT_DTB_NAMES),mediatek/k6885v1_64_alpha_mcl50)
subdir-ccflags-y += -DMT6885
subdir-ccflags-y += -DMC50_LOAD
endif

ifeq ($(PROJECT_DTB_NAMES),mediatek/k6889v1_64_mcl50)
subdir-ccflags-y += -DMT6889
subdir-ccflags-y += -DMC50_LOAD
endif

ifeq ($(PROJECT_DTB_NAMES),mediatek/k6889v1_64_90hz_mcl50)
subdir-ccflags-y += -DMT6889
subdir-ccflags-y += -DMC50_LOAD
endif

ifeq ($(PROJECT_DTB_NAMES),mediatek/k6885v1_64_alpha_mcl50glt)
subdir-ccflags-y += -DMT6885
subdir-ccflags-y += -DMC50_LOAD
endif

ifeq ($(PROJECT_DTB_NAMES),mediatek/k6889v1_64_mcl50glt)
subdir-ccflags-y += -DMT6889
subdir-ccflags-y += -DMC50_LOAD
endif

ifeq ($(PROJECT_DTB_NAMES),mediatek/k6889v1_64_90hz_mcl50glt)
subdir-ccflags-y += -DMT6889
subdir-ccflags-y += -DMC50_LOAD
endif

ifeq ($(PROJECT_DTB_NAMES),mediatek/k6885v1_64_r21)
subdir-ccflags-y += -DMT6885
subdir-ccflags-y += -DMC50_LOAD
endif

ifeq ($(PROJECT_DTB_NAMES),mediatek/k6889v1_64_r21)
subdir-ccflags-y += -DMT6889
subdir-ccflags-y += -DMC50_LOAD
endif

ifeq ($(PROJECT_DTB_NAMES),mediatek/k6883v1_64_mcl50)
subdir-ccflags-y += -DMT6885
subdir-ccflags-y += -DMC50_LOAD
endif

ifeq ($(PROJECT_DTB_NAMES),mediatek/k6883v1_64_mcl50glt)
subdir-ccflags-y += -DMT6885
subdir-ccflags-y += -DMC50_LOAD
endif

ifeq ($(PROJECT_DTB_NAMES),mediatek/k6889v1_64_120hz)
subdir-ccflags-y += -DMT6889
subdir-ccflags-y += -DMC50_LOAD
endif

ifeq ($(PROJECT_DTB_NAMES),mediatek/k6889v1_64_120hz_vdo)
subdir-ccflags-y += -DMT6889
subdir-ccflags-y += -DMC50_LOAD
endif

ifeq ($(PROJECT_DTB_NAMES),mediatek/k6885v1_64_mcl50)
subdir-ccflags-y += -DMT6885
subdir-ccflags-y += -DMC50_LOAD
endif

ifeq ($(PROJECT_DTB_NAMES),mediatek/k6889v1_64_120hz_no_switch_mipi_clk)
subdir-ccflags-y += -DMT6889
subdir-ccflags-y += -DMC50_LOAD
endif

obj-y += mtk_eem.o
obj-y += mtk_eem_platform.o
obj-y += mtk_eem_internal.o
obj-y += mtk_eem_api.o

ifeq ($(CONFIG_MTK_BASE_POWER), y)
obj-y += mtk_picachu.o
endif

