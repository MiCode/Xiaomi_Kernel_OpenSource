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


NEED_BUILD_C2K_KERNEL_DRV=n

ifeq ($(CONFIG_MTK_ECCCI_DRIVER), y)
ifneq ($(CONFIG_MTK_ECCCI_C2K), y) # SDIO CCCI
ifneq ($(CONFIG_MTK_MD3_SUPPORT),)
ifneq ($(CONFIG_MTK_MD3_SUPPORT), 0)

NEED_BUILD_C2K_KERNEL_DRV=y

endif
endif
endif
endif

ifeq ($(NEED_BUILD_C2K_KERNEL_DRV), y)

ifeq ($(EVDO_VIA_HAVE_VOLT_PROTECTION), yes) #if we have voltage protection in design
ccflags-y += -DOEM_HAVE_VOLT_PROTECTION
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccmni/
ccflags-y += -DWAKE_HOST_BY_SYNC

obj-y += c2k_sdio.o

c2k_sdio-y := core.o power.o c2k_hw.o sync.o
c2k_sdio-y += cbp_sdio.o modem_sdio.o
c2k_sdio-y += f_rawbulk.o rawbulk.o rawbulk_transfer.o

obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/

else

obj- :=dummy.o

endif
