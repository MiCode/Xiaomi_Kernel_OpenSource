# Makefile for MT76x8 combo driver

##############################################################
# Common settings
##############################################################
export MTK_COMBO_CHIP=MT6632
export HIF=usb
export CONFIG_MTK_WIFI_ONLY=m
export CONFIG_MTK_COMBO=m
export CONFIG_MTK_COMBO_WIFI=m
export CONFIG_MTK_COMBO_COMM=m
export CONFIG_MTK_COMBO_COMM_UART=m
export CONFIG_MTK_COMBO_COMM_SDIO=m
export CONFIG_MT_WIFI_CHRDEV=m
WIFI_TARGET := mt76x8

ifeq ($(HIF),)
    HIF=usb
endif

export CONFIG_MTK_COMBO_WIFI_HIF=$(HIF)
MODULE_NAME := wlan_$(WIFI_TARGET)_$(HIF)


##############################################################
# Platform specific
##############################################################
# Set default platform
PLATFORM = MT53XX

ifeq ($(PLATFORM),)
PLATFORM = MT53XX
endif

#- - - - -- - - - - - - - - -
# MT53XX start
#- - - - -- - - - - - - - - -
ifeq ($(PLATFORM),MT53XX)

# Sets DRIVER_PARTIAL_BUILD to y to compile driver only, otherwise set to n
DRIVER_PARTIAL_BUILD=n
# Set if 64 bit platform
64BIT_MODE=true
export WIFI_TARGET
TARGET = LINUX

ifeq ($(DRIVER_PARTIAL_BUILD),y)
# *****For driver partial build*****
LINUX_ROOT ?= $(word 1, $(subst /apollo/,/apollo /, $(shell pwd -L)))
#    Modify the build parameters below to fit your build
SYSBUILD_NAME=m-base
RELEASE_NAME=mediatek/mt5891_eu_64
KERNEL_VER=3.18
KERNEL_CONFIG=mt5891_android_smp_mod_defconfig
ifeq ($(64BIT_MODE),true)
export 64BIT_MODE=true
export KERNEL_64BIT=true
export TOOL_CHAIN_64BIT=4.9.3
endif
#
LINUX_SRC=$(LINUX_ROOT)/../android/$(SYSBUILD_NAME)/out/mediatek_linux/output/$(RELEASE_NAME)/rel/obj/kernel/linux_core/kernel/linux-$(KERNEL_VER)/$(KERNEL_CONFIG)_modules
OBJ_ROOT ?= $(LINUX_ROOT)/../android/$(SYSBUILD_NAME)/out/mediatek_linux/output/$(RELEASE_NAME)/rel/obj
export KERNEL_OBJ_ROOT=$(LINUX_SRC)/../..
else
# *****For system auto build*****
LINUX_SRC=$(KERNEL_OBJ_ROOT)/$(KERNEL_VER)/$(KERNEL_CONFIG)_modules
# 	overwrite $(OUT_STA_KO) for MT53XX platform
OUT_STA_KO = $(OBJ_ROOT)/third_party/source/wlan/mtk/$(WIFI_TARGET)/$(MODULE_NAME).ko
endif

include $(LINUX_ROOT)/linux_mts/mak/toolchain.mak
ifeq ($(CROSS_COMPILE),)
CROSS_COMPILE=/mtkoss/gnuarm/vfp_4.5.1_2.6.27_cortex-a9-rhel4/i686/bin/armv7a-mediatek451_001_vfp-linux-gnueabi-
endif
ifeq "$(CC)" "gcc"
CC ?= $(CROSS_COMPILE)gcc
endif
$(warning =============================================)
$(warning wifi driver LINUX_SRC=$(LINUX_SRC))
$(warning CROSS_COMPILE=$(CROSS_COMPILE))
$(warning =============================================)

DRIVER_DIR=$(LINUX_ROOT)/third_party/source/wlan/mtk/$(WIFI_TARGET)

endif
#- - - - -- - - - - - - - - -
# MT53XX end
#- - - - -- - - - - - - - - -


##############################################################
# Compile options
##############################################################
#/***** Common part ******/

# Define maximum different channels supported for ieee80211_iface_combination setting.
CFG_NUM_DIFFERENT_CHANNELS_STA=1
CFG_NUM_DIFFERENT_CHANNELS_P2P=1

# Define initial driver running mode.
# 0=RUNNING_P2P_MODE, 1=RUNNING_AP_MODE, 2=RUNNING_DUAL_AP_MODE, 3=RUNNING_P2P_AP_MODE
CFG_DRIVER_INITIAL_RUNNING_MODE=3

# Define to enable Android wake_lock
CFG_ENABLE_WAKE_LOCK=0

CFG_DEFAULT_DBG_LEVEL=0xF

CFG_TX_DIRECT_USB=0

CFG_RX_DIRECT_USB=0

CFG_USB_REQ_TX_DATA_FFA_CNT=6

CFG_USB_REQ_TX_DATA_CNT=2

CFG_USB_REQ_RX_DATA_CNT=4

CFG_ENABLE_EFUSE_MAC_ADDR=1

# Report all bss networks to cfg80211 when do p2p scan
CFG_P2P_SCAN_REPORT_ALL_BSS=0

# Support to change sta, p2p, ap interface names
# y: enable, n: disable
# eg. insmod wlan_mt76x8_usb.ko sta=wlan p2p=p2p ap=ap
CFG_DRIVER_INF_NAME_CHANGE=n

# 1: Enable SDIO RX Tasklet De-Aggregation
# 0: Disable (default)
CFG_SDIO_RX_AGG_TASKLET=0

#CFG_SUPPORT_SINGLE_SKU_LOCAL_DB=0

#/***** Platform dependent part ******/
ifeq ($(PLATFORM),MT53XX)
CFG_SUPPORT_ROAMING=0
endif

# For wpa_supplicant w/o MTK priv lib
# y: enable, n: disable
CFG_ANDROID_AOSP_PRIV_CMD=n

#/***** Manage configs into compile options ******/
ifneq ($(CFG_NUM_DIFFERENT_CHANNELS_STA),)
PLATFORM_FLAGS += -DCFG_NUM_DIFFERENT_CHANNELS_STA=$(CFG_NUM_DIFFERENT_CHANNELS_STA)
endif

ifneq ($(CFG_NUM_DIFFERENT_CHANNELS_P2P),)
PLATFORM_FLAGS += -DCFG_NUM_DIFFERENT_CHANNELS_P2P=$(CFG_NUM_DIFFERENT_CHANNELS_P2P)
endif

ifneq ($(CFG_DRIVER_INITIAL_RUNNING_MODE),)
PLATFORM_FLAGS += -DCFG_DRIVER_INITIAL_RUNNING_MODE=$(CFG_DRIVER_INITIAL_RUNNING_MODE)
endif

ifneq ($(CFG_ENABLE_WAKE_LOCK),)
PLATFORM_FLAGS += -DCFG_ENABLE_WAKE_LOCK=$(CFG_ENABLE_WAKE_LOCK)
endif

ifneq ($(CFG_DEFAULT_DBG_LEVEL),)
PLATFORM_FLAGS += -DCFG_DEFAULT_DBG_LEVEL=$(CFG_DEFAULT_DBG_LEVEL)
endif

ifneq ($(CFG_TX_DIRECT_USB),)
PLATFORM_FLAGS += -DCFG_TX_DIRECT_USB=$(CFG_TX_DIRECT_USB)
endif

ifneq ($(CFG_RX_DIRECT_USB),)
PLATFORM_FLAGS += -DCFG_RX_DIRECT_USB=$(CFG_RX_DIRECT_USB)
endif

ifneq ($(CFG_USB_REQ_TX_DATA_FFA_CNT),)
PLATFORM_FLAGS += -DCFG_USB_REQ_TX_DATA_FFA_CNT=$(CFG_USB_REQ_TX_DATA_FFA_CNT)
endif

ifneq ($(CFG_USB_REQ_TX_DATA_CNT),)
PLATFORM_FLAGS += -DCFG_USB_REQ_TX_DATA_CNT=$(CFG_USB_REQ_TX_DATA_CNT)
endif

ifneq ($(CFG_USB_REQ_RX_DATA_CNT),)
PLATFORM_FLAGS += -DCFG_USB_REQ_RX_DATA_CNT=$(CFG_USB_REQ_RX_DATA_CNT)
endif

ifneq ($(CFG_ENABLE_EFUSE_MAC_ADDR),)
PLATFORM_FLAGS += -DCFG_ENABLE_EFUSE_MAC_ADDR=$(CFG_ENABLE_EFUSE_MAC_ADDR)
endif

ifeq ($(CFG_DRIVER_INF_NAME_CHANGE), y)
PLATFORM_FLAGS += -DCFG_DRIVER_INF_NAME_CHANGE
endif

ifneq ($(CFG_P2P_SCAN_REPORT_ALL_BSS),)
PLATFORM_FLAGS += -DCFG_P2P_SCAN_REPORT_ALL_BSS=$(CFG_P2P_SCAN_REPORT_ALL_BSS)
endif

ifneq ($(CFG_SUPPORT_ROAMING),)
PLATFORM_FLAGS += -DCFG_SUPPORT_ROAMING=$(CFG_SUPPORT_ROAMING)
endif

ifeq ($(CFG_ANDROID_AOSP_PRIV_CMD), y)
PLATFORM_FLAGS += -DCFG_ANDROID_AOSP_PRIV_CMD
endif

ifneq ($(CFG_SDIO_RX_AGG_TASKLET),)
PLATFORM_FLAGS += -DCFG_SDIO_RX_AGG_TASKLET=$(CFG_SDIO_RX_AGG_TASKLET)
endif

##############################################################
# Compile settings
##############################################################

all: driver

driver:
	+cd $(DRIVER_DIR) && make -C $(LINUX_SRC) M=$(DRIVER_DIR) MODULE_NAME=$(MODULE_NAME) PLATFORM_FLAGS="$(PLATFORM_FLAGS)" modules
ifeq ($(PLATFORM),MT53XX)
ifeq ($(DRIVER_PARTIAL_BUILD), y)
	@cd $(DRIVER_DIR) && $(CROSS_COMPILE)strip --strip-unneeded $(MODULE_NAME).ko
else
ifneq ($(ANDROID),true)
	if [ ! -d $(THIRDPARTY_LIB_ROOT)/wlan/$(KERNEL_VER_FOR_3RD)/mtk/$(WIFI_TARGET) ]; then \
		mkdir -p $(THIRDPARTY_LIB_ROOT)/wlan/$(KERNEL_VER_FOR_3RD)/mtk/$(WIFI_TARGET)/; \
	fi
	cp -f $(OUT_STA_KO) $(THIRDPARTY_LIB_ROOT)/wlan/$(KERNEL_VER_FOR_3RD)/mtk/$(WIFI_TARGET)/$(MODULE_NAME).ko
else
ifeq "$(BUILD_CFG)" "debug"
	echo "ANDROID_VERSION=$(ANDROID_VERSION)"
	if [ ! -d $(LINUX_ROOT)/../android/$(ANDROID_VERSION)/vendor/mediatek/open/hardware/prebuilt/wifi/$(ANDROID_IC_SETTING)/dbg ]; then \
		mkdir -p $(LINUX_ROOT)/../android/$(ANDROID_VERSION)/vendor/mediatek/open/hardware/prebuilt/wifi/$(ANDROID_IC_SETTING)/dbg; \
	fi
	cp -f $(OUT_STA_KO) $(LINUX_ROOT)/../android/$(ANDROID_VERSION)/vendor/mediatek/open/hardware/prebuilt/wifi/$(ANDROID_IC_SETTING)/dbg/$(MODULE_NAME).ko

	if [ -d $(OUTPUT_ROOT)/basic/modules ]; then \
		cp -f $(OUT_STA_KO) $(OUTPUT_ROOT)/basic/modules/wlan.ko; \
	fi
else
	if [ ! -d $(LINUX_ROOT)/../android/$(ANDROID_VERSION)/vendor/mediatek/open/hardware/prebuilt/wifi/$(ANDROID_IC_SETTING) ]; then \
		mkdir -p $(LINUX_ROOT)/../android/$(ANDROID_VERSION)/vendor/mediatek/open/hardware/prebuilt/wifi/$(ANDROID_IC_SETTING); \
	fi
	cp -f $(OUT_STA_KO) $(LINUX_ROOT)/../android/$(ANDROID_VERSION)/vendor/mediatek/open/hardware/prebuilt/wifi/$(ANDROID_IC_SETTING)/$(MODULE_NAME).ko

	if [ -d $(OUTPUT_ROOT)/basic/modules ]; then \
		cp -f $(OUT_STA_KO) $(OUTPUT_ROOT)/basic/modules/wlan.ko; \
	fi
endif
endif
endif
endif
#	@cd $(DRIVER_DIR) && cp $(MODULES_NAME)_$(HIF).ko $(MODULES_NAME).ko

#common:
#	+cd $(COMMON_DIR) && make -C $(LINUX_SRC) M=$(COMMON_DIR) modules
#	@cat $(COMMON_DIR)/Module.symvers >> $(LINUX_SRC)/Module.symvers

clean: driver_clean

#symvers_clean: driver_clean
#	@cp $(LINUX_SRC)/Module.symvers.original $(LINUX_SRC)/Module.symvers

driver_clean:
	cd $(DRIVER_DIR) && make -C $(LINUX_SRC) M=$(DRIVER_DIR) MODULE_NAME=$(MODULE_NAME) clean
	if [ -e $(DRIVER_DIR)/$(MODULE_NAME).ko ]; then rm $(DRIVER_DIR)/$(MODULE_NAME).ko; fi;

#common_clean:
#	cd $(COMMON_DIR) && make -C $(LINUX_SRC) M=$(COMMON_DIR) clean

.PHONY: all clean driver driver_clean

