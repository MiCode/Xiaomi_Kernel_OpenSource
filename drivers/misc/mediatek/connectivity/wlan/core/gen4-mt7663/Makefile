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
WLAN_CHIP_LIST:=-UMT6620 -UMT6628 -UMT5931 -UMT6630 -UMT6632 -UMT7663 -UCONNAC -UCONNAC2X2
# '-D' and '-U' options are processed in the order they are given on the command line.
# All '-imacros file' and '-include file' options are processed after all '-D' and '-U' options.
ccflags-y += $(WLAN_CHIP_LIST)

##############################################################
# Common settings
##############################################################
export MTK_COMBO_CHIP=MT7663
export CONFIG_MTK_COMBO_WIFI=m
export HIF=sdio
export CONFIG_MTK_COMBO_COMM_SDIO=y
export CONFIG_MTK_PREALLOC_MEMORY=y
ifeq ($(MTK_COMBO_CHIP),)
MTK_COMBO_CHIP = MT6632
endif
export CONFIG_MTK_COMBO_WIFI_HIF=$(HIF)

$(info $$MTK_PLATFORM is [${MTK_PLATFORM}])
$(info $$WLAN_CHIP_ID is [${WLAN_CHIP_ID}])

ifeq ($(WLAN_CHIP_ID),)
    WLAN_CHIP_ID := MT7663
endif

ccflags-y += -DCFG_SUPPORT_DEBUG_FS=0
ccflags-y += -DWLAN_INCLUDE_PROC
ccflags-y += -DCFG_SUPPORT_AGPS_ASSIST=1
ccflags-y += -DCFG_SUPPORT_TSF_USING_BOOTTIME=1
ccflags-y += -DARP_MONITER_ENABLE=1
ccflags-y += -Werror
#ccflags-y:=$(filter-out -U$(WLAN_CHIP_ID),$(ccflags-y))
#ccflags-y += -DLINUX -D$(WLAN_CHIP_ID)
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

ifeq ($(WIFI_ENABLE_GCOV), y)
GCOV_PROFILE := y
endif

ifeq ($(MTK_ANDROID_WMT), y)
    ccflags-y += -DCFG_MTK_ANDROID_WMT=1
else ifneq ($(filter MT6632,$(MTK_COMBO_CHIP)),)
    ccflags-y += -DCFG_MTK_ANDROID_WMT=1
else
    ccflags-y += -DCFG_MTK_ANDROID_WMT=0
endif

ifeq ($(MTK_ANDROID_EMI), y)
    ccflags-y += -DCFG_MTK_ANDROID_EMI=1
else
    ccflags-y += -DCFG_MTK_ANDROID_EMI=0
endif

ifneq ($(filter MT7663 MT7668,$(MTK_COMBO_CHIP)),)
    ccflags-y += -DCFG_MTK_EMI=0
else
    ccflags-y += -DCFG_MTK_EMI=DCONFIG_MTK_EMI
endif

ifneq ($(filter MTK_WCN_REMOVE_KERNEL_MODULE,$(KBUILD_SUBDIR_CCFLAGS)),)
    ccflags-y += -DCFG_BUILT_IN_DRIVER=1
else
    ccflags-y += -DCFG_BUILT_IN_DRIVER=0
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

ifeq ($(CONFIG_MTK_WPA3_SUPPORT), y)
	ccflags-y += -DCFG_SUPPORT_CFG80211_AUTH=1
	ccflags-y += -DCFG_SUPPORT_SAE=1
	ccflags-y += -DCFG_SUPPORT_OWE=1
	ccflags-y += -DCFG_SUPPORT_SUITB=1
else
	ccflags-y += -DCFG_SUPPORT_CFG80211_AUTH=0
	ccflags-y += -DCFG_SUPPORT_SAE=0
	ccflags-y += -DCFG_SUPPORT_OWE=0
	ccflags-y += -DCFG_SUPPORT_SUITB=0
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
ifeq ($(CONFIG_SDIO_AGG_THREAD), y)
    ccflags-y += -D_SDIO_RING=1
endif
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), pcie)
    ccflags-y += -D_HIF_PCIE=1
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), usb)
    ccflags-y += -D_HIF_USB=1
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), axi)
    ccflags-y += -D_HIF_AXI=1
else
    $(error Unsuppoted HIF=$(CONFIG_MTK_COMBO_WIFI_HIF)!!)
endif

CONFIG_IPI_2CHAIN_SUPPORT=y
ifeq ($(CONFIG_IPI_2CHAIN_SUPPORT),n)
    ccflags-y += -DCFG_IPI_2CHAIN_SUPPORT=0
else
    ccflags-y += -DCFG_IPI_2CHAIN_SUPPORT=1
endif

CFG_GET_TEMPURATURE=y
ifeq ($(CFG_GET_TEMPURATURE),y)
ccflags-y += -DCFG_GET_TEMPURATURE
endif

# For Thermal module API
# 1: enable, 0: disable
ccflags-y += -DCFG_THERMAL_API_SUPPORT=1
ifeq ($(CFG_ENABLE_1RPD_MMPS_CTRL), y)
    ccflags-y += -DCFG_ENABLE_1RPD_MMPS_CTRL=1
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

ifeq ($(CONFIG_MTK_TC10_FEATURE), y)
    ccflags-y += -DCFG_TC10_FEATURE=1
else
    ccflags-y += -DCFG_TC10_FEATURE=0
endif

ifeq ($(CONFIG_MTK_TC1_FEATURE), y)
    ccflags-y += -DCFG_TC1_FEATURE=1
else
    ccflags-y += -DCFG_TC1_FEATURE=0
endif

# RX NAPI
ifneq ($(CFG_RX_NAPI_SUPPORT),)
	ccflags-y += -DCFG_RX_NAPI_SUPPORT=$(CFG_RX_NAPI_SUPPORT)
endif
#ifeq ($(MODULE_NAME),)
#	MODULE_NAME := wlan_$(shell echo $(strip $(WLAN_CHIP_ID)) | tr A-Z a-z)_$(CONFIG_MTK_COMBO_WIFI_HIF)
#endif

ccflags-y += -DDBG=0
ccflags-y += -I$(src)/os -I$(src)/os/linux/include
ccflags-y += -I$(src)/include -I$(src)/include/nic -I$(src)/include/mgmt -I$(src)/include/chips
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/performance/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(srctree)/drivers/devfreq/
ccflags-y += -I$(srctree)/net
ccflags-y += -I$(srctree)/drivers/misc/mediatek/thermal/common/inc

ifeq ($(MTK_CPU_SCHED),y)
ccflags-y += -DCFG_SUPPORT_MTK_CPU_SCHED=1
ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/sched/mtk_cpu_affinity
else
ccflags-y += -DCFG_SUPPORT_MTK_CPU_SCHED=0
endif

ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), sdio)
ccflags-y += -I$(src)/os/linux/hif/sdio/include
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), pcie)
ccflags-y += -I$(src)/os/linux/hif/common/include
ccflags-y += -I$(src)/os/linux/hif/pcie/include
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), axi)
ccflags-y += -I$(src)/os/linux/hif/common/include
ccflags-y += -I$(src)/os/linux/hif/axi/include
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), usb)
ccflags-y += -I$(src)/os/linux/hif/usb/include
endif

ifneq ($(PLATFORM_FLAGS), )
    ccflags-y += $(PLATFORM_FLAGS)
endif

#ifeq ($(CONFIG_MTK_WIFI_ONLY),$(filter $(CONFIG_MTK_WIFI_ONLY),m y))
#obj-$(CONFIG_MTK_WIFI_ONLY) += $(MODULE_NAME).o
#else
#obj-$(CONFIG_MTK_COMBO_WIFI) += $(MODULE_NAME).o
#obj-y += $(MODULE_NAME).o
#endif
MODULE_NAME := wlan_drv_gen4_mt7663
obj-m += $(MODULE_NAME).o

# ---------------------------------------------------
# Directory List
# ---------------------------------------------------
COMMON_DIR  := common/
OS_DIR      := os/linux/
HIF_COMMON_DIR := $(OS_DIR)hif/common/
ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), sdio)
HIF_DIR	    := os/linux/hif/sdio/
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), pcie)
HIF_DIR     := os/linux/hif/pcie/
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), axi)
HIF_DIR	    := os/linux/hif/axi/
else ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), usb)
HIF_DIR	    := os/linux/hif/usb/
endif
NIC_DIR     := nic/
MGMT_DIR    := mgmt/
CHIPS       := chips/
CHIPS_CMM   := $(CHIPS)common/

ifneq ($(MTK_PLATFORM),)
PLAT_DIR    := os/linux/plat/$(MTK_PLATFORM)/
endif

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
		$(NIC_DIR)nic_rx.o \
		$(NIC_DIR)nic_pwr_mgt.o \
		$(NIC_DIR)nic_rate.o \
		$(NIC_DIR)cmd_buf.o \
		$(NIC_DIR)que_mgt.o \
		$(NIC_DIR)nic_cmd_event.o \
		$(NIC_DIR)nic_umac.o

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
		$(OS_DIR)gl_vendor.o \
		$(OS_DIR)gl_custom.o \
		$(OS_DIR)platform.o

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
		$(MGMT_DIR)wmm.o

# ---------------------------------------------------
# Chips Objects List
# ---------------------------------------------------
MGMT_OBJS += $(MGMT_DIR)stats.o


CHIPS_OBJS += $(CHIPS_CMM)cmm_asic_connac.o
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

# ---------------------------------------------------
# P2P Objects List
# ---------------------------------------------------

COMMON_OBJS += $(COMMON_DIR)wlan_p2p.o

NIC_OBJS += $(NIC_DIR)p2p_nic.o

OS_OBJS += $(OS_DIR)gl_p2p.o \
           $(OS_DIR)gl_p2p_cfg80211.o \
           $(OS_DIR)gl_p2p_init.o \
           $(OS_DIR)gl_p2p_kal.o

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

ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF), sdio)
HIF_OBJS :=  $(HIF_DIR)arm.o \
             $(HIF_DIR)sdio.o \
             $(HIF_DIR)hal_api.o \
             $(HIF_DIR)sdio_test_driver_core.o \
             $(HIF_DIR)sdio_test_driver_ops.o
ifeq ($(CONFIG_SDIO_AGG_THREAD), y)
HIF_OBJS += $(HIF_DIR)sdio_ring.o
endif
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
endif

# ---------------------------------------------------
# Platform Objects List
# ---------------------------------------------------
ifneq ($(MTK_PLATFORM),)

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

$(MODULE_NAME)-objs  += $(COMMON_OBJS)
$(MODULE_NAME)-objs  += $(NIC_OBJS)
$(MODULE_NAME)-objs  += $(OS_OBJS)
$(MODULE_NAME)-objs  += $(HIF_OBJS)
$(MODULE_NAME)-objs  += $(MGMT_OBJS)
$(MODULE_NAME)-objs  += $(CHIPS_OBJS)

#
# mtprealloc
#
ifeq ($(CONFIG_MTK_PREALLOC_MEMORY), y)
ccflags-y += -DCFG_PREALLOC_MEMORY
ccflags-y += -I$(src)/prealloc/include
MODULE_NAME_PREALLOC = $(MODULE_NAME)_prealloc
PREALLOC_OBJS := prealloc/prealloc.o
$(MODULE_NAME_PREALLOC)-objs += $(PREALLOC_OBJS)
obj-m += $(MODULE_NAME_PREALLOC).o
endif

ccflags-y += -DCFG_SUPPORT_WOW_EINT=1

# Define maximum different channels supported for ieee80211_iface_combination setting.
ccflags-y += -DCFG_NUM_DIFFERENT_CHANNELS_STA=1
ccflags-y += -DCFG_NUM_DIFFERENT_CHANNELS_P2P=1

# Define initial driver running mode.
# 0=RUNNING_P2P_MODE, 1=RUNNING_AP_MODE, 2=RUNNING_DUAL_AP_MODE, 3=RUNNING_P2P_AP_MODE
ccflags-y += -DCFG_DRIVER_INITIAL_RUNNING_MODE=3

# Define to enable Android wake_lock
ccflags-y += -DCFG_ENABLE_WAKE_LOCK=1


ccflags-y += -DCFG_ENABLE_EFUSE_MAC_ADDR=1

# Report all bss networks to cfg80211 when do p2p scan
ccflags-y += -DCFG_P2P_SCAN_REPORT_ALL_BSS=0

# Support to change sta, p2p, ap interface names
# y: enable, n: disable
# eg. insmod wlan_mt76x8_usb.ko sta=wlan p2p=p2p ap=ap

# 1: Enable SDIO RX Work Queue De-Aggregation
# 0: Disable (default)
ccflags-y += -DCFG_SDIO_RX_AGG_WORKQUE=0

#Support mobile private cmd, need change cmd size to 512
ccflags-y += -DCFG_ALPS_ANDROID_AOSP_PRIV_CMD
