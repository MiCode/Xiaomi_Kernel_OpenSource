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
 ** Id: //Department/DaVinci/BRANCHES/HS2_DEV_SW/
 * MT6620_WIFI_DRIVER_V2_1_HS_2_0/mgmt/hs20.c#2
 */

/*! \file   "hs20.c"
 *    \brief  This file including the hotspot 2.0 related function.
 *
 *    This file provided the macros and functions library support for the
 *    protocol layer hotspot 2.0 related function.
 *
 */


/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */
#include "precomp.h"

#if CFG_SUPPORT_PASSPOINT

/******************************************************************************
 *                              C O N S T A N T S
 ******************************************************************************
 */

/******************************************************************************
 *                             D A T A   T Y P E S
 ******************************************************************************
 */

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

/******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 ******************************************************************************
 */

/******************************************************************************
 *                              F U N C T I O N S
 ******************************************************************************
 */

/*---------------------------------------------------------------------------*/
/*!
 * \brief    This function is called to generate Interworking IE
 *             for Probe Rsp, Bcn, Assoc Req/Rsp.
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T
 * \param[out] prMsduInfo  Pointer of the Msdu Info
 *
 * \return VOID
 */
/*---------------------------------------------------------------------------*/
void hs20GenerateInterworkingIE(IN struct ADAPTER *prAdapter,
		OUT struct MSDU_INFO *prMsduInfo)
{
}

/*---------------------------------------------------------------------------*/
/*!
 * \brief    This function is called to generate Roaming Consortium IE
 *             for Probe Rsp, Bcn, Assoc Req/Rsp.
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T
 * \param[out] prMsduInfo  Pointer of the Msdu Info
 *
 * \return VOID
 */
/*---------------------------------------------------------------------------*/
void hs20GenerateRoamingConsortiumIE(IN struct ADAPTER *prAdapter,
		OUT struct MSDU_INFO *prMsduInfo)
{
}

/*---------------------------------------------------------------------------*/
/*!
 * \brief    This function is called to generate HS2.0 IE
 *             for Probe Rsp, Bcn, Assoc Req/Rsp.
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T
 * \param[out] prMsduInfo  Pointer of the Msdu Info
 *
 * \return VOID
 */
/*---------------------------------------------------------------------------*/
void hs20GenerateHS20IE(IN struct ADAPTER *prAdapter,
		OUT struct MSDU_INFO *prMsduInfo)
{
	uint8_t *pucBuffer;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	if (prMsduInfo->ucBssIndex != KAL_NETWORK_TYPE_AIS_INDEX) {
		pr_info("[%s] prMsduInfo->ucBssIndex(%d) is not KAL_NETWORK_TYPE_AIS_INDEX\n",
			__func__, prMsduInfo->ucBssIndex);
		return;
	}

	pucBuffer = (uint8_t *)
		((unsigned long) prMsduInfo->prPacket +
		(unsigned long) prMsduInfo->u2FrameLength);

	/* ASSOC INFO IE ID: 221 :0xDD */
	if (prAdapter->prGlueInfo->u2HS20AssocInfoIELen) {
		kalMemCopy(pucBuffer,
			&prAdapter->prGlueInfo->aucHS20AssocInfoIE,
			prAdapter->prGlueInfo->u2HS20AssocInfoIELen);
		prMsduInfo->u2FrameLength +=
			prAdapter->prGlueInfo->u2HS20AssocInfoIELen;
	}

}

void hs20FillExtCapIE(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo, struct MSDU_INFO *prMsduInfo)
{
	struct IE_EXT_CAP *prExtCap;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	/* Add Extended Capabilities IE */
	prExtCap = (struct IE_EXT_CAP *)
	    (((uint8_t *) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);

	prExtCap->ucId = ELEM_ID_EXTENDED_CAP;
	if (prAdapter->prGlueInfo->fgConnectHS20AP == TRUE)
		prExtCap->ucLength = ELEM_MAX_LEN_EXT_CAP;
	else
		prExtCap->ucLength = 3 - ELEM_HDR_LEN;

	kalMemZero(prExtCap->aucCapabilities, ELEM_MAX_LEN_EXT_CAP);

	prExtCap->aucCapabilities[0] = ELEM_EXT_CAP_DEFAULT_VAL;

	if (prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		prExtCap->aucCapabilities[0] &= ~ELEM_EXT_CAP_PSMP_CAP;

	if (prAdapter->prGlueInfo->fgConnectHS20AP == TRUE) {
		SET_EXT_CAP(prExtCap->aucCapabilities,
			ELEM_MAX_LEN_EXT_CAP, ELEM_EXT_CAP_BSS_TRANSITION_BIT);
		SET_EXT_CAP(prExtCap->aucCapabilities,
			ELEM_MAX_LEN_EXT_CAP, ELEM_EXT_CAP_UTC_TSF_OFFSET_BIT);
		SET_EXT_CAP(prExtCap->aucCapabilities,
			ELEM_MAX_LEN_EXT_CAP, ELEM_EXT_CAP_INTERWORKING_BIT);
		SET_EXT_CAP(prExtCap->aucCapabilities,
			ELEM_MAX_LEN_EXT_CAP, ELEM_EXT_CAP_QOSMAPSET_BIT);

		/* For R2 WNM-Notification */
		SET_EXT_CAP(prExtCap->aucCapabilities,
			ELEM_MAX_LEN_EXT_CAP,
			ELEM_EXT_CAP_WNM_NOTIFICATION_BIT);
	}

	pr_info("IE_SIZE(prExtCap) = %d, %d %d\n",
		IE_SIZE(prExtCap), ELEM_HDR_LEN, ELEM_MAX_LEN_EXT_CAP);

	ASSERT(IE_SIZE(prExtCap) <= (ELEM_HDR_LEN + ELEM_MAX_LEN_EXT_CAP));

	prMsduInfo->u2FrameLength += IE_SIZE(prExtCap);
}

/*---------------------------------------------------------------------------*/
/*!
 * \brief    This function is called to fill up
 *            the content of Ext Cap IE bit 31.
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T
 * \param[out] pucIE  Pointer of the IE buffer
 *
 * \return VOID
 */
/*---------------------------------------------------------------------------*/
void hs20FillProreqExtCapIE(IN struct ADAPTER *prAdapter, OUT uint8_t *pucIE)
{
	struct IE_EXT_CAP *prExtCap;

	ASSERT(prAdapter);

	/* Add Extended Capabilities IE */
	prExtCap = (struct IE_EXT_CAP *) pucIE;

	prExtCap->ucId = ELEM_ID_EXTENDED_CAP;
	if (prAdapter->prGlueInfo->fgConnectHS20AP == TRUE)
		prExtCap->ucLength = ELEM_MAX_LEN_EXT_CAP;
	else
		prExtCap->ucLength = 3 - ELEM_HDR_LEN;

	kalMemZero(prExtCap->aucCapabilities, prExtCap->ucLength);

	prExtCap->aucCapabilities[0] = ELEM_EXT_CAP_DEFAULT_VAL;

	if (prAdapter->prGlueInfo->fgConnectHS20AP == TRUE) {
		SET_EXT_CAP(prExtCap->aucCapabilities,
			ELEM_MAX_LEN_EXT_CAP, ELEM_EXT_CAP_BSS_TRANSITION_BIT);
		SET_EXT_CAP(prExtCap->aucCapabilities,
			ELEM_MAX_LEN_EXT_CAP, ELEM_EXT_CAP_UTC_TSF_OFFSET_BIT);
		SET_EXT_CAP(prExtCap->aucCapabilities,
			ELEM_MAX_LEN_EXT_CAP, ELEM_EXT_CAP_INTERWORKING_BIT);
		SET_EXT_CAP(prExtCap->aucCapabilities,
			ELEM_MAX_LEN_EXT_CAP, ELEM_EXT_CAP_QOSMAPSET_BIT);

		/* For R2 WNM-Notification */
		SET_EXT_CAP(prExtCap->aucCapabilities,
			ELEM_MAX_LEN_EXT_CAP,
			ELEM_EXT_CAP_WNM_NOTIFICATION_BIT);
	}
}

/*---------------------------------------------------------------------------*/
/*!
 * \brief    This function is called to fill up the content of HS2.0 IE.
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T
 * \param[out] pucIE  Pointer of the IE buffer
 *
 * \return VOID
 */
/*---------------------------------------------------------------------------*/
void hs20FillHS20IE(IN struct ADAPTER *prAdapter, OUT uint8_t *pucIE)
{
	struct IE_HS20_INDICATION *prHS20IndicationIe;
	/* P_HS20_INFO_T prHS20Info; */
	uint8_t aucWfaOui[] = VENDOR_OUI_WFA_SPECIFIC;

	/* prHS20Info = &(prAdapter->rWifiVar.rHS20Info); */

	prHS20IndicationIe = (struct IE_HS20_INDICATION *) pucIE;

	prHS20IndicationIe->ucId = ELEM_ID_VENDOR;
	prHS20IndicationIe->ucLength =
		sizeof(struct IE_HS20_INDICATION) - ELEM_HDR_LEN;
	prHS20IndicationIe->aucOui[0] = aucWfaOui[0];
	prHS20IndicationIe->aucOui[1] = aucWfaOui[1];
	prHS20IndicationIe->aucOui[2] = aucWfaOui[2];
	prHS20IndicationIe->ucType = VENDOR_OUI_TYPE_HS20;

	/* For PASSPOINT_R1 */
	/* prHS20IndicationIe->ucHotspotConfig = 0x00; */

	/* For PASSPOINT_R2 */
	prHS20IndicationIe->ucHotspotConfig = 0x10;

}

/*---------------------------------------------------------------------------*/
/*!
 * \brief    This function is called while calculating length of
 *             hotspot 2.0 indication IE for Probe Request.
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T
 * \param[in] pucTargetBSSID  Pointer of target HESSID
 *
 * \return the length of composed HS20 IE
 */
/*---------------------------------------------------------------------------*/
uint32_t hs20CalculateHS20RelatedIEForProbeReq(IN struct ADAPTER *prAdapter,
		IN uint8_t *pucTargetBSSID)
{
	uint32_t u4IeLength;

	if (0)			/* Todo:: Not HS20 STA */
		return 0;

	u4IeLength = sizeof(struct IE_HS20_INDICATION)
		+ /* sizeof(IE_INTERWORKING_T) */ +
		(ELEM_HDR_LEN + ELEM_MAX_LEN_EXT_CAP);

	if (!pucTargetBSSID) {
		/* Todo:: Nothing */
		/* u4IeLength -= MAC_ADDR_LEN; */
	}

	return u4IeLength;
}

/*---------------------------------------------------------------------------*/
/*!
 * \brief    This function is called while composing
 *             hotspot 2.0 indication IE for Probe Request.
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T
 * \param[in] pucTargetBSSID  Pointer of target HESSID
 * \param[out] prIE  Pointer of the IE buffer
 *
 * \return the wlan status
 */
/*---------------------------------------------------------------------------*/
uint32_t hs20GenerateHS20RelatedIEForProbeReq(IN struct ADAPTER *prAdapter,
		IN uint8_t *pucTargetBSSID, OUT uint8_t *prIE)
{
	if (0)			/* Todo:: Not HS20 STA */
		return 0;
#if 0
	struct HS20_INFO *prHS20Info;

	prHS20Info = &(prAdapter->rWifiVar.rHS20Info);

	/*
	 * Generate 802.11u Interworking IE (107)
	 */
	hs20FillInterworkingIE(prAdapter,
		prHS20Info->ucAccessNetworkOptions,
		prHS20Info->ucVenueGroup,
		prHS20Info->ucVenueType,
		pucTargetBSSID, prIE);
	prIE += IE_SIZE(prIE);
#endif
	/*
	 * Generate Ext Cap IE (127)
	 */
	hs20FillProreqExtCapIE(prAdapter, prIE);
	prIE += IE_SIZE(prIE);

	/*
	 * Generate HS2.0 Indication IE (221)
	 */
	hs20FillHS20IE(prAdapter, prIE);
	prIE += IE_SIZE(prIE);

	return WLAN_STATUS_SUCCESS;
}

u_int8_t hs20IsGratuitousArp(IN struct ADAPTER *prAdapter,
		IN struct SW_RFB *prCurrSwRfb)
{
	uint8_t *pucSenderIP =
		prCurrSwRfb->pvHeader + ETHER_HEADER_LEN + ARP_SENDER_IP_OFFSET;
	uint8_t *pucTargetIP =
		prCurrSwRfb->pvHeader + ETHER_HEADER_LEN + ARP_TARGET_IP_OFFSET;
	uint8_t *pucSenderMac = ((uint8_t *)
		prCurrSwRfb->pvHeader +
		ETHER_HEADER_LEN + ARP_SNEDER_MAC_OFFSET);

#if CFG_HS20_DEBUG && 0
/* UINT_8  aucIpAllZero[4] = {0,0,0,0}; */
/* UINT_8  aucMACAllZero[MAC_ADDR_LEN] = {0,0,0,0,0,0}; */
	uint8_t *pucTargetMac = ((uint8_t *)
		prCurrSwRfb->pvHeader +
		ETHER_HEADER_LEN + ARP_TARGET_MAC_OFFSET);
#endif

#if CFG_HS20_DEBUG && 0
	uint16_t *pu2ArpOper = (uint16_t *) ((uint8_t *)
		prCurrSwRfb->pvHeader +
		ETHER_HEADER_LEN + ARP_OPERATION_OFFSET);

	kalPrint("Recv ARP 0x%04X\n", htons(*pu2ArpOper));
	kalPrint("SENDER[" MACSTR "] [%d:%d:%d:%d]\n",
		MAC2STR(pucSenderMac), *pucSenderIP,
		*(pucSenderIP + 1), *(pucSenderIP + 2), *(pucSenderIP + 3));
	kalPrint("TARGET[" MACSTR "] [%d:%d:%d:%d]\n",
		MAC2STR(pucTargetMac), *pucTargetIP,
		*(pucTargetIP + 1), *(pucTargetIP + 2), *(pucTargetIP + 3));
#endif

	/* IsGratuitousArp */
	if (!kalMemCmp(pucSenderIP, pucTargetIP, 4)) {
		kalPrint(
			"Drop Gratuitous ARP from [" MACSTR "] [%d:%d:%d:%d]\n",
			MAC2STR(pucSenderMac), *pucTargetIP, *(pucTargetIP + 1),
			*(pucTargetIP + 2), *(pucTargetIP + 3));
		return TRUE;
	}
	return FALSE;
}

u_int8_t hs20IsUnsolicitedNeighborAdv(IN struct ADAPTER *prAdapter,
		IN struct SW_RFB *prCurrSwRfb)
{
	uint8_t *pucIpv6Protocol = ((uint8_t *)
		prCurrSwRfb->pvHeader +
		ETHER_HEADER_LEN + IPV6_HDR_IP_PROTOCOL_OFFSET);

	/* kalPrint("pucIpv6Protocol [%02X:%02X]\n",
	 * *pucIpv6Protocol, IPV6_PROTOCOL_ICMPV6);
	 */
	if (*pucIpv6Protocol == IPV6_PROTOCOL_ICMPV6) {
		uint8_t *pucICMPv6Type =
		    ((uint8_t *) prCurrSwRfb->pvHeader +
		    ETHER_HEADER_LEN + IPV6_HDR_LEN + ICMPV6_TYPE_OFFSET);
		/* kalPrint("pucICMPv6Type [%02X:%02X]\n",
		 * *pucICMPv6Type, ICMPV6_TYPE_NEIGHBOR_ADVERTISEMENT);
		 */
		if (*pucICMPv6Type == ICMPV6_TYPE_NEIGHBOR_ADVERTISEMENT) {
			uint8_t *pucICMPv6Flag =
			    ((uint8_t *) prCurrSwRfb->pvHeader +
			    ETHER_HEADER_LEN +
			    IPV6_HDR_LEN + ICMPV6_FLAG_OFFSET);
			uint8_t *pucSrcMAC = ((uint8_t *)
				prCurrSwRfb->pvHeader + MAC_ADDR_LEN);

#if CFG_HS20_DEBUG
			kalPrint("NAdv Flag [%02X] [R(%d)\\S(%d)\\O(%d)]\n",
				*pucICMPv6Flag,
				(uint8_t) (*pucICMPv6Flag
					& ICMPV6_FLAG_ROUTER_BIT) >> 7,
				(uint8_t) (*pucICMPv6Flag
					& ICMPV6_FLAG_SOLICITED_BIT) >> 6,
				(uint8_t) (*pucICMPv6Flag
					& ICMPV6_FLAG_OVERWRITE_BIT) >> 5);
#endif
			if (!(*pucICMPv6Flag & ICMPV6_FLAG_SOLICITED_BIT)) {
				kalPrint(
					"Drop Unsolicited Neighbor Advertisement from ["
					MACSTR "]\n", MAC2STR(pucSrcMAC));
				return TRUE;
			}
		}
	}

	return FALSE;
}

#if CFG_ENABLE_GTK_FRAME_FILTER
u_int8_t hs20IsForgedGTKFrame(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prBssInfo, IN struct SW_RFB *prCurrSwRfb)
{
	struct CONNECTION_SETTINGS *prConnSettings =
		&prAdapter->rWifiVar.rConnSettings;
	uint8_t *pucEthDestAddr = prCurrSwRfb->pvHeader;

	/* 3 TODO: Need to verify this function before enable it */
	return FALSE;

	if ((prConnSettings->eEncStatus != ENUM_ENCRYPTION_DISABLED)
	    && IS_BMCAST_MAC_ADDR(pucEthDestAddr)) {
		uint8_t ucIdx = 0;
		uint32_t *prIpAddr, *prPacketDA;
		uint16_t *pu2PktIpVer =
		    (uint16_t *) ((uint8_t *)
		    prCurrSwRfb->pvHeader +
		    (ETHER_HEADER_LEN - ETHER_TYPE_LEN));

		if (*pu2PktIpVer == htons(ETH_P_IPV4)) {
			if (!prBssInfo->prIpV4NetAddrList)
				return FALSE;
			for (ucIdx = 0;
				ucIdx < prBssInfo
					->prIpV4NetAddrList->ucAddrCount;
				ucIdx++) {
				prIpAddr = (uint32_t *)
					&prBssInfo->prIpV4NetAddrList
					->arNetAddr[ucIdx].aucIpAddr[0];
				prPacketDA =
				    (uint32_t *) ((uint8_t *)
				    prCurrSwRfb->pvHeader +
				    ETHER_HEADER_LEN +
					IPV4_HDR_IP_DST_ADDR_OFFSET);

				if (kalMemCmp(prIpAddr, prPacketDA, 4) == 0) {
					kalPrint("Drop FORGED IPv4 packet\n");
					return TRUE;
				}
			}
		}
#ifdef CONFIG_IPV6
		else if (*pu2PktIpVer == htons(ETH_P_IPV6)) {
			uint8_t aucIPv6Mac[MAC_ADDR_LEN];
			uint8_t *pucIdx =
			    prCurrSwRfb->pvHeader +
			    ETHER_HEADER_LEN +
			    IPV6_HDR_IP_DST_ADDR_MAC_HIGH_OFFSET;

			kalMemCopy(&aucIPv6Mac[0], pucIdx, 3);
			pucIdx += 5;
			kalMemCopy(&aucIPv6Mac[3], pucIdx, 3);
			kalPrint(
				"Get IPv6 frame Dst IP MAC part " MACSTR "\n",
				MAC2STR(aucIPv6Mac));

			if (EQUAL_MAC_ADDR(aucIPv6Mac,
				prBssInfo->aucOwnMacAddr)) {
				kalPrint("Drop FORGED IPv6 packet\n");
				return TRUE;
			}
		}
#endif
	}

	return FALSE;
}
#endif

u_int8_t hs20IsUnsecuredFrame(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prBssInfo, IN struct SW_RFB *prCurrSwRfb)
{
	uint16_t *pu2PktIpVer = (uint16_t *) ((uint8_t *)
		prCurrSwRfb->pvHeader + (ETHER_HEADER_LEN - ETHER_TYPE_LEN));

	/* kalPrint("IPVER 0x%4X\n", htons(*pu2PktIpVer)); */
#if CFG_HS20_DEBUG & 0
	uint8_t i = 0;

	kalPrint("===============================================");
	for (i = 0; i < 96; i++) {
		if (!(i % 16))
			kalPrint("\n");
		kalPrint("%02X ", *((uint8_t *) prCurrSwRfb->pvHeader + i));
	}
	kalPrint("\n");
#endif

#if CFG_ENABLE_GTK_FRAME_FILTER
	if (hs20IsForgedGTKFrame(prAdapter, prBssInfo, prCurrSwRfb))
		return TRUE;
#endif
	if (*pu2PktIpVer == htons(ETH_P_ARP))
		return hs20IsGratuitousArp(prAdapter, prCurrSwRfb);
	else if (*pu2PktIpVer == htons(ETH_P_IPV6))
		return hs20IsUnsolicitedNeighborAdv(prAdapter, prCurrSwRfb);

	return FALSE;
}

u_int8_t hs20IsFrameFilterEnabled(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prBssInfo)
{
#if 1
	if (prAdapter->prGlueInfo->fgConnectHS20AP)
		return TRUE;
#else
	struct PARAM_SSID rParamSsid;
	struct BSS_DESC *prBssDesc;

	rParamSsid.u4SsidLen = prBssInfo->ucSSIDLen;
	COPY_SSID(rParamSsid.aucSsid,
		rParamSsid.u4SsidLen, prBssInfo->aucSSID, prBssInfo->ucSSIDLen);

	prBssDesc = scanSearchBssDescByBssidAndSsid(prAdapter,
		prBssInfo->aucBSSID, TRUE, &rParamSsid);

	if (!prBssDesc)
		return FALSE;

	if (prBssDesc->fgIsSupportHS20) {
		if (!(prBssDesc->ucHotspotConfig
			& ELEM_HS_CONFIG_DGAF_DISABLED_MASK))
			return TRUE;
		/* Disable frame filter only if DGAF == 1 */
		return FALSE;
	}
#endif

	/* For Now, always return true to run hs20 check even for legacy AP */
	return TRUE;
}

uint32_t hs20SetBssidPool(IN struct ADAPTER *prAdapter,
		IN void *pvBuffer,
		IN enum ENUM_KAL_NETWORK_TYPE_INDEX eNetTypeIdx)
{
	struct PARAM_HS20_SET_BSSID_POOL *prParamBssidPool =
		(struct PARAM_HS20_SET_BSSID_POOL *) pvBuffer;
	struct HS20_INFO *prHS20Info;
	uint8_t ucIdx;

	prHS20Info = &(prAdapter->rWifiVar.rHS20Info);

	pr_info("[%s]Set Bssid Pool! enable[%d] num[%d]\n",
		__func__, prParamBssidPool->fgIsEnable,
		prParamBssidPool->ucNumBssidPool);

	for (ucIdx = 0; ucIdx < prParamBssidPool->ucNumBssidPool; ucIdx++) {
		COPY_MAC_ADDR(
			prHS20Info->arBssidPool[ucIdx].aucBSSID,
			&prParamBssidPool->arBSSID[ucIdx]);

		pr_info("[%s][%d][" MACSTR "]\n",
			__func__, ucIdx,
			MAC2STR(prHS20Info->arBssidPool[ucIdx].aucBSSID));
	}
	prHS20Info->fgIsHS2SigmaMode = prParamBssidPool->fgIsEnable;
	prHS20Info->ucNumBssidPoolEntry = prParamBssidPool->ucNumBssidPool;

#if 0
	wlanClearScanningResult(prAdapter);
#endif

	return WLAN_STATUS_SUCCESS;
}

#endif /* CFG_SUPPORT_PASSPOINT */
