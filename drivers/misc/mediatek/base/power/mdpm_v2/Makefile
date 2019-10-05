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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/spm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/mdpm_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/mdpm_v2/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/

ifeq ($(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES),"mediatek/k6885v1_64_fep")
subdir-ccflags-y += -DDLPT_FEATURE_SUPPORT
endif

obj-y += mtk_mdpm_common.o
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
