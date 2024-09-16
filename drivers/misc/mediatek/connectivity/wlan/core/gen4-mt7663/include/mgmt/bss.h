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

/*! \file   "bss.h"
 *    \brief  In this file we define the function prototype used in BSS/IBSS.
 *
 *    The file contains the function declarations and defines
 *						for used in BSS/IBSS.
 */


#ifndef _BSS_H
#define _BSS_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "wlan_def.h"
extern const uint8_t *apucNetworkType[NETWORK_TYPE_NUM];

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define BSS_DEFAULT_NUM         KAL_BSS_NUM

/* Define how many concurrent operation networks. */
#define BSS_P2P_NUM             KAL_P2P_NUM

#if (BSS_P2P_NUM > MAX_BSSID_NUM)
#error Exceed HW capability (KAL_BSS_NUM or KAL_P2P_NUM)!!
#endif

/* NOTE(Kevin): change define for george */
/* #define MAX_LEN_TIM_PARTIAL_BMP     (((MAX_ASSOC_ID + 1) + 7) / 8) */
/* Required bits = (MAX_ASSOC_ID + 1) */
#define MAX_LEN_TIM_PARTIAL_BMP                     ((CFG_STA_REC_NUM + 7) / 8)
/* reserve length greater than maximum size of STA_REC */
/* obsoleted: Assume we only use AID:1~15 */

/* CTRL FLAGS for Probe Response */
#define BSS_PROBE_RESP_USE_P2P_DEV_ADDR             BIT(0)
#define BSS_PROBE_RESP_INCLUDE_P2P_IE               BIT(1)

#define MAX_BSS_INDEX           HW_BSSID_NUM
#define P2P_DEV_BSS_INDEX       MAX_BSS_INDEX

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#define IS_BSS_INDEX_VALID(_ucBssIndex)     ((_ucBssIndex) <= P2P_DEV_BSS_INDEX)

#define GET_BSS_INFO_BY_INDEX(_prAdapter, _ucBssIndex) \
	((_prAdapter)->aprBssInfo[(_ucBssIndex)])

#define bssAssignAssocID(_prStaRec)         ((_prStaRec)->ucIndex + 1)

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
/*----------------------------------------------------------------------------*/
/* Routines for all Operation Modes                                           */
/*----------------------------------------------------------------------------*/
struct STA_RECORD *
bssCreateStaRecFromBssDesc(IN struct ADAPTER *prAdapter,
			   IN enum ENUM_STA_TYPE eStaType, IN uint8_t uBssIndex,
			   IN struct BSS_DESC *prBssDesc);

void bssComposeNullFrame(IN struct ADAPTER *prAdapter,
			 IN uint8_t *pucBuffer, IN struct STA_RECORD *prStaRec);

void
bssComposeQoSNullFrame(IN struct ADAPTER *prAdapter,
		       IN uint8_t *pucBuffer, IN struct STA_RECORD *prStaRec,
		       IN uint8_t ucUP, IN u_int8_t fgSetEOSP);

uint32_t
bssSendNullFrame(IN struct ADAPTER *prAdapter,
		 IN struct STA_RECORD *prStaRec,
		 IN PFN_TX_DONE_HANDLER pfTxDoneHandler);

uint32_t
bssSendQoSNullFrame(IN struct ADAPTER *prAdapter,
		    IN struct STA_RECORD *prStaRec, IN uint8_t ucUP,
		    IN PFN_TX_DONE_HANDLER pfTxDoneHandler);

void bssDumpBssInfo(IN struct ADAPTER *prAdapter,
		    IN uint8_t ucBssIndex);

void bssDetermineApBssInfoPhyTypeSet(IN struct ADAPTER
				     *prAdapter, IN u_int8_t fgIsPureAp,
				     OUT struct BSS_INFO *prBssInfo);

/*----------------------------------------------------------------------------*/
/* Routines for both IBSS(AdHoc) and BSS(AP)                                  */
/*----------------------------------------------------------------------------*/
void bssGenerateExtSuppRate_IE(IN struct ADAPTER *prAdapter,
			       IN struct MSDU_INFO *prMsduInfo);

void
bssBuildBeaconProbeRespFrameCommonIEs(IN struct MSDU_INFO
				      *prMsduInfo,
				      IN struct BSS_INFO *prBssInfo,
				      IN uint8_t *pucDestAddr);

void
bssComposeBeaconProbeRespFrameHeaderAndFF(
	IN uint8_t *pucBuffer,
	IN uint8_t *pucDestAddr,
	IN uint8_t *pucOwnMACAddress,
	IN uint8_t *pucBSSID, IN uint16_t u2BeaconInterval,
	IN uint16_t u2CapInfo);

uint32_t
bssSendBeaconProbeResponse(IN struct ADAPTER *prAdapter,
				IN uint8_t uBssIndex, IN uint8_t *pucDestAddr,
				IN uint32_t u4ControlFlags);

uint32_t bssProcessProbeRequest(IN struct ADAPTER *prAdapter,
				IN struct SW_RFB *prSwRfb);

void bssInitializeClientList(IN struct ADAPTER *prAdapter,
				IN struct BSS_INFO *prBssInfo);

void bssAddClient(IN struct ADAPTER *prAdapter,
				IN struct BSS_INFO *prBssInfo,
				IN struct STA_RECORD *prStaRec);

u_int8_t bssRemoveClient(IN struct ADAPTER *prAdapter,
				IN struct BSS_INFO *prBssInfo,
				IN struct STA_RECORD *prStaRec);

struct STA_RECORD *bssRemoveClientByMac(IN struct ADAPTER *prAdapter,
				IN struct BSS_INFO *prBssInfo,
				IN uint8_t *pucMac);

struct STA_RECORD *bssGetClientByMac(IN struct ADAPTER *prAdapter,
				IN struct BSS_INFO *prBssInfo,
				IN uint8_t *pucMac);

struct STA_RECORD *bssRemoveHeadClient(IN struct ADAPTER *prAdapter,
				IN struct BSS_INFO *prBssInfo);

uint32_t bssGetClientCount(IN struct ADAPTER *prAdapter,
				IN struct BSS_INFO *prBssInfo);

void bssDumpClientList(IN struct ADAPTER *prAdapter,
				IN struct BSS_INFO *prBssInfo);

void bssCheckClientList(IN struct ADAPTER *prAdapter,
				IN struct BSS_INFO *prBssInfo);

/*----------------------------------------------------------------------------*/
/* Routines for IBSS(AdHoc) only                                              */
/*----------------------------------------------------------------------------*/
void
ibssProcessMatchedBeacon(IN struct ADAPTER *prAdapter,
			 IN struct BSS_INFO *prBssInfo,
			 IN struct BSS_DESC *prBssDesc, IN uint8_t ucRCPI);

uint32_t ibssCheckCapabilityForAdHocMode(IN struct ADAPTER
		*prAdapter, IN struct BSS_DESC *prBssDesc);

void ibssInitForAdHoc(IN struct ADAPTER *prAdapter,
		      IN struct BSS_INFO *prBssInfo);

uint32_t bssUpdateBeaconContent(IN struct ADAPTER
				*prAdapter, IN uint8_t uBssIndex);

/*----------------------------------------------------------------------------*/
/* Routines for BSS(AP) only                                                  */
/*----------------------------------------------------------------------------*/
void bssInitForAP(IN struct ADAPTER *prAdapter,
		  IN struct BSS_INFO *prBssInfo, IN u_int8_t fgIsRateUpdate);

void bssUpdateDTIMCount(IN struct ADAPTER *prAdapter,
			IN uint8_t uBssIndex);

void bssSetTIMBitmap(IN struct ADAPTER *prAdapter,
		     IN struct BSS_INFO *prBssInfo, IN uint16_t u2AssocId);

/*link function to p2p module for txBcnIETable*/

/* WMM-2.2.2 WMM ACI to AC coding */
enum ENUM_ACI {
	ACI_BE = 0,
	ACI_BK = 1,
	ACI_VI = 2,
	ACI_VO = 3,
	ACI_NUM
};

enum ENUM_AC_PRIORITY {
	AC_BK_PRIORITY = 0,
	AC_BE_PRIORITY,
	AC_VI_PRIORITY,
	AC_VO_PRIORITY
};

#endif /* _BSS_H */
