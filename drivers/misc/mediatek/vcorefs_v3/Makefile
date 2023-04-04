#
# Copyright (C) 2015 MediaTek Inc.
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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/irq/mt6739/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/mt6739/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/power/mt6739/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/spm_v4/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/eem_v1/inc/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/mt6739/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/mt6739/videox/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/mt6739/dispsys/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs/
ccflags-y += -I$(srctree)/drivers/devfreq/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ifneq ($(CONFIG_MTK_TINYSYS_SSPM_VERSION),"")
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_TINYSYS_SSPM_VERSION)
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/mt6739/
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/vcorefs_v3/

obj-$(CONFIG_MTK_VCOREFS_LEGACY) += vcorefs_v3.o
vcorefs_v3-y += mtk_vcorefs_manager.o
vcorefs_v3-y += mtk_vcorefs_governor.o

