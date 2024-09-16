/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/nic/adapter.h#5
*/

/*! \file   adapter.h
*    \brief  Definition of internal data structure for driver manipulation.
*
*    In this file we define the internal data structure - ADAPTER_T which stands
*    for MiniPort ADAPTER(From Windows point of view) or stands for Network ADAPTER.
*/

#ifndef _ADAPTER_H
#define _ADAPTER_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/
#if CFG_SUPPORT_CSI
#define CSI_RING_SIZE 1000
#define CSI_MAX_DATA_COUNT 256
#define CSI_MAX_RSVD1_COUNT 10
#endif


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#if CFG_SUPPORT_PASSPOINT
#include "hs20.h"
#endif /* CFG_SUPPORT_PASSPOINT */

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
enum {
	ENUM_SW_TEST_MODE_NONE = 0,
	ENUM_SW_TEST_MODE_SIGMA_AC = 0x1,
	ENUM_SW_TEST_MODE_SIGMA_WFD = 0x2,
	ENUM_SW_TEST_MODE_CTIA = 0x3,
	ENUM_SW_TEST_MODE_SIGMA_TDLS = 0x4,
	ENUM_SW_TEST_MODE_SIGMA_P2P = 0x5,
	ENUM_SW_TEST_MODE_SIGMA_N = 0x6,
	ENUM_SW_TEST_MODE_SIGMA_HS20_R1 = 0x7,
	ENUM_SW_TEST_MODE_SIGMA_HS20_R2 = 0x8,
	ENUM_SW_TEST_MODE_SIGMA_PMF = 0x9,
	ENUM_SW_TEST_MODE_SIGMA_WMMPS = 0xA,
	ENUM_SW_TEST_MODE_SIGMA_AC_R2 = 0xB,
	ENUM_SW_TEST_MODE_SIGMA_NAN = 0xC,
	ENUM_SW_TEST_MODE_SIGMA_AC_AP = 0xD,
	ENUM_SW_TEST_MODE_SIGMA_N_AP = 0xE,
	ENUM_SW_TEST_MODE_SIGMA_WFDS = 0xF,
	ENUM_SW_TEST_MODE_SIGMA_WFD_R2 = 0x10,
	ENUM_SW_TEST_MODE_SIGMA_LOCATION = 0x11,
	ENUM_SW_TEST_MODE_SIGMA_TIMING_MANAGEMENT = 0x12,
	ENUM_SW_TEST_MODE_SIGMA_WMMAC = 0x13,
	ENUM_SW_TEST_MODE_SIGMA_VOICE_ENT = 0x14
};

typedef struct _WLAN_INFO_T {
	PARAM_BSSID_EX_T rCurrBssId;

	/* Scan Result */
	PARAM_BSSID_EX_T arScanResult[CFG_MAX_NUM_BSS_LIST];
	PUINT_8 apucScanResultIEs[CFG_MAX_NUM_BSS_LIST];
	UINT_32 u4ScanResultNum;

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
	CMD_PS_PROFILE_T arPowerSaveMode[BSS_INFO_NUM];

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

	ENUM_PARAM_PHY_CONFIG_T eDesiredPhyConfig;

	/* Used for AP mode for desired channel and bandwidth */
	UINT_16 u2CountryCode;
	UINT_8 uc2G4BandwidthMode;	/* 20/40M or 20M only *//* Not used */
	UINT_8 uc5GBandwidthMode;	/* 20/40M or 20M only *//* Not used */

#if CFG_SUPPORT_802_11D
	BOOLEAN fgMultiDomainCapabilityEnabled;
#endif				/* CFG_SUPPORT_802_11D */

#if 1				/* CFG_SUPPORT_WAPI */
	BOOL fgWapiMode;
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

#if CFG_SUPPORT_CFG80211_AUTH
	struct cfg80211_bss *bss;

	BOOLEAN fgIsConnInitialized;

	BOOLEAN fgIsSendAssoc;

	BOOLEAN ucAuthDataLen;
	/* Temp assign a fixed large number
	 * Additional elements for Authentication frame,
	 * starts with the Authentication transaction sequence number field
	 */
	BOOLEAN aucAuthData[AUTH_DATA_MAX_LEN];
	UINT_8 ucChannelNum;
#endif
#if CFG_SUPPORT_OWE
	/* for OWE info store, when upper layer set rsn info */
	struct OWE_INFO_T rOweInfo;
#endif
	struct LINK_MGMT rBlackList;
} CONNECTION_SETTINGS_T, *P_CONNECTION_SETTINGS_T;

struct _BSS_INFO_T {

	ENUM_NETWORK_TYPE_T eNetworkType;

	UINT_32 u4PrivateData;	/* Private data parameter for each NETWORK type usage. */
	/* P2P network type has 3 network interface to distinguish. */

	ENUM_PARAM_MEDIA_STATE_T eConnectionState;	/* Connected Flag used in AIS_NORMAL_TR */
	ENUM_PARAM_MEDIA_STATE_T eConnectionStateIndicated;	/* The Media State that report to HOST */

	ENUM_OP_MODE_T eCurrentOPMode;	/* Current Operation Mode - Infra/IBSS */
#if CFG_ENABLE_WIFI_DIRECT
	ENUM_OP_MODE_T eIntendOPMode;
#endif

#if (CFG_SUPPORT_DFS_MASTER == 1)
	BOOLEAN fgIsDfsActive;
#endif

	BOOLEAN fgIsInUse;	/* For CNM to assign BSS_INFO */
	BOOLEAN fgIsNetActive;	/* TRUE if this network has been activated */

	UINT_8 ucBssIndex;	/* BSS_INFO_T index */

	UINT_8 ucReasonOfDisconnect;	/* Used by media state indication */

	UINT_8 ucSSIDLen;	/* Length of SSID */

#if CFG_ENABLE_WIFI_DIRECT
	ENUM_HIDDEN_SSID_TYPE_T eHiddenSsidType;	/* For Hidden SSID usage. */
#endif

	UINT_8 aucSSID[ELEM_MAX_LEN_SSID];	/* SSID used in this BSS */

	UINT_8 aucBSSID[MAC_ADDR_LEN];	/* The BSSID of the associated BSS */

	UINT_8 aucOwnMacAddr[MAC_ADDR_LEN];	/* Owned MAC Address used in this BSS */

	UINT_8 ucOwnMacIndex;	/* Owned MAC index used in this BSS */

	P_STA_RECORD_T prStaRecOfAP;	/* For Infra Mode, and valid only if
					 * eConnectionState == MEDIA_STATE_CONNECTED
					 */
	LINK_T rStaRecOfClientList;	/* For IBSS/AP Mode, all known STAs in current BSS */

	UINT_8 ucBMCWlanIndex;	/* For open Mode, BC/MC Tx wlan index, For STA, BC/MC Rx wlan index */

	UINT_8 ucBMCWlanIndexS[MAX_KEY_NUM];	/* For AP Mode, BC/MC Tx wlan index, For STA, BC/MC Rx wlan index */
	UINT_8 ucBMCWlanIndexSUsed[MAX_KEY_NUM];

	BOOLEAN fgBcDefaultKeyExist;	/* Bc Transmit key exist or not */
	UINT_8 ucBcDefaultKeyIdx;	/* Bc default key idx, for STA, the Rx just set, for AP, the tx key id */

	UINT_8 wepkeyUsed[MAX_KEY_NUM];
	UINT_8 wepkeyWlanIdx;	/* wlan index of the wep key */

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

	UINT_8 ucBeaconTimeoutCount;	/* For Infra/AP Mode, it is a threshold of Beacon Lost Count to
					*  confirm connection was lost
					*/

	BOOLEAN fgHoldSameBssidForIBSS;	/* For IBSS Mode, to keep use same BSSID to extend the life cycle of an IBSS */

	BOOLEAN fgIsBeaconActivated;	/* For AP/IBSS Mode, it is used to indicate that Beacon is sending */

	P_MSDU_INFO_T prBeacon;	/* For AP/IBSS Mode - Beacon Frame */

	BOOLEAN fgIsIBSSMaster;	/* For IBSS Mode - To indicate that we can reply ProbeResp Frame.
				* In current TBTT interval
				*/

	BOOLEAN fgIsShortPreambleAllowed;	/* From Capability Info. of AssocResp Frame
						* AND of Beacon/ProbeResp Frame
						*/
	BOOLEAN fgUseShortPreamble;	/* Short Preamble is enabled in current BSS. */
	BOOLEAN fgUseShortSlotTime;	/* Short Slot Time is enabled in current BSS. */

	UINT_16 u2OperationalRateSet;	/* Operational Rate Set of current BSS */
	UINT_16 u2BSSBasicRateSet;	/* Basic Rate Set of current BSS */

	UINT_8 ucAllSupportedRatesLen;	/* Used for composing Beacon Frame in AdHoc or AP Mode */
	UINT_8 aucAllSupportedRates[RATE_NUM_SW];

	UINT_8 ucAssocClientCnt;	/* TODO(Kevin): Number of associated clients */

	BOOLEAN fgIsProtection;
	BOOLEAN fgIsQBSS;	/* fgIsWmmBSS; *//* For Infra/AP/IBSS Mode, it is used to indicate if we support WMM in
				* current BSS.
				*/
	BOOLEAN fgIsNetAbsent;	/* TRUE: BSS is absent, FALSE: BSS is present */

	BOOLEAN fgIsWepCipherGroup;
	UINT_32 u4RsnSelectedGroupCipher;
	UINT_32 u4RsnSelectedPairwiseCipher;
	UINT_32 u4RsnSelectedAKMSuite;
	UINT_16 u2RsnSelectedCapInfo;

	UINT_8 ucOpChangeChannelWidth; /* The OpMode channel width that we want to change to*/
					/* 0:20MHz, 1:40MHz, 2:80MHz, 3:160MHz 4:80+80MHz */
	BOOLEAN fgIsOpChangeChannelWidth;

    /*------------------------------------------------------------------------*/
	/* Power Management related information                                   */
    /*------------------------------------------------------------------------*/
	PM_PROFILE_SETUP_INFO_T rPmProfSetupInfo;

    /*------------------------------------------------------------------------*/
	/* WMM/QoS related information                                            */
    /*------------------------------------------------------------------------*/
	UINT_8 ucWmmParamSetCount;	/* Used to detect the change of EDCA parameters. For AP mode,
					*the value is used in WMM IE
					*/

	AC_QUE_PARMS_T arACQueParms[WMM_AC_INDEX_NUM];

	UINT_8 aucCWminLog2ForBcast[WMM_AC_INDEX_NUM];	/* For AP mode, broadcast the CWminLog2 */
	UINT_8 aucCWmaxLog2ForBcast[WMM_AC_INDEX_NUM];	/* For AP mode, broadcast the CWmaxLog2 */
	AC_QUE_PARMS_T arACQueParmsForBcast[WMM_AC_INDEX_NUM];	/* For AP mode, broadcast the value */
	UINT_8 ucWmmQueSet;
#if (CFG_HW_WMM_BY_BSS == 1)
	BOOLEAN fgIsWmmInited;
#endif

    /*------------------------------------------------------------------------*/
	/* 802.11n HT operation IE when (prStaRec->ucPhyTypeSet & PHY_TYPE_BIT_HT) */
	/* is true. They have the same definition with fields of                  */
	/* information element (CM)                                               */
    /*------------------------------------------------------------------------*/
	ENUM_BAND_T eBand;
	UINT_8 ucPrimaryChannel;
	UINT_8 ucHtOpInfo1;
	UINT_8 ucHtPeerOpInfo1; /*Backup peer HT OP Info*/
	UINT_16 u2HtOpInfo2;
	UINT_16 u2HtOpInfo3;
	UINT_8 ucNss;
    /*------------------------------------------------------------------------*/
	/* 802.11ac VHT operation IE when (prStaRec->ucPhyTypeSet & PHY_TYPE_BIT_VHT) */
	/* is true. They have the same definition with fields of                  */
	/* information element (EASON)                                               */
    /*------------------------------------------------------------------------*/
#if 1				/* CFG_SUPPORT_802_11AC */
	UINT_8 ucVhtChannelWidth;
	UINT_8 ucVhtChannelFrequencyS1;
	UINT_8 ucVhtChannelFrequencyS2;
	UINT_16 u2VhtBasicMcsSet;

	/* Backup peer VHT OpInfo */
	UINT_8 ucVhtPeerChannelWidth;
	UINT_8 ucVhtPeerChannelFrequencyS1;
	UINT_8 ucVhtPeerChannelFrequencyS2;
#endif
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
	ENUM_CHNL_EXT_T eBssSCO;	/* Real setting for HW
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

	BOOLEAN fgObssActionForcedTo20M;	/* GO only */
	BOOLEAN fgObssBeaconForcedTo20M;	/* GO only */

    /*------------------------------------------------------------------------*/
	/* HW Related Fields (Kevin)                                              */
    /*------------------------------------------------------------------------*/
	UINT_16 u2HwDefaultFixedRateCode;	/* The default rate code copied to MAC TX Desc */
	UINT_16 u2HwLPWakeupGuardTimeUsec;

	UINT_8 ucBssFreeQuota;	/* The value is updated from FW  */

#if CFG_ENABLE_GTK_FRAME_FILTER
	P_IPV4_NETWORK_ADDRESS_LIST prIpV4NetAddrList;
#endif
	UINT_16 u2DeauthReason;

#if CFG_SUPPORT_TDLS
	BOOLEAN fgTdlsIsProhibited;
	BOOLEAN fgTdlsIsChSwProhibited;
#endif
#if CFG_SUPPORT_PNO
	BOOLEAN fgIsPNOEnable;
	BOOLEAN fgIsNetRequestInActive;
#endif

	WIFI_WMM_AC_STAT_T arLinkStatistics[WMM_AC_INDEX_NUM];	/*link layer statistics */

	ENUM_DBDC_BN_T eDBDCBand;

	UINT_32 u4CoexPhyRateLimit;

#if CFG_SUPPORT_ROAMING_SKIP_ONE_AP
	UINT_8	ucRoamSkipTimes;
	BOOLEAN fgGoodRcpiArea;
	BOOLEAN fgPoorRcpiArea;
#endif

	BOOLEAN fgIsGranted;
	ENUM_BAND_T eBandGranted;
	UINT_8 ucPrimaryChannelGranted;
	PARAM_CUSTOM_ACL rACL;

#if CFG_SUPPORT_802_11W
	/* AP PMF */
	struct AP_PMF_CFG rApPmfCfg;
#endif

#if CFG_SUPPORT_REPLAY_DETECTION
	struct SEC_DETECT_REPLAY_INFO rDetRplyInfo;
#endif

};

struct _AIS_SPECIFIC_BSS_INFO_T {
	UINT_8 ucRoamingAuthTypes;	/* This value indicate the roaming type used in AIS_JOIN */

	BOOLEAN fgIsIBSSActive;

	/*! \brief Global flag to let arbiter stay at standby and not connect to any network */
	BOOLEAN fgCounterMeasure;
	/* UINT_8                  ucTxWlanIndex; *//* Legacy wep, adhoc wep wpa Transmit key wlan index */

	/* BOOLEAN                 fgKeyMaterialExist[4]; */
	/* UINT_8                  aucKeyMaterial[32][4]; */

#if 0
	BOOLEAN fgWepWapiBcKeyExist;	/* WEP WAPI BC key exist flag */
	UINT_8 ucWepWapiBcWlanIndex;	/* WEP WAPI BC wlan index */

	BOOLEAN fgRsnBcKeyExist[4];	/* RSN BC key exist flag, map to key id 0, 1, 2, 3 */
	UINT_8 ucRsnBcWlanIndex[4];	/* RSN BC wlan index, map to key id 0, 1, 2, 3 */
#endif

	/* While Do CounterMeasure procedure, check the EAPoL Error report have send out */
	BOOLEAN fgCheckEAPoLTxDone;

	UINT_32 u4RsnaLastMICFailTime;

	/* Stored the current bss wpa rsn cap filed, used for roaming policy */
	/* UINT_16                 u2RsnCap; */
	TIMER_T rPreauthenticationTimer;

	/* By the flow chart of 802.11i,
	 *  wait 60 sec before associating to same AP
	 *  or roaming to a new AP
	 *  or sending data in IBSS,
	 *  keep a timer for handle the 60 sec counterMeasure
	 */
	TIMER_T rRsnaBlockTrafficTimer;
	TIMER_T rRsnaEAPoLReportTimeoutTimer;

	/* For Keep the Tx/Rx Mic key for TKIP SW Calculate Mic */
	/* This is only one for AIS/AP */
	UINT_8 aucTxMicKey[8];
	UINT_8 aucRxMicKey[8];

	/* Buffer for WPA2 PMKID */
	/* The PMKID cache lifetime is expire by media_disconnect_indication */
	UINT_32 u4PmkidCandicateCount;
	PMKID_CANDICATE_T arPmkidCandicate[CFG_MAX_PMKID_CACHE];
	UINT_32 u4PmkidCacheCount;
	PMKID_ENTRY_T arPmkidCache[CFG_MAX_PMKID_CACHE];
	BOOLEAN fgIndicatePMKID;
#if CFG_SUPPORT_802_11W
	BOOLEAN fgMgmtProtection;
	BOOLEAN fgAPApplyPmfReq;
	UINT_32 u4SaQueryStart;
	UINT_32 u4SaQueryCount;
	UINT_8 ucSaQueryTimedOut;
	PUINT_8 pucSaQueryTransId;
	TIMER_T rSaQueryTimer;
	BOOLEAN fgBipKeyInstalled;
#endif
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

typedef struct _WLAN_TABLE_T {
	UINT_8 ucUsed;
	UINT_8 ucBssIndex;
	UINT_8 ucKeyId;
	UINT_8 ucPairwise;
	UINT_8 aucMacAddr[MAC_ADDR_LEN];
	UINT_8 ucStaIndex;
} WLAN_TABLE_T, *P_WLAN_TABLE_T;

/* Major member variables for WiFi FW operation.
 *  Variables within this region will be ready for access after WIFI function is enabled.
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

	ENUM_PWR_STATE_T aePwrState[BSS_INFO_NUM];

	BSS_INFO_T arBssInfoPool[BSS_INFO_NUM];

	P2P_DEV_INFO_T rP2pDevInfo;

	AIS_SPECIFIC_BSS_INFO_T rAisSpecificBssInfo;

#if CFG_ENABLE_WIFI_DIRECT
	P_P2P_CONNECTION_SETTINGS_T prP2PConnSettings[BSS_P2P_NUM];

	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo[BSS_P2P_NUM];

/* P_P2P_FSM_INFO_T prP2pFsmInfo; */

	P_P2P_DEV_FSM_INFO_T prP2pDevFsmInfo;

	/* Currently we only support 2 p2p interface. */
	P_P2P_ROLE_FSM_INFO_T aprP2pRoleFsmInfo[BSS_P2P_NUM];

#endif				/* CFG_ENABLE_WIFI_DIRECT */

#if CFG_ENABLE_BT_OVER_WIFI
	BOW_SPECIFIC_BSS_INFO_T rBowSpecificBssInfo;
	BOW_FSM_INFO_T rBowFsmInfo;
#endif				/* CFG_ENABLE_BT_OVER_WIFI */

	WLAN_TABLE_T arWtbl[WTBL_SIZE];

	DEAUTH_INFO_T arDeauthInfo[MAX_DEAUTH_INFO_COUNT];

	/* Current Wi-Fi Settings and Flags */
	UINT_8 aucPermanentAddress[MAC_ADDR_LEN];
	UINT_8 aucMacAddress[MAC_ADDR_LEN];
	UINT_8 aucDeviceAddress[MAC_ADDR_LEN];
	UINT_8 aucInterfaceAddress[MAC_ADDR_LEN];

	UINT_8 ucAvailablePhyTypeSet;

	ENUM_PHY_TYPE_INDEX_T eNonHTBasicPhyType2G4;	/* Basic Phy Type used by SCN according
							 * to the set of Available PHY Types
							 */

	ENUM_PARAM_PREAMBLE_TYPE_T ePreambleType;
	ENUM_REGISTRY_FIXED_RATE_T eRateSetting;

	BOOLEAN fgIsShortSlotTimeOptionEnable;
	/* User desired setting, but will honor the capability of AP */

	BOOLEAN fgEnableJoinToHiddenSSID;
	BOOLEAN fgSupportWZCDisassociation;

#if CFG_SUPPORT_WFD
	WFD_CFG_SETTINGS_T rWfdConfigureSettings;
#endif

#if CFG_SLT_SUPPORT
	SLT_INFO_T rSltInfo;
#endif

#if CFG_SUPPORT_PASSPOINT
	HS20_INFO_T rHS20Info;
#endif				/* CFG_SUPPORT_PASSPOINT */
	UINT_8 aucMediatekOuiIE[64];
	UINT_16 u2MediatekOuiIELen;

	/* Feature Options */
	UINT_8 ucQoS;

	UINT_8 ucStaHt;
	UINT_8 ucStaVht;
	UINT_8 ucApHt;
	UINT_8 ucApVht;
	UINT_8 ucP2pGoHt;
	UINT_8 ucP2pGoVht;
	UINT_8 ucP2pGcHt;
	UINT_8 ucP2pGcVht;

	UINT_8 ucAmpduTx;
	UINT_8 ucAmpduRx;
	UINT_8 ucAmsduInAmpduTx;
	UINT_8 ucAmsduInAmpduRx;
	UINT_8 ucHtAmsduInAmpduTx;
	UINT_8 ucHtAmsduInAmpduRx;
	UINT_8 ucVhtAmsduInAmpduTx;
	UINT_8 ucVhtAmsduInAmpduRx;
	UINT_8 ucTspec;
	UINT_8 ucUapsd;
	UINT_8 ucStaUapsd;
	UINT_8 ucApUapsd;
	UINT_8 ucP2pUapsd;

	UINT_8 ucTxShortGI;
	UINT_8 ucRxShortGI;
	UINT_8 ucTxLdpc;
	UINT_8 ucRxLdpc;
	UINT_8 ucTxStbc;
	UINT_8 ucRxStbc;
	UINT_8 ucRxStbcNss;
	UINT_8 ucTxGf;
	UINT_8 ucRxGf;

	UINT_8 ucMCS32;

	UINT_8 ucTxopPsTx;
	UINT_8 ucSigTaRts;
	UINT_8 ucDynBwRts;

	UINT_8 ucStaHtBfee;
	UINT_8 ucStaVhtBfee;
	UINT_8 ucStaHtBfer;
	UINT_8 ucStaVhtBfer;
	UINT_8 ucStaVhtMuBfee;

	UINT_8 ucDataTxDone;
	UINT_8 ucDataTxRateMode;
	UINT_32 u4DataTxRateCode;

	UINT_8 ucApWpsMode;
	UINT_8 ucApChannel;

	UINT_8 ucApSco;
	UINT_8 ucP2pGoSco;

	UINT_8 ucStaBandwidth;
	UINT_8 ucSta5gBandwidth;
	UINT_8 ucSta2gBandwidth;
	UINT_8 ucApBandwidth;
	UINT_8 ucAp2gBandwidth;
	UINT_8 ucAp5gBandwidth;
	UINT_8 ucP2p5gBandwidth;
	UINT_8 ucP2p2gBandwidth;

	/* If enable, AP channel bandwidth Channel Center Frequency Segment 0/1 */
	/* and secondary channel offset will align wifi.cfg */
	/* Otherwise align cfg80211 */
	UINT_8 ucApChnlDefFromCfg;

	/*
	 * According TGn/TGac 4.2.44, AP should not connect
	 * to TKIP client with * HT/VHT capabilities. We leave
	 * a wifi.cfg item for user to decide whether * to
	 * enable HT/VHT capabilities in that case
	 */
	UINT_8 ucApAllowHtVhtTkip;

	UINT_8 ucNSS;

	UINT_8 ucRxMaxMpduLen;
	UINT_32 u4TxMaxAmsduInAmpduLen;

	UINT_8 ucTxBaSize;
	UINT_8 ucRxHtBaSize;
	UINT_8 ucRxVhtBaSize;

	UINT_8 ucStaDisconnectDetectTh;
	UINT_8 ucApDisconnectDetectTh;
	UINT_8 ucP2pDisconnectDetectTh;

	UINT_8 ucThreadScheduling;
	UINT_8 ucThreadPriority;
	INT_8 cThreadNice;

	UINT_8 ucTcRestrict;
	UINT_32 u4MaxTxDeQLimit;
	UINT_8 ucAlwaysResetUsedRes;

	UINT_32 u4NetifStopTh;
	UINT_32 u4NetifStartTh;
#if CFG_AUTO_CHANNEL_SEL_SUPPORT
	PARAM_GET_CHN_INFO rChnLoadInfo;
#endif
#if CFG_SUPPORT_MTK_SYNERGY
	UINT_8 ucMtkOui;
	UINT_32 u4MtkOuiCap;
	UINT_8 aucMtkFeature[4];
#endif

	BOOLEAN fgCsaInProgress;
	UINT_8 ucChannelSwitchMode;
	UINT_8 ucNewChannelNumber;
	UINT_8 ucChannelSwitchCount;

	UINT_32 u4HifIstLoopCount;
	UINT_32 u4Rx2OsLoopCount;
	UINT_32 u4HifTxloopCount;
	UINT_32 u4TxRxLoopCount;
	UINT_32 u4TxFromOsLoopCount;
	UINT_32 u4TxIntThCount;

	UINT_32 au4TcPageCount[TC_NUM];
	UINT_8 ucExtraTxDone;
	UINT_8 ucTxDbg;

	UINT_8 ucCmdRsvResource;
	UINT_32 u4MgmtQueueDelayTimeout;

	UINT_32 u4StatsLogTimeout;
	UINT_32 u4StatsLogDuration;
	UINT_8 ucDhcpTxDone;
	UINT_8 ucArpTxDone;

	UINT_8 ucMacAddrOverride;
	UINT_8 aucMacAddrStr[32];

	UINT_8 ucCtiaMode;
	UINT_8 ucTpTestMode;
	UINT_8 ucSigmaTestMode;
#if CFG_SUPPORT_DBDC
	UINT_8 ucDbdcMode;
	BOOLEAN fgDbDcModeEn;
	TIMER_T rDBDCDisableCountdownTimer;	/* Prevent continuously trigger by reconnection  */
	TIMER_T rDBDCSwitchGuardTimer;		/* Prevent switch too quick*/
#endif
	UINT_8 u4ScanCtrl;
	UINT_8 ucScanChannelListenTime;

#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 1)
	UINT_8 ucEfuseBufferModeCal;
#endif
	UINT_8 ucCalTimingCtrl;
	UINT_8 ucWow;
	UINT_8 ucOffload;
	UINT_8 ucAdvPws; /* enable LP multiple DTIM function, default disable */
	UINT_8 ucWowOnMdtim; /* multiple DTIM if WOW enable, default 1 */
	UINT_8 ucWowOffMdtim; /* multiple DTIM if WOW disable, default 3 */
	UINT_8 ucWowPwsMode; /* when enter wow, automatically enter wow power-saving profile */
	UINT_8 ucListenDtimInterval; /* adjust the listen interval by dtim interval */
	UINT_8 ucEapolOffload; /* eapol offload when active mode / wow mode */

#if CFG_SUPPORT_REPLAY_DETECTION
	UINT_8 ucRpyDetectOffload; /* eapol offload when active mode / wow mode */
#endif

	UINT_8 u4SwTestMode;
	UINT_8	ucCtrlFlagAssertPath;
	UINT_8	ucCtrlFlagDebugLevel;
	UINT_32 u4WakeLockRxTimeout;
	UINT_32 u4WakeLockThreadWakeup;
	UINT_32 u4RegP2pIfAtProbe; /* register p2p interface during probe */
	UINT_8 ucP2pShareMacAddr; /* p2p group interface use the same mac addr as p2p device interface */
	UINT_8 ucSmartRTS;

	UINT_32 u4UapsdAcBmp;
	UINT_32 u4MaxSpLen;
	UINT_32 fgDisOnlineScan;	/* 0: enable online scan, non-zero: disable online scan */
	UINT_32 fgDisBcnLostDetection;
	UINT_32 fgDisRoaming;		/* 0:enable roaming 1:disable */
	UINT_32 fgEnArpFilter;

	UINT_8	uDeQuePercentEnable;
	UINT_32	u4DeQuePercentVHT80Nss1;
	UINT_32	u4DeQuePercentVHT40Nss1;
	UINT_32	u4DeQuePercentVHT20Nss1;
	UINT_32	u4DeQuePercentHT40Nss1;
	UINT_32	u4DeQuePercentHT20Nss1;

	BOOLEAN fgTdlsBufferSTASleep; /* Support TDLS 5.5.4.2 optional case */
	BOOLEAN fgChipResetRecover;

	UINT_8 ucN9Log2HostCtrl;
	UINT_8 ucCR4Log2HostCtrl;

#if CFG_SUPPORT_ANT_SELECT
	UINT_8  ucSpeIdxCtrl;   /* 0:WF0, 1:WF1, 2: both WF0/1 */
#endif
#ifdef CFG_SUPPORT_ADJUST_JOIN_CH_REQ_INTERVAL
	UINT_32 u4AisJoinChReqIntervel;
#endif
} WIFI_VAR_T, *P_WIFI_VAR_T;	/* end of _WIFI_VAR_T */

/* cnm_timer module */
typedef struct {
	LINK_T rLinkHead;
	OS_SYSTIME rNextExpiredSysTime;
#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
	KAL_WAKE_LOCK_T rWakeLock;
#endif
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
	/* N9 SW */
	UINT_16 u2FwProductID;
	UINT_16 u2FwOwnVersion;
	UINT_16 u2FwPeerVersion;
	UINT_8 ucFwBuildNumber;
	UINT_8 aucFwBranchInfo[4];
	UINT_8 aucFwDateCode[16];

	/* N9 tailer */
	tailer_format_t rN9tailer;

	/* CR4 tailer */
	tailer_format_t rCR4tailer;
#if CFG_SUPPORT_COMPRESSION_FW_OPTION
	/* N9 Compressed tailer */
	tailer_format_t_2 rN9Compressedtailer;
	/* CR4 tailer */
	tailer_format_t_2 rCR4Compressedtailer;
	BOOLEAN fgIsN9CompressedFW;
	BOOLEAN fgIsCR4CompressedFW;
#endif
	/* Patch header */
	PATCH_FORMAT_T rPatchHeader;
	BOOLEAN fgPatchIsDlByDrv;
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

typedef struct _WIFI_FEM_CFG_T {
	/* WiFi FEM path */
	UINT_16 u2WifiPath;
	UINT_16 u2Reserved;
	/* Reserved  */
	UINT_32 au4Reserved[4];
} WIFI_FEM_CFG_T, *P_WIFI_FEM_CFG_T;

#if CFG_SUPPORT_CSI
/*
 * CSI_DATA_T is used for representing
 * the CSI and other useful * information
 * for application usage
 */
struct CSI_DATA_T {
	UINT_8 ucBw;
	BOOLEAN bIsCck;
	UINT_16 u2DataCount;
	INT_16 ac2IData[CSI_MAX_DATA_COUNT];
	INT_16 ac2QData[CSI_MAX_DATA_COUNT];
	UINT_8 ucDbdcIdx;
	INT_8 cRssi;
	UINT_8 ucSNR;
	UINT_64 u8TimeStamp;
	UINT_8 ucDataBw;
	UINT_8 ucPrimaryChIdx;
	UINT_8 aucTA[MAC_ADDR_LEN];
	UINT_32 u4ExtraInfo;
	UINT_8 ucRxMode;
	INT_32 ai4Rsvd1[CSI_MAX_RSVD1_COUNT];
	INT_32 au4Rsvd2[CSI_MAX_RSVD1_COUNT];
	UINT_8 ucRsvd1Cnt;
	INT_32 i4Rsvd3;
	UINT_8 ucRsvd4;
};

/*
 * CSI_INFO_T is used to store the CSI
 * settings and CSI event data
 */
struct CSI_INFO_T {
	/* Variables for manipulate the CSI data in g_aucProcBuf */
	BOOLEAN bIncomplete;
	INT_32 u4CopiedDataSize;
	INT_32 u4RemainingDataSize;
	wait_queue_head_t waitq;
	/* Variable for recording the CSI function config */
	UINT_8 ucMode;
	UINT_8 ucValue1[CSI_CONFIG_ITEM_NUM];
	UINT_8 ucValue2[CSI_CONFIG_ITEM_NUM];
	/* Variable for manipulating the CSI ring buffer */
	struct CSI_DATA_T arCSIBuffer[CSI_RING_SIZE];
	UINT_32 u4CSIBufferHead;
	UINT_32 u4CSIBufferTail;
	UINT_32 u4CSIBufferUsed;
	INT_16 ai2TempIData[CSI_MAX_DATA_COUNT];
	INT_16 ai2TempQData[CSI_MAX_DATA_COUNT];
};
#endif


/*
 * Major ADAPTER structure
 * Major data structure for driver operation
 */
struct _ADAPTER_T {
	struct mt66xx_chip_info *chip_info;
	UINT_8 ucRevID;
	BOOLEAN fgIsReadRevID;

	UINT_16 u2NicOpChnlNum;

	BOOLEAN fgIsEnableWMM;
	BOOLEAN fgIsWmmAssoc;	/* This flag is used to indicate that WMM is enable in current BSS */

	UINT_32 u4OsPacketFilter;	/* packet filter used by OS */
	BOOLEAN fgAllMulicastFilter;	/* mDNS filter used by OS */

	P_BSS_INFO_T aprBssInfo[HW_BSSID_NUM + 1];
	P_BSS_INFO_T prAisBssInfo;

#if CFG_TCP_IP_CHKSUM_OFFLOAD
	BOOLEAN fgIsSupportCsumOffload; /* Does FW support Checksum Offload feature */
	UINT_32 u4CSUMFlags;
#endif				/* CFG_TCP_IP_CHKSUM_OFFLOAD */

	ENUM_BAND_T aePreferBand[BSS_INFO_NUM];

	/* ADAPTER flags */
	UINT_32 u4Flags;
	UINT_32 u4HwFlags;

	BOOLEAN fgIsRadioOff;

	BOOLEAN fgIsEnterD3ReqIssued;

	UINT_8 aucMacAddress[MAC_ADDR_LEN];

	ENUM_PHY_TYPE_INDEX_T eCurrentPhyType;	/* Current selection basing on the set of Available PHY Types */

	UINT_32 u4CoalescingBufCachedSize;
	PUINT_8 pucCoalescingBufCached;

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

	/* Timer for restarting RFB setup procedure */
	TIMER_T rPacketDelaySetupTimer;

	/* Buffer for Authentication Event */
	/* <Todo> Move to glue layer and refine the kal function */
	/* Reference to rsnGeneratePmkidIndication function at rsn.c */
	UINT_8 aucIndicationEventBuffer[(CFG_MAX_PMKID_CACHE * 20) + 8];

	UINT_32 u4IntStatus;

	ENUM_ACPI_STATE_T rAcpiState;

	BOOLEAN fgIsIntEnable;
	BOOLEAN fgIsIntEnableWithLPOwnSet;

	BOOLEAN fgIsFwOwn;
	BOOLEAN fgWiFiInSleepyState;

	/* Set by callback to make sure WOW process done before system suspend */
	BOOLEAN fgSetPfCapabilityDone;
	BOOLEAN fgSetWowDone;

	BOOLEAN fgForceFwOwn;

	OS_SYSTIME rLastOwnFailedLogTime;
	UINT_32 u4OwnFailedCount;
	UINT_32 u4OwnFailedLogCount;

	UINT_32 u4PwrCtrlBlockCnt;

	/* TX Direct related : BEGIN */
	BOOLEAN fgTxDirectInited;

	#define TX_DIRECT_CHECK_INTERVAL	(1000 * HZ / USEC_PER_SEC)
	struct timer_list rTxDirectSkbTimer; /* check if an empty MsduInfo is available */
	struct timer_list rTxDirectHifTimer; /* check if HIF port is ready to accept a new Msdu */

	struct sk_buff_head rTxDirectSkbQueue;
	QUE_T rTxDirectHifQueue[TX_PORT_NUM];

	QUE_T rStaPsQueue[CFG_STA_REC_NUM];
	UINT_32 u4StaPsBitmap;
	QUE_T rBssAbsentQueue[HW_BSSID_NUM + 1];
	UINT_32 u4BssAbsentBitmap;
	/* TX Direct related : END */

	QUE_T rPendingCmdQueue;

#if CFG_SUPPORT_MULTITHREAD
	QUE_T rTxCmdQueue;
	QUE_T rTxCmdDoneQueue;
#if CFG_FIX_2_TX_PORT
	QUE_T rTxP0Queue;
	QUE_T rTxP1Queue;
#else
	QUE_T rTxPQueue[TX_PORT_NUM];
#endif
	QUE_T rRxQueue;
	QUE_T rTxDataDoneQueue;
#endif

	P_GLUE_INFO_T prGlueInfo;

	UINT_8 ucCmdSeqNum;
	UINT_8 ucTxSeqNum;
	UINT_8 aucPidPool[WTBL_SIZE];

#if 1				/* CFG_SUPPORT_WAPI */
	BOOLEAN fgUseWapi;
#endif

	/* RF Test flags */
	BOOLEAN fgTestMode;
	BOOLEAN fgIcapMode;

	/* WLAN Info for DRIVER_CORE OID query */
	WLAN_INFO_T rWlanInfo;

#if CFG_ENABLE_WIFI_DIRECT
	BOOLEAN fgIsP2PRegistered;
	BOOLEAN p2p_scan_report_all_bss; /* flag to report all networks in p2p scan */
	ENUM_NET_REG_STATE_T rP2PNetRegState;
	/* BOOLEAN             fgIsWlanLaunched; */
	P_P2P_INFO_T prP2pInfo;
#if CFG_SUPPORT_P2P_RSSI_QUERY
	OS_SYSTIME rP2pLinkQualityUpdateTime;
	BOOLEAN fgIsP2pLinkQualityValid;
	EVENT_LINK_QUALITY rP2pLinkQuality;
#endif
#endif

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

#if CFG_SUPPORT_MSP
	EVENT_WLAN_INFO rEventWlanInfo;
#endif

#if CFG_SUPPORT_LAST_SEC_MCS_INFO
	TIMER_T rRxMcsInfoTimer;
	BOOLEAN fgIsMcsInfoValid;
#endif

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
	UINT_8 ucOidTimeoutCount;

	/* Root Timer for cnm_timer module */
	ROOT_TIMER rRootTimer;

	BOOLEAN fgIsChipNoAck;
	BOOLEAN fgIsChipAssert;

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

	/* Bitmap is defined as #define KEEP_FULL_PWR_{FEATURE}_BIT in wlan_lib.h
	 * Each feature controls KeepFullPwr(CMD_ID_KEEP_FULL_PWR) should
	 * register bitmap to ensure low power during suspend.
	 */
	UINT_32 u4IsKeepFullPwrBitmap;

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

	BOOL fgDisStaAgingTimeoutDetection;

	UINT_32 u4FwCompileFlag0;
	UINT_32 u4FwCompileFlag1;
	UINT_32 u4FwFeatureFlag0;
	UINT_32 u4FwFeatureFlag1;

#if CFG_SUPPORT_CFG_FILE
	P_WLAN_CFG_T prWlanCfg;
	WLAN_CFG_T rWlanCfg;

	P_WLAN_CFG_REC_T prWlanCfgRec;
	WLAN_CFG_REC_T rWlanCfgRec;
#endif

#if CFG_M0VE_BA_TO_DRIVER
	TIMER_T rMqmIdleRxBaDetectionTimer;
	UINT_32 u4FlagBitmap;
#endif
#if CFG_ASSERT_DUMP
	TIMER_T rN9CorDumpTimer;
	TIMER_T rCr4CorDumpTimer;
	BOOLEAN fgN9CorDumpFileOpend;
	BOOLEAN fgCr4CorDumpFileOpend;
	BOOLEAN fgN9AssertDumpOngoing;
	BOOLEAN fgCr4AssertDumpOngoing;
	BOOLEAN fgKeepPrintCoreDump;
#endif
	/* Tx resource information */
	BOOLEAN fgIsNicTxReousrceValid;
	NIC_TX_RESOURCE_T nicTxReousrce;

    /* Efuse Start and End address */
	UINT_32 u4EfuseStartAddress;
	UINT_32 u4EfuseEndAddress;

	/* COEX feature */
	UINT_32 u4FddMode;

#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 1)
	/* MAC address Efuse Offset */
	UINT_32 u4EfuseMacAddrOffset;
#endif

#if CFG_WOW_SUPPORT
	WOW_CTRL_T	rWowCtrl;
#endif

/*#if (CFG_EEPROM_PAGE_ACCESS == 1)*/
	UINT_8 aucEepromVaule[16]; /* HQA CMD for Efuse Block size contents */
	UINT_32 u4FreeBlockNum;
	UINT_32 u4GetTxPower;
/*#endif*/
	BOOLEAN fgIsCr4FwDownloaded;
	BOOLEAN fgIsFwDownloaded;
	BOOLEAN fgIsSupportBufferBinSize16Byte;
	BOOLEAN fgIsSupportDelayCal;
	BOOLEAN fgIsSupportGetFreeEfuseBlockCount;
	BOOLEAN fgIsSupportQAAccessEfuse;
	BOOLEAN fgIsSupportPowerOnSendBufferModeCMD;
	BOOLEAN fgIsBufferBinExtract;
	BOOLEAN fgIsSupportGetTxPower;
	BOOLEAN fgIsEnableLpdvt;

	/* SER related info */
	UINT_8 ucSerState;

#if CFG_SUPPORT_BFER
	BOOLEAN fgIsHwSupportBfer;
#endif

#if (CFG_HW_WMM_BY_BSS == 1)
	UINT_8 ucHwWmmEnBit;
#endif
	WIFI_FEM_CFG_T rWifiFemCfg;

	UINT_8 ucRModeOnlyFlag;
	UINT_8 ucRModeReserve[7];

#if CFG_SUPPORT_CSI
	struct CSI_INFO_T rCSIInfo;
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
/* Macros for argument _BssIndex */
#define IS_NET_ACTIVE(_prAdapter, _BssIndex) \
	((_prAdapter)->aprBssInfo[(_BssIndex)]->fgIsNetActive)

/* Macros for argument _prBssInfo */
#define IS_BSS_ACTIVE(_prBssInfo)     ((_prBssInfo)->fgIsNetActive)

#define IS_BSS_AIS(_prBssInfo) \
	((_prBssInfo)->eNetworkType == NETWORK_TYPE_AIS)

#define IS_BSS_P2P(_prBssInfo) \
	((_prBssInfo)->eNetworkType == NETWORK_TYPE_P2P)

#define IS_BSS_BOW(_prBssInfo) \
	((_prBssInfo)->eNetworkType == NETWORK_TYPE_BOW)

#define SET_NET_ACTIVE(_prAdapter, _BssIndex) \
	{(_prAdapter)->aprBssInfo[(_BssIndex)]->fgIsNetActive = TRUE; }

#define UNSET_NET_ACTIVE(_prAdapter, _BssIndex) \
	{(_prAdapter)->aprBssInfo[(_BssIndex)]->fgIsNetActive = FALSE; }

#define BSS_INFO_INIT(_prAdapter, _prBssInfo) \
	{   UINT_8 _aucZeroMacAddr[] = NULL_MAC_ADDR; \
	    \
	    (_prBssInfo)->eConnectionState = PARAM_MEDIA_STATE_DISCONNECTED; \
	    (_prBssInfo)->eConnectionStateIndicated = PARAM_MEDIA_STATE_DISCONNECTED; \
	    (_prBssInfo)->eCurrentOPMode = OP_MODE_INFRASTRUCTURE; \
	    (_prBssInfo)->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_RESERVED; \
	    COPY_MAC_ADDR((_prBssInfo)->aucBSSID, _aucZeroMacAddr); \
	    LINK_INITIALIZE(&((_prBssInfo)->rStaRecOfClientList)); \
	    (_prBssInfo)->fgIsBeaconActivated = FALSE; \
	    (_prBssInfo)->u2HwDefaultFixedRateCode = RATE_CCK_1M_LONG; \
	}

/*----------------------------------------------------------------------------*/
/* Macros for Power State                                                     */
/*----------------------------------------------------------------------------*/
#define SET_NET_PWR_STATE_IDLE(_prAdapter, _BssIndex) \
		{_prAdapter->rWifiVar.aePwrState[(_BssIndex)] = PWR_STATE_IDLE; }

#define SET_NET_PWR_STATE_ACTIVE(_prAdapter, _BssIndex) \
		{_prAdapter->rWifiVar.aePwrState[(_BssIndex)] = PWR_STATE_ACTIVE; }

#define SET_NET_PWR_STATE_PS(_prAdapter, _BssIndex) \
		{_prAdapter->rWifiVar.aePwrState[(_BssIndex)] = PWR_STATE_PS; }

#define IS_NET_PWR_STATE_ACTIVE(_prAdapter, _BssIndex) \
		(_prAdapter->rWifiVar.aePwrState[(_BssIndex)] == PWR_STATE_ACTIVE)

#define IS_NET_PWR_STATE_IDLE(_prAdapter, _BssIndex) \
		(_prAdapter->rWifiVar.aePwrState[(_BssIndex)] == PWR_STATE_IDLE)

#define IS_SCN_PWR_STATE_ACTIVE(_prAdapter) \
		(_prAdapter->rWifiVar.rScanInfo.eScanPwrState == SCAN_PWR_STATE_ACTIVE)

#define IS_SCN_PWR_STATE_IDLE(_prAdapter) \
		(_prAdapter->rWifiVar.rScanInfo.eScanPwrState == SCAN_PWR_STATE_IDLE)

#define IS_WIFI_2G4_WF0_SUPPORT(_prAdapter) \
	((_prAdapter)->rWifiFemCfg.u2WifiPath & WLAN_FLAG_2G4_WF0)

#define IS_WIFI_5G_WF0_SUPPORT(_prAdapter) \
	((_prAdapter)->rWifiFemCfg.u2WifiPath & WLAN_FLAG_5G_WF0)

#define IS_WIFI_2G4_WF1_SUPPORT(_prAdapter) \
	((_prAdapter)->rWifiFemCfg.u2WifiPath & WLAN_FLAG_2G4_WF1)

#define IS_WIFI_5G_WF1_SUPPORT(_prAdapter) \
	((_prAdapter)->rWifiFemCfg.u2WifiPath & WLAN_FLAG_5G_WF1)

#define IS_WIFI_2G4_SISO(_prAdapter) \
	((IS_WIFI_2G4_WF0_SUPPORT(_prAdapter) && !(IS_WIFI_2G4_WF1_SUPPORT(_prAdapter))) || \
	(IS_WIFI_2G4_WF1_SUPPORT(_prAdapter) && !(IS_WIFI_2G4_WF0_SUPPORT(_prAdapter))))

#define IS_WIFI_5G_SISO(_prAdapter) \
	((IS_WIFI_5G_WF0_SUPPORT(_prAdapter) && !(IS_WIFI_5G_WF1_SUPPORT(_prAdapter))) || \
	(IS_WIFI_5G_WF1_SUPPORT(_prAdapter) && !(IS_WIFI_5G_WF0_SUPPORT(_prAdapter))))

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _ADAPTER_H */
