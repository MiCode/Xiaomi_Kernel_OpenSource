ifeq ($(MTK_PLATFORM),)
ifneq ($(MTK_PLATFORM_WMT),)
MTK_PLATFORM := $(shell echo $(MTK_PLATFORM_WMT) | tr A-Z a-z)
endif
endif

ifeq ($(MTK_PLATFORM),)
ifneq ($(CONFIG_MTK_PLATFORM),)
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif
endif

CONNSYS_PLATFORM := $(TARGET_BOARD_PLATFORM_WMT)
PRIORITY_TABLE_MTK_PLATFORM := mt6735

ifeq ($(CONNSYS_PLATFORM),)
CONNSYS_PLATFORM := $(MTK_PLATFORM)
else
ifneq ($(filter $(PRIORITY_TABLE_MTK_PLATFORM), $(MTK_PLATFORM)),)
CONNSYS_PLATFORM := $(MTK_PLATFORM)
endif
endif

###############################################################################
# Necessary Check

ifneq ($(CONFIG_MTK_COMBO),)

ifneq ($(KERNEL_OUT),)
    ccflags-y += -imacros $(KERNEL_OUT)/include/generated/autoconf.h
endif

ifeq ($(CONFIG_MTK_COMBO_CHIP),)
    $(error CONFIG_MTK_COMBO_CHIP not defined)
endif

ifeq ($(TARGET_BUILD_VARIANT),$(filter $(TARGET_BUILD_VARIANT),userdebug user))
    #ldflags-y += -s
endif

# Force build fail on modpost warning
KBUILD_MODPOST_FAIL_ON_WARNINGS := y
###############################################################################

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
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/clkbuf_v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/clkbuf_v1/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/btif/common/inc
ifeq ($(strip $(MTK_PLATFORM)), mt6735)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci1/$(MTK_PLATFORM)
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/$(MTK_PLATFORM)
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eemcs
ccflags-y += -I$(srctree)/drivers/misc/mediatek/conn_md/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include/mach
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(MTK_PLATFORM)
ifeq ($(CONFIG_MTK_PMIC_CHIP_MT6359),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6359
endif
ifeq ($(CONFIG_MTK_PMIC_CHIP_MT6359P),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6359p
endif
ccflags-y += -I$(srctree)/drivers/mmc/core
ccflags-y += -I$(srctree)/drivers/misc/mediatek/connectivity/common
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
###############################################################################


ccflags-y += -Werror

ifneq ($(filter "MT6628",$(CONFIG_MTK_COMBO_CHIP)),)
    ccflags-y += -D MT6628
    ccflags-y += -D MERGE_INTERFACE_SUPPORT
endif
ifneq ($(filter "MT6630",$(CONFIG_MTK_COMBO_CHIP)),)
    ccflags-y += -D MT6630
ifneq ($(CONFIG_ARCH_MT2601),y)
    ccflags-y += -D MERGE_INTERFACE_SUPPORT
endif
endif

ifneq ($(filter "MT6632",$(CONFIG_MTK_COMBO_CHIP)),)
    ccflags-y += -D MT6632
    ccflags-y += -D MERGE_INTERFACE_SUPPORT
endif

#obj-y   +=  common_main/
#obj-y   +=  common_detect/

ifneq ($(filter MT6631,$(MTK_CONSYS_ADIE)),)
    ccflags-y += -D CONSYS_PMIC_CTRL_6635=0
else
    ccflags-y += -D CONSYS_PMIC_CTRL_6635=1
endif

###############################################################################
MODULE_NAME := wmt_drv
ifeq ($(CONFIG_WLAN_DRV_BUILD_IN),y)
$(warning $(MODULE_NAME) build-in boot.img)
obj-y += $(MODULE_NAME).o
else
$(warning $(MODULE_NAME) is kernel module)
obj-m += $(MODULE_NAME).o
endif

###############################################################################
# common_detect
###############################################################################
ccflags-y += -I$(srctree)/arch/arm/mach-$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/dct/dct
ccflags-y += -DWMT_PLAT_ALPS=1

COMBO_CHIP_SUPPORT := false
ifneq ($(filter "MT6620E3",$(CONFIG_MTK_COMBO_CHIP)),)
    COMBO_CHIP_SUPPORT := true
endif
ifneq ($(filter "MT6628",$(CONFIG_MTK_COMBO_CHIP)),)
    COMBO_CHIP_SUPPORT := true
endif
ifneq ($(filter "MT6630",$(CONFIG_MTK_COMBO_CHIP)),)
    COMBO_CHIP_SUPPORT := true
endif
ifneq ($(filter "MT6632",$(CONFIG_MTK_COMBO_CHIP)),)
    COMBO_CHIP_SUPPORT := true
endif
ifeq ($(COMBO_CHIP_SUPPORT), true)
    ccflags-y += -D MTK_WCN_COMBO_CHIP_SUPPORT
endif

ifneq ($(filter "CONSYS_%",$(CONFIG_MTK_COMBO_CHIP)),)
    ccflags-y += -D MTK_WCN_SOC_CHIP_SUPPORT
endif

ccflags-y += -I$(src)/common_main/linux/include
ccflags-y += -I$(src)/common_detect/drv_init/inc
ccflags-y += -I$(src)/common_detect
ccflags-y += -I$(src)/debug_utility

$(MODULE_NAME)-objs += common_detect/wmt_detect_pwr.o
$(MODULE_NAME)-objs += common_detect/wmt_detect.o
$(MODULE_NAME)-objs += common_detect/sdio_detect.o
$(MODULE_NAME)-objs += common_detect/mtk_wcn_stub_alps.o
$(MODULE_NAME)-objs += common_detect/wmt_gpio.o


ifneq ($(filter "MT6630",$(CONFIG_MTK_COMBO_CHIP)),)
	ccflags-y += -D MTK_WCN_WLAN_GEN3
endif

ifneq ($(filter "MT6632",$(CONFIG_MTK_COMBO_CHIP)),)
        ccflags-y += -D MTK_WCN_WLAN_GEN4
endif

ifneq ($(filter "CONSYS_6797" "CONSYS_6759" "CONSYS_6758" "CONSYS_6771" "CONSYS_6775",$(CONFIG_MTK_COMBO_CHIP)),)
	ccflags-y += -D MTK_WCN_WLAN_GEN3
else ifneq ($(filter "CONSYS_%",$(CONFIG_MTK_COMBO_CHIP)),)
	ccflags-y += -D MTK_WCN_WLAN_GEN2
endif

$(MODULE_NAME)-objs += common_detect/drv_init/fm_drv_init.o
$(MODULE_NAME)-objs += common_detect/drv_init/conn_drv_init.o
$(MODULE_NAME)-objs += common_detect/drv_init/bluetooth_drv_init.o
$(MODULE_NAME)-objs += common_detect/drv_init/wlan_drv_init.o
$(MODULE_NAME)-objs += common_detect/drv_init/common_drv_init.o
$(MODULE_NAME)-objs += common_detect/drv_init/gps_drv_init.o


###############################################################################
# common_main
###############################################################################
ccflags-y += -I$(src)/common_main/linux/include
ccflags-y += -I$(src)/common_main/linux/pri/include
ccflags-y += -I$(src)/common_main/platform/include
ccflags-y += -I$(src)/common_main/core/include
ccflags-y += -I$(src)/common_main/include

ccflags-y += -D WMT_PLAT_ALPS=1
ccflags-y += -D WMT_UART_RX_MODE_WORK=0 # 1. work thread 0. tasklet
ccflags-y += -D WMT_SDIO_MODE=1
ccflags-y += -D WMT_CREATE_NODE_DYNAMIC=1

ifneq ($(TARGET_BUILD_VARIANT),eng)
ifeq ($(CONFIG_EXTREME_LOW_RAM), y)
ccflags-y += -DLOG_STP_DEBUG_DISABLE
endif
endif

ifneq ($(TARGET_BUILD_VARIANT), user)
    ccflags-y += -D WMT_DBG_SUPPORT=1
else
    ccflags-y += -D WMT_DBG_SUPPORT=0
endif

ifeq ($(CONFIG_MTK_DEVAPC),y)
    ccflags-y += -D WMT_DEVAPC_DBG_SUPPORT=1
else
    ccflags-y += -D WMT_DEVAPC_DBG_SUPPORT=0
endif

ifeq ($(CONFIG_ARCH_MT6580), y)
ccflags-y += -D CFG_WMT_READ_EFUSE_VCN33
endif

# STEP: (Support Connac)
# MTK eng/userdebug/user load: Support
# Customer eng/userdebug load: Support
# Customer user load: Not support

ifneq ($(TARGET_BUILD_VARIANT),user)
	ccflags-y += -D CFG_WMT_STEP
endif

ifeq ($(findstring evb, $(MTK_PROJECT)), evb)
ccflags-y += -D CFG_WMT_EVB
endif

ifneq ($(filter "CONSYS_%",$(CONFIG_MTK_COMBO_CHIP)),)
$(MODULE_NAME)-objs += common_main/platform/$(CONNSYS_PLATFORM).o
endif

#$(MODULE_NAME)-objs += common_main/platform/wmt_plat_stub.o
$(MODULE_NAME)-objs += common_main/platform/wmt_plat_alps.o
$(MODULE_NAME)-objs += common_main/platform/mtk_wcn_consys_hw.o
$(MODULE_NAME)-objs += common_main/platform/mtk_wcn_cmb_hw.o

$(MODULE_NAME)-objs += common_main/core/wmt_ic_6628.o
$(MODULE_NAME)-objs += common_main/core/wmt_conf.o
$(MODULE_NAME)-objs += common_main/core/stp_core.o
$(MODULE_NAME)-objs += common_main/core/wmt_ctrl.o
$(MODULE_NAME)-objs += common_main/core/wmt_func.o
$(MODULE_NAME)-objs += common_main/core/wmt_core.o
$(MODULE_NAME)-objs += common_main/core/psm_core.o
$(MODULE_NAME)-objs += common_main/core/wmt_ic_soc.o
$(MODULE_NAME)-objs += common_main/core/wmt_lib.o
$(MODULE_NAME)-objs += common_main/core/wmt_ic_6620.o
$(MODULE_NAME)-objs += common_main/core/stp_exp.o
$(MODULE_NAME)-objs += common_main/core/wmt_ic_6632.o
$(MODULE_NAME)-objs += common_main/core/wmt_exp.o
$(MODULE_NAME)-objs += common_main/core/btm_core.o
$(MODULE_NAME)-objs += common_main/core/wmt_ic_6630.o

$(MODULE_NAME)-objs += common_main/linux/hif_sdio.o
$(MODULE_NAME)-objs += common_main/linux/stp_dbg_soc.o
$(MODULE_NAME)-objs += common_main/linux/stp_dbg_combo.o
$(MODULE_NAME)-objs += common_main/linux/osal.o
$(MODULE_NAME)-objs += common_main/linux/wmt_dev.o
$(MODULE_NAME)-objs += common_main/linux/stp_sdio.o
$(MODULE_NAME)-objs += common_main/linux/bgw_desense.o
$(MODULE_NAME)-objs += common_main/linux/wmt_idc.o
$(MODULE_NAME)-objs += common_main/linux/stp_uart.o
$(MODULE_NAME)-objs += common_main/linux/wmt_dbg.o
$(MODULE_NAME)-objs += common_main/linux/stp_dbg.o
$(MODULE_NAME)-objs += common_main/linux/wmt_user_proc.o

$(MODULE_NAME)-objs += common_main/linux/wmt_proc_dbg.o
$(MODULE_NAME)-objs += common_main/linux/wmt_alarm.o

ifneq ($(CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH),)
$(MODULE_NAME)-objs += common_main/linux/fw_log_wmt.o
endif
$(MODULE_NAME)-objs += common_main/linux/wmt_step.o

ifeq ($(CONFIG_MTK_BTIF),$(filter $(CONFIG_MTK_BTIF),y m))
$(MODULE_NAME)-objs += common_main/linux/stp_btif.o
endif

$(MODULE_NAME)-objs += debug_utility/ring.o
$(MODULE_NAME)-objs += debug_utility/ring_emi.o
$(MODULE_NAME)-objs += debug_utility/connsys_debug_utility.o
###############################################################################
# test
###############################################################################
ifeq ($(TARGET_BUILD_VARIANT),eng)
ccflags-y += -I$(src)/test/include
endif

ifeq ($(TARGET_BUILD_VARIANT),eng)
$(MODULE_NAME)-objs += test/wmt_step_test.o
endif

endif
