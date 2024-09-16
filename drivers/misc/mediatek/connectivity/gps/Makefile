MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
# drivers/barcelona/gps/Makefile
#
# Makefile for the Barcelona GPS driver.
#
# Copyright (C) 2004,2005 TomTom BV <http://www.tomtom.com/>
# Author: Dimitry Andric <dimitry.andric@tomtom.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.

###############################################################################
# Necessary Check
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ifeq ($(CONFIG_MTK_GPS_SUPPORT), y)

ifeq ($(AUTOCONF_H),)
    $(error AUTOCONF_H is not defined)
endif

ccflags-y += -imacros $(AUTOCONF_H)
ifndef TOP
    TOP := $(srctree)/..
endif
ifeq ($(TARGET_BUILD_VARIANT),$(filter $(TARGET_BUILD_VARIANT),userdebug user))
    ldflags-y += -s
endif

# Force build fail on modpost warning
KBUILD_MODPOST_FAIL_ON_WARNINGS := y
###############################################################################

# only WMT align this design flow, but gps use this also.
#ccflags-y += -D MTK_WCN_REMOVE_KERNEL_MODULE

ifeq ($(CONFIG_ARM64), y)
    ccflags-y += -D CONFIG_MTK_WCN_ARM64
endif

ifeq ($(CONFIG_MTK_CONN_LTE_IDC_SUPPORT),y)
    ccflags-y += -D WMT_IDC_SUPPORT=1
else
    ccflags-y += -D WMT_IDC_SUPPORT=0
endif
ccflags-y += -D MTK_WCN_WMT_STP_EXP_SYMBOL_ABSTRACT

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping
ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/connectivity/common
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include/mach
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/clkbuf_v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/clkbuf_v1/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/devfreq
###############################################################################

MODULE_NAME := gps_drv
obj-m += $(MODULE_NAME).o

GPS_DRV_CONTROL_LNA := n
SELECT_GPS_DL_DRV := n
GPS_DL_HAS_MOCK := n
GPS_DL_HAS_CONNINFRA_DRV := n
GPS_SRC_FOLDER := $(TOP)/vendor/mediatek/kernel_modules/connectivity/gps

ifeq ($(CONFIG_ARCH_MTK_PROJECT),"k6833v1_64_swrgo")
ccflags-y += -DCONFIG_GPSL5_SUPPORT
ccflags-y += -DCONFIG_GPS_CTRL_LNA_SUPPORT
GPS_DRV_CONTROL_LNA := y
endif

ifeq ($(CONFIG_MACH_MT6833),y)
ccflags-y += -DCONFIG_GPSL5_SUPPORT
ccflags-y += -DCONFIG_GPS_CTRL_LNA_SUPPORT
GPS_DRV_CONTROL_LNA := y
endif

ifeq ($(CONFIG_MACH_MT6885),y)
SELECT_GPS_DL_DRV := y
ccflags-y += -I$(GPS_SRC_FOLDER)/data_link/hw/inc/connac2_0
ccflags-y += -I$(GPS_SRC_FOLDER)/data_link/hw/inc/connac2_0/coda_gen
ifeq ($(CONFIG_MTK_COMBO_CHIP_CONSYS_6885),y)
GPS_DL_HAS_CONNINFRA_DRV := y
endif
ifeq ($(CONFIG_MTK_COMBO_CHIP_CONSYS_6893),y)
GPS_DL_HAS_CONNINFRA_DRV := y
endif
endif

ifeq ($(CONFIG_MACH_MT6893),y)
SELECT_GPS_DL_DRV := y
# For MT6893, the CODA is same as connac2_0
ccflags-y += -I$(GPS_SRC_FOLDER)/data_link/hw/inc/connac2_0
ccflags-y += -I$(GPS_SRC_FOLDER)/data_link/hw/inc/connac2_0/coda_gen
ifeq ($(CONFIG_MTK_COMBO_CHIP_CONSYS_6885),y)
GPS_DL_HAS_CONNINFRA_DRV := y
endif
ifeq ($(CONFIG_MTK_COMBO_CHIP_CONSYS_6893),y)
GPS_DL_HAS_CONNINFRA_DRV := y
endif
endif

ifeq ($(SELECT_GPS_DL_DRV),y) # New GPS driver with L1+L5 support
ifeq ($(GPS_DL_HAS_CONNINFRA_DRV),y)
CONNINFRA_SRC_FOLDER := $(TOP)/vendor/mediatek/kernel_modules/connectivity/conninfra
ccflags-y += -I$(CONNINFRA_SRC_FOLDER)/include
ccflags-y += -DGPS_DL_HAS_CONNINFRA_DRV=1
endif

ccflags-y += -I$(GPS_SRC_FOLDER)/data_link/inc
ccflags-y += -I$(GPS_SRC_FOLDER)/data_link/linux/inc
ccflags-y += -I$(GPS_SRC_FOLDER)/data_link/link/inc
ccflags-y += -I$(GPS_SRC_FOLDER)/data_link/lib/inc
ccflags-y += -I$(GPS_SRC_FOLDER)/data_link/hal/inc
ccflags-y += -I$(GPS_SRC_FOLDER)/data_link/hw/inc
ccflags-y += -I$(GPS_SRC_FOLDER)/data_link_mock/mock/inc

$(MODULE_NAME)-objs += gps_dl_module.o
$(MODULE_NAME)-objs += data_link/gps_dl_context.o

$(MODULE_NAME)-objs += data_link/lib/gps_dl_dma_buf.o
$(MODULE_NAME)-objs += data_link/lib/gps_dl_lib_misc.o
$(MODULE_NAME)-objs += data_link/lib/gps_dl_hist_rec.o
$(MODULE_NAME)-objs += data_link/lib/gps_dl_time_tick.o
$(MODULE_NAME)-objs += data_link/lib/gps_dl_name_list.o

$(MODULE_NAME)-objs += data_link/hw/gps_dl_hw_conn_infra.o
$(MODULE_NAME)-objs += data_link/hw/gps_dl_hw_bgf.o
$(MODULE_NAME)-objs += data_link/hw/gps_dl_hw_gps.o
$(MODULE_NAME)-objs += data_link/hw/gps_dl_hw_power_ctrl.o
$(MODULE_NAME)-objs += data_link/hw/gps_dl_hw_usrt_apb.o
$(MODULE_NAME)-objs += data_link/hw/gps_dl_hw_util.o

$(MODULE_NAME)-objs += data_link/hal/gps_dl_hal.o
$(MODULE_NAME)-objs += data_link/hal/gps_dl_hal_util.o
$(MODULE_NAME)-objs += data_link/hal/gps_dsp_fsm.o
$(MODULE_NAME)-objs += data_link/hal/gps_dl_power_ctrl.o
$(MODULE_NAME)-objs += data_link/hal/gps_dl_isr.o
$(MODULE_NAME)-objs += data_link/hal/gps_dl_dma.o
$(MODULE_NAME)-objs += data_link/hal/gps_dl_mcub.o
$(MODULE_NAME)-objs += data_link/hal/gps_dl_zbus.o
$(MODULE_NAME)-objs += data_link/hal/gps_dl_conn_infra.o

$(MODULE_NAME)-objs += data_link/link/gps_dl_subsys_reset.o
$(MODULE_NAME)-objs += data_link/gps_each_link.o

$(MODULE_NAME)-objs += data_link/linux/gps_data_link_devices.o
$(MODULE_NAME)-objs += data_link/linux/gps_each_device.o
$(MODULE_NAME)-objs += data_link/linux/gps_dl_linux.o
$(MODULE_NAME)-objs += data_link/linux/gps_dl_linux_plat_drv.o
$(MODULE_NAME)-objs += data_link/linux/gps_dl_linux_reserved_mem.o
$(MODULE_NAME)-objs += data_link/linux/gps_dl_emi.o
$(MODULE_NAME)-objs += data_link/linux/gps_dl_ctrld.o
$(MODULE_NAME)-objs += data_link/linux/gps_dl_procfs.o
$(MODULE_NAME)-objs += data_link/linux/gps_dl_osal.o

ifeq ($(GPS_DL_HAS_MOCK),y)
$(MODULE_NAME)-objs += data_link_mock/mock/gps_mock_mvcd.o
$(MODULE_NAME)-objs += data_link_mock/mock/gps_mock_hal.o
ccflags-y += -DGPS_DL_HAS_MOCK=1
endif

else #Legacy drivers
WMT_SRC_FOLDER := $(TOP)/vendor/mediatek/kernel_modules/connectivity/common

ifeq ($(CONFIG_MTK_COMBO_CHIP),)
    $(error CONFIG_MTK_COMBO_CHIP not defined)
endif

ifneq ($(filter "CONSYS_%",$(CONFIG_MTK_COMBO_CHIP)),)
        ccflags-y += -DSOC_CO_CLOCK_FLAG=1
        ccflags-y += -DWMT_CREATE_NODE_DYNAMIC=1
        ccflags-y += -DREMOVE_MK_NODE=0

ccflags-y += -I$(WMT_SRC_FOLDER)/common_main/$(MTK_PLATFORM)/include

else
        ccflags-y += -DSOC_CO_CLOCK_FLAG=0
        ccflags-y += -DWMT_CREATE_NODE_DYNAMIC=0
        ccflags-y += -DREMOVE_MK_NODE=1
endif

ccflags-y += -I$(WMT_SRC_FOLDER)/common_main/include
ccflags-y += -I$(WMT_SRC_FOLDER)/common_main/linux/include
ccflags-y += -I$(WMT_SRC_FOLDER)/common_main/core/include
ccflags-y += -I$(WMT_SRC_FOLDER)/common_main/platform/include
ifneq ($(CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH),)
ccflags-y += -I$(WMT_SRC_FOLDER)/debug_utility
endif
ifeq ($(GPS_DRV_CONTROL_LNA),y)
ccflags-y += -I$(GPS_SRC_FOLDER)/lna_ctrl/inc
endif

ifeq ($(CONFIG_MTK_CONN_MT3337_CHIP_SUPPORT),y)
        $(MODULE_NAME)-objs += gps_mt3337.o
else
        $(MODULE_NAME)-objs += stp_chrdev_gps.o
ifeq ($(CONFIG_ARCH_MTK_PROJECT),"k6833v1_64_swrgo")
        $(MODULE_NAME)-objs += stp_chrdev_gps2.o
endif
ifeq ($(CONFIG_MACH_MT6833),y)
        $(MODULE_NAME)-objs += stp_chrdev_gps2.o
endif
ifeq ($(GPS_DRV_CONTROL_LNA),y)
        $(MODULE_NAME)-objs += lna_ctrl/src/gps_lna_drv.o
endif
endif
ifneq ($(CONFIG_MTK_GPS_EMI),)
$(MODULE_NAME)-objs += gps_emi.o
endif
ifneq ($(CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH),)
$(MODULE_NAME)-objs += fw_log_gps.o
endif

endif

endif #ifeq ($(CONFIG_MTK_GPS_SUPPORT), y)
# EOF
