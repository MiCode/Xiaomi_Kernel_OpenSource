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

#FPGA project no need build-in
ifndef CONFIG_MTK_FPGA

# ccci_util is build when ECCCI driver or CCCI driver enabled

ENABLE_CCCI_UTIL=n
ifeq ($(CONFIG_MTK_ECCCI_DRIVER), y)
ENABLE_CCCI_UTIL=y
endif

# mt6893&mt6885 co-platform config
ifeq (y, $(filter y, $(CONFIG_MACH_MT6893)))
MTK_PLATFORM := mt6885
CONFIG_MTK_PLATFORM := "mt6885"
else
MTK_PLATFORM := $(CONFIG_MTK_PLATFORM)
endif

ifeq ($(ENABLE_CCCI_UTIL), y)

ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccci_util

obj-y += ccci_util_lib.o
ccci_util_lib-y := ccci_util_lib_fo.o
ccci_util_lib-y += ccci_util_lib_load_img.o
ccci_util_lib-y += ccci_util_lib_sys.o
ccci_util_lib-y += ccci_private_log.o
ccci_util_lib-y += ccci_util_lib_time.o
ccci_util_lib-y += ccci_util_lib_main.o
ccci_util_lib-y += ccci_util_ld_md_errno.o
ccci_util_lib-y += ccci_util_broadcast.o
ccci_util_lib-y += ccci_util_pin_broadcast.o
ccci_util_lib-y += ccci_util_sib.o
ccci_util_lib-y += ccci_util_md_rat.o
endif

ifeq ($(CONFIG_MTK_SECURITY_SW_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/asf_export_inc
ccflags-y  += -DENABLE_MD_IMG_SECURITY_FEATURE
endif
ifeq ($(CONFIG_MTK_SEC_MODEM_NVRAM_ANTI_CLONE), y)
ccflags-y  += -DMTK_SEC_MODEM_NVRAM_ANTI_CLONE
endif

# always build platform, for power off md
MTK_PLATFORM_DIR=$(srctree)/drivers/misc/mediatek/ccci_util/$(subst ",,$(CONFIG_MTK_PLATFORM))
mtkplatform_exist=$(shell if [ -d ${MTK_PLATFORM_DIR} ]; then echo "y"; else echo "n"; fi;)
ifeq ($(mtkplatform_exist), y)
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

endif

obj-y += ccci_util_dummy.o # avoid build error
