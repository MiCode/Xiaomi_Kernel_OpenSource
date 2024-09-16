/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/nic/nic.h#1
*/

/*
 * ! \file   "nic.h"
 *  \brief  The declaration of nic functions
 *
 *   Detail description.
 */

#ifndef _NIC_H
#define _NIC_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

struct _REG_ENTRY_T {
	UINT_32 u4Offset;
	UINT_32 u4Value;
};

struct _TABLE_ENTRY_T {
	P_REG_ENTRY_T pu4TablePtr;
	UINT_16 u2Size;
};

/*! INT status to event map */
typedef struct _INT_EVENT_MAP_T {
	UINT_32 u4Int;
	UINT_32 u4Event;
} INT_EVENT_MAP_T, *P_INT_EVENT_MAP_T;

typedef struct _ECO_INFO_T {
	UINT_8 ucHwVer;
	UINT_8 ucRomVer;
	UINT_8 ucFactoryVer;
} ECO_INFO_T, *P_ECO_INFO_T;

enum ENUM_INT_EVENT_T {
	INT_EVENT_ABNORMAL,
	INT_EVENT_SW_INT,
	INT_EVENT_TX,
	INT_EVENT_RX,
	INT_EVENT_NUM
};

typedef enum _ENUM_IE_UPD_METHOD_T {
	IE_UPD_METHOD_UPDATE_RANDOM,
	IE_UPD_METHOD_UPDATE_ALL,
	IE_UPD_METHOD_DELETE_ALL,
} ENUM_IE_UPD_METHOD_T, *P_ENUM_IE_UPD_METHOD_T;

enum POWER_SAVE_CALLER_T {
	PS_CALLER_COMMON = 0,
	PS_CALLER_CTIA_MODE,
	PS_CALLER_SW_WRITE,
	PS_CALLER_CTIA,
	PS_CALLER_P2P,
	PS_CALLER_CAMCFG,
	PS_CALLER_GPU,
	PS_CALLER_NO_TIM,
	PS_CALLER_MAX_NUM = 24
};

#define PS_SYNC_WITH_FW		BIT(31)

enum ENUM_WMT_CHIPINFO_TYPE_T {
	WMTCHIN_CHIPID = 0x0,
	WMTCHIN_HWVER = WMTCHIN_CHIPID + 1,
	WMTCHIN_MAPPINGHWVER = WMTCHIN_HWVER + 1,
	WMTCHIN_FWVER = WMTCHIN_MAPPINGHWVER + 1,
	WMTCHIN_IPVER = WMTCHIN_FWVER + 1,
	WMTCHIN_MAX,
};


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
extern UINT_32 mtk_wcn_wmt_ic_info_get(enum ENUM_WMT_CHIPINFO_TYPE_T type);


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

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/* Routines in nic.c                                                          */
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicAllocateAdapterMemory(IN P_ADAPTER_T prAdapter);

VOID nicReleaseAdapterMemory(IN P_ADAPTER_T prAdapter);

VOID nicDisableInterrupt(IN P_ADAPTER_T prAdapter);

VOID nicEnableInterrupt(IN P_ADAPTER_T prAdapter);

WLAN_STATUS nicProcessIST(IN P_ADAPTER_T prAdapter);

WLAN_STATUS nicProcessIST_impl(IN P_ADAPTER_T prAdapter, IN UINT_32 u4IntStatus);

WLAN_STATUS nicInitializeAdapter(IN P_ADAPTER_T prAdapter);

VOID nicMCRInit(IN P_ADAPTER_T prAdapter);

UINT_16 nicGetChipID(IN P_ADAPTER_T prAdapter);

BOOL nicVerifyChipID(IN P_ADAPTER_T prAdapter);

#if CFG_SDIO_INTR_ENHANCE
VOID nicSDIOInit(IN P_ADAPTER_T prAdapter);

VOID nicSDIOReadIntStatus(IN P_ADAPTER_T prAdapter, OUT PUINT_32 pu4IntStatus);
#endif

VOID nicpmCheckAndTriggerDriverOwn(IN P_ADAPTER_T prAdapter);

BOOLEAN nicpmSetDriverOwn(IN P_ADAPTER_T prAdapter);

VOID nicpmSetFWOwn(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnableGlobalInt);

BOOLEAN nicpmSetAcpiPowerD0(IN P_ADAPTER_T prAdapter);

BOOLEAN nicpmSetAcpiPowerD3(IN P_ADAPTER_T prAdapter);

#if defined(_HIF_SPI)
void nicRestoreSpiDefMode(IN P_ADAPTER_T prAdapter);
#endif

VOID nicProcessSoftwareInterrupt(IN P_ADAPTER_T prAdapter);

VOID nicProcessAbnormalInterrupt(IN P_ADAPTER_T prAdapter);

VOID nicPutMailbox(IN P_ADAPTER_T prAdapter, IN UINT_32 u4MailboxNum, IN UINT_32 u4Data);

VOID nicGetMailbox(IN P_ADAPTER_T prAdapter, IN UINT_32 u4MailboxNum, OUT PUINT_32 pu4Data);

VOID nicSetSwIntr(IN P_ADAPTER_T prAdapter, IN UINT_32 u4SwIntrBitmap);

P_CMD_INFO_T nicGetPendingCmdInfo(IN P_ADAPTER_T prAdapter, IN UINT_8 ucSeqNum);

P_MSDU_INFO_T nicGetPendingTxMsduInfo(IN P_ADAPTER_T prAdapter, IN UINT_8 ucWlanIndex, IN UINT_8 ucSeqNum);

P_MSDU_INFO_T nicGetPendingStaMMPDU(IN P_ADAPTER_T prAdapter, IN UINT_8 ucStaRecIdx);

VOID nicFreePendingTxMsduInfoByBssIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex);

UINT_8 nicIncreaseCmdSeqNum(IN P_ADAPTER_T prAdapter);

UINT_8 nicIncreaseTxSeqNum(IN P_ADAPTER_T prAdapter);

/* Media State Change */
WLAN_STATUS
nicMediaStateChange(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_EVENT_CONNECTION_STATUS prConnectionStatus);

WLAN_STATUS nicMediaJoinFailure(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN WLAN_STATUS rStatus);

/* Utility function for channel number conversion */
UINT_32 nicChannelNum2Freq(IN UINT_32 u4ChannelNum);

UINT_32 nicFreq2ChannelNum(IN UINT_32 u4FreqInKHz);

UINT_8 nicGetVhtS1(IN UINT_8 ucPrimaryChannel);

/* firmware command wrapper */
    /* NETWORK (WIFISYS) */
WLAN_STATUS nicActivateNetwork(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex);

WLAN_STATUS nicDeactivateNetwork(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex);

    /* BSS-INFO */
WLAN_STATUS nicUpdateBss(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex);

    /* BSS-INFO Indication (PM) */
WLAN_STATUS nicPmIndicateBssCreated(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex);

WLAN_STATUS nicPmIndicateBssConnected(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex);

WLAN_STATUS nicPmIndicateBssAbort(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex);

    /* Beacon Template Update */
WLAN_STATUS
nicUpdateBeaconIETemplate(IN P_ADAPTER_T prAdapter,
			  IN ENUM_IE_UPD_METHOD_T eIeUpdMethod,
			  IN UINT_8 ucBssIndex, IN UINT_16 u2Capability, IN PUINT_8 aucIe, IN UINT_16 u2IELen);

WLAN_STATUS nicQmUpdateWmmParms(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex);

WLAN_STATUS nicSetAutoTxPower(IN P_ADAPTER_T prAdapter, IN P_CMD_AUTO_POWER_PARAM_T prAutoPwrParam);

/*----------------------------------------------------------------------------*/
/* Calibration Control                                                        */
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicUpdateTxPower(IN P_ADAPTER_T prAdapter, IN P_CMD_TX_PWR_T prTxPwrParam);

WLAN_STATUS nicUpdate5GOffset(IN P_ADAPTER_T prAdapter, IN P_CMD_5G_PWR_OFFSET_T pr5GPwrOffset);

WLAN_STATUS nicUpdateDPD(IN P_ADAPTER_T prAdapter, IN P_CMD_PWR_PARAM_T prDpdCalResult);

/*----------------------------------------------------------------------------*/
/* PHY configuration                                                          */
/*----------------------------------------------------------------------------*/
VOID nicSetAvailablePhyTypeSet(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* MGMT and System Service Control                                            */
/*----------------------------------------------------------------------------*/
VOID nicInitSystemService(IN P_ADAPTER_T prAdapter);

VOID nicResetSystemService(IN P_ADAPTER_T prAdapter);

VOID nicUninitSystemService(IN P_ADAPTER_T prAdapter);

VOID nicInitMGMT(IN P_ADAPTER_T prAdapter, IN P_REG_INFO_T prRegInfo);

VOID nicUninitMGMT(IN P_ADAPTER_T prAdapter);

WLAN_STATUS
nicConfigPowerSaveProfile(IN P_ADAPTER_T prAdapter, UINT_8 ucBssIndex, PARAM_POWER_MODE ePwrMode,
			BOOLEAN fgEnCmdEvent);

VOID nicPowerSaveInfoMap(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN IN PARAM_POWER_MODE ePowerMode,
			IN enum POWER_SAVE_CALLER_T ucCaller);

WLAN_STATUS nicEnterCtiaMode(IN P_ADAPTER_T prAdapter, BOOLEAN fgEnterCtia, BOOLEAN fgEnCmdEvent);

WLAN_STATUS nicEnterCtiaModeOfScan(IN P_ADAPTER_T prAdapter, BOOLEAN fgEnterCtia, BOOLEAN fgEnCmdEvent);
WLAN_STATUS nicEnterCtiaModeOfRoaming(IN P_ADAPTER_T prAdapter, BOOLEAN fgEnterCtia, BOOLEAN fgEnCmdEvent);
WLAN_STATUS nicEnterCtiaModeOfCAM(IN P_ADAPTER_T prAdapter, BOOLEAN fgEnterCtia, BOOLEAN fgEnCmdEvent);
WLAN_STATUS nicEnterCtiaModeOfBCNTimeout(IN P_ADAPTER_T prAdapter, BOOLEAN fgEnterCtia, BOOLEAN fgEnCmdEvent);
WLAN_STATUS nicEnterCtiaModeOfAutoTxPower(IN P_ADAPTER_T prAdapter, BOOLEAN fgEnterCtia, BOOLEAN fgEnCmdEvent);
WLAN_STATUS nicEnterCtiaModeOfFIFOFullNoAck(IN P_ADAPTER_T prAdapter, BOOLEAN fgEnterCtia, BOOLEAN fgEnCmdEvent);

/*----------------------------------------------------------------------------*/
/* Scan Result Processing                                                     */
/*----------------------------------------------------------------------------*/
UINT_32
nicAddScanResult(IN P_ADAPTER_T prAdapter,
		 IN PARAM_MAC_ADDRESS rMacAddr,
		 IN P_PARAM_SSID_T prSsid,
		 IN UINT_16 u2CapInfo,
		 IN PARAM_RSSI rRssi,
		 IN ENUM_PARAM_NETWORK_TYPE_T eNetworkType,
		 IN P_PARAM_802_11_CONFIG_T prConfiguration,
		 IN ENUM_PARAM_OP_MODE_T eOpMode,
		 IN PARAM_RATES_EX rSupportedRates, IN UINT_16 u2IELength, IN PUINT_8 pucIEBuf);

VOID nicFreeScanResultIE(IN P_ADAPTER_T prAdapter, IN UINT_32 u4Idx);

/*----------------------------------------------------------------------------*/
/* Fixed Rate Hacking                                                         */
/*----------------------------------------------------------------------------*/
WLAN_STATUS
nicUpdateRateParams(IN P_ADAPTER_T prAdapter,
		    IN ENUM_REGISTRY_FIXED_RATE_T eRateSetting,
		    IN PUINT_8 pucDesiredPhyTypeSet,
		    IN PUINT_16 pu2DesiredNonHTRateSet,
		    IN PUINT_16 pu2BSSBasicRateSet,
		    IN PUINT_8 pucMcsSet, IN PUINT_8 pucSupMcs32, IN PUINT_16 u2HtCapInfo);

/*----------------------------------------------------------------------------*/
/* Write registers                                                            */
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicWriteMcr(IN P_ADAPTER_T prAdapter, IN UINT_32 u4Address, IN UINT_32 u4Value);

/*----------------------------------------------------------------------------*/
/* Update auto rate                                                           */
/*----------------------------------------------------------------------------*/
WLAN_STATUS
nicRlmArUpdateParms(IN P_ADAPTER_T prAdapter,
		    IN UINT_32 u4ArSysParam0,
		    IN UINT_32 u4ArSysParam1, IN UINT_32 u4ArSysParam2, IN UINT_32 u4ArSysParam3);

/*----------------------------------------------------------------------------*/
/* Enable/Disable Roaming                                                     */
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicRoamingUpdateParams(IN P_ADAPTER_T prAdapter, IN UINT_32 u4EnableRoaming);

VOID nicPrintFirmwareAssertInfo(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* Link Quality Updating                                                      */
/*----------------------------------------------------------------------------*/
VOID
nicUpdateLinkQuality(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_EVENT_LINK_QUALITY_V2 prEventLinkQuality);

VOID nicUpdateRSSI(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN INT_8 cRssi, IN INT_8 cLinkQuality);

VOID nicUpdateLinkSpeed(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN UINT_16 u2LinkSpeed);

#if CFG_SUPPORT_RDD_TEST_MODE
WLAN_STATUS nicUpdateRddTestMode(IN P_ADAPTER_T prAdapter, IN P_CMD_RDD_CH_T prRddChParam);
#endif

/*----------------------------------------------------------------------------*/
/* Address Setting Apply                                                      */
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicApplyNetworkAddress(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* ECO Version                                                                */
/*----------------------------------------------------------------------------*/
UINT_8 nicGetChipEcoVer(VOID);
BOOLEAN nicIsEcoVerEqualTo(UINT_8 ucEcoVer);
BOOLEAN nicIsEcoVerEqualOrLaterTo(UINT_8 ucEcoVer);

/*----------------------------------------------------------------------------*/
/* uApsd setting                                                           */
/*----------------------------------------------------------------------------*/

WLAN_STATUS nicSetUapsdParam(IN P_ADAPTER_T prAdapter,
			     IN P_PARAM_CUSTOM_UAPSD_PARAM_STRUCT_T prUapsdParams,
			     IN ENUM_NETWORK_TYPE_T eNetworkTypeIdx);

#endif /* _NIC_H */
