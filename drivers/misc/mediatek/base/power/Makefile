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
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

obj-$(CONFIG_ARCH_MT6755) += ppm_v1/
obj-$(CONFIG_ARCH_MT6797) += ppm_v1/
obj-$(CONFIG_ARCH_MT6757) += ppm_v2/
ifeq ($(CONFIG_MTK_PLATFORM),"elbrus")
obj-y += ppm_v2/
endif
obj-$(CONFIG_ARCH_MT6735) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT6735M) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT6570) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT6580) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT6570) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT6753) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT6755) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT6757) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT6797) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
#obj-$(CONFIG_ARCH_ELBRUS) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT8127) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT8163) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT8167) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT8173) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT8127) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_MACH_MT2701) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT7623) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_MT2701_DCM)  += mt2701_dcm/
obj-$(CONFIG_MACH_MT2701) += hps/v1/
obj-$(CONFIG_ARCH_MT7623) += hps/v1/
obj-$(CONFIG_ARCH_MT8127) += hps/v1/
obj-$(CONFIG_ARCH_MT8163) += hps/v1/
obj-$(CONFIG_ARCH_MT8167) += hps/v1/
obj-$(CONFIG_ARCH_MT8173) += hps/v1/
obj-$(CONFIG_ARCH_MT6757) += hps/v2/
obj-$(CONFIG_ARCH_MT6735) += spm_v1/
obj-$(CONFIG_ARCH_MT6735M) += spm_v1/
obj-$(CONFIG_ARCH_MT6753) += spm_v1/
obj-$(CONFIG_ARCH_MT6570) += spm_v1/
obj-$(CONFIG_ARCH_MT6580) += spm_v1/
obj-$(CONFIG_ARCH_MT6755) += spm_v2/
obj-$(CONFIG_ARCH_MT6757) += spm_v2/
obj-$(CONFIG_ARCH_MT6797) += spm_v2/
#obj-$(CONFIG_ARCH_ELBRUS) += spm_v3/
obj-$(CONFIG_ARCH_ELBRUS) += clkbuf_v1/

ifeq ($(CONFIG_MTK_PLATFORM),"elbrus")
obj-y += cpufreq_v1/
endif

obj-$(CONFIG_ARCH_MT6755) += pbm_v1/
obj-$(CONFIG_ARCH_MT6797) += pbm_v1/
obj-$(CONFIG_ARCH_MT6757) += pbm_v1/
obj-$(CONFIG_ARCH_MT6735) += cpuidle_v1/
obj-$(CONFIG_ARCH_MT6735M) += cpuidle_v1/
obj-$(CONFIG_ARCH_MT6753) += cpuidle_v1/
obj-$(CONFIG_ARCH_MT6580) += cpuidle_v1/
obj-$(CONFIG_ARCH_MT6755) += cpuidle_v1/
obj-$(CONFIG_ARCH_MT6797) += cpuidle_v1/
