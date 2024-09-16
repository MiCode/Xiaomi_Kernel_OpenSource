MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
# ---------------------------------------------------
# OS option
# ---------------------------------------------------
os=$(CONFIG_MTK_SUPPORT_OS)

ifeq ($(os),)
os=linux
endif

ccflags-y += -Wno-unused-value
ccflags-y += -Wno-unused-result
ccflags-y += -Wno-format
ccflags-y += -Wno-parentheses

ifeq ($(os), none)
ccflags-y += -I/usr/include/
ccflags-y += -DCFG_VIRTUAL_OS
ccflags-y += -DCFG_REMIND_IMPLEMENT
endif

ifndef TOP
    TOP := $(srctree)/..
endif
$(info os option: $(os))
# ---------------------------------------------------
# ALPS Setting
# ---------------------------------------------------
ifneq ($(KERNEL_OUT),)
    ccflags-y += -imacros $(KERNEL_OUT)/include/generated/autoconf.h
endif

ifeq ($(KBUILD_MODPOST_FAIL_ON_WARNINGS),)
    # Force build fail on modpost warning
    KBUILD_MODPOST_FAIL_ON_WARNINGS=y
endif

DRIVER_BUILD_DATE=$(shell date +%Y%m%d%H%M%S)
ccflags-y += -DDRIVER_BUILD_DATE='"$(DRIVER_BUILD_DATE)"'
# ---------------------------------------------------
# Compile Options
# ---------------------------------------------------
WLAN_CHIP_LIST:=-UMT6620 -UMT6628 -UMT5931 -UMT6630 -UMT6632 -UMT7663 -UCONNAC -UCONNAC2X2 -UUT_TEST_MODE -UMT7915 -USOC3_0 -UMT7961
# '-D' and '-U' options are processed in the order they are given on the command line.
# All '-imacros file' and '-include file' options are processed after all '-D' and '-U' options.
ccflags-y += $(WLAN_CHIP_LIST)

ifeq ($(MTK_COMBO_CHIP),)
MTK_COMBO_CHIP = MT6632
endif

$(info $$MTK_PLATFORM is [${MTK_PLATFORM}])
$(info $$WLAN_CHIP_ID is [${WLAN_CHIP_ID}])

ifneq ($(CONFIG_MTK_EMI),)
ccflags-y += -DCONFIG_MTK_EMI=1
endif

ifneq ($(CONFIG_MEDIATEK_EMI),)
ccflags-y += -DCONFIG_MTK_EMI=1
endif

ifeq ($(WLAN_CHIP_ID),)
WLAN_CHIP_ID=$(word 1, $(MTK_COMBO_CHIP))
endif

ccflags-y += -DCFG_SUPPORT_DEBUG_FS=0
ccflags-y += -DWLAN_INCLUDE_PROC
ccflags-y += -DCFG_SUPPORT_AGPS_ASSIST=0
ccflags-y += -DCFG_SUPPORT_TSF_USING_BOOTTIME=1
ccflags-y += -DARP_MONITER_ENABLE=1
ccflags-y += -Werror
#ccflags-y:=$(filter-out -U$(WLAN_CHIP_ID),$(ccflags-y))
#ccflags-y += -DLINUX -D$(WLAN_CHIP_ID)
#workaround: also needed for none LINUX system
# because some of common part code is surrounded with this flag
ccflags-y += -DLINUX

ifneq ($(filter MT6632,$(MTK_COMBO_CHIP)),)
ccflags-y:=$(filter-out -UMT6632,$(ccflags-y))
ccflags-y += -DMT6632
endif

ifneq ($(filter MT7668,$(MTK_COMBO_CHIP)),)
ccflags-y:=$(filter-out -UMT7668,$(ccflags-y))
ccflags-y += -DMT7668
endif

ifneq ($(filter MT7663,$(MTK_COMBO_CHIP)),)
ccflags-y:=$(filter-out -UMT7663,$(ccflags-y))
ccflags-y += -DMT7663
endif

ifneq ($(filter CONNAC,$(MTK_COMBO_CHIP)),)
ccflags-y:=$(filter-out -UCONNAC,$(ccflags-y))
ccflags-y += -DCONNAC
endif

ifneq ($(filter CONNAC2X2,$(MTK_COMBO_CHIP)),)
ccflags-y:=$(filter-out -UCONNAC2X2,$(ccflags-y))
ccflags-y += -DCONNAC2X2
endif

ifneq ($(findstring MT7915,$(MTK_COMBO_CHIP)),)
ccflags-y:=$(filter-out -UMT7915,$(ccflags-y))
ccflags-y += -DMT7915
CONFIG_MTK_WIFI_CONNAC2X=y
CONFIG_MTK_WIFI_11AX_SUPPORT=y
CONFIG_MTK_WIFI_TWT_SUPPORT=y
CONFIG_NUM_OF_WFDMA_RX_RING=5
CONFIG_NUM_OF_WFDMA_TX_RING=1
endif

ifneq ($(findstring 3_0,$(MTK_COMBO_CHIP)),)
ccflags-y:=$(filter-out -USOC3_0,$(ccflags-y))
ccflags-y += -DSOC3_0
CONFIG_MTK_WIFI_CONNAC2X=y
CONFIG_MTK_WIFI_11AX_SUPPORT=y
CONFIG_MTK_WIFI_TWT_SUPPORT=y
CONFIG_NUM_OF_WFDMA_RX_RING=3
CONFIG_NUM_OF_WFDMA_TX_RING=0
CONFIG_MTK_WIFI_CONNINFRA_SUPPORT=y
CONFIG_MTK_WIFI_CONNAC2X_2x2=y
ccflags-y += -DCFG_POWER_ON_DOWNLOAD_EMI_ROM_PATCH=1
ccflags-y += -DCFG_DOWNLOAD_DYN_MEMORY_MAP=1
ccflags-y += -DCFG_ROM_PATCH_NO_SEM_CTRL=1
ccflags-y += -DCFG_ANDORID_CONNINFRA_SUPPORT=1
else
ccflags-y += -DCFG_ANDORID_CONNINFRA_SUPPORT=0
endif

ifneq ($(findstring MT7961,$(MTK_COMBO_CHIP)),)
ccflags-y:=$(filter-out -UMT7961,$(ccflags-y))
ccflags-y += -DMT7961
CONFIG_MTK_WIFI_CONNAC2X=y
CONFIG_MTK_WIFI_11AX_SUPPORT=y
CONFIG_MTK_WIFI_TWT_SUPPORT=y
CONFIG_NUM_OF_WFDMA_RX_RING=5
CONFIG_NUM_OF_WFDMA_TX_RING=1
endif

ifneq ($(filter 6873, $(WLAN_CHIP_ID)),)
    ccflags-y += -DCFG_ENABLE_HOST_BUS_TIMEOUT=1
else
    ccflags-y += -DCFG_ENABLE_HOST_BUS_TIMEOUT=0
endif

ifeq ($(CONFIG_MTK_WIFI_CONNAC2X), y)
    ccflags-y += -DCFG_SUPPORT_CONNAC2X=1
else
    ccflags-y += -DCFG_SUPPORT_CONNAC2X=0
endif

ifeq ($(CONFIG_MTK_WIFI_CONNAC2X_2x2), y)
    ccflags-y += -DCFG_SUPPORT_CONNAC2X_2x2=1
else
    ccflags-y += -DCFG_SUPPORT_CONNAC2X_2x2=0
endif

ifeq ($(CONFIG_MTK_WIFI_11AX_SUPPORT), y)
    ccflags-y += -DCFG_SUPPORT_802_11AX=1
else
    ccflags-y += -DCFG_SUPPORT_802_11AX=0
endif

ifeq ($(CONFIG_MTK_WIFI_TWT_SUPPORT), y)
    ccflags-y += -DCFG_SUPPORT_TWT=1
else
    ccflags-y += -DCFG_SUPPORT_TWT=0
endif

ifneq ($(CONFIG_NUM_OF_WFDMA_TX_RING),)
    ccflags-y += -DCONFIG_NUM_OF_WFDMA_TX_RING=$(CONFIG_NUM_OF_WFDMA_TX_RING)
endif

ifneq ($(CONFIG_NUM_OF_WFDMA_RX_RING),)
    ccflags-y += -DCONFIG_NUM_OF_WFDMA_RX_RING=$(CONFIG_NUM_OF_WFDMA_RX_RING)
endif

ifeq ($(WIFI_ENABLE_GCOV), y)
    GCOV_PROFILE := y
endif

ccflags-y += -DCFG_DRIVER_INITIAL_RUNNING_MODE=3

ifneq ($(filter 6765, $(WLAN_CHIP_ID)),)
    ccflags-y += -DCFG_SUPPORT_DUAL_STA=0
else ifeq ($(CONFIG_MTK_TC10_FEATURE), y)
    ccflags-y += -DCFG_SUPPORT_DUAL_STA=0
else
    ccflags-y += -DCFG_SUPPORT_DUAL_STA=1
endif

ifneq ($(filter 6779, $(WLAN_CHIP_ID)),)
    ccflags-y += -DCFG_FORCE_AP1NSS
endif

ifeq ($(MTK_ANDROID_WMT), y)
    ccflags-y += -DCFG_MTK_ANDROID_WMT=1
    WMT_SUPPORT := y
else ifneq ($(filter MT6632,$(MTK_COMBO_CHIP)),)
    ccflags-y += -DCFG_MTK_ANDROID_WMT=1
    WMT_SUPPORT := y
else
    ccflags-y += -DCFG_MTK_ANDROID_WMT=0
    WMT_SUPPORT := n
endif

ifeq ($(CONFIG_MTK_WIFI_CONNINFRA_SUPPORT), y)
    ccflags-y += -DCFG_SUPPORT_CONNINFRA=1
    ccflags-y += -DCFG_SUPPORT_PRE_ON_PHY_ACTION=1
    ccflags-y += -I$(TOP)/vendor/mediatek/kernel_modules/connectivity/conninfra/include
    ccflags-y += -DCFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT=1
    ifneq ($(CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH),)
        ccflags-y += -I$(TOP)/vendor/mediatek/kernel_modules/connectivity/conninfra/include
        ccflags-y += -I$(TOP)/vendor/mediatek/kernel_modules/connectivity/conninfra/platform/include
        ccflags-y += -I$(TOP)/vendor/mediatek/kernel_modules/connectivity/conninfra/base/include
        ccflags-y += -I$(TOP)/vendor/mediatek/kernel_modules/connectivity/conninfra/debug_utility
        ccflags-y += -I$(TOP)/vendor/mediatek/kernel_modules/connectivity/conninfra/debug_utility/include
        ccflags-y += -I$(TOP)/vendor/mediatek/kernel_modules/connectivity/conninfra/debug_utility/connsyslog
        ccflags-y += -I$(TOP)/vendor/mediatek/kernel_modules/connectivity/conninfra/debug_utility/coredump
        ccflags-y += -I$(TOP)/vendor/mediatek/kernel_modules/connectivity/conninfra/debug_utility/coredump/platform/include
        ccflags-y += -I$(TOP)/vendor/mediatek/kernel_modules/connectivity/wlan/adaptor
    endif
else
    ccflags-y += -DCFG_SUPPORT_CONNINFRA=0
    ccflags-y += -DCFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT=0
    ccflags-y += -DCFG_SUPPORT_PRE_ON_PHY_ACTION=0
    ifeq ($(WMT_SUPPORT), y)
        ccflags-y += -I$(TOP)/vendor/mediatek/kernel_modules/connectivity/common/common_main/include
        ccflags-y += -I$(TOP)/vendor/mediatek/kernel_modules/connectivity/common/common_main/linux/include
        ifeq ($(CONFIG_MTK_CONN_LTE_IDC_SUPPORT),y)
            ccflags-y += -DWMT_IDC_SUPPORT=1
        else
            ccflags-y += -DWMT_IDC_SUPPORT=0
        endif
        ccflags-y += -DMTK_WCN_WMT_STP_EXP_SYMBOL_ABSTRACT
    endif
endif

ifeq ($(MTK_ANDROID_EMI), y)
    ccflags-y += -DCFG_MTK_ANDROID_EMI=1
else
    ccflags-y += -DCFG_MTK_ANDROID_EMI=0
endif

ifneq ($(WIFI_IP_SET),)
    ccflags-y += -DCFG_WIFI_IP_SET=$(WIFI_IP_SET)
else
    ccflags-y += -DCFG_WIFI_IP_SET=1
endif

ifneq ($(filter MTK_WCN_REMOVE_KERNEL_MODULE,$(KBUILD_SUBDIR_CCFLAGS)),)
    ccflags-y += -DCFG_BUILT_IN_DRIVER=1
else
    ccflags-y += -DCFG_BUILT_IN_DRIVER=0
endif

ifneq ($(findstring UT_TEST_MODE,$(MTK_COMBO_CHIP)),)
ccflags-y:=$(filter-out -UUT_TEST_MODE,$(ccflags-y))
ccflags-y += -DUT_TEST_MODE
endif

CONFIG_MTK_WIFI_MCC_SUPPORT=y
ifeq ($(CONFIG_MTK_WIFI_MCC_SUPPORT), y)
    ccflags-y += -DCFG_SUPPORT_CHNL_CONFLICT_REVISE=0
else
    ccflags-y += -DCFG_SUPPORT_CHNL_CONFLICT_REVISE=1
endif

ifeq ($(CONFIG_MTK_AEE_FEATURE), y)
    ccflags-y += -DCFG_SUPPORT_AEE=1
else
    ccflags-y += -DCFG_SUPPORT_AEE=0
endif

# Disable ASSERT() for user load, enable for others
ifneq ($(TARGET_BUILD_VARIANT),user)
    ccflags-y += -DBUILD_QA_DBG=1
else
    ccflags-y += -DBUILD_QA_DBG=0
endif

ifeq ($(CONFIG_MTK_COMBO_WIFI),y)
    ccflags-y += -DCFG_WPS_DISCONNECT=1
endif

ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), sdio)
    ccflags-y += -D_HIF_SDIO=1
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), pcie)
    ccflags-y += -D_HIF_PCIE=1
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), usb)
    ccflags-y += -D_HIF_USB=1
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), axi)
    ccflags-y += -D_HIF_AXI=1
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), ut)
    # Increase frame size to 2048 because of 'cfg80211_connect_result' exceed stack size
    ccflags-y += -D_HIF_UT=1 -Wno-unused-function -Wno-unused-variable -Wframe-larger-than=2048
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), none)
	ccflags-y += -D_HIF_NONE=1
else
    $(error Unsuppoted HIF=$(CONFIG_MTK_COMBO_WIFI_HIF)!!)
endif

ifneq ($(CFG_CFG80211_VERSION),)
VERSION_STR = $(subst \",,$(subst ., , $(subst -, ,$(subst v,,$(CFG_CFG80211_VERSION)))))
$(info VERSION_STR=$(VERSION_STR))
X = $(firstword $(VERSION_STR))
Y = $(word 2 ,$(VERSION_STR))
Z = $(word 3 ,$(VERSION_STR))
VERSION := $(shell echo "$$(( $X * 65536 + $Y * 256 + $Z))" )
ccflags-y += -DCFG_CFG80211_VERSION=$(VERSION)
$(info DCFG_CFG80211_VERSION=$(VERSION))
endif


ifeq ($(CONFIG_MTK_PASSPOINT_R2_SUPPORT), y)
    ccflags-y += -DCFG_SUPPORT_PASSPOINT=1
    ccflags-y += -DCFG_HS20_DEBUG=1
    ccflags-y += -DCFG_ENABLE_GTK_FRAME_FILTER=1
else
    ccflags-y += -DCFG_SUPPORT_PASSPOINT=0
    ccflags-y += -DCFG_HS20_DEBUG=0
    ccflags-y += -DCFG_ENABLE_GTK_FRAME_FILTER=0
endif

MTK_MET_PROFILING_SUPPORT = yes
ifeq ($(MTK_MET_PROFILING_SUPPORT), yes)
    ccflags-y += -DCFG_MET_PACKET_TRACE_SUPPORT=1
else
    ccflags-y += -DCFG_MET_PACKET_TRACE_SUPPORT=0
endif

MTK_MET_TAG_SUPPORT = no
ifeq ($(MTK_MET_TAG_SUPPORT), yes)
    ccflags-y += -DMET_USER_EVENT_SUPPORT
    ccflags-y += -DCFG_MET_TAG_SUPPORT=1
else
    ccflags-y += -DCFG_MET_TAG_SUPPORT=0
endif

MTK_TC3_SUPPORT = no
ifeq ($(MTK_TC3_SUPPORT), yes)
    ccflags-y += -DCFG_TC3_FEATURE=1
    ccflags-y += -DCFG_P2P_CONNECT_ALL_BSS=1
    ccflags-y += -DCFG_P2P_DEFAULT_CLIENT_COUNT=1
    ccflags-y += -DCFG_P2P_SCAN_REPORT_ALL_BSS=1
else
    ccflags-y += -DCFG_TC3_FEATURE=0
endif

ifeq ($(CONFIG_MTK_TC10_FEATURE), y)
    ccflags-y += -DCFG_TC10_FEATURE=1
else
    ccflags-y += -DCFG_TC10_FEATURE=0
endif

ifeq ($(CONFIG_MTK_TC1_FEATURE), y)
    ccflags-y += -I$(srctree)/drivers/misc/mediatek/tc1_interface
    ccflags-y += -DCFG_TC1_FEATURE=1
else
    ccflags-y += -DCFG_TC1_FEATURE=0
endif

ifeq ($(MODULE_NAME),)
	MODULE_NAME := wlan_$(shell echo $(strip $(WLAN_CHIP_ID)) | tr A-Z a-z)_$(CONFIG_MTK_COMBO_WIFI_HIF)
endif

ccflags-y += -DDBG=0
ccflags-y += -I$(src)/os -I$(src)/os/$(os)/include
ccflags-y += -I$(src)/include -I$(src)/include/nic -I$(src)/include/mgmt -I$(src)/include/chips
ifeq ($(CFG_SUPPORT_WIFI_SYSDVT), 1)
ccflags-y += -I$(src)/include/dvt
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/performance/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(srctree)/drivers/misc/mediatek/connectivity/common
ccflags-y += -I$(srctree)/drivers/devfreq/
ccflags-y += -I$(srctree)/net

ifeq ($(CONFIG_MTK_MCIF_WIFI_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mddp/include/
ccflags-y += -DCFG_MTK_MCIF_WIFI_SUPPORT=1
ifeq ($(CONFIG_MTK_MDDP_WH_SUPPORT), y)
ccflags-y += -DCFG_MTK_MDDP_WH_SUPPORT=1
else
ccflags-y += -DCFG_MTK_MDDP_WH_SUPPORT=0
endif
else
ccflags-y += -DCFG_MTK_MCIF_WIFI_SUPPORT=0
ccflags-y += -DCFG_MTK_MDDP_WH_SUPPORT=0
endif

ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), sdio)
ccflags-y += -I$(src)/os/$(os)/hif/sdio/include
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), pcie)
ccflags-y += -I$(src)/os/$(os)/hif/common/include
ccflags-y += -I$(src)/os/$(os)/hif/pcie/include
ifneq ($(findstring 3_0,$(MTK_COMBO_CHIP)),)
ccflags-y += -I$(src)/include/chips/coda/soc3_0
endif
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), axi)
ccflags-y += -I$(src)/os/$(os)/hif/common/include
ccflags-y += -I$(src)/os/$(os)/hif/axi/include
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), usb)
ccflags-y += -I$(src)/os/$(os)/hif/usb/include
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), ut)
ccflags-y += -I$(src)/test -I$(src)/test/lib/include -I$(src)/test/testcases -I$(src)/test/lib/hif
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), none)
ccflags-y += -I$(src)/os/$(os)/hif/none/include
endif

ifneq ($(PLATFORM_FLAGS), )
    ccflags-y += $(PLATFORM_FLAGS)
endif

ifeq ($(CONFIG_MTK_WIFI_ONLY),$(filter $(CONFIG_MTK_WIFI_ONLY),m y))
obj-$(CONFIG_MTK_WIFI_ONLY) += $(MODULE_NAME).o
else
obj-$(CONFIG_MTK_COMBO_WIFI) += $(MODULE_NAME).o
#obj-y += $(MODULE_NAME).o
endif

ifeq ($(CONFIG_WLAN_DRV_BUILD_IN),y)
$(warning $(MODULE_NAME) build-in boot.img)
obj-y += $(MODULE_NAME).o
ccflags-y += -DCONFIG_WLAN_DRV_BUILD_IN=1
else
$(warning $(MODULE_NAME) is kernel module)
obj-m += $(MODULE_NAME).o
ccflags-y += -DCONFIG_WLAN_DRV_BUILD_IN=0
endif

# ---------------------------------------------------
# Directory List
# ---------------------------------------------------
COMMON_DIR  := common/
OS_DIR      := os/$(os)/
HIF_COMMON_DIR := $(OS_DIR)hif/common/
ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), sdio)
HIF_DIR	    := os/$(os)/hif/sdio/
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), pcie)
HIF_DIR     := os/$(os)/hif/pcie/
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), axi)
HIF_DIR	    := os/$(os)/hif/axi/
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), usb)
HIF_DIR	    := os/$(os)/hif/usb/
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), ut)
HIF_DIR	    := test/lib/hif/
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), none)
HIF_DIR	    := os/$(os)/hif/none/
endif
NIC_DIR     := nic/
MGMT_DIR    := mgmt/
CHIPS       := chips/
CHIPS_CMM   := $(CHIPS)common/

ifneq ($(WLAN_CHIP_ID),)
PLAT_DIR    := os/$(os)/plat/mt$(WLAN_CHIP_ID)/
endif
SYSDVT_DIR  := dvt/

# ---------------------------------------------------
# Objects List
# ---------------------------------------------------

COMMON_OBJS := 	$(COMMON_DIR)dump.o \
		$(COMMON_DIR)wlan_lib.o \
		$(COMMON_DIR)wlan_oid.o \
		$(COMMON_DIR)wlan_bow.o \
		$(COMMON_DIR)debug.o

NIC_OBJS := 	$(NIC_DIR)nic.o \
		$(NIC_DIR)nic_tx.o \
		$(NIC_DIR)nic_txd_v1.o \
		$(NIC_DIR)nic_rxd_v1.o \
		$(NIC_DIR)nic_rx.o \
		$(NIC_DIR)nic_pwr_mgt.o \
		$(NIC_DIR)nic_rate.o \
		$(NIC_DIR)cmd_buf.o \
		$(NIC_DIR)que_mgt.o \
		$(NIC_DIR)nic_cmd_event.o \
		$(NIC_DIR)nic_umac.o

ifeq ($(os), none)
OS_OBJS := 	$(OS_DIR)gl_dependent.o \
		$(OS_DIR)gl_init.o \
		$(OS_DIR)gl_kal.o \
		$(OS_DIR)gl_ate_agent.o \
		$(OS_DIR)gl_qa_agent.o
else
OS_OBJS := 	$(OS_DIR)gl_init.o \
		$(OS_DIR)gl_kal.o \
		$(OS_DIR)gl_bow.o \
		$(OS_DIR)gl_wext.o \
		$(OS_DIR)gl_wext_priv.o \
		$(OS_DIR)gl_ate_agent.o \
		$(OS_DIR)gl_qa_agent.o \
		$(OS_DIR)gl_hook_api.o \
		$(OS_DIR)gl_rst.o \
		$(OS_DIR)gl_cfg80211.o \
		$(OS_DIR)gl_proc.o \
		$(OS_DIR)gl_sys.o \
		$(OS_DIR)gl_vendor.o \
		$(OS_DIR)platform.o
endif

MGMT_OBJS := 	$(MGMT_DIR)ais_fsm.o \
		$(MGMT_DIR)aaa_fsm.o \
		$(MGMT_DIR)assoc.o \
		$(MGMT_DIR)auth.o \
		$(MGMT_DIR)bss.o \
		$(MGMT_DIR)cnm.o \
		$(MGMT_DIR)cnm_timer.o \
		$(MGMT_DIR)cnm_mem.o \
		$(MGMT_DIR)hem_mbox.o \
		$(MGMT_DIR)mib.o \
		$(MGMT_DIR)privacy.o \
		$(MGMT_DIR)rate.o \
		$(MGMT_DIR)rlm.o \
		$(MGMT_DIR)rlm_domain.o \
		$(MGMT_DIR)reg_rule.o \
		$(MGMT_DIR)rlm_obss.o \
		$(MGMT_DIR)rlm_protection.o \
		$(MGMT_DIR)rrm.o \
		$(MGMT_DIR)rsn.o \
		$(MGMT_DIR)saa_fsm.o \
		$(MGMT_DIR)scan.o \
		$(MGMT_DIR)scan_fsm.o \
		$(MGMT_DIR)scan_cache.o \
		$(MGMT_DIR)swcr.o \
		$(MGMT_DIR)roaming_fsm.o \
		$(MGMT_DIR)tkip_mic.o \
		$(MGMT_DIR)hs20.o \
		$(MGMT_DIR)tdls.o \
		$(MGMT_DIR)wnm.o \
		$(MGMT_DIR)qosmap.o \
		$(MGMT_DIR)ap_selection.o \
		$(MGMT_DIR)wmm.o \
		$(MGMT_DIR)mddp.o \

# ---------------------------------------------------
# Chips Objects List
# ---------------------------------------------------
MGMT_OBJS += $(MGMT_DIR)stats.o


CHIPS_OBJS += $(CHIPS_CMM)cmm_asic_connac.o
CHIPS_OBJS +=  $(CHIPS_CMM)dbg_connac.o
ifeq ($(CONFIG_MTK_WIFI_CONNAC2X), y)
CHIPS_OBJS +=  $(CHIPS_CMM)dbg_connac2x.o
endif

ifeq ($(CONFIG_MTK_WIFI_CONNAC2X), y)
CHIPS_OBJS += $(CHIPS_CMM)cmm_asic_connac2x.o
NIC_OBJS += $(NIC_DIR)nic_ext_cmd_event.o \
			$(NIC_DIR)nic_txd_v2.o \
			$(NIC_DIR)nic_rxd_v2.o
endif
CHIPS_OBJS += $(CHIPS_CMM)fw_dl.o

ifneq ($(filter MT6632,$(MTK_COMBO_CHIP)),)
CHIPS_OBJS += $(CHIPS)mt6632/mt6632.o
endif
ifneq ($(filter MT7668,$(MTK_COMBO_CHIP)),)
CHIPS_OBJS += $(CHIPS)mt7668/mt7668.o
endif
ifneq ($(filter MT7663,$(MTK_COMBO_CHIP)),)
CHIPS_OBJS += $(CHIPS)mt7663/mt7663.o
endif
ifneq ($(filter CONNAC,$(MTK_COMBO_CHIP)),)
CHIPS_OBJS += $(CHIPS)connac/connac.o
endif
ifneq ($(filter CONNAC2X2,$(MTK_COMBO_CHIP)),)
CHIPS_OBJS += $(CHIPS)connac2x2/connac2x2.o
endif
ifneq ($(findstring MT7915,$(MTK_COMBO_CHIP)),)
CHIPS_OBJS += $(CHIPS)mt7915/mt7915.o
CHIPS_OBJS +=  $(CHIPS)mt7915/dbg_mt7915.o
endif
ifneq ($(findstring 3_0,$(MTK_COMBO_CHIP)),)
CHIPS_OBJS += $(CHIPS)soc3_0/soc3_0.o
CHIPS_OBJS += $(CHIPS)soc3_0/dbg_soc3_0.o
CHIPS_OBJS += $(CHIPS)soc3_0/hal_dmashdl_soc3_0.o
endif
ifneq ($(findstring MT7961,$(MTK_COMBO_CHIP)),)
CHIPS_OBJS += $(CHIPS)mt7961/mt7961.o
CHIPS_OBJS += $(CHIPS)mt7961/dbg_mt7961.o
CHIPS_OBJS += $(CHIPS)mt7961/hal_dmashdl_mt7961.o
endif

# ---------------------------------------------------
# P2P Objects List
# ---------------------------------------------------

COMMON_OBJS += $(COMMON_DIR)wlan_p2p.o

NIC_OBJS += $(NIC_DIR)p2p_nic.o

ifneq ($(os), none)
OS_OBJS += $(OS_DIR)gl_p2p.o \
           $(OS_DIR)gl_p2p_cfg80211.o \
           $(OS_DIR)gl_p2p_init.o \
           $(OS_DIR)gl_p2p_kal.o
endif

MGMT_OBJS += $(MGMT_DIR)p2p_dev_fsm.o\
            $(MGMT_DIR)p2p_dev_state.o\
            $(MGMT_DIR)p2p_role_fsm.o\
            $(MGMT_DIR)p2p_role_state.o\
            $(MGMT_DIR)p2p_func.o\
            $(MGMT_DIR)p2p_scan.o\
            $(MGMT_DIR)p2p_ie.o\
            $(MGMT_DIR)p2p_rlm.o\
            $(MGMT_DIR)p2p_assoc.o\
            $(MGMT_DIR)p2p_bss.o\
            $(MGMT_DIR)p2p_rlm_obss.o\
            $(MGMT_DIR)p2p_fsm.o

MGMT_OBJS += $(MGMT_DIR)wapi.o

# ---------------------------------------------------
# HE Objects List
# ---------------------------------------------------

COMMON_OBJS += $(COMMON_DIR)wlan_he.o

ifeq ($(CONFIG_MTK_WIFI_11AX_SUPPORT), y)
MGMT_OBJS += $(MGMT_DIR)he_ie.o \
             $(MGMT_DIR)he_rlm.o
endif

ifeq ($(CONFIG_MTK_WIFI_TWT_SUPPORT), y)
MGMT_OBJS += $(MGMT_DIR)twt_req_fsm.o \
             $(MGMT_DIR)twt.o \
             $(MGMT_DIR)twt_planner.o
endif

ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), sdio)
HIF_OBJS :=  $(HIF_DIR)arm.o \
             $(HIF_DIR)sdio.o \
             $(HIF_DIR)hal_api.o \
             $(HIF_DIR)sdio_test_driver_core.o \
             $(HIF_DIR)sdio_test_driver_ops.o
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), pcie)
HIF_OBJS :=  $(HIF_COMMON_DIR)hal_pdma.o \
             $(HIF_COMMON_DIR)kal_pdma.o \
             $(HIF_COMMON_DIR)dbg_pdma.o \
             $(HIF_DIR)pcie.o
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), axi)
HIF_OBJS :=  $(HIF_COMMON_DIR)hal_pdma.o \
             $(HIF_COMMON_DIR)kal_pdma.o \
             $(HIF_COMMON_DIR)dbg_pdma.o \
             $(HIF_DIR)axi.o
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), usb)
HIF_OBJS :=  $(HIF_DIR)usb.o \
             $(HIF_DIR)hal_api.o
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), ut)
HIF_OBJS :=  $(HIF_DIR)ut.o \
             $(HIF_DIR)hal_api.o
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), none)
HIF_OBJS :=  $(HIF_DIR)none.o
endif
# ---------------------------------------------------
# Platform Objects List
# ---------------------------------------------------
ifneq ($(PLAT_DIR),)

PLAT_PRIV_C = $(src)/$(PLAT_DIR)plat_priv.c

# search path (out of kernel tree)
IS_EXIST_PLAT_PRIV_C := $(wildcard $(PLAT_PRIV_C))
# search path (build-in kernel tree)
IS_EXIST_PLAT_PRIV_C += $(wildcard $(srctree)/$(PLAT_PRIV_C))

ifneq ($(strip $(IS_EXIST_PLAT_PRIV_C)),)
PLAT_OBJS := $(PLAT_DIR)plat_priv.o
$(MODULE_NAME)-objs  += $(PLAT_OBJS)
endif
endif

# ---------------------------------------------------
# System Dvt Objects List
# ---------------------------------------------------
ifeq ($(CFG_SUPPORT_WIFI_SYSDVT), 1)
SYSDVT_OBJS += $(SYSDVT_DIR)dvt_common.o

ifeq ($(CFG_SUPPORT_DMASHDL_SYSDVT), 1)
SYSDVT_OBJS += $(SYSDVT_DIR)dvt_dmashdl.o
endif
endif

# ---------------------------------------------------
# Service git List
# ---------------------------------------------------
SERVICE_DIR  := wlan_service/

ifneq ($(findstring wlan_service,$(MTK_WLAN_SERVICE_PATH)),)
MTK_WLAN_SERVICE=yes
SERVICE_DIR  := $(MTK_WLAN_SERVICE_PATH)
$(info SERVICE_DIR is [{$(MTK_WLAN_SERVICE_PATH)}])
endif

ifeq ($(MTK_WLAN_SERVICE), yes)
ccflags-y += -DCONFIG_WLAN_SERVICE=1
ccflags-y += -DCONFIG_TEST_ENGINE_OFFLOAD=1
ccflags-y += -I$(src)/$(SERVICE_DIR)include
ccflags-y += -I$(src)/$(SERVICE_DIR)service/include
ccflags-y += -I$(src)/$(SERVICE_DIR)glue/osal/include
ccflags-y += -I$(src)/$(SERVICE_DIR)glue/hal/include
$(info $$CCFLAG is [{$(ccflags-y)}])
SERVICE_OBJS := $(SERVICE_DIR)agent/agent.o \
                $(SERVICE_DIR)service/service_test.o \
                $(SERVICE_DIR)service/test_engine.o \
                $(SERVICE_DIR)glue/osal/gen4m/sys_adaption_gen4m.o \
                $(SERVICE_DIR)glue/osal/gen4m/net_adaption_gen4m.o \
                $(SERVICE_DIR)glue/hal/gen4m/operation_gen4m.o
$(MODULE_NAME)-objs  += $(SERVICE_OBJS)
$(info $$MTK_WLAN_SERVICE is [{$(SERVICE_OBJS)}])
else
ccflags-y += -DCONFIG_WLAN_SERVICE=0
ccflags-y += -DCONFIG_TEST_ENGINE_OFFLOAD=0
endif

$(MODULE_NAME)-objs  += $(COMMON_OBJS)
$(MODULE_NAME)-objs  += $(NIC_OBJS)
$(MODULE_NAME)-objs  += $(OS_OBJS)
$(MODULE_NAME)-objs  += $(HIF_OBJS)
$(MODULE_NAME)-objs  += $(MGMT_OBJS)
$(MODULE_NAME)-objs  += $(CHIPS_OBJS)
$(MODULE_NAME)-objs  += $(SYSDVT_OBJS)

ifneq ($(findstring UT_TEST_MODE,$(MTK_COMBO_CHIP)),)
include $(src)/test/ut.make
endif
