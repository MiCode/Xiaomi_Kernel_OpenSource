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

/*! \file   hs20.h
 *  \brief This file contains the function declaration for hs20.c.
 */

#ifndef _HS20_H
#define _HS20_H

#if CFG_SUPPORT_PASSPOINT
/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */

/******************************************************************************
 *                              C O N S T A N T S
 ******************************************************************************
 */
#define BSSID_POOL_MAX_SIZE             8
#define HS20_SIGMA_SCAN_RESULT_TIMEOUT  30	/* sec */

/******************************************************************************
 *                             D A T A   T Y P E S
 ******************************************************************************
 */

#if CFG_ENABLE_GTK_FRAME_FILTER
/*For GTK Frame Filter*/
struct IPV4_NETWORK_ADDRESS_LIST {
	uint8_t ucAddrCount;
	struct IPV4_NETWORK_ADDRESS arNetAddr[1];
};
#endif

/* Entry of BSSID Pool - For SIGMA Test */
struct BSSID_ENTRY {
	uint8_t aucBSSID[MAC_ADDR_LEN];
};

struct HS20_INFO {
	/*Hotspot 2.0 Information */
	uint8_t aucHESSID[MAC_ADDR_LEN];
	uint8_t ucAccessNetworkOptions;
	uint8_t ucVenueGroup;	/* VenueInfo - Group */
	uint8_t ucVenueType;
	uint8_t ucHotspotConfig;

	/*Roaming Consortium Information */
	/* PARAM_HS20_ROAMING_CONSORTIUM_INFO rRCInfo; */

	/*Hotspot 2.0 dummy AP Info */

	/*Time Advertisement Information */
	/* UINT_32                 u4UTCOffsetTime; */
	/* UINT_8                  aucTimeZone[ELEM_MAX_LEN_TIME_ZONE]; */
	/* UINT_8                  ucLenTimeZone; */

	/* For SIGMA Test */
	/* BSSID Pool */
	struct BSSID_ENTRY arBssidPool[BSSID_POOL_MAX_SIZE];
	uint8_t ucNumBssidPoolEntry;
	u_int8_t fgIsHS2SigmaMode;
};

/******************************************************************************
 *                            P U B L I C   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                           P R I V A T E   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                                 M A C R O S
 ******************************************************************************
 */

/*For GTK Frame Filter*/
#if DBG
#define FREE_IPV4_NETWORK_ADDR_LIST(_prAddrList)    \
	{   \
		uint32_t u4Size =  \
				 OFFSET_OF(struct IPV4_NETWORK_ADDRESS_LIST,  \
				 arNetAddr) +  \
				 (((_prAddrList)->ucAddrCount) *  \
				 sizeof(struct IPV4_NETWORK_ADDRESS));  \
		kalMemFree((_prAddrList), VIR_MEM_TYPE, u4Size);    \
		(_prAddrList) = NULL;   \
	}
#else
#define FREE_IPV4_NETWORK_ADDR_LIST(_prAddrList)    \
	{   \
		kalMemFree((_prAddrList), VIR_MEM_TYPE, 0);    \
		(_prAddrList) = NULL;   \
	}
#endif

/******************************************************************************
 *                              F U N C T I O N S
 ******************************************************************************
 */

void hs20GenerateInterworkingIE(IN struct ADAPTER *prAdapter,
		OUT struct MSDU_INFO *prMsduInfo);

void hs20GenerateRoamingConsortiumIE(IN struct ADAPTER *prAdapter,
		OUT struct MSDU_INFO *prMsduInfo);

void hs20GenerateHS20IE(IN struct ADAPTER *prAdapter,
		OUT struct MSDU_INFO *prMsduInfo);

void hs20FillExtCapIE(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo, struct MSDU_INFO *prMsduInfo);

void hs20FillProreqExtCapIE(IN struct ADAPTER *prAdapter, OUT uint8_t *pucIE);

void hs20FillHS20IE(IN struct ADAPTER *prAdapter, OUT uint8_t *pucIE);

uint32_t hs20CalculateHS20RelatedIEForProbeReq(IN struct ADAPTER *prAdapter,
		IN uint8_t *pucTargetBSSID);

uint32_t hs20GenerateHS20RelatedIEForProbeReq(IN struct ADAPTER *prAdapter,
		IN uint8_t *pucTargetBSSID, OUT uint8_t *prIE);

u_int8_t hs20IsGratuitousArp(IN struct ADAPTER *prAdapter,
		IN struct SW_RFB *prCurrSwRfb);

u_int8_t hs20IsUnsolicitedNeighborAdv(IN struct ADAPTER *prAdapter,
		IN struct SW_RFB *prCurrSwRfb);

#if CFG_ENABLE_GTK_FRAME_FILTER
u_int8_t hs20IsForgedGTKFrame(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prBssInfo, IN struct SW_RFB *prCurrSwRfb);
#endif

u_int8_t hs20IsUnsecuredFrame(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prBssInfo, IN struct SW_RFB *prCurrSwRfb);

u_int8_t hs20IsFrameFilterEnabled(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prBssInfo);

uint32_t hs20SetBssidPool(IN struct ADAPTER *prAdapter,
		IN void *pvBuffer,
		IN enum ENUM_KAL_NETWORK_TYPE_INDEX eNetTypeIdx);

#endif /* CFG_SUPPORT_PASSPOINT */
#endif
