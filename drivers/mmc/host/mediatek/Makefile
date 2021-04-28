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

#
# Makefile for Mediatek(R) MMC/SD host controller drivers
#

#add kernel source code path as head file search path
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ifeq (y, $(filter y, $(CONFIG_MACH_MT6833)))
MTK_PLATFORM := mt6833
MMC_FOLDER := mt6833
else ifeq (y, $(filter y, $(CONFIG_MACH_MT6893)))
MMC_FOLDER := mt6885
else ifeq (y, $(filter y, $(CONFIG_MACH_MT6781)))
MTK_PLATFORM := mt6785
MMC_FOLDER := mt6781
else
MMC_FOLDER := $(MTK_PLATFORM)
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
#workaround for include mmc/card/queue.h and mmc/core/core.h
ccflags-y += -I$(srctree)/drivers
ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/ComboA
ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/ComboA/$(MMC_FOLDER)

ifeq ($(CONFIG_MTK_HW_FDE_AES),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/fde_aes/$(MTK_PLATFORM)
endif

ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/rpmb/drrpmb/public
ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/rpmb/drrpmb_gp/public
ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/rpmb/tlrpmb/public
endif

obj-$(CONFIG_MMC_MTK_PRO) += ComboA/
