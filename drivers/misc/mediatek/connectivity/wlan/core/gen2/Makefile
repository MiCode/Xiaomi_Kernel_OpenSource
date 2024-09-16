MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
###############################################################################
# Necessary Check

ifneq ($(KERNEL_OUT),)
    ccflags-y += -imacros $(KERNEL_OUT)/include/generated/autoconf.h
endif

ifeq ($(CONFIG_MTK_COMBO_CHIP),)
    $(error CONFIG_MTK_COMBO_CHIP not defined)
endif

#Force to m
export CONFIG_MTK_COMBO_WIFI=m

# Force build fail on modpost warning
KBUILD_MODPOST_FAIL_ON_WARNINGS := y
###############################################################################

# Driver Configure
CONFIG_MTK_COMBO_WIFI_HIF=sdio
CONFIG_MTK_WIFI_ONLY=n

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)

DRIVER_BUILD_DATE=$(shell date +%Y%m%d%H%M%S)
ccflags-y += -DDRIVER_BUILD_DATE='"$(DRIVER_BUILD_DATE)"'

# ---------------------------------------------------
# Compile Options
# ---------------------------------------------------
ccflags-y += -DLINUX -DMT6628

ccflags-y += -DCFG_SUPPORT_WIFI_FW_LOG_UI=1

ccflags-y += -DCFG_SUPPORT_AGPS_ASSIST=1
ccflags-y += -DCFG_SUPPORT_TSF_USING_BOOTTIME=1
ccflags-y += -DCFG_P2P_LEGACY_COEX_REVISE=1
ccflags-y += -DARP_MONITER_ENABLE=1

ifeq ($(CONFIG_MTK_CONN_LTE_IDC_SUPPORT), y)
    ccflags-y += -DWMT_IDC_SUPPORT=1
else
    ccflags-y += -DWMT_IDC_SUPPORT=0
endif

ifeq ($(CONFIG_MTK_WIFI_MCC_SUPPORT), y)
    ccflags-y += -DCFG_SUPPORT_MCC=1
else
	ccflags-y += -DCFG_SUPPORT_MCC=0
endif

ifeq ($(CONFIG_HAVE_XLOG_FEATURE), y)
    ccflags-y += -DCFG_SUPPORT_XLOG=1
else
    ccflags-y += -DCFG_SUPPORT_XLOG=0
endif

ifeq ($(CONFIG_MTK_AEE_FEATURE), y)
    ccflags-y += -DCFG_SUPPORT_AEE=1
else
    ccflags-y += -DCFG_SUPPORT_AEE=0
endif

#ifeq ($(CONFIG_MTK_COMBO_WIFI_HIF_SDIO1), y)
#    ccflags-y += -D_HIF_SDIO=1
#endif

ifeq ($(CONFIG_MTK_PASSPOINT_R2_SUPPORT), y)
    ccflags-y += -DCFG_SUPPORT_HOTSPOT_2_0=1
    ccflags-y += -DCFG_HS20_DEBUG=1
    ccflags-y += -DCFG_ENABLE_GTK_FRAME_FILTER=1
else
    ccflags-y += -DCFG_SUPPORT_HOTSPOT_2_0=0
    ccflags-y += -DCFG_HS20_DEBUG=0
    ccflags-y += -DCFG_ENABLE_GTK_FRAME_FILTER=0
endif

MTK_MET_PROFILING_SUPPORT = yes
ifeq ($(MTK_MET_PROFILING_SUPPORT), yes)
    ccflags-y += -DCFG_SUPPORT_MET_PROFILING=1
else
    ccflags-y += -DCFG_SUPPORT_MET_PROFILING=0
endif

ifeq ($(CONFIG_MTK_TC1_FEATURE), y)
    ccflags-y += -I$(srctree)/drivers/misc/mediatek/tc1_interface
    ccflags-y += -DCFG_TC1_FEATURE=1
    ccflags-y += -DCFG_SUPPORT_CFG_FILE=1
else
    ccflags-y += -DCFG_TC1_FEATURE=0
endif

ifeq ($(CONFIG_MTK_TC10_FEATURE), y)
    ccflags-y += -DCFG_TC10_FEATURE=1
    ccflags-y += -DCFG_SKIP_MAC_INFO_CHECK
else
    ccflags-y += -DCFG_TC10_FEATURE=0
endif

MTK_SRAM_SIZE_OPTION=0
ifeq ($(CONFIG_ARCH_MT6755), y)
    MTK_SRAM_SIZE_OPTION=2
endif
ifeq ($(CONFIG_MACH_MT6757), y)
    MTK_SRAM_SIZE_OPTION=2
endif
ifeq ($(CONFIG_ARCH_MT6735), y)
    MTK_SRAM_SIZE_OPTION=1
endif
ifeq ($(CONFIG_ARCH_MT6735M), y)
    MTK_SRAM_SIZE_OPTION=1
endif
ifeq ($(CONFIG_MACH_MT6739), y)
    MTK_SRAM_SIZE_OPTION=2
endif
ifeq ($(CONFIG_ARCH_MT6753), y)
    MTK_SRAM_SIZE_OPTION=1
endif
ifeq ($(CONFIG_MACH_MT6763), y)
    MTK_SRAM_SIZE_OPTION=2
endif
ifeq ($(CONFIG_ARCH_MT6580), y)
    MTK_SRAM_SIZE_OPTION=1
endif
ifeq ($(CONFIG_ARCH_MT6570), y)
    MTK_SRAM_SIZE_OPTION=1
endif
ifeq ($(CONFIG_ARCH_MT8163), y)
    MTK_SRAM_SIZE_OPTION=1
endif
ifeq ($(CONFIG_ARCH_MT8167), y)
    MTK_SRAM_SIZE_OPTION=2
endif
ccflags-y += -DCFG_SRAM_SIZE_OPTION=$(MTK_SRAM_SIZE_OPTION)

ifeq ($(strip $(TRUSTONIC_TEE_SUPPORT)),yes)
ifeq ($(strip $(MTK_TEE_CCCI_SECURE_SHARE_MEM_SUPPORT)),yes)
    ccflags-y += -DTRUSTONIC_TEE_SUPPORT
    ccflags-y += -DMTK_TEE_CCCI_SECURE_SHARE_MEM_SUPPORT
endif
endif

ccflags-y += -D_HIF_SDIO=1

ccflags-y += -DDBG=0
ccflags-y += -I$(src)/os -I$(src)/os/linux/include -I$(src)/os/linux/hif/ahb/include
ccflags-y += -I$(src)/include -I$(src)/include/nic -I$(src)/include/mgmt
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/connectivity/common
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/
ccflags-y += -I$(src)/../../../common/common_main/include
ccflags-y += -I$(src)/../../../common/common_main/linux/include
ccflags-y += -DMTK_WCN_WMT_STP_EXP_SYMBOL_ABSTRACT

MODULE_NAME := wlan_drv_gen2
#obj-$(CONFIG_MTK_COMBO_WIFI) += $(MODULE_NAME).o

#if CONFIG_MTK_COMBO_WIFI=m ==> obj-m means ko module, not build in obj-y
obj-m += $(MODULE_NAME).o

# ---------------------------------------------------
# Directory List
# ---------------------------------------------------
COMMON_DIR  := common/
OS_DIR      := os/linux/
HIF_DIR	    := os/linux/hif/ahb/
NIC_DIR     := nic/
MGMT_DIR    := mgmt/
FWCFG_DIR   := $(srctree)/drivers/misc/mediatek/connectivity/wlan/gen2/$(MTK_PROJECT)/
DMA_DIR     := ../../../../platform/$(call lc,$(MTK_PLATFORM))/kernel/drivers/wifi/
PLAT_DIR    := os/linux/plat/$(MTK_PLATFORM)/
HIF_AHB_PDMA := $(HIF_DIR)$(MTK_PLATFORM)/
#$(call lc,$(MTK_PLATFORM))


# ---------------------------------------------------
# Objects List
# ---------------------------------------------------

COMMON_OBJS :=	$(COMMON_DIR)dump.o \
	       		$(COMMON_DIR)wlan_lib.o \
	       		$(COMMON_DIR)wlan_oid.o \
	       		$(COMMON_DIR)wlan_bow.o \
				$(COMMON_DIR)debug.o

NIC_OBJS := $(NIC_DIR)nic.o \
			$(NIC_DIR)nic_tx.o \
			$(NIC_DIR)nic_rx.o \
			$(NIC_DIR)nic_pwr_mgt.o \
			$(NIC_DIR)cmd_buf.o \
			$(NIC_DIR)que_mgt.o \
			$(NIC_DIR)nic_cmd_event.o

OS_OBJS :=	$(OS_DIR)gl_init.o \
			$(OS_DIR)gl_kal.o  \
			$(OS_DIR)gl_bow.o \
			$(OS_DIR)gl_wext.o \
			$(OS_DIR)gl_wext_priv.o \
			$(OS_DIR)gl_rst.o \
			$(OS_DIR)gl_cfg80211.o \
			$(OS_DIR)gl_vendor.o \
			$(OS_DIR)platform.o \
			$(OS_DIR)gl_sys.o \
			$(OS_DIR)gl_proc.o

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
		 	 $(MGMT_DIR)sec_fsm.o \
             $(MGMT_DIR)swcr.o \
 			 $(MGMT_DIR)swcr.o \
			 $(MGMT_DIR)roaming_fsm.o \
			 $(MGMT_DIR)hs20.o \
			$(MGMT_DIR)wnm.o \
			$(MGMT_DIR)qosmap.o \

# ---------------------------------------------------
# TDLS Objects List
# ---------------------------------------------------
MGMT_OBJS += $(MGMT_DIR)tdls.o \
			 $(MGMT_DIR)tdls_com.o

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

MGMT_OBJS += $(MGMT_DIR)p2p_assoc.o \
             $(MGMT_DIR)p2p_bss.o \
             $(MGMT_DIR)p2p_fsm.o \
             $(MGMT_DIR)p2p_func.o \
             $(MGMT_DIR)p2p_rlm.o \
             $(MGMT_DIR)p2p_rlm_obss.o \
             $(MGMT_DIR)p2p_scan.o \
             $(MGMT_DIR)p2p_ie.o \
             $(MGMT_DIR)p2p_state.o \
             $(MGMT_DIR)wnm.o \
             $(MGMT_DIR)wmm.o

MGMT_OBJS += $(MGMT_DIR)wapi.o

ifeq ($(WLAN_PROC), y)
OS_OBJS += gl_proc.o
endif

ifeq ($(CONFIG_ARCH_MT7623), y)
HIF_AHB_PDMA = $(HIF_DIR)mt8127/
endif
HIF_OBJS :=  $(HIF_DIR)arm.o \
             $(HIF_DIR)ahb.o \
             $(HIF_AHB_PDMA)ahb_pdma.o
ifeq ($(CONFIG_ARCH_MT6755), y)
PLAT_OBJS := $(PLAT_DIR)plat_priv.o
$(MODULE_NAME)-objs  += $(PLAT_OBJS)
endif

ifeq ($(CONFIG_MACH_MT6757), y)
PLAT_OBJS := $(PLAT_DIR)plat_priv.o
$(MODULE_NAME)-objs  += $(PLAT_OBJS)
endif

ifeq ($(CONFIG_ARCH_MT6570), y)
PLAT_OBJS := $(PLAT_DIR)plat_priv.o
$(MODULE_NAME)-objs  += $(PLAT_OBJS)
endif

ifeq ($(CONFIG_ARCH_MT6735), y)
PLAT_OBJS := $(PLAT_DIR)plat_priv.o
$(MODULE_NAME)-objs  += $(PLAT_OBJS)
endif

ifeq ($(CONFIG_ARCH_MT6735M), y)
PLAT_OBJS := $(PLAT_DIR)plat_priv.o
$(MODULE_NAME)-objs  += $(PLAT_OBJS)
endif

ifeq ($(CONFIG_MACH_MT6735M), y)
PLAT_OBJS := $(PLAT_DIR)plat_priv.o
$(MODULE_NAME)-objs  += $(PLAT_OBJS)
endif

ifeq ($(CONFIG_MACH_MT6763), y)
PLAT_OBJS := $(PLAT_DIR)plat_priv.o
$(MODULE_NAME)-objs  += $(PLAT_OBJS)
endif

ifeq ($(CONFIG_MACH_MT6739), y)
PLAT_OBJS := $(PLAT_DIR)plat_priv.o
$(MODULE_NAME)-objs  += $(PLAT_OBJS)
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
# WLAN Parse BUILD VARINAT
# ---------------------------------------------------
ifeq ($(TARGET_BUILD_VARIANT), eng)
    ccflags-y += -DWLAN_ENG_LOAD
endif
ifeq ($(TARGET_BUILD_VARIANT), userdebug)
    ccflags-y += -DWLAN_USERDEBUG_LOAD
endif
ifeq ($(TARGET_BUILD_VARIANT), user)
    ccflags-y += -DWLAN_USER_LOAD
endif
# ---------------------------------------------------

$(MODULE_NAME)-objs  += $(COMMON_OBJS)
$(MODULE_NAME)-objs  += $(NIC_OBJS)
$(MODULE_NAME)-objs  += $(OS_OBJS)
$(MODULE_NAME)-objs  += $(HIF_OBJS)
$(MODULE_NAME)-objs  += $(MGMT_OBJS)

