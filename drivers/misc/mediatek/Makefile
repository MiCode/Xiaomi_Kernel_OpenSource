#
# Copyright (C) 2018 MediaTek Inc.
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

#$(call all-subdir-src-or-makefile)
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
subdir-ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/$(MTK_PLATFORM)


obj-$(CONFIG_MTK_BASE_POWER) += base/
obj-$(CONFIG_MTK_PMIC_NEW_ARCH)	+= pmic/
obj-$(CONFIG_MTK_M4U)   += m4u/

obj-$(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM) += trusted_mem/
obj-y += atf/
obj-y += devinfo/
obj-y += sspm/
obj-y += timer/
obj-$(CONFIG_MTK_ACCDET) += accdet/
obj-$(CONFIG_MTK_GIC_EXT) += ext_gic/
obj-$(CONFIG_MTK_FREQ_HOPPING) += freqhopping/
obj-$(CONFIG_MTK_LENS)   += lens/
obj-$(CONFIG_MTK_FLASHLIGHT) += flashlight/
obj-$(CONFIG_RT_FLASHLIGHT)     += flashlight/richtek/

obj-$(CONFIG_MTK_MEM) += mem/
obj-$(CONFIG_MTK_MEMORY_LOWPOWER) += memory-lowpower/
obj-y += memory-amms/
obj-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp/
obj-$(CONFIG_MTK_SSMR) += memory-ssmr/
obj-$(CONFIG_ZONE_MOVABLE_CMA) += zone_movable_cma/
obj-$(CONFIG_MTK_VIBRATOR) += vibrator/
obj-$(CONFIG_MTK_BOOT) += boot/
obj-$(CONFIG_MTK_BOOT_REASON) += boot_reason/
obj-$(CONFIG_MTK_CHIP) += chip/
obj-$(CONFIG_MTK_RAM_CONSOLE) += ram_console/
obj-$(CONFIG_MU3_PHY)   += mu3phy/
obj-$(CONFIG_USB_MU3D_DRV)  += mu3d/
