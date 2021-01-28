# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2017 MediaTek Inc.
#
#-I$(srctree)/drivers/misc/mediatek/base/power/eem_v2/$(CONFIG_MTK_PLATFORM)/inc/	\
#-I$(srctree)/drivers/misc/mediatek/pmic/include/	\
#

ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/base/power/include/	\
	-I$(srctree)/drivers/misc/mediatek/base/power/include/spm_v3/	\
	-I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)\
    -I$(srctree)/drivers/misc/mediatek/base/power/cpufreq_v1/src/mach/$(CONFIG_MTK_PLATFORM)\
	-I$(srctree)/drivers/misc/mediatek/base/power/gpufreq_mt3967/ \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/	\
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/mach	\
	-I$(srctree)/drivers/misc/mediatek/pmic/	\
	-I$(srctree)/drivers/misc/mediatek/pmic/include	\
	-I$(srctree)/drivers/misc/mediatek/pmic/extbuck \
	-I$(srctree)/drivers/misc/mediatek/pmic/include/mt6356	\
	-I$(srctree)/drivers/misc/mediatek/pmic/include/mt6358	\
	-I$(srctree)/drivers/misc/mediatek/pmic/include/mt6355	\
	-I$(srctree)/drivers/misc/mediatek/thermal/$(CONFIG_MTK_PLATFORM)/inc/mach	\
	-I$(srctree)/include/linux/	\
	-I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)	\
	-I$(srctree)/drivers/misc/mediatek/vpu/$(MTK_PLATFORM)	\
	-I$(EEM_ROOT_DIR)/$(CONFIG_MTK_PLATFORM)/inc	\
	-I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/	\
	-I$(srctree)/drivers/gpu/mediatek/gpufreq/include/

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_PLATFORM)
endif

# HQA load
#ifeq ($(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES),"mediatek/evb6779_64_hqact")
#subdir-ccflags-y += -DCORN_LOAD
#endif

# HQA load
#ifeq ($(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES),"mediatek/k79v1_64_ctighten")
#subdir-ccflags-y += -DCORN_LOAD
#endif
subdir-ccflags-y += -DCORN_LOAD

obj-y += mtk_eem.o
obj-y += mtk_eem_platform.o
obj-y += mtk_eem_internal.o
obj-y += mtk_eem_api.o

