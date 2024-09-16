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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/mgmt/auth.h#1
*/

/*! \file  auth.h
*    \brief This file contains the authentication REQ/RESP of
*	   IEEE 802.11 family for MediaTek 802.11 Wireless LAN Adapters.
*/


#ifndef _AUTH_H
#define _AUTH_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                         D A T A   T Y P E S
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
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/* Routines in auth.c                                                         */
/*----------------------------------------------------------------------------*/
VOID authAddIEChallengeText(IN P_ADAPTER_T prAdapter, IN OUT P_MSDU_INFO_T prMsduInfo);

#if !CFG_SUPPORT_AAA
WLAN_STATUS authSendAuthFrame(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN UINT_16 u2TransactionSeqNum);
#else
WLAN_STATUS
authSendAuthFrame(IN P_ADAPTER_T prAdapter,
		  IN P_STA_RECORD_T prStaRec,
		  IN UINT_8 uBssIndex,
		  IN P_SW_RFB_T prFalseAuthSwRfb, IN UINT_16 u2TransactionSeqNum, IN UINT_16 u2StatusCode);
#endif /* CFG_SUPPORT_AAA */

WLAN_STATUS authCheckTxAuthFrame(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN UINT_16 u2TransactionSeqNum);

WLAN_STATUS authCheckRxAuthFrameTransSeq(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

WLAN_STATUS
authCheckRxAuthFrameStatus(IN P_ADAPTER_T prAdapter,
			   IN P_SW_RFB_T prSwRfb, IN UINT_16 u2TransactionSeqNum, OUT PUINT_16 pu2StatusCode);

VOID authHandleIEChallengeText(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb, P_IE_HDR_T prIEHdr);

WLAN_STATUS authProcessRxAuth2_Auth4Frame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

WLAN_STATUS
authSendDeauthFrame(IN P_ADAPTER_T prAdapter,
		    IN P_BSS_INFO_T prBssInfo,
		    IN P_STA_RECORD_T prStaRec,
		    IN P_SW_RFB_T prClassErrSwRfb, IN UINT_16 u2ReasonCode, IN PFN_TX_DONE_HANDLER pfTxDoneHandler);

WLAN_STATUS authProcessRxDeauthFrame(IN P_SW_RFB_T prSwRfb, IN UINT_8 aucBSSID[], OUT PUINT_16 pu2ReasonCode);

WLAN_STATUS
authProcessRxAuth1Frame(IN P_ADAPTER_T prAdapter,
			IN P_SW_RFB_T prSwRfb,
			IN UINT_8 aucExpectedBSSID[],
			IN UINT_16 u2ExpectedAuthAlgNum,
			IN UINT_16 u2ExpectedTransSeqNum, OUT PUINT_16 pu2ReturnStatusCode);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _AUTH_H */
