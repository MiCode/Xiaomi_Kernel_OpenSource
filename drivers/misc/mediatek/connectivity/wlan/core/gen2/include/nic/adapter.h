/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef _ADAPTER_H
#define _ADAPTER_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#if CFG_SUPPORT_HOTSPOT_2_0
#include "hs20.h"
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef struct _ENHANCE_MODE_DATA_STRUCT_T SDIO_CTRL_T, *P_SDIO_CTRL_T;

struct ESS_SCAN_RESULT_T {
	UINT_8 aucBSSID[MAC_ADDR_LEN];
	UINT_16 u2SSIDLen;
	UINT_8 aucSSID[32];
};

typedef struct _WLAN_INFO_T {
	PARAM_BSSID_EX_T rCurrBssId;

	/* Scan Result */
	PARAM_BSSID_EX_T arScanResult[CFG_MAX_NUM_BSS_LIST];
	PUINT_8 apucScanResultIEs[CFG_MAX_NUM_BSS_LIST];
	UINT_32 u4ScanResultNum;

	struct ESS_SCAN_RESULT_T arScanResultEss[CFG_MAX_NUM_BSS_LIST];
	UINT_32 u4ScanResultEssNum;
	UINT_32 u4ScanDbgTimes1;
	UINT_32 u4ScanDbgTimes2;
	UINT_32 u4ScanDbgTimes3;
	UINT_32 u4ScanDbgTimes4;

	/* IE pool for Scanning Result */
	UINT_8 aucScanIEBuf[CFG_MAX_COMMON_IE_BUF_LEN];
	UINT_32 u4ScanIEBufferUsage;

	OS_SYSTIME u4SysTime;

	/* connection parameter (for Ad-Hoc) */
	UINT_16 u2BeaconPeriod;
	UINT_16 u2AtimWindow;

	PARAM_RATES eDesiredRates;
	CMD_LINK_ATTRIB eLinkAttr;
/* CMD_PS_PROFILE_T         ePowerSaveMode; */
	CMD_PS_PROFILE_T arPowerSaveMode[NETWORK_TYPE_INDEX_NUM];

	/* trigger parameter */
	ENUM_RSSI_TRIGGER_TYPE eRssiTriggerType;
	PARAM_RSSI rRssiTriggerValue;

	/* Privacy Filter */
	ENUM_PARAM_PRIVACY_FILTER_T ePrivacyFilter;

	/* RTS Threshold */
	PARAM_RTS_THRESHOLD eRtsThreshold;

	/* Network Type */
	UINT_8 ucNetworkType;

	/* Network Type In Use */
	UINT_8 ucNetworkTypeInUse;

} WLAN_INFO_T, *P_WLAN_INFO_T;

/* Session for CONNECTION SETTINGS */
typedef struct _CONNECTION_SETTINGS_T {

	UINT_8 aucMacAddress[MAC_ADDR_LEN];

	UINT_8 ucDelayTimeOfDisconnectEvent;

	BOOLEAN fgIsConnByBssidIssued;
	UINT_8 aucBSSID[MAC_ADDR_LEN];

	BOOLEAN fgIsConnReqIssued;
	BOOLEAN fgIsDisconnectedByNonRequest;
	ENUM_RECONNECT_LEVEL_T eReConnectLevel;

	UINT_8 ucSSIDLen;
	UINT_8 aucSSID[ELEM_MAX_LEN_SSID];

	ENUM_PARAM_OP_MODE_T eOPMode;

	ENUM_PARAM_CONNECTION_POLICY_T eConnectionPolicy;

	ENUM_PARAM_AD_HOC_MODE_T eAdHocMode;

	ENUM_PARAM_AUTH_MODE_T eAuthMode;

	ENUM_PARAM_ENCRYPTION_STATUS_T eEncStatus;

	BOOLEAN fgIsScanReqIssued;

	/* MIB attributes */
	UINT_16 u2BeaconPeriod;

	UINT_16 u2RTSThreshold;	/* User desired setting */

	UINT_16 u2DesiredNonHTRateSet;	/* User desired setting */

	UINT_8 ucAdHocChannelNum;	/* For AdHoc */

	ENUM_BAND_T eAdHocBand;	/* For AdHoc */

	UINT_32 u4FreqInKHz;	/* Center frequency */

	/* ATIM windows using for IBSS power saving function */
	UINT_16 u2AtimWindow;

	/* Features */
	BOOLEAN fgIsEnableRoaming;

	BOOLEAN fgIsAdHocQoSEnable;
#if (CFG_REFACTORY_PMKSA == 0)
	/* Indicates if OKC feature is enabled in wpa_supplicant for this ESS */
	BOOLEAN fgOkcEnabled;
	/* Indicates that there's a PMKSA associated with this ESS
	** in supplicant to generate PMKID for each BSS
	*/
	BOOLEAN fgOkcPmksaReady;
#endif
	ENUM_PARAM_PHY_CONFIG_T eDesiredPhyConfig;

	/* Used for AP mode for desired channel and bandwidth */
	UINT_16 u2CountryCode;
	UINT_16 u2CountryCodeBakup;
	UINT_8 uc2G4BandwidthMode;	/* 20/40M or 20M only */
	UINT_8 uc5GBandwidthMode;	/* 20/40M or 20M only */

	BOOLEAN fgTxShortGIDisabled;
	BOOLEAN fgRxShortGIDisabled;

#if CFG_SUPPORT_802_11D
	BOOLEAN fgMultiDomainCapabilityEnabled;
#endif				/* CFG_SUPPORT_802_11D */

#if 1				/* CFG_SUPPORT_WAPI */
	BOOLEAN fgWapiMode;
	UINT_32 u4WapiSelectedGroupCipher;
	UINT_32 u4WapiSelectedPairwiseCipher;
	UINT_32 u4WapiSelectedAKMSuite;
#endif

	/* CR1486, CR1640 */
	/* for WPS, disable the privacy check for AP selection policy */
	BOOLEAN fgPrivacyCheckDisable;

	/* b0~3: trigger-en AC0~3. b4~7: delivery-en AC0~3 */
	UINT_8 bmfgApsdEnAc;

	/* for RSN info store, when upper layer set rsn info */
	RSN_INFO_T rRsnInfo;

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
	BOOLEAN fgSecModeChangeStartTimer;
#endif
	struct LINK_MGMT rBlackList;
	uint8_t *pucAssocIEs;
	size_t assocIeLen;
} CONNECTION_SETTINGS_T, *P_CONNECTION_SETTINGS_T;

struct _BSS_INFO_T {

	ENUM_PARAM_MEDIA_STATE_T eConnectionState;	/* Connected Flag used in AIS_NORMAL_TR */
	ENUM_PARAM_MEDIA_STATE_T eConnectionStateIndicated;	/* The Media State that report to HOST */

	ENUM_OP_MODE_T eCurrentOPMode;	/* Current Operation Mode - Infra/IBSS */
#if CFG_ENABLE_WIFI_DIRECT
	ENUM_OP_MODE_T eIntendOPMode;
#endif

	BOOLEAN fgIsNetActive;	/* TRUE if this network has been activated */

#if CFG_SUPPORT_RLM_ACT_NETWORK
	UINT_8 ucNetActiveSrc;	/* Trigger source: connect/scan/pno */
#endif

	UINT_8 ucNetTypeIndex;	/* ENUM_NETWORK_TYPE_INDEX_T */

	UINT_8 ucReasonOfDisconnect;	/* Used by media state indication */

	UINT_8 ucSSIDLen;	/* Length of SSID */

#if CFG_ENABLE_WIFI_DIRECT
	ENUM_HIDDEN_SSID_TYPE_T eHiddenSsidType;	/* For Hidden SSID usage. */
#endif

	UINT_8 aucSSID[ELEM_MAX_LEN_SSID];	/* SSID used in this BSS */

	UINT_8 aucBSSID[MAC_ADDR_LEN];	/* The BSSID of the associated BSS */

	UINT_8 aucOwnMacAddr[MAC_ADDR_LEN];	/* Owned MAC Address used in this BSS */

	P_STA_RECORD_T prStaRecOfAP;	/* For Infra Mode, and valid only if
					 * eConnectionState == MEDIA_STATE_CONNECTED
					 */
	LINK_T rStaRecOfClientList;	/* For IBSS/AP Mode, all known STAs in current BSS */

	UINT_16 u2CapInfo;	/* Change Detection */

	UINT_16 u2BeaconInterval;	/* The Beacon Interval of this BSS */

	UINT_16 u2ATIMWindow;	/* For IBSS Mode */

	UINT_16 u2AssocId;	/* For Infra Mode, it is the Assoc ID assigned by AP.
				 */

	UINT_8 ucDTIMPeriod;	/* For Infra/AP Mode */

	UINT_8 ucDTIMCount;	/* For AP Mode, it is the DTIM value we should carried in
				 * the Beacon of next TBTT.
				 */

	UINT_8 ucPhyTypeSet;	/* Available PHY Type Set of this peer
				 * (This is deduced from received BSS_DESC_T)
				 */

	UINT_8 ucNonHTBasicPhyType;	/* The Basic PHY Type Index, used to setup Phy Capability */

	UINT_8 ucConfigAdHocAPMode;	/* The configuration of AdHoc/AP Mode. e.g. 11g or 11b */

	UINT_8 ucBeaconTimeoutCount;	/*
					 * For Infra/AP Mode, it is a threshold of Beacon Lost Count to
					 * confirm connection was lost
					 */

	BOOLEAN fgHoldSameBssidForIBSS;	/* For IBSS Mode, to keep use same BSSID to extend the life cycle of an IBSS */

	BOOLEAN fgIsBeaconActivated;	/* For AP/IBSS Mode, it is used to indicate that Beacon is sending */

	P_MSDU_INFO_T prBeacon;	/* For AP/IBSS Mode - Beacon Frame */

	BOOLEAN fgIsIBSSMaster;	/*
				 * For IBSS Mode - To indicate that we can reply ProbeResp Frame.
				 * In current TBTT interval
				 */

	BOOLEAN fgIsShortPreambleAllowed;	/*
						 * From Capability Info. of AssocResp Frame
						 * AND of Beacon/ProbeResp Frame
						 */
	BOOLEAN fgUseShortPreamble;	/* Short Preamble is enabled in current BSS. */
	BOOLEAN fgUseShortSlotTime;	/* Short Slot Time is enabled in current BSS. */

	UINT_16 u2OperationalRateSet;	/* Operational Rate Set of current BSS */
	UINT_16 u2BSSBasicRateSet;	/* Basic Rate Set of current BSS */

	UINT_8 ucAllSupportedRatesLen;	/* Used for composing Beacon Frame in AdHoc or AP Mode */
	UINT_8 aucAllSupportedRates[RATE_NUM];

	UINT_8 ucAssocClientCnt;	/* TODO(Kevin): Number of associated clients */

	BOOLEAN fgIsProtection;
	BOOLEAN fgIsQBSS;	/* fgIsWmmBSS; */
				/*
				 * For Infra/AP/IBSS Mode, it is used to indicate if we support WMM in
				 * current BSS.
				 */
	BOOLEAN fgIsNetAbsent;	/* TRUE: BSS is absent, FALSE: BSS is present */

	UINT_32 u4RsnSelectedGroupCipher;
	UINT_32 u4RsnSelectedPairwiseCipher;
	UINT_32 u4RsnSelectedAKMSuite;
	UINT_16 u2RsnSelectedCapInfo;

	enum ENUM_KEY_ACTION_TYPE_T eKeyAction;

	/*------------------------------------------------------------------------*/
	/* Power Management related information                                   */
	/*------------------------------------------------------------------------*/
	PM_PROFILE_SETUP_INFO_T rPmProfSetupInfo;

	/*------------------------------------------------------------------------*/
	/* WMM/QoS related information                                            */
	/*------------------------------------------------------------------------*/
	UINT_8 ucWmmParamSetCount;	/*
					 * Used to detect the change of EDCA parameters. For AP mode,
					 * the value is used in WMM IE
					 */

	AC_QUE_PARMS_T arACQueParms[WMM_AC_INDEX_NUM];

	UINT_8 aucCWminLog2ForBcast[WMM_AC_INDEX_NUM];	/* For AP mode, broadcast the CWminLog2 */
	UINT_8 aucCWmaxLog2ForBcast[WMM_AC_INDEX_NUM];	/* For AP mode, broadcast the CWmaxLog2 */
	AC_QUE_PARMS_T arACQueParmsForBcast[WMM_AC_INDEX_NUM];	/* For AP mode, broadcast the value */

	/*------------------------------------------------------------------------*/
	/* 802.11n HT operation IE when (prStaRec->ucPhyTypeSet & PHY_TYPE_BIT_HT) */
	/* is true. They have the same definition with fields of                  */
	/* information element (CM)                                               */
	/*------------------------------------------------------------------------*/
	ENUM_BAND_T eBand;
	UINT_8 ucPrimaryChannel;
#if CFG_SUPPORT_P2P_ECSA
	UINT_8 ucEcsaChannel;
	UINT_8 ucOpClass;
	UINT_8 ucSwitchCount;
	UINT_8 ucSwitchMode;
	BOOLEAN fgChanSwitching;
#endif
	UINT_8 ucHtOpInfo1;
	UINT_16 u2HtOpInfo2;
	UINT_16 u2HtOpInfo3;

	/*------------------------------------------------------------------------*/
	/* Required protection modes (CM)                                         */
	/*------------------------------------------------------------------------*/
	BOOLEAN fgErpProtectMode;
	ENUM_HT_PROTECT_MODE_T eHtProtectMode;
	ENUM_GF_MODE_T eGfOperationMode;
	ENUM_RIFS_MODE_T eRifsOperationMode;

	BOOLEAN fgObssErpProtectMode;	/* GO only */
	ENUM_HT_PROTECT_MODE_T eObssHtProtectMode;	/* GO only */
	ENUM_GF_MODE_T eObssGfOperationMode;	/* GO only */
	BOOLEAN fgObssRifsOperationMode;	/* GO only */

	/*------------------------------------------------------------------------*/
	/* OBSS to decide if 20/40M bandwidth is permitted.                       */
	/* The first member indicates the following channel list length.          */
	/*------------------------------------------------------------------------*/
	BOOLEAN fgAssoc40mBwAllowed;
	BOOLEAN fg40mBwAllowed;
	ENUM_CHNL_EXT_T eBssSCO;	/*
					 * Real setting for HW
					 * 20/40M AP mode will always set 40M,
					 * but its OP IE can be changed.
					 */
	UINT_8 auc2G_20mReqChnlList[CHNL_LIST_SZ_2G + 1];
	UINT_8 auc2G_NonHtChnlList[CHNL_LIST_SZ_2G + 1];
	UINT_8 auc2G_PriChnlList[CHNL_LIST_SZ_2G + 1];
	UINT_8 auc2G_SecChnlList[CHNL_LIST_SZ_2G + 1];

	UINT_8 auc5G_20mReqChnlList[CHNL_LIST_SZ_5G + 1];
	UINT_8 auc5G_NonHtChnlList[CHNL_LIST_SZ_5G + 1];
	UINT_8 auc5G_PriChnlList[CHNL_LIST_SZ_5G + 1];
	UINT_8 auc5G_SecChnlList[CHNL_LIST_SZ_5G + 1];

	TIMER_T rObssScanTimer;
	UINT_16 u2ObssScanInterval;	/* in unit of sec */

	/* Set scanning MAC OUI */
	UINT_8 fgIsScanOuiSet;
	UINT_8 ucScanOui[MAC_OUI_LEN];

	BOOLEAN fgObssActionForcedTo20M;	/* GO only */
	BOOLEAN fgObssBeaconForcedTo20M;	/* GO only */

	/*------------------------------------------------------------------------*/
	/* HW Related Fields (Kevin)                                              */
	/*------------------------------------------------------------------------*/
	UINT_8 ucHwDefaultFixedRateCode;	/* The default rate code copied to MAC TX Desc */
	UINT_16 u2HwLPWakeupGuardTimeUsec;

	UINT_8 ucBssFreeQuota;	/* The value is updated from FW  */
#if CFG_ENABLE_GTK_FRAME_FILTER
	P_IPV4_NETWORK_ADDRESS_LIST prIpV4NetAddrList;
#endif
	UINT_16 u2DeauthReason;

#if (CFG_SUPPORT_TDLS == 1)
	BOOLEAN fgTdlsIsProhibited;	/* TRUE: AP prohibits TDLS links */
	BOOLEAN fgTdlsIsChSwProhibited;	/* TRUE: AP prohibits TDLS chan switch */
#endif				/* CFG_SUPPORT_TDLS */

#if CFG_SUPPORT_RN
	OS_SYSTIME	rConnTime;
	BOOLEAN		fgDisConnReassoc;
#endif

	UINT_8 ucRoamSkipTimes;
	BOOLEAN fgGoodRcpiArea;
	BOOLEAN fgPoorRcpiArea;
#if CFG_SUPPORT_P2P_EAP_FAIL_WORKAROUND
	BOOLEAN fgP2PPendingDeauth;
	UINT_32 u4P2PEapTxDoneTime;
#endif

	BOOLEAN fgIsDelayIndication;
	enum ENUM_IFTYPE eIftype;

#if CFG_SUPPORT_802_11W
	/* AP PMF */
	struct AP_PMF_CFG rApPmfCfg;
#endif
};

struct ESS_CHNL_INFO {
	UINT_8 ucChannel;
	UINT_8 ucUtilization;
	UINT_8 ucApNum;
};

struct _AIS_SPECIFIC_BSS_INFO_T {
	UINT_8 ucRoamingAuthTypes;	/* This value indicate the roaming type used in AIS_JOIN */

	BOOLEAN fgIsIBSSActive;

	/*! \brief Global flag to let arbiter stay at standby and not connect to any network */
	BOOLEAN fgCounterMeasure;
	UINT_8 ucWEPDefaultKeyID;
	BOOLEAN fgTransmitKeyExist;	/* Legacy wep Transmit key exist or not */

	/* While Do CounterMeasure procedure, check the EAPoL Error report have send out */
	BOOLEAN fgCheckEAPoLTxDone;

	UINT_32 u4RsnaLastMICFailTime;
#if (CFG_REFACTORY_PMKSA == 0)
	/* Stored the current bss wpa rsn cap filed, used for roaming policy */
	/* UINT_16                 u2RsnCap; */
	TIMER_T rPreauthenticationTimer;
#endif
	/*
	 * By the flow chart of 802.11i,
	 * wait 60 sec before associating to same AP
	 * or roaming to a new AP
	 * or sending data in IBSS,
	 * keep a timer for handle the 60 sec counterMeasure
	 */
	TIMER_T rRsnaBlockTrafficTimer;
	TIMER_T rRsnaEAPoLReportTimeoutTimer;

	/* For Keep the Tx/Rx Mic key for TKIP SW Calculate Mic */
	/* This is only one for AIS/AP */
	UINT_8 aucTxMicKey[8];
	UINT_8 aucRxMicKey[8];

#if (CFG_REFACTORY_PMKSA == 0)
	/* Buffer for WPA2 PMKID */
	/* The PMKID cache lifetime is expire by media_disconnect_indication */
	UINT_32 u4PmkidCandicateCount;
	PMKID_CANDICATE_T arPmkidCandicate[CFG_MAX_PMKID_CACHE];
	UINT_32 u4PmkidCacheCount;
	PMKID_ENTRY_T arPmkidCache[CFG_MAX_PMKID_CACHE];
	BOOLEAN fgIndicatePMKID;
#else
	LINK_T rPmkidCache;
#endif
#if CFG_SUPPORT_802_11W
	BOOLEAN fgMgmtProtection;
	UINT_32 u4SaQueryStart;
	UINT_32 u4SaQueryCount;
	UINT_8 ucSaQueryTimedOut;
	PUINT_8 pucSaQueryTransId;
	TIMER_T rSaQueryTimer;
	BOOLEAN fgBipKeyInstalled;
#endif
	struct ESS_CHNL_INFO arCurEssChnlInfo[CFG_MAX_NUM_OF_CHNL_INFO];
	UINT_8 ucCurEssChnlInfoNum;
	LINK_T rCurEssLink;
	struct BSS_TRANSITION_MGT_PARAM_T rBTMParam;
	UINT_8 ucKeyAlgorithmId;
};

struct _BOW_SPECIFIC_BSS_INFO_T {
	UINT_16 u2Reserved;	/* Reserved for Data Type Check */
};

#if CFG_SLT_SUPPORT
typedef struct _SLT_INFO_T {

	P_BSS_DESC_T prPseudoBssDesc;
	UINT_16 u2SiteID;
	UINT_8 ucChannel2G4;
	UINT_8 ucChannel5G;
	BOOLEAN fgIsDUT;
	UINT_32 u4BeaconReceiveCnt;
	/* ///////Deprecated///////// */
	P_STA_RECORD_T prPseudoStaRec;
} SLT_INFO_T, *P_SLT_INFO_T;
#endif

/*
 * Major member variables for WiFi FW operation.
 * Variables within this region will be ready for access after WIFI function is enabled.
 */
typedef struct _WIFI_VAR_T {
	BOOLEAN fgIsRadioOff;

	BOOLEAN fgIsEnterD3ReqIssued;

	BOOLEAN fgDebugCmdResp;

	CONNECTION_SETTINGS_T rConnSettings;

	SCAN_INFO_T rScanInfo;

#if CFG_SUPPORT_ROAMING
	ROAMING_INFO_T rRoamingInfo;
#endif				/* CFG_SUPPORT_ROAMING */

	AIS_FSM_INFO_T rAisFsmInfo;

	ENUM_PWR_STATE_T aePwrState[NETWORK_TYPE_INDEX_NUM];

	BSS_INFO_T arBssInfo[NETWORK_TYPE_INDEX_NUM];

	AIS_SPECIFIC_BSS_INFO_T rAisSpecificBssInfo;

#if CFG_ENABLE_WIFI_DIRECT
	P_P2P_CONNECTION_SETTINGS_T prP2PConnSettings;

	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo;

	P_P2P_FSM_INFO_T prP2pFsmInfo;
#endif				/* CFG_ENABLE_WIFI_DIRECT */

#if CFG_ENABLE_BT_OVER_WIFI
	BOW_SPECIFIC_BSS_INFO_T rBowSpecificBssInfo;
	BOW_FSM_INFO_T rBowFsmInfo;
#endif				/* CFG_ENABLE_BT_OVER_WIFI */

	DEAUTH_INFO_T arDeauthInfo[MAX_DEAUTH_INFO_COUNT];

	/* Current Wi-Fi Settings and Flags */
	UINT_8 aucPermanentAddress[MAC_ADDR_LEN];
	UINT_8 aucMacAddress[MAC_ADDR_LEN];
	UINT_8 aucDeviceAddress[MAC_ADDR_LEN];
	UINT_8 aucInterfaceAddress[MAC_ADDR_LEN];

	UINT_8 ucAvailablePhyTypeSet;
	UINT_8 ucWithPhyTypeSpecificIE;

	ENUM_PHY_TYPE_INDEX_T eNonHTBasicPhyType2G4;	/* Basic Phy Type used by SCN according
							 * to the set of Available PHY Types
							 */

	ENUM_PARAM_PREAMBLE_TYPE_T ePreambleType;
	ENUM_REGISTRY_FIXED_RATE_T eRateSetting;

	BOOLEAN fgIsShortSlotTimeOptionEnable;
	/* User desired setting, but will honor the capability of AP */

	BOOLEAN fgEnableJoinToHiddenSSID;
	BOOLEAN fgSupportWZCDisassociation;

	BOOLEAN fgSupportQoS;
	BOOLEAN fgSupportAmpduTx;
	BOOLEAN fgSupportAmpduRx;
	BOOLEAN fgSupportTspec;
	BOOLEAN fgSupportUAPSD;
	BOOLEAN fgSupportULPSMP;
	UINT_8 u8SupportRxSgi20;	/* 0: default 1: enable 2:disble */
	UINT_8 u8SupportRxSgi40;
	UINT_8 u8SupportRxGf;
	UINT_8 u8SupportRxSTBC;
#if CFG_SUPPORT_CFG_FILE
	UINT_8 ucApWpsMode;
	UINT_8 ucCert11nMode;
	UINT_8 ucApChannel;
#endif
#if CFG_SUPPORT_CE_FCC_TXPWR_LIMIT
	UINT_8 ucCeFccTxPwrLimit;
	UINT_8 ucCeFccTxPwrLimitCck;
	UINT_8 ucCeFccTxPwrLimitOfdmHt20;
	UINT_8 ucCeFccTxPwrLimitHt40;
#endif
#if CFG_SLT_SUPPORT
	SLT_INFO_T rSltInfo;
#endif
#if CFG_SUPPORT_HOTSPOT_2_0
	HS20_INFO_T rHS20Info;
#endif

	PARAM_GET_CHN_INFO rChnLoadInfo;

#if CFG_RX_BA_REORDERING_ENHANCEMENT
	BOOLEAN fgEnableReportIndependentPkt;
#endif
#if CFG_SUPPORT_MTK_SYNERGY
	UINT_8 ucMtkOui;
	UINT_32 u4MtkOuiCap;
	UINT_8 aucMtkFeature[4];
#endif
	struct RADIO_MEASUREMENT_REQ_PARAMS rRmReqParams;
	struct RADIO_MEASUREMENT_REPORT_PARAMS rRmRepParams;

	struct WMM_INFO rWmmInfo;

	UINT_8 aucAifsN[WMM_AC_INDEX_NUM];
	UINT_8 aucCwMin[WMM_AC_INDEX_NUM];
	UINT_16 au2CwMax[WMM_AC_INDEX_NUM];
	UINT_16 au2TxOp[WMM_AC_INDEX_NUM];
} WIFI_VAR_T, *P_WIFI_VAR_T;	/* end of _WIFI_VAR_T */

/* cnm_timer module */
typedef struct {
	LINK_T rLinkHead;
	OS_SYSTIME rNextExpiredSysTime;
	KAL_WAKE_LOCK_T *rWakeLock;
	BOOLEAN fgWakeLocked;
} ROOT_TIMER, *P_ROOT_TIMER;

/* FW/DRV/NVRAM version information */
typedef struct {

	/* NVRAM or Registry */
	UINT_16 u2Part1CfgOwnVersion;
	UINT_16 u2Part1CfgPeerVersion;
	UINT_16 u2Part2CfgOwnVersion;
	UINT_16 u2Part2CfgPeerVersion;

	/* Firmware */
	UINT_16 u2FwProductID;
	UINT_16 u2FwOwnVersion;
	UINT_16 u2FwPeerVersion;
	UINT_16 u2FwOwnVersionExtend; /*support version extended*/
} WIFI_VER_INFO_T, *P_WIFI_VER_INFO_T;

#if CFG_ENABLE_WIFI_DIRECT
/*
* p2p function pointer structure
*/

typedef struct _P2P_FUNCTION_LINKER {
	P2P_REMOVE prP2pRemove;
	/* NIC_P2P_MEDIA_STATE_CHANGE                  prNicP2pMediaStateChange; */
	/* SCAN_UPDATE_P2P_DEVICE_DESC                 prScanUpdateP2pDeviceDesc; */
	/* P2P_FSM_RUN_EVENT_RX_PROBE_RESPONSE_FRAME   prP2pFsmRunEventRxProbeResponseFrame; */
	P2P_GENERATE_P2P_IE prP2pGenerateWSC_IEForBeacon;
	/* P2P_CALCULATE_WSC_IE_LEN_FOR_PROBE_RSP      prP2pCalculateWSC_IELenForProbeRsp; */
	/* P2P_GENERATE_WSC_IE_FOR_PROBE_RSP           prP2pGenerateWSC_IEForProbeRsp; */
	/* SCAN_REMOVE_P2P_BSS_DESC                    prScanRemoveP2pBssDesc; */
	/* P2P_HANDLE_SEC_CHECK_RSP                    prP2pHandleSecCheckRsp; */
	P2P_NET_REGISTER prP2pNetRegister;
	P2P_NET_UNREGISTER prP2pNetUnregister;
	P2P_CALCULATE_P2P_IE_LEN prP2pCalculateP2p_IELenForAssocReq;	/* All IEs generated from supplicant. */
	P2P_GENERATE_P2P_IE prP2pGenerateP2p_IEForAssocReq;	/* All IEs generated from supplicant. */
} P2P_FUNCTION_LINKER, *P_P2P_FUNCTION_LINKER;

#endif


#if CFG_SUPPORT_NCHO
typedef enum _ENUM_NCHO_ITEM_SET_TYPE_T {
	ITEM_SET_TYPE_NUM,
	ITEM_SET_TYPE_STR
} ENUM_NCHO_ITEM_SET_TYPE, *P_ENUM_NCHO_ITEM_SET_TYPE;

typedef enum _ENUM_NCHO_BAND_T {
	NCHO_BAND_AUTO = 0,
	NCHO_BAND_5G,
	NCHO_BAND_2G4,
	NCHO_BAND_NUM
} ENUM_NCHO_BAND, *P_ENUM_NCHO_BAND;

typedef enum _ENUM_NCHO_DFS_SCN_MODE_T {
	NCHO_DFS_SCN_DISABLE = 0,
	NCHO_DFS_SCN_ENABLE1,
	NCHO_DFS_SCN_ENABLE2,
	NCHO_DFS_SCN_NUM
} ENUM_NCHO_DFS_SCN_MODE, *P_ENUM_NCHO_DFS_SCN_MODE;

typedef struct _CFG_NCHO_RE_ASSOC_T {
	UINT_32 u4SsidLen;	/*!< SSID length in bytes. Zero length is broadcast(any) SSID */
	UINT_8 aucSsid[ELEM_MAX_LEN_SSID];
	UINT_8 aucBssid[MAC_ADDR_LEN];
	UINT_32 u4CenterFreq;
} CFG_NCHO_RE_ASSOC_T, *PCFG_NCHO_RE_ASSOC_T;

typedef struct _CFG_NCHO_SCAN_CHNL_T {
	UINT_8 ucChannelListNum;
	RF_CHANNEL_INFO_T arChnlInfoList[MAXIMUM_OPERATION_CHANNEL_LIST];
} CFG_NCHO_SCAN_CHNL_T, *PCFG_NCHO_SCAN_CHNL_T;

typedef struct _NCHO_ACTION_FRAME_PARAMS_T {
	UCHAR aucBssid[MAC_ADDR_LEN];
	INT32 i4channel;
	INT32 i4DwellTime;
	INT32 i4len;
	UCHAR aucData[520];
} NCHO_ACTION_FRAME_PARAMS, *P_NCHO_ACTION_FRAME_PARAMS;

typedef struct _NCHO_AF_INFO_T {
	PUCHAR aucBssid;
	INT32 i4channel;
	INT32 i4DwellTime;
	INT32 i4len;
	PUCHAR pucData;
} NCHO_AF_INFO, *P_NCHO_AF_INFO;

typedef struct _NCHO_INFO_T {
	BOOLEAN fgECHOEnabled;
	BOOLEAN fgChGranted;
	BOOLEAN fgIsSendingAF;
	INT32 i4RoamTrigger;		/* db */
	INT32 i4RoamDelta;		/* db */
	UINT32 u4RoamScanPeriod;	/* ms */
	UINT32 u4ScanChannelTime;	/* ms */
	UINT32 u4ScanHomeTime;		/* ms */
	UINT32 u4ScanHomeawayTime;	/* ms */
	UINT32 u4ScanNProbes;
	UINT32 u4WesMode;
	ENUM_NCHO_BAND eBand;
	ENUM_NCHO_DFS_SCN_MODE eDFSScnMode;
	UINT32 u4RoamScanControl;
	CFG_NCHO_SCAN_CHNL_T rRoamScnChnl;
	NCHO_ACTION_FRAME_PARAMS rParamActionFrame;
} NCHO_INFO, *P_NCHO_INFO;
#endif

/*
 * State Machine:
 * --> STOP: No Tx/Rx traffic
 * -----> clear RUNNING
 * --> DISABLE: Screen is off
 * --> ENABLE: Screen is on && Tx/Rx traffic is active
 * -----> clear DISABLE
 * --> RUNNING: Screen was on && Tx/Rx was ongoing
 */
struct PERF_MONITOR_T {
	TIMER_T rPerfMonTimer;
	ULONG ulPerfMonFlag;
	ULONG ulLastTxBytes;
	ULONG ulLastRxBytes;
	ULONG ulP2PLastTxBytes;
	ULONG ulP2PLastRxBytes;
	ULONG ulThroughput;     /* in bps */
	UINT32 u4UpdatePeriod;  /* in ms */
	UINT32 u4TarPerfLevel;
	UINT32 u4CurrPerfLevel;
	UINT8 u1ShutdownCoreCount;
};

/*
 * Major ADAPTER structure
 * Major data structure for driver operation
 */
struct _ADAPTER_T {
	UINT_8 ucRevID;

	UINT_16 u2NicOpChnlNum;

	BOOLEAN fgIsEnableWMM;
	BOOLEAN fgIsWmmAssoc;	/* This flag is used to indicate that WMM is enable in current BSS */

	UINT_32 u4OsPacketFilter;	/* packet filter used by OS */

#if CFG_TCP_IP_CHKSUM_OFFLOAD
	UINT_32 u4CSUMFlags;
#endif				/* CFG_TCP_IP_CHKSUM_OFFLOAD */

	ENUM_BAND_T aePreferBand[NETWORK_TYPE_INDEX_NUM];

	ENUM_BAND_T aeSetBand[NETWORK_TYPE_INDEX_NUM];

	/* ADAPTER flags */
	UINT_32 u4Flags;
	UINT_32 u4HwFlags;

	BOOLEAN fgIsRadioOff;

	BOOLEAN fgIsEnterD3ReqIssued;

	UINT_8 aucMacAddress[MAC_ADDR_LEN];

	ENUM_PHY_TYPE_INDEX_T eCurrentPhyType;	/* Current selection basing on the set of Available PHY Types */

#if CFG_COALESCING_BUFFER_SIZE || CFG_SDIO_RX_AGG
	UINT_32 u4CoalescingBufCachedSize;
	PUINT_8 pucCoalescingBufCached;
#endif				/* CFG_COALESCING_BUFFER_SIZE */

	/* Buffer for CMD_INFO_T, Mgt packet and mailbox message */
	BUF_INFO_T rMgtBufInfo;
	BUF_INFO_T rMsgBufInfo;
	PUINT_8 pucMgtBufCached;
	UINT_32 u4MgtBufCachedSize;
	UINT_8 aucMsgBuf[MSG_BUFFER_SIZE];
#if CFG_DBG_MGT_BUF
	UINT_32 u4MemAllocDynamicCount;	/* Debug only */
	UINT_32 u4MemFreeDynamicCount;	/* Debug only */
#endif

	STA_RECORD_T arStaRec[CFG_STA_REC_NUM];

	/* Element for TX PATH */
	TX_CTRL_T rTxCtrl;
	QUE_T rFreeCmdList;
	CMD_INFO_T arHifCmdDesc[CFG_TX_MAX_CMD_PKT_NUM];

	/* Element for RX PATH */
	RX_CTRL_T rRxCtrl;

	P_SDIO_CTRL_T prSDIOCtrl;

#if (MT6620_E1_ASIC_HIFSYS_WORKAROUND == 1)
	/* Element for MT6620 E1 HIFSYS workaround */
	BOOLEAN fgIsClockGatingEnabled;
#endif

#if (CFG_REFACTORY_PMKSA == 0)
	/* Buffer for Authentication Event */
	/* <Todo> Move to glue layer and refine the kal function */
	/* Reference to rsnGeneratePmkidIndication function at rsn.c */
	UINT_8 aucIndicationEventBuffer[(CFG_MAX_PMKID_CACHE * 20) + 8];
#endif
	UINT_32 u4IntStatus;

	ENUM_ACPI_STATE_T rAcpiState;

	BOOLEAN fgIsIntEnable;
	BOOLEAN fgIsIntEnableWithLPOwnSet;

	BOOLEAN fgIsFwOwn;
	BOOLEAN fgWiFiInSleepyState;

	UINT_32 u4PwrCtrlBlockCnt;

	QUE_T rPendingCmdQueue;

	P_GLUE_INFO_T prGlueInfo;

	UINT_8 ucCmdSeqNum;
	UINT_8 ucTxSeqNum;

#if 1				/* CFG_SUPPORT_WAPI */
	BOOLEAN fgUseWapi;
#endif

	/* RF Test flags */
	BOOLEAN fgTestMode;

	/* WLAN Info for DRIVER_CORE OID query */
	WLAN_INFO_T rWlanInfo;

#if CFG_ENABLE_WIFI_DIRECT
	BOOLEAN fgIsP2PRegistered;
	ENUM_NET_REG_STATE_T rP2PNetRegState;
	BOOLEAN fgIsWlanLaunched;
	P_P2P_INFO_T prP2pInfo;
#if CFG_SUPPORT_P2P_RSSI_QUERY
	OS_SYSTIME rP2pLinkQualityUpdateTime;
	BOOLEAN fgIsP2pLinkQualityValid;
	EVENT_LINK_QUALITY rP2pLinkQuality;
#endif

	/* FSM Timer */
	TIMER_T rP2pFsmTimeoutTimer;
#endif

	TIMER_T rTdlsStateTimer;

	/* Online Scan Option */
	BOOLEAN fgEnOnlineScan;

	/* Online Scan Option */
	BOOLEAN fgDisBcnLostDetection;

	/* MAC address */
	PARAM_MAC_ADDRESS rMyMacAddr;

	/* Wake-up Event for WOL */
	UINT_32 u4WakeupEventEnable;

	/* Event Buffering */
	EVENT_STATISTICS rStatStruct;
	OS_SYSTIME rStatUpdateTime;
	BOOLEAN fgIsStatValid;

	EVENT_LINK_QUALITY rLinkQuality;
	OS_SYSTIME rLinkQualityUpdateTime;
	BOOLEAN fgIsLinkQualityValid;
	OS_SYSTIME rLinkRateUpdateTime;
	BOOLEAN fgIsLinkRateValid;

	/* WIFI_VAR_T */
	WIFI_VAR_T rWifiVar;

	/* MTK WLAN NIC driver IEEE 802.11 MIB */
	IEEE_802_11_MIB_T rMib;

	/* Mailboxs for inter-module communication */
	MBOX_T arMbox[MBOX_ID_TOTAL_NUM];

	/* Timers for OID Pending Handling */
	TIMER_T rOidTimeoutTimer;

	TIMER_T rReturnIndicatedRfbListTimer;

	/* Root Timer for cnm_timer module */
	ROOT_TIMER rRootTimer;

	/* RLM maintenance */
	ENUM_CHNL_EXT_T eRfSco;
	ENUM_SYS_PROTECT_MODE_T eSysProtectMode;
	ENUM_GF_MODE_T eSysHtGfMode;
	ENUM_RIFS_MODE_T eSysTxRifsMode;
	ENUM_SYS_PCO_PHASE_T eSysPcoPhase;

	P_DOMAIN_INFO_ENTRY prDomainInfo;

	/* QM */
	QUE_MGT_T rQM;

	CNM_INFO_T rCnmInfo;

	UINT_32 u4PowerMode;

	UINT_32 u4CtiaPowerMode;
	BOOLEAN fgEnCtiaPowerMode;

	UINT_32 fgEnArpFilter;

	UINT_32 u4UapsdAcBmp;

	UINT_32 u4MaxSpLen;

	UINT_32 u4PsCurrentMeasureEn;

	/* Version Information */
	WIFI_VER_INFO_T rVerInfo;

	/* 5GHz support (from F/W) */
	BOOLEAN fgIsHw5GBandDisabled;
	BOOLEAN fgEnable5GBand;
	BOOLEAN fgIsEepromUsed;
	BOOLEAN fgIsEfuseValid;
	BOOLEAN fgIsEmbbededMacAddrValid;

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
	BOOLEAN fgIsPowerLimitTableValid;
#endif

	/* Packet Forwarding Tracking */
	INT_32 i4PendingFwdFrameCount;

#if CFG_SUPPORT_RDD_TEST_MODE
	UINT_8 ucRddStatus;
#endif

	BOOLEAN fgDisStaAgingTimeoutDetection;
#if CFG_SUPPORT_CFG_FILE
	P_WLAN_CFG_T prWlanCfg;
	WLAN_CFG_T rWlanCfg;
#endif
#if CFG_SPM_WORKAROUND_FOR_HOTSPOT
	KAL_WAKE_LOCK_T *rApWakeLock;
#endif
	UINT_32 u4FwCompileFlag0;
	UINT_32 u4FwCompileFlag1;
	KAL_WAKE_LOCK_T *rTxThreadWakeLock;
	KAL_WAKE_LOCK_T *rAhbIsrWakeLock;

#if CFG_SUPPORT_ROAMING_ENC
	BOOLEAN fgIsRoamingEncEnabled;
#endif				/* CFG_SUPPORT_ROAMING_ENC */

#if (CFG_SUPPORT_TDLS == 1)
	BOOLEAN fgTdlsIsSup;
#endif				/* CFG_SUPPORT_TDLS */

	UINT_8 ucScanTime;

#if CFG_SUPPORT_DBG_POWERMODE
	BOOLEAN fgEnDbgPowerMode;	/*  dbg privilege power mode, always keep in active */
#endif
#if CFG_SUPPORT_NCHO			/*  NCHO information */
	NCHO_INFO rNchoInfo;
#endif
	UINT_32 u4AirDelayTotal;	/*  dbg privilege power mode, always keep in active */
	ULONG	ulSuspendFlag;
	UINT_8 ucFlushCount;	/*FW flush packet count*/
	struct PERF_MONITOR_T rPerMonitor;

#ifdef CFG_TC1_FEATURE /* for Passive Scan */
	UINT_8 ucScanType;
#endif

	/* NLO Timer */
	TIMER_T rScanNloTimeoutTimer;

	struct WLAN_DEBUG_INFO rDebugInfo;

	OS_SYSTIME rStasEnvReportDetectTime;

	UINT_32 u4QmRxBaMissTimeout;
	BOOLEAN fgEnCfg80211Scan;

#if CFG_SUPPORT_GAMING_MODE
	BOOLEAN fgEnGamingMode;
#endif
#if CFG_SUPPORT_OSHARE
	BOOLEAN fgEnOshareMode;
#endif
};				/* end of _ADAPTER_T */

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define SUSPEND_FLAG_FOR_WAKEUP_REASON	(0)
#define SUSPEND_FLAG_CLEAR_WHEN_RESUME	(1)

/*----------------------------------------------------------------------------*/
/* Macros for BSS_INFO_T - Flag of Net Active                                 */
/*----------------------------------------------------------------------------*/
#define IS_NET_ACTIVE(_prAdapter, _NetTypeIndex) \
		(_prAdapter->rWifiVar.arBssInfo[(_NetTypeIndex)].fgIsNetActive)
#define IS_BSS_ACTIVE(_prBssInfo)     ((_prBssInfo)->fgIsNetActive)

#define IS_BSS_AIS(_prBssInfo) \
	((_prBssInfo)->ucNetTypeIndex == NETWORK_TYPE_AIS_INDEX)

#define IS_AIS_ACTIVE(_prAdapter)     IS_NET_ACTIVE(_prAdapter, NETWORK_TYPE_AIS_INDEX)
#define IS_P2P_ACTIVE(_prAdapter)     IS_NET_ACTIVE(_prAdapter, NETWORK_TYPE_P2P_INDEX)
#define IS_BOW_ACTIVE(_prAdapter)     IS_NET_ACTIVE(_prAdapter, NETWORK_TYPE_BOW_INDEX)

#define SET_NET_ACTIVE(_prAdapter, _NetTypeIndex) \
	{_prAdapter->rWifiVar.arBssInfo[(_NetTypeIndex)].fgIsNetActive = TRUE; }

#define UNSET_NET_ACTIVE(_prAdapter, _NetTypeIndex) \
	{_prAdapter->rWifiVar.arBssInfo[(_NetTypeIndex)].fgIsNetActive = FALSE; }

#define BSS_INFO_INIT(_prAdapter, _NetTypeIndex) \
		{   UINT_8 _aucZeroMacAddr[] = NULL_MAC_ADDR; \
		    P_BSS_INFO_T _prBssInfo = &(_prAdapter->rWifiVar.arBssInfo[(_NetTypeIndex)]); \
		    \
		    _prBssInfo->eConnectionState = PARAM_MEDIA_STATE_DISCONNECTED; \
		    _prBssInfo->eConnectionStateIndicated = PARAM_MEDIA_STATE_DISCONNECTED; \
		    _prBssInfo->eCurrentOPMode = OP_MODE_INFRASTRUCTURE; \
		    _prBssInfo->fgIsNetActive = FALSE; \
		    _prBssInfo->ucNetTypeIndex = (_NetTypeIndex); \
		    _prBssInfo->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_RESERVED; \
		    COPY_MAC_ADDR(_prBssInfo->aucBSSID, _aucZeroMacAddr); \
		    LINK_INITIALIZE(&_prBssInfo->rStaRecOfClientList); \
		    _prBssInfo->fgIsBeaconActivated = FALSE; \
		    _prBssInfo->ucHwDefaultFixedRateCode = RATE_CCK_1M_LONG; \
		    _prBssInfo->fgIsNetAbsent = FALSE; \
		}

#if CFG_ENABLE_BT_OVER_WIFI
#define BOW_BSS_INFO_INIT(_prAdapter, _NetTypeIndex) \
		{  \
		    P_BSS_INFO_T _prBssInfo = &(_prAdapter->rWifiVar.arBssInfo[(_NetTypeIndex)]); \
		    \
		    _prBssInfo->eConnectionState = PARAM_MEDIA_STATE_DISCONNECTED; \
		    _prBssInfo->eConnectionStateIndicated = PARAM_MEDIA_STATE_DISCONNECTED; \
		    _prBssInfo->eCurrentOPMode = OP_MODE_BOW; \
		    _prBssInfo->ucNetTypeIndex = (_NetTypeIndex); \
		    _prBssInfo->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_RESERVED; \
		    LINK_INITIALIZE(&_prBssInfo->rStaRecOfClientList); \
		    _prBssInfo->fgIsBeaconActivated = TRUE; \
		    _prBssInfo->ucHwDefaultFixedRateCode = RATE_CCK_1M_LONG; \
		    _prBssInfo->fgIsNetAbsent = FALSE; \
		}
#endif

#define PERF_MON_DISABLE_BIT    (0)
#define PERF_MON_STOP_BIT       (1)
#define PERF_MON_RUNNING_BIT    (2)

#define THROUGHPUT_L1_THRESHOLD		(20*1024*1024)
#define THROUGHPUT_L2_THRESHOLD		(40*1024*1024)
#define THROUGHPUT_L3_THRESHOLD		(60*1024*1024)
#define THROUGHPUT_L4_THRESHOLD		(135*1024*1024)

#define THROUGHPUT_AP_L1_THRESHOLD	(5*1024*1024)
#define THROUGHPUT_AP_L2_THRESHOLD	(10*1024*1024)
#define THROUGHPUT_AP_L3_THRESHOLD	(60*1024*1024)
#define THROUGHPUT_AP_L4_THRESHOLD	(135*1024*1024)


#define THROUGHPUT_SHUTDOWN_CORE_COUNT	5

/*----------------------------------------------------------------------------*/
/* Macros for Power State                                                     */
/*----------------------------------------------------------------------------*/
#define SET_NET_PWR_STATE_IDLE(_prAdapter, _NetTypeIndex) \
	{_prAdapter->rWifiVar.aePwrState[(_NetTypeIndex)] = PWR_STATE_IDLE; }

#define SET_NET_PWR_STATE_ACTIVE(_prAdapter, _NetTypeIndex) \
	{_prAdapter->rWifiVar.aePwrState[(_NetTypeIndex)] = PWR_STATE_ACTIVE; }

#define SET_NET_PWR_STATE_PS(_prAdapter, _NetTypeIndex) \
	{_prAdapter->rWifiVar.aePwrState[(_NetTypeIndex)] = PWR_STATE_PS; }

#define IS_NET_PWR_STATE_ACTIVE(_prAdapter, _NetTypeIndex) \
	(_prAdapter->rWifiVar.aePwrState[(_NetTypeIndex)] == PWR_STATE_ACTIVE)

#define IS_NET_PWR_STATE_IDLE(_prAdapter, _NetTypeIndex) \
	(_prAdapter->rWifiVar.aePwrState[(_NetTypeIndex)] == PWR_STATE_IDLE)

#define IS_SCN_PWR_STATE_ACTIVE(_prAdapter) \
	(_prAdapter->rWifiVar.rScanInfo.eScanPwrState == SCAN_PWR_STATE_ACTIVE)

#define IS_SCN_PWR_STATE_IDLE(_prAdapter) \
	(_prAdapter->rWifiVar.rScanInfo.eScanPwrState == SCAN_PWR_STATE_IDLE)

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _ADAPTER_H */
