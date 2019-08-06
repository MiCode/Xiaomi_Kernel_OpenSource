
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

ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/	\
	-I$(srctree)/drivers/misc/mediatek/base/power/include/	\
	-I$(srctree)/drivers/misc/mediatek/base/power/include/swpm_v1/			\
	-I$(srctree)/drivers/misc/mediatek/base/power/swpm_v1/$(CONFIG_MTK_PLATFORM)/	\
	-I$(srctree)/drivers/misc/mediatek/dramc/$(CONFIG_MTK_PLATFORM)/		\

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_PLATFORM)
endif
ifeq ($(CONFIG_MTK_GPU_SWPM_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/gpu/gpu_mali/mali_bifrost/mali-r16p0/drivers/gpu/arm/midgard/platform/mtk_platform_common/
endif
ifneq ($(CONFIG_FPGA_EARLY_PORTING),y)
obj-y += mtk_swpm.o
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
else
# dummy driver to avoid build error
obj-y := mtk_swpm_dummy.o
endif
