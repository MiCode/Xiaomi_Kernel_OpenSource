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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/nic/nic.h#1
*/

/*! \file   "nic.h"
*    \brief  The declaration of nic functions
*
*    Detail description.
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
	UINT_8 ucEcoVer;
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

typedef enum _ENUM_SER_STATE_T {
	SER_IDLE_DONE,       /* SER is idle or done */
	SER_STOP_HOST_TX,    /* Host HIF Tx is stopped */
	SER_STOP_HOST_TX_RX, /* Host HIF Tx/Rx is stopped */
	SER_REINIT_HIF,      /* Host HIF is reinit */

	SER_STATE_NUM
} ENUM_SER_STATE_T, *P_ENUM_SER_STATE_T;

/* Test mode bitmask of disable flag */
#define TEST_MODE_DISABLE_ONLINE_SCAN  BIT(0)
#define TEST_MODE_DISABLE_ROAMING      BIT(1)
#define TEST_MODE_FIXED_CAM_MODE       BIT(2)
#define TEST_MODE_DISABLE_BCN_LOST_DET BIT(3)
#define TEST_MODE_NONE                0
#define TEST_MODE_THROUGHPUT \
		(TEST_MODE_DISABLE_ONLINE_SCAN | TEST_MODE_DISABLE_ROAMING | \
		TEST_MODE_FIXED_CAM_MODE | TEST_MODE_DISABLE_BCN_LOST_DET)
#define TEST_MODE_SIGMA_AC_N_PMF (TEST_MODE_DISABLE_ONLINE_SCAN | TEST_MODE_FIXED_CAM_MODE)
#define TEST_MODE_SIGMA_WMM_PS (TEST_MODE_DISABLE_ONLINE_SCAN)
/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

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

BOOL nicVerifyChipID(IN P_ADAPTER_T prAdapter);

VOID nicpmWakeUpWiFi(IN P_ADAPTER_T prAdapter);

BOOLEAN nicpmSetDriverOwn(IN P_ADAPTER_T prAdapter);

VOID nicpmSetFWOwn(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnableGlobalInt);

BOOLEAN nicpmSetAcpiPowerD0(IN P_ADAPTER_T prAdapter);

BOOLEAN nicpmSetAcpiPowerD3(IN P_ADAPTER_T prAdapter);

#if defined(_HIF_SPI)
void nicRestoreSpiDefMode(IN P_ADAPTER_T prAdapter);
#endif

VOID nicProcessSoftwareInterrupt(IN P_ADAPTER_T prAdapter);

VOID nicProcessAbnormalInterrupt(IN P_ADAPTER_T prAdapter);

VOID nicSetSwIntr(IN P_ADAPTER_T prAdapter, IN UINT_32 u4SwIntrBitmap);

P_CMD_INFO_T nicGetPendingCmdInfo(IN P_ADAPTER_T prAdapter, IN UINT_8 ucSeqNum);

P_MSDU_INFO_T nicGetPendingTxMsduInfo(IN P_ADAPTER_T prAdapter, IN UINT_8 ucWlanIndex, IN UINT_8 ucSeqNum);

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

UINT_8 nicGetVhtS1(IN UINT_8 ucPrimaryChannel, IN UINT_8 ucBandwidth);

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
nicConfigPowerSaveProfile(IN P_ADAPTER_T prAdapter, UINT_8 ucBssIndex, PARAM_POWER_MODE ePwrMode, BOOLEAN fgEnCmdEvent);

WLAN_STATUS
nicConfigPowerSaveWowProfile(IN P_ADAPTER_T prAdapter, UINT_8 ucBssIndex, PARAM_POWER_MODE ePwrMode,
	BOOLEAN fgEnCmdEvent, BOOLEAN fgSuspend);

WLAN_STATUS nicEnterCtiaMode(IN P_ADAPTER_T prAdapter, BOOLEAN fgEnterCtia, BOOLEAN fgEnCmdEvent);
WLAN_STATUS nicEnterTPTestMode(IN P_ADAPTER_T prAdapter, IN UINT_8 ucFuncMask);

/*----------------------------------------------------------------------------*/
/* Scan Result Processing                                                     */
/*----------------------------------------------------------------------------*/
VOID
nicAddScanResult(IN P_ADAPTER_T prAdapter,
		 IN PARAM_MAC_ADDRESS rMacAddr,
		 IN P_PARAM_SSID_T prSsid,
		 IN UINT_32 u4Privacy,
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
uint8_t nicGetChipSwVer(void);
UINT_8 nicGetChipEcoVer(IN P_ADAPTER_T prAdapter);
BOOLEAN nicIsEcoVerEqualTo(IN P_ADAPTER_T prAdapter, UINT_8 ucEcoVer);
BOOLEAN nicIsEcoVerEqualOrLaterTo(IN P_ADAPTER_T prAdapter, UINT_8 ucEcoVer);
UINT_8 nicSetChipHwVer(UINT_8 value);
UINT_8 nicSetChipSwVer(UINT_8 value);
UINT_8 nicSetChipFactoryVer(UINT_8 value);

VOID nicSerStopTxRx(IN P_ADAPTER_T prAdapter);
VOID nicSerStopTx(IN P_ADAPTER_T prAdapter);
VOID nicSerStartTxRx(IN P_ADAPTER_T prAdapter);
BOOLEAN nicSerIsWaitingReset(IN P_ADAPTER_T prAdapter);
BOOLEAN nicSerIsTxStop(IN P_ADAPTER_T prAdapter);
BOOLEAN nicSerIsRxStop(IN P_ADAPTER_T prAdapter);

#endif /* _NIC_H */
