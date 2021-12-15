# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2019 MediaTek Inc.


ifeq ($(CONFIG_MTK_ECCCI_DRIVER), y)

ccflags-y += -I$(srctree)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi_bwl/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/devfreq/
ccflags-y += -I$(srctree)/drivers/clk/mediatek
# security objects
MASP_CORE_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/drivers/masp

ifeq ($(CONFIG_MTK_SECURITY_SW_SUPPORT), y)
ifeq ($(CONFIG_CUSTOM_SEC_AUTH_SUPPORT), y)
clean-files += $(CCCI_CUSTOM_DRIVER_DIR)/cust_auth.o
else
clean-files += $(patsubst $(obj)/%,%,$(wildcard auth/*.o))
endif
endif

ifeq ($(CONFIG_MTK_SECURITY_SW_SUPPORT), y)
ifeq ($(CONFIG_CUSTOM_SEC_AUTH_SUPPORT), y)
CCCI_CUSTOM_DRIVER_DIR := $(MTK_PATH_CUSTOM)/$(call lc,$(MTK_PROJECT))/kernel/ccci
ccci_plat-y += $(CCCI_CUSTOM_DRIVER_DIR)/cust_auth.o
endif
endif

ifeq ($(CONFIG_MTK_SECURITY_SW_SUPPORT), y)
ccflags-y  += -DENABLE_MD_IMG_SECURITY_FEATURE
endif
ifeq ($(CONFIG_MTK_SEC_MODEM_NVRAM_ANTI_CLONE), y)
ccflags-y  += -DMTK_SEC_MODEM_NVRAM_ANTI_CLONE
endif

# CCCI objects
obj-$(CONFIG_MTK_ECCCI_DRIVER) := ccci.o

# platform dependent parts
ccci-y := ccci_platform.o $(core_obj) $(mach_obj) $(sec_obj)
ifeq ($(CONFIG_MTK_ECCCI_CLDMA), y)
ccci-y += md_sys1_platform.o
endif

else
obj-  := dummy.o # avoid build error
endif
