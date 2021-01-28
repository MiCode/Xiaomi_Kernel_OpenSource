#
# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2017 MediaTek Inc.
#
#
ccflags-y +=	\
	-I$(PPM_ROOT_DIR)/inc \
	-I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)/	\
	-I$(srctree)/drivers/misc/mediatek/base/power/include \
	-I$(srctree)/drivers/gpu/mediatek/gpufreq/include

ccflags-y += -I$(srctree)/drivers/misc/mediatek/thermal/$(CONFIG_MTK_PLATFORM)/inc/	\


# platform dependent functions
obj-y += mtk_ppm_platform.o
# power data related
obj-y += mtk_ppm_power_data.o
# limit calculation algorithm
obj-y += mtk_ppm_cobra_algo.o
