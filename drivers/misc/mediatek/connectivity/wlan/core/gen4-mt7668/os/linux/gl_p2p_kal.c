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
** Id: @(#) gl_p2p_cfg80211.c@@
*/

/*! \file   gl_p2p_kal.c
*    \brief
*
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "net/cfg80211.h"
#include "precomp.h"
#include "gl_wext.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

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

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

struct ieee80211_channel *kalP2pFuncGetChannelEntry(IN P_GL_P2P_INFO_T prP2pInfo, IN P_RF_CHANNEL_INFO_T prChannelInfo);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve Wi-Fi Direct state from glue layer
*
* \param[in]
*           prGlueInfo
*           rPeerAddr
* \return
*           ENUM_BOW_DEVICE_STATE
*/
/*----------------------------------------------------------------------------*/
#if 0
ENUM_PARAM_MEDIA_STATE_T kalP2PGetState(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->prP2PInfo[0]->eState;
}				/* end of kalP2PGetState() */
#endif
/*----------------------------------------------------------------------------*/
/*!
* \brief to update the assoc req to p2p
*
* \param[in]
*           prGlueInfo
*           pucFrameBody
*           u4FrameBodyLen
*           fgReassocRequest
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
VOID
kalP2PUpdateAssocInfo(IN P_GLUE_INFO_T prGlueInfo,
	IN PUINT_8 pucFrameBody, IN UINT_32 u4FrameBodyLen, IN BOOLEAN fgReassocRequest, IN UINT_8 ucBssIndex)
{
	P_BSS_INFO_T prBssInfo;
	union iwreq_data wrqu;
	unsigned char *pucExtraInfo = NULL;
	unsigned char *pucDesiredIE = NULL;
/* unsigned char aucExtraInfoBuf[200]; */
	PUINT_8 cp;
	struct net_device *prNetdevice = (struct net_device *)NULL;

	memset(&wrqu, 0, sizeof(wrqu));

	if (fgReassocRequest) {
		if (u4FrameBodyLen < 15) {
			/*
			 *  printk(KERN_WARNING "frameBodyLen too short:%ld\n", frameBodyLen);
			 */
			return;
		}
	} else {
		if (u4FrameBodyLen < 9) {
			/*
			 *  printk(KERN_WARNING "frameBodyLen too short:%ld\n", frameBodyLen);
			 */
			return;
		}
	}

	cp = pucFrameBody;

	if (fgReassocRequest) {
		/* Capability information field 2 */
		/* Listen interval field 2 */
		/* Current AP address 6 */
		cp += 10;
		u4FrameBodyLen -= 10;
	} else {
		/* Capability information field 2 */
		/* Listen interval field 2 */
		cp += 4;
		u4FrameBodyLen -= 4;
	}

	/* do supplicant a favor, parse to the start of WPA/RSN IE */
	if (wextSrchDesiredWPSIE(cp, u4FrameBodyLen, 0xDD, &pucDesiredIE)) {
		/* printk("wextSrchDesiredWPSIE!!\n"); */
		/* WPS IE found */
	} else if (wextSrchDesiredWPAIE(cp, u4FrameBodyLen, 0x30, &pucDesiredIE)) {
		/* printk("wextSrchDesiredWPAIE!!\n"); */
		/* RSN IE found */
	} else if (wextSrchDesiredWPAIE(cp, u4FrameBodyLen, 0xDD, &pucDesiredIE)) {
		/* printk("wextSrchDesiredWPAIE!!\n"); */
		/* WPA IE found */
	} else {
		/* no WPA/RSN IE found, skip this event */
		return;
	}

	/* IWEVASSOCREQIE, indicate binary string */
	pucExtraInfo = pucDesiredIE;
	wrqu.data.length = pucDesiredIE[1] + 2;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter, ucBssIndex);

	if (ucBssIndex == P2P_DEV_BSS_INDEX)
		prNetdevice = prGlueInfo->prP2PInfo[prBssInfo->u4PrivateData]->prDevHandler;
	else
		prNetdevice = prGlueInfo->prP2PInfo[prBssInfo->u4PrivateData]->aprRoleHandler;

	/* Send event to user space */
	wireless_send_event(prNetdevice, IWEVASSOCREQIE, &wrqu, pucExtraInfo);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to set Wi-Fi Direct state in glue layer
*
* \param[in]
*           prGlueInfo
*           eBowState
*           rPeerAddr
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
#if 0
VOID
kalP2PSetState(IN P_GLUE_INFO_T prGlueInfo,
	       IN ENUM_PARAM_MEDIA_STATE_T eState, IN PARAM_MAC_ADDRESS rPeerAddr, IN UINT_8 ucRole)
{
	union iwreq_data evt;
	UINT_8 aucBuffer[IW_CUSTOM_MAX];

	ASSERT(prGlueInfo);

	memset(&evt, 0, sizeof(evt));

	if (eState == PARAM_MEDIA_STATE_CONNECTED) {
		prGlueInfo->prP2PInfo[0]->eState = PARAM_MEDIA_STATE_CONNECTED;

		snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_STA_CONNECT=" MACSTR, MAC2STR(rPeerAddr));
		evt.data.length = strlen(aucBuffer);

		/* indicate in IWECUSTOM event */
		wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);

	} else if (eState == PARAM_MEDIA_STATE_DISCONNECTED) {
		snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_STA_DISCONNECT=" MACSTR, MAC2STR(rPeerAddr));
		evt.data.length = strlen(aucBuffer);

		/* indicate in IWECUSTOM event */
		wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);
	} else {
		ASSERT(0);
	}

}				/* end of kalP2PSetState() */
#endif
/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve Wi-Fi Direct operating frequency
*
* \param[in]
*           prGlueInfo
*
* \return
*           in unit of KHz
*/
/*----------------------------------------------------------------------------*/
#if 0
UINT_32 kalP2PGetFreqInKHz(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->prP2PInfo[0]->u4FreqInKHz;
}				/* end of kalP2PGetFreqInKHz() */
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve Bluetooth-over-Wi-Fi role
*
* \param[in]
*           prGlueInfo
*
* \return
*           0: P2P Device
*           1: Group Client
*           2: Group Owner
*/
/*----------------------------------------------------------------------------*/
UINT_8 kalP2PGetRole(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIdx)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->prP2PInfo[ucRoleIdx]->ucRole;
}				/* end of kalP2PGetRole() */

/*----------------------------------------------------------------------------*/
/*!
* \brief to set Wi-Fi Direct role
*
* \param[in]
*           prGlueInfo
*           ucResult
*                   0: successful
*                   1: error
*           ucRole
*                   0: P2P Device
*                   1: Group Client
*                   2: Group Owner
*
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
#if 1
VOID kalP2PSetRole(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRole, IN UINT_8 ucRoleIdx)
{
	ASSERT(prGlueInfo);
	ASSERT(ucRole <= 2);

	prGlueInfo->prP2PInfo[ucRoleIdx]->ucRole = ucRole;
	/* Remove non-used code */
}				/* end of kalP2PSetRole() */

#else
VOID
kalP2PSetRole(IN P_GLUE_INFO_T prGlueInfo,
	      IN UINT_8 ucResult, IN PUINT_8 pucSSID, IN UINT_8 ucSSIDLen, IN UINT_8 ucRole)
{
	union iwreq_data evt;
	UINT_8 aucBuffer[IW_CUSTOM_MAX];

	ASSERT(prGlueInfo);
	ASSERT(ucRole <= 2);

	memset(&evt, 0, sizeof(evt));

	if (ucResult == 0)
		prGlueInfo->prP2PInfo[0]->ucRole = ucRole;

	if (pucSSID)
		snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_FORMATION_RST=%d%d%d%c%c", ucResult,
			 ucRole, 1 /* persistence or not */, pucSSID[7], pucSSID[8]);
	else
		snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_FORMATION_RST=%d%d%d%c%c", ucResult,
			 ucRole, 1 /* persistence or not */, '0', '0');

	evt.data.length = strlen(aucBuffer);

	/* if (pucSSID) */
	/* printk("P2P GO SSID DIRECT-%c%c\n", pucSSID[7], pucSSID[8]); */

	/* indicate in IWECUSTOM event */
	wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);

}				/* end of kalP2PSetRole() */

#endif
/*----------------------------------------------------------------------------*/
/*!
* \brief to set the cipher for p2p
*
* \param[in]
*           prGlueInfo
*           u4Cipher
*
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
VOID kalP2PSetCipher(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Cipher, IN UINT_8 ucRoleIdx)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PInfo[ucRoleIdx]);

	/* It can be WEP40 (used to identify cipher is WEP), TKIP and CCMP */
	prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise = u4Cipher;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief to get the cipher, return false for security is none
*
* \param[in]
*           prGlueInfo
*
* \return
*           TRUE: cipher is ccmp
*           FALSE: cipher is none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalP2PGetCipher(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIdx)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PInfo[ucRoleIdx]);

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise == IW_AUTH_CIPHER_CCMP)
		return TRUE;

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise == IW_AUTH_CIPHER_TKIP)
		return TRUE;

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise == IW_AUTH_CIPHER_WEP40)
		return TRUE;

	return FALSE;
}

BOOLEAN kalP2PGetWepCipher(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIdx)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PInfo[ucRoleIdx]);

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise == IW_AUTH_CIPHER_WEP40)
		return TRUE;

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise == IW_AUTH_CIPHER_WEP104)
		return TRUE;

	return FALSE;
}

#if CFG_SUPPORT_SUITB
BOOLEAN kalP2PGetGcmp256Cipher(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIdx)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PInfo[ucRoleIdx]);

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise
		== IW_AUTH_CIPHER_GCMP256)
		return TRUE;

	return FALSE;
}
#endif

BOOLEAN kalP2PGetCcmpCipher(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIdx)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PInfo[ucRoleIdx]);

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise == IW_AUTH_CIPHER_CCMP)
		return TRUE;

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise == IW_AUTH_CIPHER_TKIP)
		return FALSE;

	return FALSE;
}

BOOLEAN kalP2PGetTkipCipher(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIdx)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PInfo[ucRoleIdx]);

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise == IW_AUTH_CIPHER_CCMP)
		return FALSE;

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise == IW_AUTH_CIPHER_TKIP)
		return TRUE;

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to set the status of WSC
*
* \param[in]
*           prGlueInfo
*
* \return
*/
/*----------------------------------------------------------------------------*/
VOID kalP2PSetWscMode(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucWscMode)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PDevInfo);

	prGlueInfo->prP2PDevInfo->ucWSCRunning = ucWscMode;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to get the status of WSC
*
* \param[in]
*           prGlueInfo
*
* \return
*/
/*----------------------------------------------------------------------------*/
UINT_8 kalP2PGetWscMode(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PDevInfo);

	return prGlueInfo->prP2PDevInfo->ucWSCRunning;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to get the wsc ie length
*
* \param[in]
*           prGlueInfo
*           ucType : 0 for beacon, 1 for probe req, 2 for probe resp
*
* \return
*           The WSC IE length
*/
/*----------------------------------------------------------------------------*/
UINT_16 kalP2PCalWSC_IELen(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucType, IN UINT_8 ucRoleIdx)
{
	ASSERT(prGlueInfo);

	ASSERT(ucType < 4);

	return prGlueInfo->prP2PInfo[ucRoleIdx]->u2WSCIELen[ucType];
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to copy the wsc ie setting from p2p supplicant
*
* \param[in]
*           prGlueInfo
*
* \return
*           The WPS IE length
*/
/*----------------------------------------------------------------------------*/
VOID kalP2PGenWSC_IE(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucType, IN PUINT_8 pucBuffer, IN UINT_8 ucRoleIdx)
{
	P_GL_P2P_INFO_T prGlP2pInfo = (P_GL_P2P_INFO_T) NULL;

	do {
		if ((prGlueInfo == NULL) || (ucType >= 4) || (pucBuffer == NULL))
			break;

		prGlP2pInfo = prGlueInfo->prP2PInfo[ucRoleIdx];

		kalMemCopy(pucBuffer, prGlP2pInfo->aucWSCIE[ucType], prGlP2pInfo->u2WSCIELen[ucType]);

	} while (FALSE);

}

VOID kalP2PUpdateWSC_IE(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucType, IN PUINT_8 pucBuffer,
	IN UINT_16 u2BufferLength, IN UINT_8 ucRoleIdx)
{
	P_GL_P2P_INFO_T prGlP2pInfo = (P_GL_P2P_INFO_T) NULL;

	do {
		if ((prGlueInfo == NULL) || (ucType >= 4) || ((u2BufferLength > 0) && (pucBuffer == NULL)))
			break;

		if (u2BufferLength > VENDOR_SPECIFIC_IE_LENGTH) {
			DBGLOG(P2P, ERROR, "Allow %d bytes but %d received\n",
				VENDOR_SPECIFIC_IE_LENGTH, u2BufferLength);
			ASSERT(FALSE);
			break;
		}

		prGlP2pInfo = prGlueInfo->prP2PInfo[ucRoleIdx];

		kalMemCopy(prGlP2pInfo->aucWSCIE[ucType], pucBuffer, u2BufferLength);

		prGlP2pInfo->u2WSCIELen[ucType] = u2BufferLength;

	} while (FALSE);

}				/* kalP2PUpdateWSC_IE */

UINT_16 kalP2PCalP2P_IELen(IN P_GLUE_INFO_T prGlueInfo,
	IN UINT_32 u4IEIdx, IN UINT_8 ucRoleIdx)
{
	if (u4IEIdx >= MAX_MULTI_P2P_IE_COUNT)
		return 0;

	return prGlueInfo->prP2PInfo[ucRoleIdx]->u2P2PIELen[u4IEIdx];
}

/* kalP2PGenP2P_IE
 * append p2p ie to output frame
 */
VOID kalP2PGenP2P_IE(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4IEIdx,
	IN PUINT_8 pucBuffer, IN UINT_8 ucRoleIdx)
{
	P_GL_P2P_INFO_T prGlP2pInfo = (P_GL_P2P_INFO_T) NULL;

	if (!pucBuffer)
		return;

	if (u4IEIdx >= MAX_MULTI_P2P_IE_COUNT)
		return;

	prGlP2pInfo = prGlueInfo->prP2PInfo[ucRoleIdx];

	if (prGlP2pInfo->u2P2PIELen[u4IEIdx] > 0)
		kalMemCopy(pucBuffer, prGlP2pInfo->aucP2PIE[u4IEIdx],
			prGlP2pInfo->u2P2PIELen[u4IEIdx]);
}

VOID kalP2PResetP2P_IE(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIdx)
{
	P_GL_P2P_INFO_T prGlP2pInfo = (P_GL_P2P_INFO_T) NULL;
	UINT_32 u4IEIdx = MAX_MULTI_P2P_IE_COUNT;

	prGlP2pInfo = prGlueInfo->prP2PInfo[ucRoleIdx];

	while (u4IEIdx-- > 0)
		prGlP2pInfo->u2P2PIELen[u4IEIdx] = 0;
}

/* kalP2PUpdateP2P_IE
 * copy multiple p2p ie to local buffer
 */
VOID kalP2PUpdateP2P_IE(IN P_GLUE_INFO_T prGlueInfo,
	IN PUINT_8 pucBuffer, IN UINT_16 u2BufferLength, IN UINT_8 ucRoleIdx)
{
	P_GL_P2P_INFO_T prGlP2pInfo = (P_GL_P2P_INFO_T) NULL;
	UINT_32 u4IEIdx;

	if (!prGlueInfo) {
		DBGLOG(P2P, ERROR, "NULL prGlueInfo!\n");
		return;
	}

	if (!pucBuffer) {
		DBGLOG(P2P, ERROR, "NULL pucBuffer!\n");
		return;
	}

	if (u2BufferLength == 0) {
		DBGLOG(P2P, WARN, "0 IE length, skip update\n");
		return;
	}

	if (u2BufferLength > VENDOR_SPECIFIC_IE_LENGTH) {
		DBGLOG(P2P, ERROR, "Allow %d bytes but %d received\n",
			VENDOR_SPECIFIC_IE_LENGTH, u2BufferLength);
		return;
	}

	DBGLOG(P2P, TRACE, "ucRoleIdx=%d, u2BufferLength=%d\n",
		ucRoleIdx, u2BufferLength);

	prGlP2pInfo = prGlueInfo->prP2PInfo[ucRoleIdx];

	for (u4IEIdx = 0; u4IEIdx < MAX_MULTI_P2P_IE_COUNT; u4IEIdx++) {
		if (prGlP2pInfo->u2P2PIELen[u4IEIdx] == 0) {
			DBGLOG(P2P, INFO,
				"ucRoleIdx=%d, u4IEIdx=%d, u2BufferLength=%d\n",
				ucRoleIdx, u4IEIdx, u2BufferLength);
			kalMemCopy(prGlP2pInfo->aucP2PIE[u4IEIdx],
				pucBuffer, u2BufferLength);
			prGlP2pInfo->u2P2PIELen[u4IEIdx] = u2BufferLength;
			break;
		}
	}

	if (u4IEIdx == MAX_MULTI_P2P_IE_COUNT)
		DBGLOG(P2P, WARN, "No available aucP2PIE\n");
}

#if 0
/*----------------------------------------------------------------------------*/
/*!
* \brief indicate an event to supplicant for device connection request
*
* \param[in] prGlueInfo Pointer of GLUE_INFO_T
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalP2PIndicateConnReq(IN P_GLUE_INFO_T prGlueInfo, IN PUINT_8 pucDevName, IN INT_32 u4NameLength,
							IN PARAM_MAC_ADDRESS rPeerAddr,
							IN UINT_8 ucDevType,/* 0: P2P Device / 1: GC / 2: GO */
							IN INT_32 i4ConfigMethod, IN INT_32 i4ActiveConfigMethod
)
{
	union iwreq_data evt;
	UINT_8 aucBuffer[IW_CUSTOM_MAX];

	ASSERT(prGlueInfo);

	/* buffer peer information for later IOC_P2P_GET_REQ_DEVICE_INFO access */
	prGlueInfo->prP2PInfo[0]->u4ConnReqNameLength = u4NameLength > 32 ? 32 : u4NameLength;
	kalMemCopy(prGlueInfo->prP2PInfo[0]->aucConnReqDevName,
		pucDevName,
		prGlueInfo->prP2PInfo[0]->u4ConnReqNameLength);
	COPY_MAC_ADDR(prGlueInfo->prP2PInfo[0]->rConnReqPeerAddr, rPeerAddr);
	prGlueInfo->prP2PInfo[0]->ucConnReqDevType = ucDevType;
	prGlueInfo->prP2PInfo[0]->i4ConnReqConfigMethod = i4ConfigMethod;
	prGlueInfo->prP2PInfo[0]->i4ConnReqActiveConfigMethod = i4ActiveConfigMethod;

	/* prepare event structure */
	memset(&evt, 0, sizeof(evt));

	snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_DVC_REQ");
	evt.data.length = strlen(aucBuffer);

	/* indicate in IWEVCUSTOM event */
	wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);

}				/* end of kalP2PIndicateConnReq() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Indicate an event to supplicant for device connection request from other device.
*
* \param[in] prGlueInfo Pointer of GLUE_INFO_T
* \param[in] pucGroupBssid  Only valid when invitation Type equals to 0.
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID
kalP2PInvitationIndication(IN P_GLUE_INFO_T prGlueInfo,
			   IN P_P2P_DEVICE_DESC_T prP2pDevDesc,
			   IN PUINT_8 pucSsid,
			   IN UINT_8 ucSsidLen,
			   IN UINT_8 ucOperatingChnl, IN UINT_8 ucInvitationType, IN PUINT_8 pucGroupBssid)
{
#if 1
	union iwreq_data evt;
	UINT_8 aucBuffer[IW_CUSTOM_MAX];

	ASSERT(prGlueInfo);

	/* buffer peer information for later IOC_P2P_GET_STRUCT access */
	prGlueInfo->prP2PInfo[0]->u4ConnReqNameLength =
	    (UINT_32) ((prP2pDevDesc->u2NameLength > 32) ? 32 : prP2pDevDesc->u2NameLength);
	kalMemCopy(prGlueInfo->prP2PInfo[0]->aucConnReqDevName, prP2pDevDesc->aucName,
		   prGlueInfo->prP2PInfo[0]->u4ConnReqNameLength);
	COPY_MAC_ADDR(prGlueInfo->prP2PInfo[0]->rConnReqPeerAddr, prP2pDevDesc->aucDeviceAddr);
	COPY_MAC_ADDR(prGlueInfo->prP2PInfo[0]->rConnReqGroupAddr, pucGroupBssid);
	prGlueInfo->prP2PInfo[0]->i4ConnReqConfigMethod = (INT_32) (prP2pDevDesc->u2ConfigMethod);
	prGlueInfo->prP2PInfo[0]->ucOperatingChnl = ucOperatingChnl;
	prGlueInfo->prP2PInfo[0]->ucInvitationType = ucInvitationType;

	/* prepare event structure */
	memset(&evt, 0, sizeof(evt));

	snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_INV_INDICATE");
	evt.data.length = strlen(aucBuffer);

	/* indicate in IWEVCUSTOM event */
	wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);

#else
	P_MSG_P2P_CONNECTION_REQUEST_T prP2pConnReq = (P_MSG_P2P_CONNECTION_REQUEST_T) NULL;
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;
	P_P2P_CONNECTION_SETTINGS_T prP2pConnSettings = (P_P2P_CONNECTION_SETTINGS_T) NULL;

	do {
		ASSERT_BREAK((prGlueInfo != NULL) && (prP2pDevDesc != NULL));

		/* Not a real solution */

		prP2pSpecificBssInfo = prGlueInfo->prAdapter->rWifiVar.prP2pSpecificBssInfo;
		prP2pConnSettings = prGlueInfo->prAdapter->rWifiVar.prP2PConnSettings;

		prP2pConnReq = (P_MSG_P2P_CONNECTION_REQUEST_T) cnmMemAlloc(prGlueInfo->prAdapter,
									    RAM_TYPE_MSG,
									    sizeof(MSG_P2P_CONNECTION_REQUEST_T));

		if (prP2pConnReq == NULL)
			break;

		kalMemZero(prP2pConnReq, sizeof(MSG_P2P_CONNECTION_REQUEST_T));

		prP2pConnReq->rMsgHdr.eMsgId = MID_MNY_P2P_CONNECTION_REQ;

		prP2pConnReq->eFormationPolicy = ENUM_P2P_FORMATION_POLICY_AUTO;

		COPY_MAC_ADDR(prP2pConnReq->aucDeviceID, prP2pDevDesc->aucDeviceAddr);

		prP2pConnReq->u2ConfigMethod = prP2pDevDesc->u2ConfigMethod;

		if (ucInvitationType == P2P_INVITATION_TYPE_INVITATION) {
			prP2pConnReq->fgIsPersistentGroup = FALSE;
			prP2pConnReq->fgIsTobeGO = FALSE;

		}

		else if (ucInvitationType == P2P_INVITATION_TYPE_REINVOKE) {
			DBGLOG(P2P, TRACE, "Re-invoke Persistent Group\n");
			prP2pConnReq->fgIsPersistentGroup = TRUE;
			prP2pConnReq->fgIsTobeGO = (prGlueInfo->prP2PInfo[0]->ucRole == 2) ? TRUE : FALSE;

		}

		p2pFsmRunEventDeviceDiscoveryAbort(prGlueInfo->prAdapter, NULL);

		if (ucOperatingChnl != 0)
			prP2pSpecificBssInfo->ucPreferredChannel = ucOperatingChnl;

		if ((ucSsidLen < 32) && (pucSsid != NULL))
			COPY_SSID(prP2pConnSettings->aucSSID, prP2pConnSettings->ucSSIDLen, pucSsid, ucSsidLen);

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prP2pConnReq, MSG_SEND_METHOD_BUF);

	} while (FALSE);

	/* frog add. */
	/* TODO: Invitation Indication */

#endif

}				/* kalP2PInvitationIndication */
#endif

#if 0
/*----------------------------------------------------------------------------*/
/*!
* \brief Indicate an status to supplicant for device invitation status.
*
* \param[in] prGlueInfo Pointer of GLUE_INFO_T
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalP2PInvitationStatus(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4InvStatus)
{
	union iwreq_data evt;
	UINT_8 aucBuffer[IW_CUSTOM_MAX];

	ASSERT(prGlueInfo);

	/* buffer peer information for later IOC_P2P_GET_STRUCT access */
	prGlueInfo->prP2PInfo[0]->u4InvStatus = u4InvStatus;

	/* prepare event structure */
	memset(&evt, 0, sizeof(evt));

	snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_INV_STATUS");
	evt.data.length = strlen(aucBuffer);

	/* indicate in IWEVCUSTOM event */
	wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);

}				/* kalP2PInvitationStatus */
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief Indicate an event to supplicant for Service Discovery request from other device.
*
* \param[in] prGlueInfo Pointer of GLUE_INFO_T
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalP2PIndicateSDRequest(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rPeerAddr, IN UINT_8 ucSeqNum)
{
	union iwreq_data evt;
	UINT_8 aucBuffer[IW_CUSTOM_MAX];

	ASSERT(prGlueInfo);

	memset(&evt, 0, sizeof(evt));

	snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_SD_REQ %d", ucSeqNum);
	evt.data.length = strlen(aucBuffer);

	/* indicate IWEVP2PSDREQ event */
	wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);

}				/* end of kalP2PIndicateSDRequest() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Indicate an event to supplicant for Service Discovery response
*         from other device.
*
* \param[in] prGlueInfo Pointer of GLUE_INFO_T
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
void kalP2PIndicateSDResponse(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rPeerAddr, IN UINT_8 ucSeqNum)
{
	union iwreq_data evt;
	UINT_8 aucBuffer[IW_CUSTOM_MAX];

	ASSERT(prGlueInfo);

	memset(&evt, 0, sizeof(evt));

	snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_SD_RESP %d", ucSeqNum);
	evt.data.length = strlen(aucBuffer);

	/* indicate IWEVP2PSDREQ event */
	wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);

}				/* end of kalP2PIndicateSDResponse() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Indicate an event to supplicant for Service Discovery TX Done
*         from other device.
*
* \param[in] prGlueInfo Pointer of GLUE_INFO_T
* \param[in] ucSeqNum   Sequence number of the frame
* \param[in] ucStatus   Status code for TX
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalP2PIndicateTXDone(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucSeqNum, IN UINT_8 ucStatus)
{
	union iwreq_data evt;
	UINT_8 aucBuffer[IW_CUSTOM_MAX];

	ASSERT(prGlueInfo);

	memset(&evt, 0, sizeof(evt));

	snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_SD_XMITTED: %d %d", ucSeqNum, ucStatus);
	evt.data.length = strlen(aucBuffer);

	/* indicate IWEVP2PSDREQ event */
	wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);

}				/* end of kalP2PIndicateSDResponse() */

struct net_device *kalP2PGetDevHdlr(P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PInfo[0]);
	return prGlueInfo->prP2PInfo[0]->prDevHandler;
}

#if CFG_SUPPORT_ANTI_PIRACY
#if 0
/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID kalP2PIndicateSecCheckRsp(IN P_GLUE_INFO_T prGlueInfo, IN PUINT_8 pucRsp, IN UINT_16 u2RspLen)
{
	union iwreq_data evt;
	UINT_8 aucBuffer[IW_CUSTOM_MAX];

	ASSERT(prGlueInfo);

	memset(&evt, 0, sizeof(evt));
	snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_SEC_CHECK_RSP=");

	kalMemCopy(prGlueInfo->prP2PInfo[0]->aucSecCheckRsp, pucRsp, u2RspLen);
	evt.data.length = strlen(aucBuffer);

#if DBG
	DBGLOG_MEM8(SEC, LOUD, prGlueInfo->prP2PInfo[0]->aucSecCheckRsp, u2RspLen);
#endif
	/* indicate in IWECUSTOM event */
	wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);
}				/* p2pFsmRunEventRxDisassociation */
#endif
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID
kalGetChnlList(IN P_GLUE_INFO_T prGlueInfo,
	       IN ENUM_BAND_T eSpecificBand,
	       IN UINT_8 ucMaxChannelNum, IN PUINT_8 pucNumOfChannel, IN P_RF_CHANNEL_INFO_T paucChannelList)
{
	rlmDomainGetChnlList(prGlueInfo->prAdapter, eSpecificBand,
		FALSE, ucMaxChannelNum, pucNumOfChannel, paucChannelList);
}				/* kalGetChnlList */

/* ////////////////////////////////////ICS SUPPORT////////////////////////////////////// */

VOID
kalP2PIndicateChannelReady(IN P_GLUE_INFO_T prGlueInfo,
			   IN UINT_64 u8SeqNum,
			   IN UINT_32 u4ChannelNum,
			   IN ENUM_BAND_T eBand, IN ENUM_CHNL_EXT_T eSco, IN UINT_32 u4Duration)
{
	struct ieee80211_channel *prIEEE80211ChnlStruct = (struct ieee80211_channel *)NULL;
	RF_CHANNEL_INFO_T rChannelInfo;
	enum nl80211_channel_type eChnlType = NL80211_CHAN_NO_HT;

	do {
		if (prGlueInfo == NULL)
			break;

		kalMemZero(&rChannelInfo, sizeof(RF_CHANNEL_INFO_T));

		rChannelInfo.ucChannelNum = u4ChannelNum;
		rChannelInfo.eBand = eBand;

		prIEEE80211ChnlStruct = kalP2pFuncGetChannelEntry(prGlueInfo->prP2PInfo[0], &rChannelInfo);

		kalP2pFuncGetChannelType(eSco, &eChnlType);

		cfg80211_ready_on_channel(prGlueInfo->prP2PInfo[0]->prWdev,	/* struct wireless_dev, */
					  u8SeqNum,	/* u64 cookie, */
					  prIEEE80211ChnlStruct,	/* struct ieee80211_channel * chan, */
					  u4Duration,	/* unsigned int duration, */
					  GFP_KERNEL);	/* gfp_t gfp *//* allocation flags */
	} while (FALSE);

}				/* kalP2PIndicateChannelReady */

VOID
kalP2PIndicateChannelExpired(IN P_GLUE_INFO_T prGlueInfo,
			     IN UINT_64 u8SeqNum,
			     IN UINT_32 u4ChannelNum, IN ENUM_BAND_T eBand, IN ENUM_CHNL_EXT_T eSco)
{

	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;
	struct ieee80211_channel *prIEEE80211ChnlStruct = (struct ieee80211_channel *)NULL;
	enum nl80211_channel_type eChnlType = NL80211_CHAN_NO_HT;
	RF_CHANNEL_INFO_T rRfChannelInfo;

	do {
		if (prGlueInfo == NULL) {
			ASSERT(FALSE);
			break;
		}

		prGlueP2pInfo = prGlueInfo->prP2PInfo[0];

		if (prGlueP2pInfo == NULL) {
			ASSERT(FALSE);
			break;
		}

		DBGLOG(P2P, TRACE, "kalP2PIndicateChannelExpired\n");

		rRfChannelInfo.eBand = eBand;
		rRfChannelInfo.ucChannelNum = u4ChannelNum;

		prIEEE80211ChnlStruct = kalP2pFuncGetChannelEntry(prGlueP2pInfo, &rRfChannelInfo);
		if (!prIEEE80211ChnlStruct) {
			DBGLOG(P2P, ERROR, "prIEEE80211ChnlStruct is NULL!\n");
			break;
		}

		kalP2pFuncGetChannelType(eSco, &eChnlType);

		cfg80211_remain_on_channel_expired(prGlueP2pInfo->prWdev,	/* struct wireless_dev, */
						   u8SeqNum, prIEEE80211ChnlStruct, GFP_KERNEL);
	} while (FALSE);

}				/* kalP2PIndicateChannelExpired */

VOID kalP2PIndicateScanDone(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIndex, IN BOOLEAN fgIsAbort)
{
	P_GL_P2P_DEV_INFO_T prP2pGlueDevInfo = (P_GL_P2P_DEV_INFO_T) NULL;
	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;
	struct cfg80211_scan_request *prScanRequest = NULL;

	GLUE_SPIN_LOCK_DECLARATION();

	do {
		if (prGlueInfo == NULL) {

			ASSERT(FALSE);
			break;
		}

		prGlueP2pInfo = prGlueInfo->prP2PInfo[0];
		prP2pGlueDevInfo = prGlueInfo->prP2PDevInfo;

		if ((prGlueP2pInfo == NULL) || (prP2pGlueDevInfo == NULL)) {
			ASSERT(FALSE);
			break;
		}

		DBGLOG(INIT, INFO, "[p2p] scan complete %p\n", prP2pGlueDevInfo->prScanRequest);

		KAL_ACQUIRE_MUTEX(prGlueInfo->prAdapter, MUTEX_DEL_INF);

		/* The cfg80211_scan_done may be interruptd by the p2pStop.
		 * And the following kernel process calls __cfg80211_scan_done,
		 * that causes some issue. the temporary solution is putting
		 * the scan_done inside the lock. ref: change 1022227.
		 */
		if (prGlueInfo->prAdapter->fgIsP2PRegistered == TRUE) {
			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
			prScanRequest = prP2pGlueDevInfo->prScanRequest;
			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

			if (prScanRequest != NULL) {
				scanReportBss2Cfg80211(prGlueInfo->prAdapter,
						BSS_TYPE_P2P_DEVICE, NULL);
			}
			/* scanReportBss2Cfg80211() do many works, so don't put
			 * it inside the lock. And its main function puts bss
			 * to cfg80211, that isn't related to scan_req.
			 */
			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
			prScanRequest = prP2pGlueDevInfo->prScanRequest;
			if (prScanRequest != NULL) {
				DBGLOG(INIT, INFO, "DBG:p2p_cfg_scan_done\n");
				kalCfg80211ScanDone(prScanRequest, fgIsAbort);
				prP2pGlueDevInfo->prScanRequest = NULL;
			}
			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
		}
		KAL_RELEASE_MUTEX(prGlueInfo->prAdapter, MUTEX_DEL_INF);

	} while (FALSE);

}				/* kalP2PIndicateScanDone */

VOID
kalP2PIndicateBssInfo(IN P_GLUE_INFO_T prGlueInfo,
		      IN PUINT_8 pucFrameBuf,
		      IN UINT_32 u4BufLen, IN P_RF_CHANNEL_INFO_T prChannelInfo, IN INT_32 i4SignalStrength)
{
	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;
	struct ieee80211_channel *prChannelEntry = (struct ieee80211_channel *)NULL;
	struct ieee80211_mgmt *prBcnProbeRspFrame = (struct ieee80211_mgmt *)pucFrameBuf;
	struct cfg80211_bss *prCfg80211Bss = (struct cfg80211_bss *)NULL;

	do {
		if ((prGlueInfo == NULL) || (pucFrameBuf == NULL) || (prChannelInfo == NULL)) {
			ASSERT(FALSE);
			break;
		}

		prGlueP2pInfo = prGlueInfo->prP2PInfo[0];

		if (prGlueP2pInfo == NULL) {
			ASSERT(FALSE);
			break;
		}

		prChannelEntry = kalP2pFuncGetChannelEntry(prGlueP2pInfo, prChannelInfo);

		if (prChannelEntry == NULL) {
			DBGLOG(P2P, TRACE, "Unknown channel info\n");
			break;
		}

		/* rChannelInfo.center_freq = nicChannelNum2Freq((UINT_32)prChannelInfo->ucChannelNum) / 1000; */

		prCfg80211Bss = cfg80211_inform_bss_frame(prGlueP2pInfo->prWdev->wiphy,	/* struct wiphy * wiphy, */
							  prChannelEntry,
							  prBcnProbeRspFrame, u4BufLen, i4SignalStrength, GFP_KERNEL);

		/* Return this structure. */
		cfg80211_put_bss(prGlueP2pInfo->prWdev->wiphy, prCfg80211Bss);

	} while (FALSE);

	return;

}				/* kalP2PIndicateBssInfo */

VOID kalP2PIndicateMgmtTxStatus(IN P_GLUE_INFO_T prGlueInfo, IN P_MSDU_INFO_T prMsduInfo, IN BOOLEAN fgIsAck)
{
	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;
	PUINT_64 pu8GlCookie = (PUINT_64) NULL;
	struct net_device *prNetdevice = (struct net_device *)NULL;

	do {
		if ((prGlueInfo == NULL) || (prMsduInfo == NULL)) {
			DBGLOG(P2P, WARN, "Unexpected pointer PARAM. 0x%lx, 0x%lx.\n", prGlueInfo, prMsduInfo);
			ASSERT(FALSE);
			break;
		}

		pu8GlCookie =
		    (PUINT_64) ((ULONG) prMsduInfo->prPacket +
				(ULONG) prMsduInfo->u2FrameLength + MAC_TX_RESERVED_FIELD);

		if (prMsduInfo->ucBssIndex == P2P_DEV_BSS_INDEX) {
			prGlueP2pInfo = prGlueInfo->prP2PInfo[0];
			prNetdevice = prGlueP2pInfo->prDevHandler;

		} else {
			P_BSS_INFO_T prP2pBssInfo =
			    GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter, prMsduInfo->ucBssIndex);
			prGlueP2pInfo = prGlueInfo->prP2PInfo[prP2pBssInfo->u4PrivateData];
			prNetdevice = prGlueP2pInfo->aprRoleHandler;
		}

		cfg80211_mgmt_tx_status(prNetdevice->ieee80211_ptr,	/* struct net_device * dev, */
					*pu8GlCookie,
					(PUINT_8) ((ULONG) prMsduInfo->prPacket +
						   MAC_TX_RESERVED_FIELD),
					prMsduInfo->u2FrameLength, fgIsAck, GFP_KERNEL);

	} while (FALSE);

}				/* kalP2PIndicateMgmtTxStatus */

VOID
kalP2PIndicateRxMgmtFrame(IN P_GLUE_INFO_T prGlueInfo,
			  IN P_SW_RFB_T prSwRfb, IN BOOLEAN fgIsDevInterface, IN UINT_8 ucRoleIdx)
{
#define DBG_P2P_MGMT_FRAME_INDICATION 1
	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;
	INT_32 i4Freq = 0;
	UINT_8 ucChnlNum = 0;
#if DBG_P2P_MGMT_FRAME_INDICATION
	P_WLAN_MAC_HEADER_T prWlanHeader = (P_WLAN_MAC_HEADER_T) NULL;
#endif
	struct net_device *prNetdevice = (struct net_device *)NULL;

	do {
		if ((prGlueInfo == NULL) || (prSwRfb == NULL)) {
			ASSERT(FALSE);
			break;
		}

		prGlueP2pInfo = prGlueInfo->prP2PInfo[ucRoleIdx];

		/* ToDo[6630]: Get the following by channel freq */
		/* HAL_RX_STATUS_GET_CHAN_FREQ( prSwRfb->prRxStatus) */
		/* ucChnlNum = prSwRfb->prHifRxHdr->ucHwChannelNum; */

		ucChnlNum = HAL_RX_STATUS_GET_CHNL_NUM(prSwRfb->prRxStatus);

#if DBG_P2P_MGMT_FRAME_INDICATION

		prWlanHeader = (P_WLAN_MAC_HEADER_T) prSwRfb->pvHeader;

		switch (prWlanHeader->u2FrameCtrl) {
		case MAC_FRAME_PROBE_REQ:
			DBGLOG(P2P, TRACE, "RX Probe Req at channel %d ", ucChnlNum);
			break;
		case MAC_FRAME_PROBE_RSP:
			DBGLOG(P2P, TRACE, "RX Probe Rsp at channel %d ", ucChnlNum);
			break;
		case MAC_FRAME_ACTION:
			DBGLOG(P2P, TRACE, "RX Action frame at channel %d ", ucChnlNum);
			break;
		default:
			DBGLOG(P2P, TRACE, "RX Packet:%d at channel %d ", prWlanHeader->u2FrameCtrl, ucChnlNum);
			break;
		}

		DBGLOG(P2P, TRACE, "from: " MACSTR "\n", MAC2STR(prWlanHeader->aucAddr2));
#endif
		i4Freq = nicChannelNum2Freq(ucChnlNum) / 1000;

		if (fgIsDevInterface)
			prNetdevice = prGlueP2pInfo->prDevHandler;
		else
			prNetdevice = prGlueP2pInfo->aprRoleHandler;
		if (prNetdevice == NULL) {
			DBGLOG(AIS, WARN,
				"prNetdevice is NULL!\n");
			break;
		}
		if (prGlueInfo->prDevHandler->ieee80211_ptr == NULL) {
			DBGLOG(AIS, WARN,
				"ieee80211_ptr is NULL!\n");
			break;
		}

#if (KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE)
		cfg80211_rx_mgmt(prNetdevice->ieee80211_ptr,	/* struct net_device * dev, */
				 i4Freq,
				 RCPI_TO_dBm(nicRxGetRcpiValueFromRxv(RCPI_MODE_WF0, prSwRfb)),
				 prSwRfb->pvHeader,
				 prSwRfb->u2PacketLen,
				 NL80211_RXMGMT_FLAG_ANSWERED);
#elif (KERNEL_VERSION(3, 12, 0) <= CFG80211_VERSION_CODE)
		cfg80211_rx_mgmt(prNetdevice->ieee80211_ptr,	/* struct net_device * dev, */
				 i4Freq,
				 RCPI_TO_dBm(nicRxGetRcpiValueFromRxv(RCPI_MODE_WF0, prSwRfb)),
				 prSwRfb->pvHeader,
				 prSwRfb->u2PacketLen,
				 NL80211_RXMGMT_FLAG_ANSWERED,
				 GFP_ATOMIC);
#else
		cfg80211_rx_mgmt(prNetdevice->ieee80211_ptr,	/* struct net_device * dev, */
				 i4Freq,
				 RCPI_TO_dBm(nicRxGetRcpiValueFromRxv(RCPI_MODE_WF0, prSwRfb)),
				 prSwRfb->pvHeader,
				 prSwRfb->u2PacketLen,
				 GFP_ATOMIC);
#endif


	} while (FALSE);

}				/* kalP2PIndicateRxMgmtFrame */
#if CFG_WPS_DISCONNECT || (KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE)
VOID
kalP2PGCIndicateConnectionStatus(IN P_GLUE_INFO_T prGlueInfo,
				 IN UINT_8 ucRoleIndex,
				 IN P_P2P_CONNECTION_REQ_INFO_T prP2pConnInfo,
				 IN PUINT_8 pucRxIEBuf, IN UINT_16 u2RxIELen, IN UINT_16 u2StatusReason,
				 IN WLAN_STATUS eStatus)
{
	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;

	do {
		if (prGlueInfo == NULL) {
			ASSERT(FALSE);
			break;
		}

		prGlueP2pInfo = prGlueInfo->prP2PInfo[ucRoleIndex];

		if (prP2pConnInfo) {
			cfg80211_connect_result(prGlueP2pInfo->aprRoleHandler,
						/* struct net_device * dev, */
						prP2pConnInfo->aucBssid, prP2pConnInfo->aucIEBuf,
						prP2pConnInfo->u4BufLength,
						pucRxIEBuf, u2RxIELen, u2StatusReason,
						GFP_KERNEL);	/* gfp_t gfp *//* allocation flags */

			prP2pConnInfo->eConnRequest = P2P_CONNECTION_TYPE_IDLE;
		} else {
			/* Disconnect, what if u2StatusReason == 0? */
			cfg80211_disconnected(prGlueP2pInfo->aprRoleHandler,
					      /* struct net_device * dev, */
					      u2StatusReason, pucRxIEBuf, u2RxIELen,
					      eStatus == WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY, GFP_KERNEL);
		}

	} while (FALSE);

}				/* kalP2PGCIndicateConnectionStatus */

#else
VOID
kalP2PGCIndicateConnectionStatus(IN P_GLUE_INFO_T prGlueInfo,
				 IN UINT_8 ucRoleIndex,
				 IN P_P2P_CONNECTION_REQ_INFO_T prP2pConnInfo,
				 IN PUINT_8 pucRxIEBuf, IN UINT_16 u2RxIELen, IN UINT_16 u2StatusReason)
{
	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;

	do {
		if (prGlueInfo == NULL) {
			ASSERT(FALSE);
			break;
		}

		prGlueP2pInfo = prGlueInfo->prP2PInfo[ucRoleIndex];

		if (prP2pConnInfo) {
			cfg80211_connect_result(prGlueP2pInfo->aprRoleHandler,
						/* struct net_device * dev, */
						prP2pConnInfo->aucBssid, prP2pConnInfo->aucIEBuf,
						prP2pConnInfo->u4BufLength,
						pucRxIEBuf, u2RxIELen, u2StatusReason,
						GFP_KERNEL);	/* gfp_t gfp *//* allocation flags */

			prP2pConnInfo->eConnRequest = P2P_CONNECTION_TYPE_IDLE;
		} else {
			/* Disconnect, what if u2StatusReason == 0? */
			cfg80211_disconnected(prGlueP2pInfo->aprRoleHandler,
					      /* struct net_device * dev, */
					      u2StatusReason, pucRxIEBuf, u2RxIELen, GFP_KERNEL);
		}

	} while (FALSE);

}				/* kalP2PGCIndicateConnectionStatus */

#endif

VOID
kalP2PGOStationUpdate(IN P_GLUE_INFO_T prGlueInfo,
		      IN UINT_8 ucRoleIndex, IN P_STA_RECORD_T prCliStaRec, IN BOOLEAN fgIsNew)
{
	P_GL_P2P_INFO_T prP2pGlueInfo = (P_GL_P2P_INFO_T) NULL;

	do {
		if ((prGlueInfo == NULL) || (prCliStaRec == NULL) || (ucRoleIndex >= 2))
			break;

		prP2pGlueInfo = prGlueInfo->prP2PInfo[ucRoleIndex];

		if (fgIsNew) {
			struct station_info rStationInfo;

			kalMemZero(&rStationInfo, sizeof(rStationInfo));

#if KERNEL_VERSION(4, 0, 0) > CFG80211_VERSION_CODE
			rStationInfo.filled = STATION_INFO_ASSOC_REQ_IES;
#endif
			rStationInfo.generation = ++prP2pGlueInfo->i4Generation;

			rStationInfo.assoc_req_ies = prCliStaRec->pucAssocReqIe;
			rStationInfo.assoc_req_ies_len = prCliStaRec->u2AssocReqIeLen;

			cfg80211_new_sta(prP2pGlueInfo->aprRoleHandler,
					 /* struct net_device * dev, */
					 prCliStaRec->aucMacAddr, &rStationInfo, GFP_KERNEL);
		} else {
			++prP2pGlueInfo->i4Generation;

			cfg80211_del_sta(prP2pGlueInfo->aprRoleHandler,
					 /* struct net_device * dev, */
					 prCliStaRec->aucMacAddr, GFP_KERNEL);
		}

	} while (FALSE);

	return;

}				/* kalP2PGOStationUpdate */

#if (CFG_SUPPORT_DFS_MASTER == 1)
VOID kalP2PRddDetectUpdate(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIndex)
{
	DBGLOG(INIT, INFO, "Radar Detection event\n");

	do {
		if (prGlueInfo == NULL) {
			ASSERT(FALSE);
			break;
		}

		if (prGlueInfo->prP2PInfo[ucRoleIndex]->chandef == NULL) {
			ASSERT(FALSE);
			break;
		}

		/* cac start disable for next cac slot if enable in dfs channel */
		prGlueInfo->prP2PInfo[ucRoleIndex]->prWdev->cac_started = FALSE;
		DBGLOG(INIT, INFO, "kalP2PRddDetectUpdate: Update to OS\n");
		cfg80211_radar_event(prGlueInfo->prP2PInfo[ucRoleIndex]->prWdev->wiphy,
				prGlueInfo->prP2PInfo[ucRoleIndex]->chandef, GFP_KERNEL);
		DBGLOG(INIT, INFO, "kalP2PRddDetectUpdate: Update to OS Done\n");

		netif_device_detach(prGlueInfo->
				prP2PInfo[ucRoleIndex]->prDevHandler);
		prGlueInfo->prP2PInfo[ucRoleIndex]->fgIsNetDevDetach = TRUE;

	} while (FALSE);

}				/* kalP2PRddDetectUpdate */

VOID kalP2PCacFinishedUpdate(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRoleIndex)
{
	DBGLOG(INIT, INFO, "CAC Finished event\n");

	if (prGlueInfo == NULL)
		return;

	if (prGlueInfo->prP2PInfo[ucRoleIndex]->chandef == NULL)
		return;

	DBGLOG(INIT, INFO, "kalP2PCacFinishedUpdate: Update to OS\n");
#if KERNEL_VERSION(3, 14, 0) <= CFG80211_VERSION_CODE
	cfg80211_cac_event(prGlueInfo->prP2PInfo[ucRoleIndex]->prDevHandler,
				prGlueInfo->prP2PInfo[ucRoleIndex]->chandef,
				NL80211_RADAR_CAC_FINISHED, GFP_KERNEL);
#else
	cfg80211_cac_event(prGlueInfo->prP2PInfo[ucRoleIndex]->prDevHandler,
				NL80211_RADAR_CAC_FINISHED, GFP_KERNEL);
#endif
	DBGLOG(INIT, INFO, "kalP2PCacFinishedUpdate: Update to OS Done\n");
}				/* kalP2PRddDetectUpdate */
#endif

BOOLEAN kalP2pFuncGetChannelType(IN ENUM_CHNL_EXT_T rChnlSco, OUT enum nl80211_channel_type *channel_type)
{
	BOOLEAN fgIsValid = FALSE;

	do {
		if (channel_type) {

			switch (rChnlSco) {
			case CHNL_EXT_SCN:
				*channel_type = NL80211_CHAN_NO_HT;
				break;
			case CHNL_EXT_SCA:
				*channel_type = NL80211_CHAN_HT40MINUS;
				break;
			case CHNL_EXT_SCB:
				*channel_type = NL80211_CHAN_HT40PLUS;
				break;
			default:
				ASSERT(FALSE);
				*channel_type = NL80211_CHAN_NO_HT;
				break;
			}

		}

		fgIsValid = TRUE;
	} while (FALSE);

	return fgIsValid;
}				/* kalP2pFuncGetChannelType */

struct ieee80211_channel *kalP2pFuncGetChannelEntry(IN P_GL_P2P_INFO_T prP2pInfo, IN P_RF_CHANNEL_INFO_T prChannelInfo)
{
	struct ieee80211_channel *prTargetChannelEntry = (struct ieee80211_channel *)NULL;
	struct wiphy *wiphy = (struct wiphy *) NULL;
	UINT_32 u4TblSize = 0, u4Idx = 0;

	if ((prP2pInfo == NULL) || (prChannelInfo == NULL))
		return NULL;

	wiphy = prP2pInfo->prWdev->wiphy;

	do {

		switch (prChannelInfo->eBand) {
		case BAND_2G4:
			prTargetChannelEntry = wiphy->bands[KAL_BAND_2GHZ]->channels;
			u4TblSize = wiphy->bands[KAL_BAND_2GHZ]->n_channels;
			break;
		case BAND_5G:
			prTargetChannelEntry = wiphy->bands[KAL_BAND_5GHZ]->channels;
			u4TblSize = wiphy->bands[KAL_BAND_5GHZ]->n_channels;
			break;
		default:
			break;
		}

		if (prTargetChannelEntry == NULL)
			break;

		for (u4Idx = 0; u4Idx < u4TblSize; u4Idx++, prTargetChannelEntry++) {
			if (prTargetChannelEntry->hw_value == prChannelInfo->ucChannelNum)
				break;

		}

		if (u4Idx == u4TblSize) {
			prTargetChannelEntry = NULL;
			break;
		}

	} while (FALSE);

	return prTargetChannelEntry;
}				/* kalP2pFuncGetChannelEntry */

#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER

/*----------------------------------------------------------------------------*/
/*!
* \brief to set the block list of Hotspot
*
* \param[in]
*           prGlueInfo
*
* \return
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalP2PSetBlackList(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rbssid, IN BOOLEAN fgIsblock,
	IN UINT_8 ucRoleIndex)
{
	UINT_8 aucNullAddr[] = NULL_MAC_ADDR;
	BOOLEAN fgIsValid = FALSE;
	UINT_32 i;

	ASSERT(prGlueInfo);
	/*ASSERT(prGlueInfo->prP2PInfo[ucRoleIndex]);*/


	/*if only one ap mode register, prGlueInfo->prP2PInfo[1] would be null*/
	if (!prGlueInfo->prP2PInfo[ucRoleIndex])
		return fgIsValid;


	if (fgIsblock) {
		for (i = 0; i < P2P_MAXIMUM_CLIENT_COUNT; i++) {
			if (UNEQUAL_MAC_ADDR(rbssid, aucNullAddr)) {
				if (UNEQUAL_MAC_ADDR(&(prGlueInfo->prP2PInfo[ucRoleIndex]->aucblackMACList[i]),
					rbssid)) {
					if (EQUAL_MAC_ADDR(&(prGlueInfo->prP2PInfo[ucRoleIndex]->aucblackMACList[i]),
							aucNullAddr)) {
						COPY_MAC_ADDR(&(prGlueInfo->prP2PInfo[ucRoleIndex]
							->aucblackMACList[i]), rbssid);
						fgIsValid = FALSE;
						return fgIsValid;
					}
				}
			}
		}
	} else {
		for (i = 0; i < P2P_MAXIMUM_CLIENT_COUNT; i++) {
			if (EQUAL_MAC_ADDR(&(prGlueInfo->prP2PInfo[ucRoleIndex]->aucblackMACList[i]), rbssid)) {
				COPY_MAC_ADDR(&(prGlueInfo->prP2PInfo[ucRoleIndex]->aucblackMACList[i]), aucNullAddr);
				fgIsValid = FALSE;
				return fgIsValid;
			}
		}
	}

	return fgIsValid;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief to compare the black list of Hotspot
*
* \param[in]
*           prGlueInfo
*
* \return
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalP2PCmpBlackList(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rbssid, IN UINT_8 ucRoleIndex)
{
	UINT_8 aucNullAddr[] = NULL_MAC_ADDR;
	BOOLEAN fgIsExsit = FALSE;
	UINT_32 i;

	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PInfo[ucRoleIndex]);

	for (i = 0; i < P2P_MAXIMUM_CLIENT_COUNT; i++) {
		if (UNEQUAL_MAC_ADDR(rbssid, aucNullAddr)) {
			if (EQUAL_MAC_ADDR(&(prGlueInfo->prP2PInfo[ucRoleIndex]->aucblackMACList[i]), rbssid)) {
				fgIsExsit = TRUE;
				return fgIsExsit;
			}
		}
	}

	return fgIsExsit;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief to return the max clients of Hotspot
*
* \param[in]
*           prGlueInfo
*
* \return
*/
/*----------------------------------------------------------------------------*/
VOID kalP2PSetMaxClients(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4MaxClient, IN UINT_8 ucRoleIndex)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PInfo[ucRoleIndex]);

	if (u4MaxClient == 0 || prGlueInfo->prP2PInfo[ucRoleIndex]->ucMaxClients >= P2P_MAXIMUM_CLIENT_COUNT)
		prGlueInfo->prP2PInfo[ucRoleIndex]->ucMaxClients = P2P_MAXIMUM_CLIENT_COUNT;
	else
		prGlueInfo->prP2PInfo[ucRoleIndex]->ucMaxClients = u4MaxClient;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to return the max clients of Hotspot
*
* \param[in]
*           prGlueInfo
*
* \return
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalP2PMaxClients(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4NumClient, IN UINT_8 ucRoleIndex)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PInfo[ucRoleIndex]);

	if (prGlueInfo->prP2PInfo[ucRoleIndex]->ucMaxClients) {
		if ((UINT_8) u4NumClient > prGlueInfo->prP2PInfo[ucRoleIndex]->ucMaxClients)
			return TRUE;
		else
			return FALSE;
	}

	return FALSE;
}

#endif
