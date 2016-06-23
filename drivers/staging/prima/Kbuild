# We can build either as part of a standalone Kernel build or part
# of an Android build.  Determine which mechanism is being used
ifeq ($(MODNAME),)
	KERNEL_BUILD := 1
else
	KERNEL_BUILD := 0
endif

ifeq ($(KERNEL_BUILD),1)
	# These are provided in Android-based builds
	# Need to explicitly define for Kernel-based builds
	MODNAME := wlan
	WLAN_ROOT := drivers/staging/prima
endif

ifeq ($(KERNEL_BUILD), 0)
	# These are configurable via Kconfig for kernel-based builds
	# Need to explicitly configure for Android-based builds

	#Flag to enable BlueTooth AMP feature
	CONFIG_PRIMA_WLAN_BTAMP := n

	#Flag to enable Legacy Fast Roaming(LFR)
	CONFIG_PRIMA_WLAN_LFR := y

	#JB kernel has PMKSA patches, hence enabling this flag
	CONFIG_PRIMA_WLAN_OKC := y

	# JB kernel has CPU enablement patches, so enable
	CONFIG_PRIMA_WLAN_11AC_HIGH_TP := y

	#Flag to enable TDLS feature
	CONFIG_QCOM_TDLS := y

	#Flag to enable Fast Transition (11r) feature
	CONFIG_QCOM_VOWIFI_11R := y

	#Flag to enable Protected Managment Frames (11w) feature
	ifneq ($(CONFIG_PRONTO_WLAN),)
	CONFIG_WLAN_FEATURE_11W := y
	endif

	#Flag to enable new Linux Regulatory implementation
	CONFIG_ENABLE_LINUX_REG := y

endif

# To enable CONFIG_QCOM_ESE_UPLOAD, dependent config
# CONFIG_QCOM_ESE must be enabled.
CONFIG_QCOM_ESE := n
CONFIG_QCOM_ESE_UPLOAD := n

# Feature flags which are not (currently) configurable via Kconfig

#Whether to build debug version
BUILD_DEBUG_VERSION := 1

#Enable this flag to build driver in diag version
BUILD_DIAG_VERSION := 1

#Do we panic on bug?  default is to warn
PANIC_ON_BUG := 1

#Re-enable wifi on WDI timeout
RE_ENABLE_WIFI_ON_WDI_TIMEOUT := 0

#Measure Roam Delay
MEASURE_ROAM_TIME_DELAY := 0

ifeq ($(CONFIG_CFG80211),y)
HAVE_CFG80211 := 1
else
ifeq ($(CONFIG_CFG80211),m)
HAVE_CFG80211 := 1
else
HAVE_CFG80211 := 0
endif
endif

############ BAP ############
BAP_DIR :=	CORE/BAP
BAP_INC_DIR :=	$(BAP_DIR)/inc
BAP_SRC_DIR :=	$(BAP_DIR)/src

BAP_INC := 	-I$(WLAN_ROOT)/$(BAP_INC_DIR) \
		-I$(WLAN_ROOT)/$(BAP_SRC_DIR)

BAP_OBJS := 	$(BAP_SRC_DIR)/bapApiData.o \
		$(BAP_SRC_DIR)/bapApiDebug.o \
		$(BAP_SRC_DIR)/bapApiExt.o \
		$(BAP_SRC_DIR)/bapApiHCBB.o \
		$(BAP_SRC_DIR)/bapApiInfo.o \
		$(BAP_SRC_DIR)/bapApiLinkCntl.o \
		$(BAP_SRC_DIR)/bapApiLinkSupervision.o \
		$(BAP_SRC_DIR)/bapApiStatus.o \
		$(BAP_SRC_DIR)/bapApiTimer.o \
		$(BAP_SRC_DIR)/bapModule.o \
		$(BAP_SRC_DIR)/bapRsn8021xAuthFsm.o \
		$(BAP_SRC_DIR)/bapRsn8021xPrf.o \
		$(BAP_SRC_DIR)/bapRsn8021xSuppRsnFsm.o \
		$(BAP_SRC_DIR)/bapRsnAsfPacket.o \
		$(BAP_SRC_DIR)/bapRsnSsmAesKeyWrap.o \
		$(BAP_SRC_DIR)/bapRsnSsmEapol.o \
		$(BAP_SRC_DIR)/bapRsnSsmReplayCtr.o \
		$(BAP_SRC_DIR)/bapRsnTxRx.o \
		$(BAP_SRC_DIR)/btampFsm.o \
		$(BAP_SRC_DIR)/btampHCI.o

############ DXE ############
DXE_DIR :=	CORE/DXE
DXE_INC_DIR :=	$(DXE_DIR)/inc
DXE_SRC_DIR :=	$(DXE_DIR)/src

DXE_INC := 	-I$(WLAN_ROOT)/$(DXE_INC_DIR) \
		-I$(WLAN_ROOT)/$(DXE_SRC_DIR)

DXE_OBJS = 	$(DXE_SRC_DIR)/wlan_qct_dxe.o \
		$(DXE_SRC_DIR)/wlan_qct_dxe_cfg_i.o

############ HDD ############
HDD_DIR :=	CORE/HDD
HDD_INC_DIR :=	$(HDD_DIR)/inc
HDD_SRC_DIR :=	$(HDD_DIR)/src

HDD_INC := 	-I$(WLAN_ROOT)/$(HDD_INC_DIR) \
		-I$(WLAN_ROOT)/$(HDD_SRC_DIR)

HDD_OBJS := 	$(HDD_SRC_DIR)/bap_hdd_main.o \
		$(HDD_SRC_DIR)/wlan_hdd_assoc.o \
		$(HDD_SRC_DIR)/wlan_hdd_cfg.o \
		$(HDD_SRC_DIR)/wlan_hdd_debugfs.o \
		$(HDD_SRC_DIR)/wlan_hdd_dev_pwr.o \
		$(HDD_SRC_DIR)/wlan_hdd_dp_utils.o \
		$(HDD_SRC_DIR)/wlan_hdd_early_suspend.o \
		$(HDD_SRC_DIR)/wlan_hdd_ftm.o \
		$(HDD_SRC_DIR)/wlan_hdd_hostapd.o \
		$(HDD_SRC_DIR)/wlan_hdd_main.o \
		$(HDD_SRC_DIR)/wlan_hdd_mib.o \
		$(HDD_SRC_DIR)/wlan_hdd_oemdata.o \
		$(HDD_SRC_DIR)/wlan_hdd_scan.o \
		$(HDD_SRC_DIR)/wlan_hdd_softap_tx_rx.o \
		$(HDD_SRC_DIR)/wlan_hdd_tx_rx.o \
                $(HDD_SRC_DIR)/wlan_hdd_trace.o \
		$(HDD_SRC_DIR)/wlan_hdd_wext.o \
		$(HDD_SRC_DIR)/wlan_hdd_wmm.o \
		$(HDD_SRC_DIR)/wlan_hdd_wowl.o

ifeq ($(HAVE_CFG80211),1)
HDD_OBJS +=	$(HDD_SRC_DIR)/wlan_hdd_cfg80211.o \
		$(HDD_SRC_DIR)/wlan_hdd_p2p.o
endif

ifeq ($(CONFIG_QCOM_TDLS),y)
HDD_OBJS +=	$(HDD_SRC_DIR)/wlan_hdd_tdls.o
endif

############ MAC ############
MAC_DIR :=	CORE/MAC
MAC_INC_DIR :=	$(MAC_DIR)/inc
MAC_SRC_DIR :=	$(MAC_DIR)/src

MAC_INC := 	-I$(WLAN_ROOT)/$(MAC_INC_DIR) \
		-I$(WLAN_ROOT)/$(MAC_SRC_DIR)/dph \
		-I$(WLAN_ROOT)/$(MAC_SRC_DIR)/include \
		-I$(WLAN_ROOT)/$(MAC_SRC_DIR)/pe/include \
		-I$(WLAN_ROOT)/$(MAC_SRC_DIR)/pe/lim

MAC_CFG_OBJS := $(MAC_SRC_DIR)/cfg/cfgApi.o \
		$(MAC_SRC_DIR)/cfg/cfgDebug.o \
		$(MAC_SRC_DIR)/cfg/cfgParamName.o \
		$(MAC_SRC_DIR)/cfg/cfgProcMsg.o \
		$(MAC_SRC_DIR)/cfg/cfgSendMsg.o

MAC_DPH_OBJS :=	$(MAC_SRC_DIR)/dph/dphHashTable.o

MAC_LIM_OBJS := $(MAC_SRC_DIR)/pe/lim/limAIDmgmt.o \
		$(MAC_SRC_DIR)/pe/lim/limAdmitControl.o \
		$(MAC_SRC_DIR)/pe/lim/limApi.o \
		$(MAC_SRC_DIR)/pe/lim/limAssocUtils.o \
		$(MAC_SRC_DIR)/pe/lim/limDebug.o \
		$(MAC_SRC_DIR)/pe/lim/limFT.o \
		$(MAC_SRC_DIR)/pe/lim/limIbssPeerMgmt.o \
		$(MAC_SRC_DIR)/pe/lim/limLinkMonitoringAlgo.o \
		$(MAC_SRC_DIR)/pe/lim/limLogDump.o \
		$(MAC_SRC_DIR)/pe/lim/limP2P.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessActionFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessAssocReqFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessAssocRspFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessAuthFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessBeaconFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessCfgUpdates.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessDeauthFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessDisassocFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessLmmMessages.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessMessageQueue.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessMlmReqMessages.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessMlmRspMessages.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessProbeReqFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessProbeRspFrame.o \
		$(MAC_SRC_DIR)/pe/lim/limProcessSmeReqMessages.o \
		$(MAC_SRC_DIR)/pe/lim/limPropExtsUtils.o \
		$(MAC_SRC_DIR)/pe/lim/limRoamingAlgo.o \
		$(MAC_SRC_DIR)/pe/lim/limScanResultUtils.o \
		$(MAC_SRC_DIR)/pe/lim/limSecurityUtils.o \
		$(MAC_SRC_DIR)/pe/lim/limSendManagementFrames.o \
		$(MAC_SRC_DIR)/pe/lim/limSendMessages.o \
		$(MAC_SRC_DIR)/pe/lim/limSendSmeRspMessages.o \
		$(MAC_SRC_DIR)/pe/lim/limSerDesUtils.o \
		$(MAC_SRC_DIR)/pe/lim/limSession.o \
		$(MAC_SRC_DIR)/pe/lim/limSessionUtils.o \
		$(MAC_SRC_DIR)/pe/lim/limSmeReqUtils.o \
		$(MAC_SRC_DIR)/pe/lim/limStaHashApi.o \
		$(MAC_SRC_DIR)/pe/lim/limTimerUtils.o \
		$(MAC_SRC_DIR)/pe/lim/limTrace.o \
		$(MAC_SRC_DIR)/pe/lim/limUtils.o

ifeq ($(CONFIG_QCOM_ESE),y)
ifneq ($(CONFIG_QCOM_ESE_UPLOAD),y)
MAC_LIM_OBJS += $(MAC_SRC_DIR)/pe/lim/limProcessEseFrame.o
endif
endif

ifeq ($(CONFIG_QCOM_TDLS),y)
MAC_LIM_OBJS += $(MAC_SRC_DIR)/pe/lim/limProcessTdls.o
endif

MAC_PMM_OBJS := $(MAC_SRC_DIR)/pe/pmm/pmmAP.o \
		$(MAC_SRC_DIR)/pe/pmm/pmmApi.o \
		$(MAC_SRC_DIR)/pe/pmm/pmmDebug.o

MAC_SCH_OBJS := $(MAC_SRC_DIR)/pe/sch/schApi.o \
		$(MAC_SRC_DIR)/pe/sch/schBeaconGen.o \
		$(MAC_SRC_DIR)/pe/sch/schBeaconProcess.o \
		$(MAC_SRC_DIR)/pe/sch/schDebug.o \
		$(MAC_SRC_DIR)/pe/sch/schMessage.o

MAC_RRM_OBJS :=	$(MAC_SRC_DIR)/pe/rrm/rrmApi.o

MAC_OBJS := 	$(MAC_CFG_OBJS) \
		$(MAC_DPH_OBJS) \
		$(MAC_LIM_OBJS) \
		$(MAC_PMM_OBJS) \
		$(MAC_SCH_OBJS) \
		$(MAC_RRM_OBJS)

############ SAP ############
SAP_DIR :=	CORE/SAP
SAP_INC_DIR :=	$(SAP_DIR)/inc
SAP_SRC_DIR :=	$(SAP_DIR)/src

SAP_INC := 	-I$(WLAN_ROOT)/$(SAP_INC_DIR) \
		-I$(WLAN_ROOT)/$(SAP_SRC_DIR)

SAP_OBJS :=	$(SAP_SRC_DIR)/sapApiLinkCntl.o \
		$(SAP_SRC_DIR)/sapChSelect.o \
		$(SAP_SRC_DIR)/sapFsm.o \
		$(SAP_SRC_DIR)/sapModule.o

############ SME ############
SME_DIR :=	CORE/SME
SME_INC_DIR :=	$(SME_DIR)/inc
SME_SRC_DIR :=	$(SME_DIR)/src

SME_INC := 	-I$(WLAN_ROOT)/$(SME_INC_DIR) \
		-I$(WLAN_ROOT)/$(SME_SRC_DIR)/csr

SME_CCM_OBJS := $(SME_SRC_DIR)/ccm/ccmApi.o \
		$(SME_SRC_DIR)/ccm/ccmLogDump.o

SME_CSR_OBJS := $(SME_SRC_DIR)/csr/csrApiRoam.o \
		$(SME_SRC_DIR)/csr/csrApiScan.o \
		$(SME_SRC_DIR)/csr/csrCmdProcess.o \
		$(SME_SRC_DIR)/csr/csrLinkList.o \
		$(SME_SRC_DIR)/csr/csrLogDump.o \
		$(SME_SRC_DIR)/csr/csrNeighborRoam.o \
		$(SME_SRC_DIR)/csr/csrUtil.o

ifeq ($(CONFIG_QCOM_ESE),y)
ifneq ($(CONFIG_QCOM_ESE_UPLOAD),y)
SME_CSR_OBJS += $(SME_SRC_DIR)/csr/csrEse.o
endif
endif

ifeq ($(CONFIG_QCOM_TDLS),y)
SME_CSR_OBJS += $(SME_SRC_DIR)/csr/csrTdlsProcess.o
endif

SME_PMC_OBJS := $(SME_SRC_DIR)/pmc/pmcApi.o \
		$(SME_SRC_DIR)/pmc/pmc.o \
		$(SME_SRC_DIR)/pmc/pmcLogDump.o

SME_QOS_OBJS := $(SME_SRC_DIR)/QoS/sme_Qos.o

SME_CMN_OBJS := $(SME_SRC_DIR)/sme_common/sme_Api.o \
		$(SME_SRC_DIR)/sme_common/sme_FTApi.o \
		$(SME_SRC_DIR)/sme_common/sme_Trace.o

SME_BTC_OBJS := $(SME_SRC_DIR)/btc/btcApi.o

SME_OEM_DATA_OBJS := $(SME_SRC_DIR)/oemData/oemDataApi.o

SME_P2P_OBJS = $(SME_SRC_DIR)/p2p/p2p_Api.o

SME_RRM_OBJS := $(SME_SRC_DIR)/rrm/sme_rrm.o

SME_OBJS :=	$(SME_BTC_OBJS) \
		$(SME_CCM_OBJS) \
		$(SME_CMN_OBJS) \
		$(SME_CSR_OBJS) \
		$(SME_OEM_DATA_OBJS) \
		$(SME_P2P_OBJS) \
		$(SME_PMC_OBJS) \
		$(SME_QOS_OBJS) \
		$(SME_RRM_OBJS)

############ SVC ############
SVC_DIR :=	CORE/SVC
SVC_INC_DIR :=	$(SVC_DIR)/inc
SVC_SRC_DIR :=	$(SVC_DIR)/src

SVC_INC := 	-I$(WLAN_ROOT)/$(SVC_INC_DIR) \
		-I$(WLAN_ROOT)/$(SVC_DIR)/external

BTC_SRC_DIR :=	$(SVC_SRC_DIR)/btc
BTC_OBJS :=	$(BTC_SRC_DIR)/wlan_btc_svc.o

NLINK_SRC_DIR := $(SVC_SRC_DIR)/nlink
NLINK_OBJS :=	$(NLINK_SRC_DIR)/wlan_nlink_srv.o

PTT_SRC_DIR :=	$(SVC_SRC_DIR)/ptt
PTT_OBJS :=	$(PTT_SRC_DIR)/wlan_ptt_sock_svc.o

WLAN_LOGGING_SRC_DIR := $(SVC_SRC_DIR)/logging
WLAN_LOGGING_OBJS := $(WLAN_LOGGING_SRC_DIR)/wlan_logging_sock_svc.o

SVC_OBJS :=	$(BTC_OBJS) \
		$(NLINK_OBJS) \
		$(PTT_OBJS) \
                $(WLAN_LOGGING_OBJS)

############ SYS ############
SYS_DIR :=	CORE/SYS

SYS_INC := 	-I$(WLAN_ROOT)/$(SYS_DIR)/common/inc \
		-I$(WLAN_ROOT)/$(SYS_DIR)/legacy/src/pal/inc \
		-I$(WLAN_ROOT)/$(SYS_DIR)/legacy/src/platform/inc \
		-I$(WLAN_ROOT)/$(SYS_DIR)/legacy/src/system/inc \
		-I$(WLAN_ROOT)/$(SYS_DIR)/legacy/src/utils/inc

SYS_COMMON_SRC_DIR := $(SYS_DIR)/common/src
SYS_LEGACY_SRC_DIR := $(SYS_DIR)/legacy/src
SYS_OBJS :=	$(SYS_COMMON_SRC_DIR)/wlan_qct_sys.o \
		$(SYS_LEGACY_SRC_DIR)/pal/src/palApiComm.o \
		$(SYS_LEGACY_SRC_DIR)/pal/src/palTimer.o \
		$(SYS_LEGACY_SRC_DIR)/platform/src/VossWrapper.o \
		$(SYS_LEGACY_SRC_DIR)/system/src/macInitApi.o \
		$(SYS_LEGACY_SRC_DIR)/system/src/sysEntryFunc.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/dot11f.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/logApi.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/logDump.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/macTrace.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/parserApi.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/utilsApi.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/utilsParser.o

############ TL ############
TL_DIR :=	CORE/TL
TL_INC_DIR :=	$(TL_DIR)/inc
TL_SRC_DIR :=	$(TL_DIR)/src

TL_INC := 	-I$(WLAN_ROOT)/$(TL_INC_DIR) \
		-I$(WLAN_ROOT)/$(TL_SRC_DIR)

TL_OBJS := 	$(TL_SRC_DIR)/wlan_qct_tl.o \
		$(TL_SRC_DIR)/wlan_qct_tl_ba.o \
		$(TL_SRC_DIR)/wlan_qct_tl_hosupport.o \
               $(TL_SRC_DIR)/wlan_qct_tl_trace.o

############ VOSS ############
VOSS_DIR :=	CORE/VOSS
VOSS_INC_DIR :=	$(VOSS_DIR)/inc
VOSS_SRC_DIR :=	$(VOSS_DIR)/src

VOSS_INC := 	-I$(WLAN_ROOT)/$(VOSS_INC_DIR) \
		-I$(WLAN_ROOT)/$(VOSS_SRC_DIR)

VOSS_OBJS :=    $(VOSS_SRC_DIR)/vos_api.o \
		$(VOSS_SRC_DIR)/vos_event.o \
		$(VOSS_SRC_DIR)/vos_getBin.o \
		$(VOSS_SRC_DIR)/vos_list.o \
		$(VOSS_SRC_DIR)/vos_lock.o \
		$(VOSS_SRC_DIR)/vos_memory.o \
		$(VOSS_SRC_DIR)/vos_mq.o \
		$(VOSS_SRC_DIR)/vos_nvitem.o \
		$(VOSS_SRC_DIR)/vos_packet.o \
		$(VOSS_SRC_DIR)/vos_sched.o \
		$(VOSS_SRC_DIR)/vos_threads.o \
		$(VOSS_SRC_DIR)/vos_timer.o \
		$(VOSS_SRC_DIR)/vos_trace.o \
		$(VOSS_SRC_DIR)/vos_types.o \
                $(VOSS_SRC_DIR)/vos_utils.o \
                $(VOSS_SRC_DIR)/wlan_nv_parser.o \
                $(VOSS_SRC_DIR)/wlan_nv_stream_read.o \
                $(VOSS_SRC_DIR)/wlan_nv_template_builtin.o

ifeq ($(BUILD_DIAG_VERSION),1)
VOSS_OBJS += $(VOSS_SRC_DIR)/vos_diag.o
endif

############ WDA ############
WDA_DIR :=	CORE/WDA
WDA_INC_DIR :=	$(WDA_DIR)/inc
WDA_SRC_DIR :=	$(WDA_DIR)/src

WDA_INC := 	-I$(WLAN_ROOT)/$(WDA_INC_DIR) \
		-I$(WLAN_ROOT)/$(WDA_INC_DIR)/legacy \
		-I$(WLAN_ROOT)/$(WDA_SRC_DIR)

WDA_OBJS :=	$(WDA_SRC_DIR)/wlan_qct_wda.o \
		$(WDA_SRC_DIR)/wlan_qct_wda_debug.o \
		$(WDA_SRC_DIR)/wlan_qct_wda_ds.o \
		$(WDA_SRC_DIR)/wlan_qct_wda_legacy.o \
		$(WDA_SRC_DIR)/wlan_nv.o

############ WDI ############
WDI_DIR :=	CORE/WDI

WDI_CP_INC :=	-I$(WLAN_ROOT)/$(WDI_DIR)/CP/inc/

WDI_CP_SRC_DIR := $(WDI_DIR)/CP/src
WDI_CP_OBJS :=	$(WDI_CP_SRC_DIR)/wlan_qct_wdi.o \
		$(WDI_CP_SRC_DIR)/wlan_qct_wdi_dp.o \
		$(WDI_CP_SRC_DIR)/wlan_qct_wdi_sta.o

WDI_DP_INC := -I$(WLAN_ROOT)/$(WDI_DIR)/DP/inc/

WDI_DP_SRC_DIR := $(WDI_DIR)/DP/src
WDI_DP_OBJS :=	$(WDI_DP_SRC_DIR)/wlan_qct_wdi_bd.o \
		$(WDI_DP_SRC_DIR)/wlan_qct_wdi_ds.o

WDI_TRP_INC :=	-I$(WLAN_ROOT)/$(WDI_DIR)/TRP/CTS/inc/ \
		-I$(WLAN_ROOT)/$(WDI_DIR)/TRP/DTS/inc/

WDI_TRP_CTS_SRC_DIR :=	$(WDI_DIR)/TRP/CTS/src
WDI_TRP_CTS_OBJS :=	$(WDI_TRP_CTS_SRC_DIR)/wlan_qct_wdi_cts.o

WDI_TRP_DTS_SRC_DIR :=	$(WDI_DIR)/TRP/DTS/src
WDI_TRP_DTS_OBJS :=	$(WDI_TRP_DTS_SRC_DIR)/wlan_qct_wdi_dts.o

WDI_TRP_OBJS :=	$(WDI_TRP_CTS_OBJS) \
		$(WDI_TRP_DTS_OBJS)

WDI_WPAL_INC := -I$(WLAN_ROOT)/$(WDI_DIR)/WPAL/inc

WDI_WPAL_SRC_DIR := $(WDI_DIR)/WPAL/src
WDI_WPAL_OBJS := $(WDI_WPAL_SRC_DIR)/wlan_qct_pal_api.o \
		$(WDI_WPAL_SRC_DIR)/wlan_qct_pal_device.o \
		$(WDI_WPAL_SRC_DIR)/wlan_qct_pal_msg.o \
		$(WDI_WPAL_SRC_DIR)/wlan_qct_pal_packet.o \
		$(WDI_WPAL_SRC_DIR)/wlan_qct_pal_sync.o \
		$(WDI_WPAL_SRC_DIR)/wlan_qct_pal_timer.o \
		$(WDI_WPAL_SRC_DIR)/wlan_qct_pal_trace.o

WDI_INC :=	$(WDI_CP_INC) \
		$(WDI_DP_INC) \
		$(WDI_TRP_INC) \
		$(WDI_WPAL_INC)

WDI_OBJS :=	$(WDI_CP_OBJS) \
		$(WDI_DP_OBJS) \
		$(WDI_TRP_OBJS) \
		$(WDI_WPAL_OBJS)


RIVA_INC :=	-I$(WLAN_ROOT)/riva/inc

LINUX_INC :=	-Iinclude/linux

INCS :=		$(BAP_INC) \
		$(DXE_INC) \
		$(HDD_INC) \
		$(LINUX_INC) \
		$(MAC_INC) \
		$(RIVA_INC) \
		$(SAP_INC) \
		$(SME_INC) \
		$(SVC_INC) \
		$(SYS_INC) \
		$(TL_INC) \
		$(VOSS_INC) \
		$(WDA_INC) \
		$(WDI_INC)

OBJS :=		$(BAP_OBJS) \
		$(DXE_OBJS) \
		$(HDD_OBJS) \
		$(MAC_OBJS) \
		$(SAP_OBJS) \
		$(SME_OBJS) \
		$(SVC_OBJS) \
		$(SYS_OBJS) \
		$(TL_OBJS) \
		$(VOSS_OBJS) \
		$(WDA_OBJS) \
		$(WDI_OBJS)

EXTRA_CFLAGS += $(INCS)

CDEFINES :=	-DANI_BUS_TYPE_PLATFORM=1 \
		-DANI_LITTLE_BYTE_ENDIAN \
		-DANI_LITTLE_BIT_ENDIAN \
		-DQC_WLAN_CHIPSET_PRIMA \
		-DINTEGRATION_READY \
		-DDOT11F_LITTLE_ENDIAN_HOST \
		-DGEN6_ONWARDS \
		-DANI_COMPILER_TYPE_GCC \
		-DANI_OS_TYPE_ANDROID=6 \
		-DANI_LOGDUMP \
		-DWLAN_PERF \
		-DPTT_SOCK_SVC_ENABLE \
		-Wall\
		-D__linux__ \
		-DMSM_PLATFORM \
		-DHAL_SELF_STA_PER_BSS=1 \
		-DWLAN_FEATURE_VOWIFI_11R \
		-DWLAN_FEATURE_NEIGHBOR_ROAMING \
		-DWLAN_FEATURE_NEIGHBOR_ROAMING_DEBUG \
		-DWLAN_FEATURE_VOWIFI_11R_DEBUG \
		-DFEATURE_WLAN_WAPI \
		-DFEATURE_OEM_DATA_SUPPORT\
		-DSOFTAP_CHANNEL_RANGE \
		-DWLAN_AP_STA_CONCURRENCY \
		-DFEATURE_WLAN_SCAN_PNO \
		-DWLAN_FEATURE_PACKET_FILTERING \
		-DWLAN_FEATURE_VOWIFI \
		-DWLAN_FEATURE_11AC \
		-DWLAN_FEATURE_P2P_DEBUG \
		-DWLAN_ENABLE_AGEIE_ON_SCAN_RESULTS \
		-DWLANTL_DEBUG\
		-DWLAN_NS_OFFLOAD \
		-DWLAN_ACTIVEMODE_OFFLOAD_FEATURE \
		-DWLAN_FEATURE_HOLD_RX_WAKELOCK \
		-DWLAN_SOFTAP_VSTA_FEATURE \
		-DWLAN_FEATURE_ROAM_SCAN_OFFLOAD \
		-DWLAN_FEATURE_GTK_OFFLOAD \
		-DWLAN_WAKEUP_EVENTS \
	        -DWLAN_KD_READY_NOTIFIER \
		-DWLAN_NL80211_TESTMODE \
		-DFEATURE_WLAN_BATCH_SCAN \
		-DFEATURE_WLAN_LPHB \
                -DFEATURE_WLAN_PAL_TIMER_DISABLE \
                -DFEATURE_WLAN_PAL_MEM_DISABLE \
                -DFEATURE_WLAN_CH144 \
                -DWLAN_BUG_ON_SKB_ERROR \
                -DWLAN_DXE_LOW_RESOURCE_TIMER \
                -DWLAN_LOGGING_SOCK_SVC_ENABLE \
                -DWLAN_FEATURE_LINK_LAYER_STATS \
                -DWLAN_FEATURE_EXTSCAN

ifneq ($(CONFIG_PRONTO_WLAN),)
CDEFINES += -DWCN_PRONTO
CDEFINES += -DWCN_PRONTO_V1
endif

ifeq ($(BUILD_DEBUG_VERSION),1)
CDEFINES +=	-DWLAN_DEBUG \
		-DTRACE_RECORD \
		-DLIM_TRACE_RECORD \
		-DSME_TRACE_RECORD \
		-DPE_DEBUG_LOGW \
		-DPE_DEBUG_LOGE \
		-DDEBUG
endif

ifeq ($(CONFIG_SLUB_DEBUG_ON),y)
CDEFINES += -DTIMER_MANAGER
CDEFINES += -DMEMORY_DEBUG
endif

ifeq ($(HAVE_CFG80211),1)
CDEFINES += -DWLAN_FEATURE_P2P
CDEFINES += -DWLAN_FEATURE_WFD
ifeq ($(CONFIG_QCOM_VOWIFI_11R),y)
CDEFINES += -DKERNEL_SUPPORT_11R_CFG80211
CDEFINES += -DUSE_80211_WMMTSPEC_FOR_RIC
endif
endif

ifeq ($(CONFIG_QCOM_ESE),y)
CDEFINES += -DFEATURE_WLAN_ESE
ifeq ($(CONFIG_QCOM_ESE_UPLOAD),y)
CDEFINES += -DFEATURE_WLAN_ESE_UPLOAD
endif
endif

#normally, TDLS negative behavior is not needed
ifeq ($(CONFIG_QCOM_TDLS),y)
CDEFINES += -DFEATURE_WLAN_TDLS
ifeq ($(BUILD_DEBUG_VERSION),1)
CDEFINES += -DWLAN_FEATURE_TDLS_DEBUG
endif
CDEFINES += -DCONFIG_TDLS_IMPLICIT
#CDEFINES += -DFEATURE_WLAN_TDLS_NEGATIVE
#Code under FEATURE_WLAN_TDLS_INTERNAL is ported from volans, This code
#is not tested only verifed that it compiles. This is not required for
#supplicant based implementation
#CDEFINES += -DFEATURE_WLAN_TDLS_INTERNAL
endif

ifeq ($(CONFIG_PRIMA_WLAN_BTAMP),y)
CDEFINES += -DWLAN_BTAMP_FEATURE
endif

ifeq ($(CONFIG_PRIMA_WLAN_LFR),y)
CDEFINES += -DFEATURE_WLAN_LFR
endif

ifeq ($(CONFIG_PRIMA_WLAN_OKC),y)
CDEFINES += -DFEATURE_WLAN_OKC
endif

ifeq ($(CONFIG_PRIMA_WLAN_11AC_HIGH_TP),y)
CDEFINES += -DWLAN_FEATURE_11AC_HIGH_TP
endif

ifeq ($(BUILD_DIAG_VERSION),1)
CDEFINES += -DFEATURE_WLAN_DIAG_SUPPORT
CDEFINES += -DFEATURE_WLAN_DIAG_SUPPORT_CSR
CDEFINES += -DFEATURE_WLAN_DIAG_SUPPORT_LIM
endif

# enable the MAC Address auto-generation feature
CDEFINES += -DWLAN_AUTOGEN_MACADDR_FEATURE

ifeq ($(CONFIG_WLAN_FEATURE_11W),y)
CDEFINES += -DWLAN_FEATURE_11W
endif

ifeq ($(PANIC_ON_BUG),1)
CDEFINES += -DPANIC_ON_BUG
endif

ifeq ($(RE_ENABLE_WIFI_ON_WDI_TIMEOUT),1)
CDEFINES += -DWDI_RE_ENABLE_WIFI_ON_WDI_TIMEOUT
endif

ifeq ($(KERNEL_BUILD),1)
CDEFINES += -DWLAN_OPEN_SOURCE
endif

ifeq ($(findstring opensource, $(WLAN_ROOT)), opensource)
CDEFINES += -DWLAN_OPEN_SOURCE
endif

ifeq ($(CONFIG_ENABLE_LINUX_REG), y)
CDEFINES += -DCONFIG_ENABLE_LINUX_REG
endif

ifeq ($(MEASURE_ROAM_TIME_DELAY),1)
CDEFINES += -DDEBUG_ROAM_DELAY
endif

CDEFINES += -DFEATURE_WLAN_CH_AVOID

# Some kernel include files are being moved.  Check to see if
# the old version of the files are present

ifneq ($(wildcard $(srctree)/arch/$(SRCARCH)/mach-msm/include/mach/msm_smd.h),)
CDEFINES += -DEXISTS_MSM_SMD
endif

ifneq ($(wildcard $(srctree)/arch/$(SRCARCH)/mach-msm/include/mach/msm_smsm.h),)
CDEFINES += -DEXISTS_MSM_SMSM
endif

# Fix build for GCC 4.7
EXTRA_CFLAGS += -Wno-maybe-uninitialized -Wno-unused-function

KBUILD_CPPFLAGS += $(CDEFINES)

# Module information used by KBuild framework
obj-$(CONFIG_PRIMA_WLAN) += $(MODNAME).o
obj-$(CONFIG_PRONTO_WLAN) += $(MODNAME).o
$(MODNAME)-y := $(OBJS)
