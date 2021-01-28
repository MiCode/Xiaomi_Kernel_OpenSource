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
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/
#workaround for include mmc/card/queue.h and mmc/core/core.h
ccflags-y += -I$(srctree)/drivers

ifeq ($(CONFIG_MTK_PLATFORM), "mt6761")

ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/ComboA/mt6765
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6357
ccflags-y += -I$(srctree)/drivers/devfreq
msdc-y += mt6765/msdc_cust.o
msdc-y += mt6765/autok_dvfs.o

else

ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/ComboA/$(MTK_PLATFORM)
msdc-y += $(MTK_PLATFORM)/msdc_cust.o
msdc-y += $(MTK_PLATFORM)/autok_dvfs.o

endif

ifeq ($(CONFIG_MTK_PLATFORM), "mt6765")
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6357
ccflags-y += -I$(srctree)/drivers/devfreq
endif

ifeq ($(CONFIG_MTK_HW_FDE_AES),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/fde_aes/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif

ccflags-y += -Wno-error=unused-function

obj-$(CONFIG_MMC_MTK_PRO) += msdc.o

msdc-y += sd.o
#msdc-y += dbg.o
#msdc-y += mtk_sd_misc.o
msdc-y += msdc_tune.o
#msdc-y += msdc_partition.o
msdc-y += autok.o
ifeq ($(CONFIG_MMC_CRYPTO),y)
msdc-y += sw-cqhci-crypto.o
endif

ifeq ($(CONFIG_FPGA_EARLY_PORTING),y)
msdc-y += $(MTK_PLATFORM)/msdc_fpga_io.o
endif
