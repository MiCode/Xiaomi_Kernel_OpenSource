#
# Copyright (C) 2019 MediaTek Inc.
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
ifeq ($(CONFIG_MACH_MT6877),y)
		PDIR = mt6877
else ifeq ($(CONFIG_MACH_MT6833),y)
		PDIR = mt6833
else
		PDIR = $(CONFIG_MTK_PLATFORM)
endif
ccflags-y +=	\
	-I$(PPM_ROOT_DIR)/inc \
	-I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)/	\
	-I$(srctree)/drivers/misc/mediatek/thermal/$(PDIR)/inc/  \
	-I$(srctree)/drivers/gpu/mediatek/gpufreq/   \
	-I$(srctree)/drivers/misc/mediatek/base/power/include

# platform dependent functions
obj-y += mtk_ppm_platform.o
# power data related
obj-y += mtk_ppm_power_data.o
# limit calculation algorithm
obj-y += mtk_ppm_cobra_algo.o
