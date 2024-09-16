MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
###############################################################################
# Necessary Check

ifeq ($(AUTOCONF_H),)
    $(error AUTOCONF_H is not defined)
endif

ccflags-y += -imacros $(AUTOCONF_H)

# Force build fail on modpost warning
KBUILD_MODPOST_FAIL_ON_WARNINGS := y
###############################################################################

# ---------------------------------------------------
# Compile Options
# ---------------------------------------------------
ifneq ($(filter "CONSYS_6797" "CONSYS_6759" "CONSYS_6758" "CONSYS_6775" "CONSYS_6771", $(CONFIG_MTK_COMBO_CHIP)),)
WLAN_CHIP_ID=MT6631
else
WLAN_CHIP_ID=MT6630
endif
DRIVER_BUILD_DATE=$(shell date +%Y%m%d%H%M%S)
ccflags-y += -DDRIVER_BUILD_DATE='"$(DRIVER_BUILD_DATE)"'

ccflags-y += -DLINUX -D$(WLAN_CHIP_ID)

ccflags-y += -DCFG_SUPPORT_DEBUG_FS=0
ccflags-y += -DWLAN_INCLUDE_PROC
ccflags-y += -DCFG_SUPPORT_AGPS_ASSIST=1
ccflags-y += -DCFG_SUPPORT_TSF_USING_BOOTTIME=1
ccflags-y += -DARP_MONITER_ENABLE=1

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

ifeq ($(CONFIG_MTK_CONN_LTE_IDC_SUPPORT),y)
    ccflags-y += -DWMT_IDC_SUPPORT=1
else
    ccflags-y += -DWMT_IDC_SUPPORT=0
endif


ccflags-y += -DDBG=0

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(srctree)/drivers/devfreq
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include/

ifeq ($(WLAN_CHIP_ID), MT6631)
# temp disable emi_mpu on these platforms since it is not ready
ifneq ($(filter "CONSYS_6775" "CONSYS_6771", $(CONFIG_MTK_COMBO_CHIP)),)
ccflags-y += -DCFG_ENABLE_EMI_MPU=0
else
ccflags-y += -DCFG_ENABLE_EMI_MPU=1
endif
# Only MT6631 firmware support to set or get log level by UI
ccflags-y += -DCFG_SUPPORT_WIFI_FW_LOG_UI=1
ccflags-y += -I$(src)/os -I$(src)/os/linux/include -I$(src)/os/linux/hif/ahb_sdioLike/include
else
ccflags-y += -D_HIF_SDIO=1
ccflags-y += -I$(src)/os -I$(src)/os/linux/include -I$(src)/os/linux/hif/sdio/include
endif

ccflags-y += -I$(src)/include -I$(src)/include/nic -I$(src)/include/mgmt
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include

WMT_ANDROID_MK := $(TOP)/vendor/mediatek/kernel_modules/connectivity/common/Android.mk
ifeq (,$(wildcard $(WMT_ANDROID_MK)))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/connectivity/common/common_main/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/connectivity/common/common_main/linux/include
else
ccflags-y += -I$(TOP)/vendor/mediatek/kernel_modules/connectivity/common/common_main/include
ccflags-y += -I$(TOP)/vendor/mediatek/kernel_modules/connectivity/common/common_main/linux/include
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/connectivity/common/
ccflags-y += -D MTK_WCN_WMT_STP_EXP_SYMBOL_ABSTRACT

MODULE_NAME := wlan_drv_gen3
obj-m += $(MODULE_NAME).o

# ---------------------------------------------------
# Directory List
# ---------------------------------------------------
COMMON_DIR  := common/
OS_DIR      := os/linux/
NIC_DIR     := nic/
MGMT_DIR    := mgmt/
FWCFG_DIR   := $(src)/$(MTK_PROJECT)/

ifeq ($(WLAN_CHIP_ID), MT6631)
HIF_DIR     := os/linux/hif/ahb_sdioLike/
PLAT_DIR    := os/linux/plat/$(MTK_PLATFORM)/
else
HIF_DIR	    := os/linux/hif/sdio/
endif

# ---------------------------------------------------
# Objects List
# ---------------------------------------------------

COMMON_OBJS := $(COMMON_DIR)dump.o \
	       $(COMMON_DIR)wlan_lib.o \
	       $(COMMON_DIR)wlan_oid.o \
	       $(COMMON_DIR)wlan_bow.o \
	       $(COMMON_DIR)debug.o

NIC_OBJS := $(NIC_DIR)nic.o \
	    $(NIC_DIR)nic_tx.o \
	    $(NIC_DIR)nic_rx.o \
	    $(NIC_DIR)nic_pwr_mgt.o \
	    $(NIC_DIR)nic_rate.o \
	    $(NIC_DIR)cmd_buf.o \
	    $(NIC_DIR)que_mgt.o \
	    $(NIC_DIR)nic_cmd_event.o

OS_OBJS :=  $(OS_DIR)gl_init.o \
	    $(OS_DIR)gl_kal.o  \
	    $(OS_DIR)gl_bow.o \
	    $(OS_DIR)gl_wext.o \
	    $(OS_DIR)gl_wext_priv.o \
	    $(OS_DIR)gl_ate_agent.o \
	    $(OS_DIR)gl_qa_agent.o \
	    $(OS_DIR)gl_hook_api.o \
	    $(OS_DIR)gl_proc.o \
	    $(OS_DIR)gl_rst.o \
	    $(OS_DIR)gl_cfg80211.o \
	    $(OS_DIR)gl_vendor.o \
	    $(OS_DIR)platform.o

MGMT_OBJS := $(MGMT_DIR)ais_fsm.o \
	     $(MGMT_DIR)aaa_fsm.o \
	     $(MGMT_DIR)assoc.o \
	     $(MGMT_DIR)auth.o \
	     $(MGMT_DIR)bss.o \
	     $(MGMT_DIR)cnm.o \
	     $(MGMT_DIR)cnm_timer.o \
	     $(MGMT_DIR)cnm_mem.o \
	     $(MGMT_DIR)hem_mbox.o \
	     $(MGMT_DIR)mib.o \
	     $(MGMT_DIR)privacy.o  \
	     $(MGMT_DIR)rate.o \
	     $(MGMT_DIR)rlm.o \
	     $(MGMT_DIR)rlm_domain.o \
	     $(MGMT_DIR)rlm_obss.o \
	     $(MGMT_DIR)rlm_protection.o \
	     $(MGMT_DIR)rsn.o \
	     $(MGMT_DIR)saa_fsm.o \
	     $(MGMT_DIR)scan.o \
	     $(MGMT_DIR)scan_fsm.o \
	     $(MGMT_DIR)swcr.o \
	     $(MGMT_DIR)roaming_fsm.o \
	     $(MGMT_DIR)tkip_mic.o \
	     $(MGMT_DIR)hs20.o \
	     $(MGMT_DIR)tdls.o \
	     $(MGMT_DIR)wnm.o \
	     $(MGMT_DIR)qosmap.o \
	     $(MGMT_DIR)wmm.o
# ---------------------------------------------------
# STATS Objects List
# ---------------------------------------------------
MGMT_OBJS += $(MGMT_DIR)stats.o

# ---------------------------------------------------
# P2P Objects List
# ---------------------------------------------------

COMMON_OBJS += $(COMMON_DIR)wlan_p2p.o

NIC_OBJS += $(NIC_DIR)p2p_nic.o

OS_OBJS += $(OS_DIR)gl_p2p.o \
	   $(OS_DIR)gl_p2p_cfg80211.o \
	   $(OS_DIR)gl_p2p_init.o \
	   $(OS_DIR)gl_p2p_kal.o

MGMT_OBJS += $(MGMT_DIR)p2p_dev_fsm.o \
	     $(MGMT_DIR)p2p_dev_state.o \
	     $(MGMT_DIR)p2p_role_fsm.o \
	     $(MGMT_DIR)p2p_role_state.o \
	     $(MGMT_DIR)p2p_func.o \
	     $(MGMT_DIR)p2p_scan.o \
	     $(MGMT_DIR)p2p_ie.o \
	     $(MGMT_DIR)p2p_rlm.o \
	     $(MGMT_DIR)p2p_assoc.o \
	     $(MGMT_DIR)p2p_bss.o \
	     $(MGMT_DIR)p2p_rlm_obss.o

MGMT_OBJS += $(MGMT_DIR)wapi.o


HIF_OBJS :=  $(HIF_DIR)arm.o
ifeq ($(WLAN_CHIP_ID), MT6631)
HIF_OBJS += $(HIF_DIR)ahb.o\
	    $(HIF_DIR)sdio_bus_driver.o

ifeq ($(CONFIG_ARCH_MT6797), y)
ccflags-y += -DMT6797
HIF_OBJS += $(HIF_DIR)ahb_pdma.o
else
HIF_OBJS += $(HIF_DIR)ahb_cqdma.o
endif
ifneq ($(wildcard $(src)/$(PLAT_DIR)plat_priv.c),)
PLAT_OBJS := $(PLAT_DIR)plat_priv.o
endif
$(MODULE_NAME)-objs  += $(PLAT_OBJS)

else
HIF_OBJS += $(HIF_DIR)sdio.o
endif

# ---------------------------------------------------
# FW customization
# ---------------------------------------------------
FW_CUSTOMIZATION = y
ifeq ($(FW_CUSTOMIZATION), y)
ifneq ($(wildcard $(FWCFG_DIR)fw_config.c),)
    COMMON_OBJS += $(MTK_PROJECT)/fw_config.o
endif
    COMMON_OBJS += $(COMMON_DIR)fwcfg.o
    ccflags-y += -DFW_CFG_SUPPORT
    ccflags-y += -DENABLED_IN_ENGUSERDEBUG
endif
# ---------------------------------------------------

$(MODULE_NAME)-objs  += $(COMMON_OBJS)
$(MODULE_NAME)-objs  += $(NIC_OBJS)
$(MODULE_NAME)-objs  += $(OS_OBJS)
$(MODULE_NAME)-objs  += $(HIF_OBJS)
$(MODULE_NAME)-objs  += $(MGMT_OBJS)

