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

#add kernel source code path as head file search path
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/$(MTK_PLATFORM)

#workaround for include mmc/card/queue.h
ccflags-y += -I$(srctree)/drivers/mmc/card

obj-$(CONFIG_MMC_MTK) += msdc_hw_ett.o
obj-$(CONFIG_MMC_MTK) += sd.o

obj-$(CONFIG_MMC_MTK) += dbg.o
obj-$(CONFIG_MMC_MTK) += mt_sd_misc.o
obj-$(CONFIG_SDIOAUTOK_SUPPORT) += sdio_autok.o sdio_autok_proc.o

# kdump/kpainc interface move to lk
ifeq ($(CONFIG_MTK_AEE_IPANIC),y)
#obj-$(CONFIG_MMC_MTK) += mt_dump.o
endif

ifeq ($(CONFIG_MTK_EMMC_SUPPORT_OTP),y)
obj-$(CONFIG_MMC_MTK) += emmc_otp.o
endif
