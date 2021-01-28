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

#add kernel source code path as head file search path
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ifeq (y, $(filter y, $(CONFIG_MACH_MT6833)))
MTK_PLATFORM := mt6833
MMC_FOLDER := mt6833
else ifeq (y, $(filter y, $(CONFIG_MACH_MT6893)))
MMC_FOLDER := mt6885
else
MMC_FOLDER := $(MTK_PLATFORM)
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/
ccflags-y += -I$(srctree)/drivers/devfreq
#workaround for include mmc/card/queue.h and mmc/core/core.h
ccflags-y += -I$(srctree)/drivers
ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/ComboA/$(MMC_FOLDER)

ifeq ($(CONFIG_MTK_HW_FDE_AES),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/fde_aes/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif

ccflags-y += -Wno-error=unused-function

obj-$(CONFIG_MMC_MTK_PRO) += msdc.o

msdc-y += sd.o
msdc-y += dbg.o
msdc-y += mtk_sd_misc.o
msdc-y += msdc_tune.o
msdc-y += msdc_partition.o
msdc-y += autok.o
msdc-y += $(MMC_FOLDER)/msdc_cust.o
msdc-y += $(MMC_FOLDER)/autok_dvfs.o
msdc-$(CONFIG_FPGA_EARLY_PORTING) += $(MMC_FOLDER)/msdc_fpga_io.o

obj-$(CONFIG_MMC_CRYPTO) += sw-cqhci-crypto.o
