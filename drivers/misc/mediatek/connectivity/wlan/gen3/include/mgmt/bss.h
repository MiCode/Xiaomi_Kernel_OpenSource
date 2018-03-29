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
** Id: @(#) bss.h
*/

/*! \file   "bss.h"
    \brief  In this file we define the function prototype used in BSS/IBSS.

    The file contains the function declarations and defines for used in BSS/IBSS.
*/

#ifndef _BSS_H
#define _BSS_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "wlan_def.h"
extern const PUINT_8 apucNetworkType[NETWORK_TYPE_NUM];

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* Fixed value=4 for MT6630.
 * It is the biggest index of this pointer array prAdapter->aprBssInfo[].
 */
#define MAX_BSS_INDEX           HW_BSSID_NUM
#define P2P_DEV_BSS_INDEX       MAX_BSS_INDEX

/* Define how many concurrent operation networks. */
#define BSS_INFO_NUM            KAL_BSS_NUM
#define BSS_P2P_NUM             KAL_P2P_NUM

#if (KAL_BSS_NUM > HW_BSSID_NUM) || (KAL_P2P_NUM > KAL_BSS_NUM)
#error Exceed HW capability (KAL_BSS_NUM or KAL_P2P_NUM)!!
#endif

/* NOTE(Kevin): change define for george */
/* #define MAX_LEN_TIM_PARTIAL_BMP     (((MAX_ASSOC_ID + 1) + 7) / 8) */ /* Required bits = (MAX_ASSOC_ID + 1) */
#define MAX_LEN_TIM_PARTIAL_BMP                     ((CFG_STA_REC_NUM + 7) / 8)
/* reserve length greater than maximum size of STA_REC */	/* obsoleted: Assume we only use AID:1~15 */

/* CTRL FLAGS for Probe Response */
#define BSS_PROBE_RESP_USE_P2P_DEV_ADDR             BIT(0)
#define BSS_PROBE_RESP_INCLUDE_P2P_IE               BIT(1)

/*******************************************************************************
*                             D A T A   T Y P E S
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
#define IS_BSS_INDEX_VALID(_ucBssIndex)     ((_ucBssIndex) <= P2P_DEV_BSS_INDEX)

#define GET_BSS_INFO_BY_INDEX(_prAdapter, _ucBssIndex) \
	((_prAdapter)->aprBssInfo[(_ucBssIndex)])

#define bssAssignAssocID(_prStaRec)         ((_prStaRec)->ucIndex + 1)

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/* Routines for all Operation Modes                                           */
/*----------------------------------------------------------------------------*/
P_STA_RECORD_T
bssCreateStaRecFromBssDesc(IN P_ADAPTER_T prAdapter,
			   IN ENUM_STA_TYPE_T eStaType, IN UINT_8 uBssIndex, IN P_BSS_DESC_T prBssDesc);

VOID bssComposeNullFrame(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuffer, IN P_STA_RECORD_T prStaRec);

VOID
bssComposeQoSNullFrame(IN P_ADAPTER_T prAdapter,
		       IN PUINT_8 pucBuffer, IN P_STA_RECORD_T prStaRec, IN UINT_8 ucUP, IN BOOLEAN fgSetEOSP);

WLAN_STATUS
bssSendNullFrame(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN PFN_TX_DONE_HANDLER pfTxDoneHandler);

WLAN_STATUS
bssSendQoSNullFrame(IN P_ADAPTER_T prAdapter,
		    IN P_STA_RECORD_T prStaRec, IN UINT_8 ucUP, IN PFN_TX_DONE_HANDLER pfTxDoneHandler);

VOID bssDumpBssInfo(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex);

VOID bssDetermineApBssInfoPhyTypeSet(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsPureAp, OUT P_BSS_INFO_T prBssInfo);

/*----------------------------------------------------------------------------*/
/* Routines for both IBSS(AdHoc) and BSS(AP)                                  */
/*----------------------------------------------------------------------------*/
VOID bssGenerateExtSuppRate_IE(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

VOID
bssBuildBeaconProbeRespFrameCommonIEs(IN P_MSDU_INFO_T prMsduInfo, IN P_BSS_INFO_T prBssInfo, IN PUINT_8 pucDestAddr);

VOID
bssComposeBeaconProbeRespFrameHeaderAndFF(IN PUINT_8 pucBuffer,
					  IN PUINT_8 pucDestAddr,
					  IN PUINT_8 pucOwnMACAddress,
					  IN PUINT_8 pucBSSID, IN UINT_16 u2BeaconInterval, IN UINT_16 u2CapInfo);

WLAN_STATUS
bssSendBeaconProbeResponse(IN P_ADAPTER_T prAdapter,
			   IN UINT_8 uBssIndex, IN PUINT_8 pucDestAddr, IN UINT_32 u4ControlFlags);

WLAN_STATUS bssProcessProbeRequest(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

VOID bssInitializeClientList(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo);

VOID bssAddClient(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo, IN P_STA_RECORD_T prStaRec);

BOOLEAN bssRemoveClient(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo, IN P_STA_RECORD_T prStaRec);

P_STA_RECORD_T bssRemoveClientByMac(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo, IN PUINT_8 pucMac);

P_STA_RECORD_T bssRemoveHeadClient(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo);

UINT_32 bssGetClientCount(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo);

VOID bssDumpClientList(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo);

VOID bssCheckClientList(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo);

/*----------------------------------------------------------------------------*/
/* Routines for IBSS(AdHoc) only                                              */
/*----------------------------------------------------------------------------*/
VOID
ibssProcessMatchedBeacon(IN P_ADAPTER_T prAdapter,
			 IN P_BSS_INFO_T prBssInfo, IN P_BSS_DESC_T prBssDesc, IN UINT_8 ucRCPI);

WLAN_STATUS ibssCheckCapabilityForAdHocMode(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc);

VOID ibssInitForAdHoc(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo);

WLAN_STATUS bssUpdateBeaconContent(IN P_ADAPTER_T prAdapter, IN UINT_8 uBssIndex);

/*----------------------------------------------------------------------------*/
/* Routines for BSS(AP) only                                                  */
/*----------------------------------------------------------------------------*/
VOID bssInitForAP(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo, IN BOOLEAN fgIsRateUpdate);

VOID bssUpdateDTIMCount(IN P_ADAPTER_T prAdapter, IN UINT_8 uBssIndex);

VOID bssSetTIMBitmap(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo, IN UINT_16 u2AssocId);

/*link function to p2p module for txBcnIETable*/

/* WMM-2.2.2 WMM ACI to AC coding */
typedef enum _ENUM_ACI_T {
	ACI_BE = 0,
	ACI_BK = 1,
	ACI_VI = 2,
	ACI_VO = 3,
	ACI_NUM
} ENUM_ACI_T, *P_ENUM_ACI_T;

typedef enum _ENUM_AC_PRIORITY_T {
	AC_BK_PRIORITY = 0,
	AC_BE_PRIORITY,
	AC_VI_PRIORITY,
	AC_VO_PRIORITY
} ENUM_AC_PRIORITY_T, *P_ENUM_AC_PRIORITY_T;

#endif /* _BSS_H */
