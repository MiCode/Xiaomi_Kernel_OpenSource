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

/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */
#include "net/cfg80211.h"
#include "precomp.h"
#include "gl_wext.h"

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

struct ieee80211_channel *kalP2pFuncGetChannelEntry(
		IN struct GL_P2P_INFO *prP2pInfo,
		IN struct RF_CHANNEL_INFO *prChannelInfo);

/******************************************************************************
 *                              F U N C T I O N S
 ******************************************************************************
 */

/*---------------------------------------------------------------------------*/
/*!
 * \brief to retrieve Wi-Fi Direct state from glue layer
 *
 * \param[in]
 *           prGlueInfo
 *           rPeerAddr
 * \return
 *           ENUM_BOW_DEVICE_STATE
 */
/*---------------------------------------------------------------------------*/
#if 0
enum ENUM_PARAM_MEDIA_STATE kalP2PGetState(IN struct GLUE_INFO *prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->prP2PInfo[0]->eState;
}				/* end of kalP2PGetState() */
#endif
/*---------------------------------------------------------------------------*/
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
/*---------------------------------------------------------------------------*/
void
kalP2PUpdateAssocInfo(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t *pucFrameBody,
		IN uint32_t u4FrameBodyLen,
		IN u_int8_t fgReassocRequest,
		IN uint8_t ucBssIndex)
{
	struct BSS_INFO *prBssInfo;
	union iwreq_data wrqu;
	unsigned char *pucExtraInfo = NULL;
	unsigned char *pucDesiredIE = NULL;
/* unsigned char aucExtraInfoBuf[200]; */
	uint8_t *cp;
	struct net_device *prNetdevice = (struct net_device *)NULL;

	memset(&wrqu, 0, sizeof(wrqu));

	if (fgReassocRequest) {
		if (u4FrameBodyLen < 15) {
			/*
			 *  printk(KERN_WARNING
			 *  "frameBodyLen too short:%ld\n", frameBodyLen);
			 */
			return;
		}
	} else {
		if (u4FrameBodyLen < 9) {
			/*
			 *  printk(KERN_WARNING
			 *  "frameBodyLen too short:%ld\n", frameBodyLen);
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
	} else if (wextSrchDesiredWPAIE(cp,
			u4FrameBodyLen, 0x30, &pucDesiredIE)) {
		/* printk("wextSrchDesiredWPAIE!!\n"); */
		/* RSN IE found */
	} else if (wextSrchDesiredWPAIE(cp,
			u4FrameBodyLen, 0xDD, &pucDesiredIE)) {
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

	if (ucBssIndex == prGlueInfo->prAdapter->ucP2PDevBssIdx)
		prNetdevice = prGlueInfo->prP2PInfo
			[prBssInfo->u4PrivateData]->prDevHandler;
	else
		prNetdevice = prGlueInfo->prP2PInfo
			[prBssInfo->u4PrivateData]->aprRoleHandler;

	/* Send event to user space */
	wireless_send_event(prNetdevice, IWEVASSOCREQIE, &wrqu, pucExtraInfo);
}

/*---------------------------------------------------------------------------*/
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
/*---------------------------------------------------------------------------*/
#if 0
void
kalP2PSetState(IN struct GLUE_INFO *prGlueInfo,
		IN enum ENUM_PARAM_MEDIA_STATE eState,
		IN uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN],
		IN uint8_t ucRole)
{
	union iwreq_data evt;
	uint8_t aucBuffer[IW_CUSTOM_MAX];

	ASSERT(prGlueInfo);

	memset(&evt, 0, sizeof(evt));

	if (eState == MEDIA_STATE_CONNECTED) {
		prGlueInfo->prP2PInfo[0]->eState = MEDIA_STATE_CONNECTED;

		snprintf(aucBuffer, IW_CUSTOM_MAX - 1,
			"P2P_STA_CONNECT=" MACSTR, MAC2STR(rPeerAddr));
		evt.data.length = strlen(aucBuffer);

		/* indicate in IWECUSTOM event */
		wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler,
			IWEVCUSTOM, &evt, aucBuffer);

	} else if (eState == MEDIA_STATE_DISCONNECTED) {
		snprintf(aucBuffer, IW_CUSTOM_MAX - 1,
			"P2P_STA_DISCONNECT=" MACSTR, MAC2STR(rPeerAddr));
		evt.data.length = strlen(aucBuffer);

		/* indicate in IWECUSTOM event */
		wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler,
			IWEVCUSTOM, &evt, aucBuffer);
	} else {
		ASSERT(0);
	}

}				/* end of kalP2PSetState() */
#endif
/*---------------------------------------------------------------------------*/
/*!
 * \brief to retrieve Wi-Fi Direct operating frequency
 *
 * \param[in]
 *           prGlueInfo
 *
 * \return
 *           in unit of KHz
 */
/*---------------------------------------------------------------------------*/
#if 0
uint32_t kalP2PGetFreqInKHz(IN struct GLUE_INFO *prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->prP2PInfo[0]->u4FreqInKHz;
}				/* end of kalP2PGetFreqInKHz() */
#endif

/*---------------------------------------------------------------------------*/
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
uint8_t kalP2PGetRole(IN struct GLUE_INFO *prGlueInfo, IN uint8_t ucRoleIdx)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->prP2PInfo[ucRoleIdx]->ucRole;
}				/* end of kalP2PGetRole() */

/*---------------------------------------------------------------------------*/
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
/*---------------------------------------------------------------------------*/
#if 1
void kalP2PSetRole(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRole, IN uint8_t ucRoleIdx)
{
	ASSERT(prGlueInfo);
	ASSERT(ucRole <= 2);

	prGlueInfo->prP2PInfo[ucRoleIdx]->ucRole = ucRole;
	/* Remove non-used code */
}				/* end of kalP2PSetRole() */

#else
void
kalP2PSetRole(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucResult, IN uint8_t *pucSSID,
		IN uint8_t ucSSIDLen, IN uint8_t ucRole)
{
	union iwreq_data evt;
	uint8_t aucBuffer[IW_CUSTOM_MAX];

	ASSERT(prGlueInfo);
	ASSERT(ucRole <= 2);

	memset(&evt, 0, sizeof(evt));

	if (ucResult == 0)
		prGlueInfo->prP2PInfo[0]->ucRole = ucRole;

	if (pucSSID)
		snprintf(aucBuffer, IW_CUSTOM_MAX - 1,
			"P2P_FORMATION_RST=%d%d%d%c%c", ucResult,
			ucRole, 1 /* persistence or not */,
			pucSSID[7], pucSSID[8]);
	else
		snprintf(aucBuffer, IW_CUSTOM_MAX - 1,
			"P2P_FORMATION_RST=%d%d%d%c%c", ucResult,
			ucRole, 1 /* persistence or not */, '0', '0');

	evt.data.length = strlen(aucBuffer);

	/* if (pucSSID) */
	/* printk("P2P GO SSID DIRECT-%c%c\n", pucSSID[7], pucSSID[8]); */

	/* indicate in IWECUSTOM event */
	wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler,
		IWEVCUSTOM, &evt, aucBuffer);

}				/* end of kalP2PSetRole() */

#endif
/*---------------------------------------------------------------------------*/
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
/*---------------------------------------------------------------------------*/
void kalP2PSetCipher(IN struct GLUE_INFO *prGlueInfo,
		IN uint32_t u4Cipher, IN uint8_t ucRoleIdx)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PInfo[ucRoleIdx]);

	/* It can be WEP40 (used to identify cipher is WEP), TKIP and CCMP */
	prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise = u4Cipher;

}

/*---------------------------------------------------------------------------*/
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
/*---------------------------------------------------------------------------*/
u_int8_t kalP2PGetCipher(IN struct GLUE_INFO *prGlueInfo, IN uint8_t ucRoleIdx)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PInfo[ucRoleIdx]);

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise
		== IW_AUTH_CIPHER_CCMP)
		return TRUE;

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise
		== IW_AUTH_CIPHER_TKIP)
		return TRUE;

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise
		== IW_AUTH_CIPHER_WEP40)
		return TRUE;

	return FALSE;
}

u_int8_t kalP2PGetWepCipher(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIdx)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PInfo[ucRoleIdx]);

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise
		== IW_AUTH_CIPHER_WEP40)
		return TRUE;

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise
		== IW_AUTH_CIPHER_WEP104)
		return TRUE;

	return FALSE;
}

u_int8_t kalP2PGetCcmpCipher(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIdx)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PInfo[ucRoleIdx]);

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise
		== IW_AUTH_CIPHER_CCMP)
		return TRUE;

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise
		== IW_AUTH_CIPHER_TKIP)
		return FALSE;

	return FALSE;
}

u_int8_t kalP2PGetTkipCipher(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIdx)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PInfo[ucRoleIdx]);

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise
		== IW_AUTH_CIPHER_CCMP)
		return FALSE;

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->u4CipherPairwise
		== IW_AUTH_CIPHER_TKIP)
		return TRUE;

	return FALSE;
}

/*---------------------------------------------------------------------------*/
/*!
 * \brief to set the status of WSC
 *
 * \param[in]
 *           prGlueInfo
 *
 * \return
 */
/*---------------------------------------------------------------------------*/
void kalP2PSetWscMode(IN struct GLUE_INFO *prGlueInfo, IN uint8_t ucWscMode)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PDevInfo);

	prGlueInfo->prP2PDevInfo->ucWSCRunning = ucWscMode;
}

/*---------------------------------------------------------------------------*/
/*!
 * \brief to get the status of WSC
 *
 * \param[in]
 *           prGlueInfo
 *
 * \return
 */
/*---------------------------------------------------------------------------*/
uint8_t kalP2PGetWscMode(IN struct GLUE_INFO *prGlueInfo)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PDevInfo);

	return prGlueInfo->prP2PDevInfo->ucWSCRunning;
}

/*---------------------------------------------------------------------------*/
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
/*---------------------------------------------------------------------------*/
uint16_t kalP2PCalWSC_IELen(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucType, IN uint8_t ucRoleIdx)
{
	ASSERT(prGlueInfo);

	ASSERT(ucType < 4);

	return prGlueInfo->prP2PInfo[ucRoleIdx]->u2WSCIELen[ucType];
}

/*---------------------------------------------------------------------------*/
/*!
 * \brief to copy the wsc ie setting from p2p supplicant
 *
 * \param[in]
 *           prGlueInfo
 *
 * \return
 *           The WPS IE length
 */
/*---------------------------------------------------------------------------*/
void kalP2PGenWSC_IE(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucType, IN uint8_t *pucBuffer, IN uint8_t ucRoleIdx)
{
	struct GL_P2P_INFO *prGlP2pInfo = (struct GL_P2P_INFO *) NULL;

	do {
		if ((prGlueInfo == NULL)
			|| (ucType >= 4) || (pucBuffer == NULL))
			break;

		prGlP2pInfo = prGlueInfo->prP2PInfo[ucRoleIdx];

		kalMemCopy(pucBuffer,
			prGlP2pInfo->aucWSCIE[ucType],
			prGlP2pInfo->u2WSCIELen[ucType]);

	} while (FALSE);

}

void kalP2PUpdateWSC_IE(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucType, IN uint8_t *pucBuffer,
		IN uint16_t u2BufferLength, IN uint8_t ucRoleIdx)
{
	struct GL_P2P_INFO *prGlP2pInfo = (struct GL_P2P_INFO *) NULL;

	do {
		if ((prGlueInfo == NULL) || (ucType >= 4)
			|| ((u2BufferLength > 0) && (pucBuffer == NULL)))
			break;

		if (u2BufferLength > 400) {
			log_dbg(P2P, ERROR,
				"Buffer length is not enough, GLUE only 400 bytes but %d received\n",
				u2BufferLength);
			ASSERT(FALSE);
			break;
		}

		prGlP2pInfo = prGlueInfo->prP2PInfo[ucRoleIdx];

		kalMemCopy(prGlP2pInfo->aucWSCIE[ucType],
			pucBuffer, u2BufferLength);

		prGlP2pInfo->u2WSCIELen[ucType] = u2BufferLength;

	} while (FALSE);

}				/* kalP2PUpdateWSC_IE */

uint16_t kalP2PCalP2P_IELen(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucIndex, IN uint8_t ucRoleIdx)
{
	ASSERT(prGlueInfo);

	ASSERT(ucIndex < MAX_P2P_IE_SIZE);

	return prGlueInfo->prP2PInfo[ucRoleIdx]->u2P2PIELen[ucIndex];
}

void kalP2PGenP2P_IE(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucIndex, IN uint8_t *pucBuffer, IN uint8_t ucRoleIdx)
{
	struct GL_P2P_INFO *prGlP2pInfo = (struct GL_P2P_INFO *) NULL;

	do {
		if ((prGlueInfo == NULL) || (ucIndex >= MAX_P2P_IE_SIZE)
			|| (pucBuffer == NULL))
			break;

		prGlP2pInfo = prGlueInfo->prP2PInfo[ucRoleIdx];

		kalMemCopy(pucBuffer,
			prGlP2pInfo->aucP2PIE[ucIndex],
			prGlP2pInfo->u2P2PIELen[ucIndex]);

	} while (FALSE);
}

void kalP2PUpdateP2P_IE(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucIndex, IN uint8_t *pucBuffer,
		IN uint16_t u2BufferLength, IN uint8_t ucRoleIdx)
{
	struct GL_P2P_INFO *prGlP2pInfo = (struct GL_P2P_INFO *) NULL;

	do {
		if ((prGlueInfo == NULL) ||
			(ucIndex >= MAX_P2P_IE_SIZE) ||
			((u2BufferLength > 0) && (pucBuffer == NULL)))
			break;

		if (u2BufferLength > 400) {
			log_dbg(P2P, ERROR,
			       "kalP2PUpdateP2P_IE > Buffer length is not enough, GLUE only 400 bytes but %d received\n",
					u2BufferLength);
			ASSERT(FALSE);
			break;
		}

		prGlP2pInfo = prGlueInfo->prP2PInfo[ucRoleIdx];

		kalMemCopy(prGlP2pInfo->aucP2PIE[ucIndex],
			pucBuffer, u2BufferLength);

		prGlP2pInfo->u2P2PIELen[ucIndex] = u2BufferLength;

	} while (FALSE);

}

#if 0
/*---------------------------------------------------------------------------*/
/*!
 * \brief indicate an event to supplicant for device connection request
 *
 * \param[in] prGlueInfo Pointer of GLUE_INFO_T
 *
 * \retval none
 */
/*---------------------------------------------------------------------------*/
void kalP2PIndicateConnReq(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t *pucDevName, IN int32_t u4NameLength,
		IN uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN],
		IN uint8_t ucDevType,/* 0: P2P Device / 1: GC / 2: GO */
		IN int32_t i4ConfigMethod, IN int32_t i4ActiveConfigMethod
)
{
	union iwreq_data evt;
	uint8_t aucBuffer[IW_CUSTOM_MAX];

	ASSERT(prGlueInfo);

	/* buffer peer information
	 * for later IOC_P2P_GET_REQ_DEVICE_INFO access
	 */
	prGlueInfo->prP2PInfo[0]->u4ConnReqNameLength =
		u4NameLength > 32 ? 32 : u4NameLength;
	kalMemCopy(prGlueInfo->prP2PInfo[0]->aucConnReqDevName,
		pucDevName,
		prGlueInfo->prP2PInfo[0]->u4ConnReqNameLength);
	COPY_MAC_ADDR(prGlueInfo->prP2PInfo[0]->rConnReqPeerAddr, rPeerAddr);
	prGlueInfo->prP2PInfo[0]->ucConnReqDevType = ucDevType;
	prGlueInfo->prP2PInfo[0]->i4ConnReqConfigMethod = i4ConfigMethod;
	prGlueInfo->prP2PInfo[0]->i4ConnReqActiveConfigMethod =
		i4ActiveConfigMethod;

	/* prepare event structure */
	memset(&evt, 0, sizeof(evt));

	snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_DVC_REQ");
	evt.data.length = strlen(aucBuffer);

	/* indicate in IWEVCUSTOM event */
	wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler,
		IWEVCUSTOM, &evt, aucBuffer);

}				/* end of kalP2PIndicateConnReq() */

/*---------------------------------------------------------------------------*/
/*!
 * \brief Indicate an event to supplicant
 *        for device connection request from other device.
 *
 * \param[in] prGlueInfo Pointer of GLUE_INFO_T
 * \param[in] pucGroupBssid  Only valid when invitation Type equals to 0.
 *
 * \retval none
 */
/*---------------------------------------------------------------------------*/
void
kalP2PInvitationIndication(IN struct GLUE_INFO *prGlueInfo,
		IN struct P2P_DEVICE_DESC *prP2pDevDesc,
		IN uint8_t *pucSsid,
		IN uint8_t ucSsidLen,
		IN uint8_t ucOperatingChnl,
		IN uint8_t ucInvitationType,
		IN uint8_t *pucGroupBssid)
{
#if 1
	union iwreq_data evt;
	uint8_t aucBuffer[IW_CUSTOM_MAX];

	ASSERT(prGlueInfo);

	/* buffer peer information for later IOC_P2P_GET_STRUCT access */
	prGlueInfo->prP2PInfo[0]->u4ConnReqNameLength =
	    (uint32_t) ((prP2pDevDesc->u2NameLength > 32)
	    ? 32 : prP2pDevDesc->u2NameLength);
	kalMemCopy(prGlueInfo->prP2PInfo[0]->aucConnReqDevName,
		prP2pDevDesc->aucName,
		prGlueInfo->prP2PInfo[0]->u4ConnReqNameLength);
	COPY_MAC_ADDR(prGlueInfo->prP2PInfo[0]->rConnReqPeerAddr,
		prP2pDevDesc->aucDeviceAddr);
	COPY_MAC_ADDR(prGlueInfo->prP2PInfo[0]->rConnReqGroupAddr,
		pucGroupBssid);
	prGlueInfo->prP2PInfo[0]->i4ConnReqConfigMethod = (int32_t)
		(prP2pDevDesc->u2ConfigMethod);
	prGlueInfo->prP2PInfo[0]->ucOperatingChnl = ucOperatingChnl;
	prGlueInfo->prP2PInfo[0]->ucInvitationType = ucInvitationType;

	/* prepare event structure */
	memset(&evt, 0, sizeof(evt));

	snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_INV_INDICATE");
	evt.data.length = strlen(aucBuffer);

	/* indicate in IWEVCUSTOM event */
	wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler,
		IWEVCUSTOM, &evt, aucBuffer);

#else
	struct MSG_P2P_CONNECTION_REQUEST *prP2pConnReq =
		(struct MSG_P2P_CONNECTION_REQUEST *) NULL;
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecificBssInfo =
		(struct P2P_SPECIFIC_BSS_INFO *) NULL;
	struct P2P_CONNECTION_SETTINGS *prP2pConnSettings =
		(struct P2P_CONNECTION_SETTINGS *) NULL;

	do {
		ASSERT_BREAK((prGlueInfo != NULL) && (prP2pDevDesc != NULL));

		/* Not a real solution */

		prP2pSpecificBssInfo =
			prGlueInfo->prAdapter->rWifiVar.prP2pSpecificBssInfo;
		prP2pConnSettings =
			prGlueInfo->prAdapter->rWifiVar.prP2PConnSettings;

		prP2pConnReq = (struct MSG_P2P_CONNECTION_REQUEST *)
			cnmMemAlloc(prGlueInfo->prAdapter,
			    RAM_TYPE_MSG,
			    sizeof(struct MSG_P2P_CONNECTION_REQUEST));

		if (prP2pConnReq == NULL)
			break;

		kalMemZero(prP2pConnReq,
			sizeof(struct MSG_P2P_CONNECTION_REQUEST));

		prP2pConnReq->rMsgHdr.eMsgId = MID_MNY_P2P_CONNECTION_REQ;

		prP2pConnReq->eFormationPolicy = ENUM_P2P_FORMATION_POLICY_AUTO;

		COPY_MAC_ADDR(prP2pConnReq->aucDeviceID,
			prP2pDevDesc->aucDeviceAddr);

		prP2pConnReq->u2ConfigMethod = prP2pDevDesc->u2ConfigMethod;

		if (ucInvitationType == P2P_INVITATION_TYPE_INVITATION) {
			prP2pConnReq->fgIsPersistentGroup = FALSE;
			prP2pConnReq->fgIsTobeGO = FALSE;

		}

		else if (ucInvitationType == P2P_INVITATION_TYPE_REINVOKE) {
			DBGLOG(P2P, TRACE, "Re-invoke Persistent Group\n");
			prP2pConnReq->fgIsPersistentGroup = TRUE;
			prP2pConnReq->fgIsTobeGO =
				(prGlueInfo->prP2PInfo[0]->ucRole == 2)
				? TRUE : FALSE;

		}

		p2pFsmRunEventDeviceDiscoveryAbort(prGlueInfo->prAdapter, NULL);

		if (ucOperatingChnl != 0)
			prP2pSpecificBssInfo->ucPreferredChannel =
				ucOperatingChnl;

		if ((ucSsidLen < 32) && (pucSsid != NULL))
			COPY_SSID(prP2pConnSettings->aucSSID,
				prP2pConnSettings->ucSSIDLen,
				pucSsid, ucSsidLen);

		mboxSendMsg(prGlueInfo->prAdapter,
			MBOX_ID_0,
			(struct MSG_HDR *) prP2pConnReq,
			MSG_SEND_METHOD_BUF);

	} while (FALSE);

	/* frog add. */
	/* TODO: Invitation Indication */

#endif

}				/* kalP2PInvitationIndication */
#endif

#if 0
/*---------------------------------------------------------------------------*/
/*!
 * \brief Indicate an status to supplicant for device invitation status.
 *
 * \param[in] prGlueInfo Pointer of GLUE_INFO_T
 *
 * \retval none
 */
/*---------------------------------------------------------------------------*/
void kalP2PInvitationStatus(IN struct GLUE_INFO *prGlueInfo,
		IN uint32_t u4InvStatus)
{
	union iwreq_data evt;
	uint8_t aucBuffer[IW_CUSTOM_MAX];

	ASSERT(prGlueInfo);

	/* buffer peer information for later IOC_P2P_GET_STRUCT access */
	prGlueInfo->prP2PInfo[0]->u4InvStatus = u4InvStatus;

	/* prepare event structure */
	memset(&evt, 0, sizeof(evt));

	snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_INV_STATUS");
	evt.data.length = strlen(aucBuffer);

	/* indicate in IWEVCUSTOM event */
	wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler,
		IWEVCUSTOM, &evt, aucBuffer);

}				/* kalP2PInvitationStatus */
#endif

/*---------------------------------------------------------------------------*/
/*!
 * \brief Indicate an event to supplicant
 *         for Service Discovery request from other device.
 *
 * \param[in] prGlueInfo Pointer of GLUE_INFO_T
 *
 * \retval none
 */
/*---------------------------------------------------------------------------*/
void kalP2PIndicateSDRequest(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN], IN uint8_t ucSeqNum)
{
	union iwreq_data evt;
	uint8_t aucBuffer[IW_CUSTOM_MAX];
	int32_t i4Ret = 0;

	ASSERT(prGlueInfo);

	memset(&evt, 0, sizeof(evt));

	i4Ret =
		snprintf(aucBuffer, IW_CUSTOM_MAX - 1,
		"P2P_SD_REQ %d", ucSeqNum);
	if (i4Ret < 0) {
		DBGLOG(INIT, WARN, "sprintf failed:%d\n", i4Ret);
		return;
	}

	evt.data.length = strlen(aucBuffer);

	/* indicate IWEVP2PSDREQ event */
	wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler,
		IWEVCUSTOM, &evt, aucBuffer);
}				/* end of kalP2PIndicateSDRequest() */

/*---------------------------------------------------------------------------*/
/*!
 * \brief Indicate an event to supplicant for Service Discovery response
 *         from other device.
 *
 * \param[in] prGlueInfo Pointer of GLUE_INFO_T
 *
 * \retval none
 */
/*---------------------------------------------------------------------------*/
void kalP2PIndicateSDResponse(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN], IN uint8_t ucSeqNum)
{
	union iwreq_data evt;
	uint8_t aucBuffer[IW_CUSTOM_MAX];
	int32_t i4Ret = 0;

	ASSERT(prGlueInfo);

	memset(&evt, 0, sizeof(evt));

	i4Ret =
		snprintf(aucBuffer, IW_CUSTOM_MAX - 1,
		"P2P_SD_RESP %d", ucSeqNum);
	if (i4Ret < 0) {
		DBGLOG(INIT, WARN, "sprintf failed:%d\n", i4Ret);
		return;
	}
	evt.data.length = strlen(aucBuffer);

	/* indicate IWEVP2PSDREQ event */
	wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler,
		IWEVCUSTOM, &evt, aucBuffer);
}				/* end of kalP2PIndicateSDResponse() */

/*---------------------------------------------------------------------------*/
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
/*---------------------------------------------------------------------------*/
void kalP2PIndicateTXDone(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucSeqNum, IN uint8_t ucStatus)
{
	union iwreq_data evt;
	uint8_t aucBuffer[IW_CUSTOM_MAX];
	int32_t i4Ret = 0;

	ASSERT(prGlueInfo);

	memset(&evt, 0, sizeof(evt));

	i4Ret =
		snprintf(aucBuffer, IW_CUSTOM_MAX - 1,
		"P2P_SD_XMITTED: %d %d", ucSeqNum, ucStatus);
	if (i4Ret < 0) {
		DBGLOG(INIT, WARN, "sprintf failed:%d\n", i4Ret);
		return;
	}
	evt.data.length = strlen(aucBuffer);

	/* indicate IWEVP2PSDREQ event */
	wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler,
		IWEVCUSTOM, &evt, aucBuffer);
}				/* end of kalP2PIndicateSDResponse() */

struct net_device *kalP2PGetDevHdlr(struct GLUE_INFO *prGlueInfo)
{
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PInfo[0]);
	return prGlueInfo->prP2PInfo[0]->prDevHandler;
}

#if CFG_SUPPORT_ANTI_PIRACY
#if 0
/*---------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T
 *
 * \return none
 */
/*---------------------------------------------------------------------------*/
void kalP2PIndicateSecCheckRsp(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t *pucRsp, IN uint16_t u2RspLen)
{
	union iwreq_data evt;
	uint8_t aucBuffer[IW_CUSTOM_MAX];

	ASSERT(prGlueInfo);

	memset(&evt, 0, sizeof(evt));
	snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_SEC_CHECK_RSP=");

	kalMemCopy(prGlueInfo->prP2PInfo[0]->aucSecCheckRsp, pucRsp, u2RspLen);
	evt.data.length = strlen(aucBuffer);

#if DBG
	DBGLOG_MEM8(SEC, LOUD,
		prGlueInfo->prP2PInfo[0]->aucSecCheckRsp, u2RspLen);
#endif
	/* indicate in IWECUSTOM event */
	wireless_send_event(prGlueInfo->prP2PInfo[0]->prDevHandler,
		IWEVCUSTOM, &evt, aucBuffer);
}				/* p2pFsmRunEventRxDisassociation */
#endif
#endif

/*---------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T
 *
 * \return none
 */
/*---------------------------------------------------------------------------*/
void
kalGetChnlList(IN struct GLUE_INFO *prGlueInfo,
		IN enum ENUM_BAND eSpecificBand,
		IN uint8_t ucMaxChannelNum,
		IN uint8_t *pucNumOfChannel,
		IN struct RF_CHANNEL_INFO *paucChannelList)
{
	rlmDomainGetChnlList(prGlueInfo->prAdapter, eSpecificBand,
		FALSE, ucMaxChannelNum, pucNumOfChannel, paucChannelList);
}				/* kalGetChnlList */

/* ////////////////////////////ICS SUPPORT////////////////////////////// */

void
kalP2PIndicateChannelReady(IN struct GLUE_INFO *prGlueInfo,
		IN uint64_t u8SeqNum,
		IN uint32_t u4ChannelNum,
		IN enum ENUM_BAND eBand,
		IN enum ENUM_CHNL_EXT eSco,
		IN uint32_t u4Duration)
{
	struct ieee80211_channel *prIEEE80211ChnlStruct =
		(struct ieee80211_channel *)NULL;
	struct RF_CHANNEL_INFO rChannelInfo;
	enum nl80211_channel_type eChnlType = NL80211_CHAN_NO_HT;

	do {
		if (prGlueInfo == NULL)
			break;

		kalMemZero(&rChannelInfo, sizeof(struct RF_CHANNEL_INFO));

		rChannelInfo.ucChannelNum = u4ChannelNum;
		rChannelInfo.eBand = eBand;

		prIEEE80211ChnlStruct =
			kalP2pFuncGetChannelEntry(prGlueInfo->prP2PInfo[0],
				&rChannelInfo);

		kalP2pFuncGetChannelType(eSco, &eChnlType);

		if (!prIEEE80211ChnlStruct) {
			DBGLOG(P2P, WARN, "prIEEE80211ChnlStruct is NULL\n");
			break;
		}

		cfg80211_ready_on_channel(
			/* struct wireless_dev, */
			prGlueInfo->prP2PInfo[0]->prWdev,
			/* u64 cookie, */
			u8SeqNum,
			/* struct ieee80211_channel * chan, */
			prIEEE80211ChnlStruct,
			/* unsigned int duration, */
			u4Duration,
			/* gfp_t gfp *//* allocation flags */
			GFP_KERNEL);
	} while (FALSE);

}				/* kalP2PIndicateChannelReady */

void
kalP2PIndicateChannelExpired(IN struct GLUE_INFO *prGlueInfo,
		IN uint64_t u8SeqNum,
		IN uint32_t u4ChannelNum,
		IN enum ENUM_BAND eBand,
		IN enum ENUM_CHNL_EXT eSco)
{

	struct GL_P2P_INFO *prGlueP2pInfo = (struct GL_P2P_INFO *) NULL;
	struct ieee80211_channel *prIEEE80211ChnlStruct =
		(struct ieee80211_channel *)NULL;
	enum nl80211_channel_type eChnlType = NL80211_CHAN_NO_HT;
	struct RF_CHANNEL_INFO rRfChannelInfo;

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

		prIEEE80211ChnlStruct =
			kalP2pFuncGetChannelEntry(prGlueP2pInfo,
				&rRfChannelInfo);

		kalP2pFuncGetChannelType(eSco, &eChnlType);

		if (!prIEEE80211ChnlStruct) {
			DBGLOG(P2P, WARN, "prIEEE80211ChnlStruct is NULL\n");
			break;
		}

		/* struct wireless_dev, */
		cfg80211_remain_on_channel_expired(prGlueP2pInfo->prWdev,
			u8SeqNum, prIEEE80211ChnlStruct, GFP_KERNEL);
	} while (FALSE);

}				/* kalP2PIndicateChannelExpired */

void kalP2PIndicateScanDone(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIndex, IN u_int8_t fgIsAbort)
{
	struct GL_P2P_DEV_INFO *prP2pGlueDevInfo =
		(struct GL_P2P_DEV_INFO *) NULL;
	struct GL_P2P_INFO *prGlueP2pInfo = (struct GL_P2P_INFO *) NULL;
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

		DBGLOG(INIT, INFO,
			"[p2p] scan complete %p\n",
			prP2pGlueDevInfo->prScanRequest);

		KAL_ACQUIRE_MUTEX(prGlueInfo->prAdapter, MUTEX_DEL_INF);
		GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

		if (prP2pGlueDevInfo->prScanRequest != NULL) {
			prScanRequest = prP2pGlueDevInfo->prScanRequest;
			kalCfg80211ScanDone(prScanRequest, fgIsAbort);
			prP2pGlueDevInfo->prScanRequest = NULL;
		}
		GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

		if ((prScanRequest != NULL)
			&& (prGlueInfo->prAdapter->fgIsP2PRegistered == TRUE)) {

			/* report all queued beacon/probe response frames
			 * to upper layer
			 */
			scanReportBss2Cfg80211(prGlueInfo->prAdapter,
				BSS_TYPE_P2P_DEVICE, NULL);

			DBGLOG(INIT, TRACE, "DBG:p2p_cfg_scan_done\n");
		}
		KAL_RELEASE_MUTEX(prGlueInfo->prAdapter, MUTEX_DEL_INF);

	} while (FALSE);

}				/* kalP2PIndicateScanDone */

void
kalP2PIndicateBssInfo(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t *pucFrameBuf,
		IN uint32_t u4BufLen,
		IN struct RF_CHANNEL_INFO *prChannelInfo,
		IN int32_t i4SignalStrength)
{
	struct GL_P2P_INFO *prGlueP2pInfo = (struct GL_P2P_INFO *) NULL;
	struct ieee80211_channel *prChannelEntry =
		(struct ieee80211_channel *)NULL;
	struct ieee80211_mgmt *prBcnProbeRspFrame =
		(struct ieee80211_mgmt *)pucFrameBuf;
	struct cfg80211_bss *prCfg80211Bss = (struct cfg80211_bss *)NULL;

	do {
		if ((prGlueInfo == NULL) || (pucFrameBuf == NULL)
			|| (prChannelInfo == NULL)) {
			ASSERT(FALSE);
			break;
		}

		prGlueP2pInfo = prGlueInfo->prP2PInfo[0];

		if (prGlueP2pInfo == NULL) {
			ASSERT(FALSE);
			break;
		}

		prChannelEntry =
			kalP2pFuncGetChannelEntry(prGlueP2pInfo,
				prChannelInfo);

		if (prChannelEntry == NULL) {
			DBGLOG(P2P, TRACE, "Unknown channel info\n");
			break;
		}

		/* rChannelInfo.center_freq =
		 * nicChannelNum2Freq((UINT_32)prChannelInfo->ucChannelNum)
		 * / 1000;
		 */

		if (u4BufLen > 0) {
#if CFG_SUPPORT_TSF_USING_BOOTTIME
			prBcnProbeRspFrame->u.beacon.timestamp =
				kalGetBootTime();
#endif
			prCfg80211Bss = cfg80211_inform_bss_frame(
				/* struct wiphy * wiphy, */
				prGlueP2pInfo->prWdev->wiphy,
				prChannelEntry,
				prBcnProbeRspFrame,
				u4BufLen, i4SignalStrength, GFP_KERNEL);
		}

		/* Return this structure. */
		if (prCfg80211Bss)
			cfg80211_put_bss(prGlueP2pInfo->prWdev->wiphy,
				prCfg80211Bss);
		else
			DBGLOG(P2P, WARN,
				"indicate BSS to cfg80211 failed [" MACSTR
				"]: bss channel %d, rcpi %d, frame_len=%d\n",
				MAC2STR(prBcnProbeRspFrame->bssid),
				prChannelInfo->ucChannelNum,
				i4SignalStrength, u4BufLen);

	} while (FALSE);

	return;

}				/* kalP2PIndicateBssInfo */

void kalP2PIndicateMgmtTxStatus(IN struct GLUE_INFO *prGlueInfo,
		IN struct MSDU_INFO *prMsduInfo, IN u_int8_t fgIsAck)
{
	struct GL_P2P_INFO *prGlueP2pInfo = (struct GL_P2P_INFO *) NULL;
	uint64_t *pu8GlCookie = (uint64_t *) NULL;
	struct net_device *prNetdevice = (struct net_device *)NULL;

	do {
		if ((prGlueInfo == NULL) || (prMsduInfo == NULL)) {
			DBGLOG(P2P, WARN,
				"Unexpected pointer PARAM. 0x%lx, 0x%lx.\n",
				prGlueInfo, prMsduInfo);
			ASSERT(FALSE);
			break;
		}

		pu8GlCookie =
		    (uint64_t *) ((unsigned long) prMsduInfo->prPacket +
				(unsigned long) prMsduInfo->u2FrameLength +
				MAC_TX_RESERVED_FIELD);

		if (prMsduInfo->ucBssIndex
			== prGlueInfo->prAdapter->ucP2PDevBssIdx) {

			prGlueP2pInfo = prGlueInfo->prP2PInfo[0];

			if (prGlueP2pInfo == NULL)
				return;

			prNetdevice = prGlueP2pInfo->prDevHandler;

		} else {
			struct BSS_INFO *prP2pBssInfo =
				GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter,
				prMsduInfo->ucBssIndex);

			prGlueP2pInfo =
				prGlueInfo->prP2PInfo
					[prP2pBssInfo->u4PrivateData];

			if (prGlueP2pInfo == NULL)
				return;

			prNetdevice = prGlueP2pInfo->aprRoleHandler;
		}

		cfg80211_mgmt_tx_status(
			/* struct net_device * dev, */
			prNetdevice->ieee80211_ptr,
			*pu8GlCookie,
			(uint8_t *) ((unsigned long) prMsduInfo->prPacket +
			MAC_TX_RESERVED_FIELD),
			prMsduInfo->u2FrameLength, fgIsAck, GFP_KERNEL);

	} while (FALSE);

}				/* kalP2PIndicateMgmtTxStatus */

void
kalP2PIndicateRxMgmtFrame(IN struct GLUE_INFO *prGlueInfo,
		IN struct SW_RFB *prSwRfb,
		IN u_int8_t fgIsDevInterface,
		IN uint8_t ucRoleIdx)
{
#define DBG_P2P_MGMT_FRAME_INDICATION 1
	struct GL_P2P_INFO *prGlueP2pInfo = (struct GL_P2P_INFO *) NULL;
	int32_t i4Freq = 0;
	uint8_t ucChnlNum = 0;
#if DBG_P2P_MGMT_FRAME_INDICATION
	struct WLAN_MAC_HEADER *prWlanHeader = (struct WLAN_MAC_HEADER *) NULL;
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

		ucChnlNum = prSwRfb->ucChnlNum;

#if DBG_P2P_MGMT_FRAME_INDICATION

		prWlanHeader = (struct WLAN_MAC_HEADER *) prSwRfb->pvHeader;

		switch (prWlanHeader->u2FrameCtrl) {
		case MAC_FRAME_PROBE_REQ:
			DBGLOG(P2P, TRACE,
				"RX Probe Req at channel %d ",
				ucChnlNum);
			break;
		case MAC_FRAME_PROBE_RSP:
			DBGLOG(P2P, TRACE,
				"RX Probe Rsp at channel %d ",
				ucChnlNum);
			break;
		case MAC_FRAME_ACTION:
			DBGLOG(P2P, TRACE,
				"RX Action frame at channel %d ",
				ucChnlNum);
			break;
		default:
			DBGLOG(P2P, TRACE,
				"RX Packet:%d at channel %d ",
				prWlanHeader->u2FrameCtrl, ucChnlNum);
			break;
		}
#endif
		i4Freq = nicChannelNum2Freq(ucChnlNum) / 1000;

		if (fgIsDevInterface)
			prNetdevice = prGlueP2pInfo->prDevHandler;
		else
			prNetdevice = prGlueP2pInfo->aprRoleHandler;

		DBGLOG(P2P, TRACE, "from: " MACSTR ", netdev: %p\n",
				MAC2STR(prWlanHeader->aucAddr2),
				prNetdevice);

		if (!prGlueInfo->fgIsRegistered ||
			(prNetdevice == NULL) ||
			(prNetdevice->ieee80211_ptr == NULL)) {
			DBGLOG(P2P, WARN,
				"prNetdevice is not ready or NULL!\n");
			break;
		}

#if (KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE)
		cfg80211_rx_mgmt(
			/* struct net_device * dev, */
			prNetdevice->ieee80211_ptr,
			i4Freq,
			RCPI_TO_dBm(
				nicRxGetRcpiValueFromRxv(prGlueInfo->prAdapter,
				RCPI_MODE_MAX,
				prSwRfb)),
			prSwRfb->pvHeader,
			prSwRfb->u2PacketLen,
			NL80211_RXMGMT_FLAG_ANSWERED);
#elif (KERNEL_VERSION(3, 12, 0) <= CFG80211_VERSION_CODE)
		cfg80211_rx_mgmt(
			/* struct net_device * dev, */
			prNetdevice->ieee80211_ptr,
			i4Freq,
			RCPI_TO_dBm(
				nicRxGetRcpiValueFromRxv(prGlueInfo->prAdapter,
				RCPI_MODE_WF0,
				prSwRfb)),
			prSwRfb->pvHeader,
			prSwRfb->u2PacketLen,
			NL80211_RXMGMT_FLAG_ANSWERED,
			GFP_ATOMIC);
#else
		cfg80211_rx_mgmt(
			/* struct net_device * dev, */
			prNetdevice->ieee80211_ptr,
			i4Freq,
			RCPI_TO_dBm(
				nicRxGetRcpiValueFromRxv(prGlueInfo->prAdapter,
				RCPI_MODE_WF0,
				prSwRfb)),
			prSwRfb->pvHeader,
			prSwRfb->u2PacketLen,
			GFP_ATOMIC);
#endif


	} while (FALSE);

}				/* kalP2PIndicateRxMgmtFrame */

#if CFG_WPS_DISCONNECT || (KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE)
void
kalP2PGCIndicateConnectionStatus(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIndex,
		IN struct P2P_CONNECTION_REQ_INFO *prP2pConnInfo,
		IN uint8_t *pucRxIEBuf,
		IN uint16_t u2RxIELen,
		IN uint16_t u2StatusReason,
		IN uint32_t eStatus)
{
	struct GL_P2P_INFO *prGlueP2pInfo = (struct GL_P2P_INFO *) NULL;
	struct ADAPTER *prAdapter = NULL;

	do {
		if (prGlueInfo == NULL) {
			ASSERT(FALSE);
			break;
		}

		prAdapter = prGlueInfo->prAdapter;
		prGlueP2pInfo = prGlueInfo->prP2PInfo[ucRoleIndex];

		/* FIXME: This exception occurs at wlanRemove. */
		if ((prGlueP2pInfo == NULL) ||
		    (prGlueP2pInfo->aprRoleHandler == NULL) ||
		    (prAdapter->rP2PNetRegState !=
				ENUM_NET_REG_STATE_REGISTERED) ||
		    ((prGlueInfo->ulFlag & GLUE_FLAG_HALT) == 1)) {
			break;
		}

		if (prP2pConnInfo) {
			/* switch netif on */
			netif_carrier_on(prGlueP2pInfo->aprRoleHandler);

			cfg80211_connect_result(prGlueP2pInfo->aprRoleHandler,
				/* struct net_device * dev, */
				prP2pConnInfo->aucBssid,
				prP2pConnInfo->aucIEBuf,
				prP2pConnInfo->u4BufLength,
				pucRxIEBuf, u2RxIELen,
				u2StatusReason,
				/* gfp_t gfp *//* allocation flags */
				GFP_KERNEL);

			prP2pConnInfo->eConnRequest = P2P_CONNECTION_TYPE_IDLE;
		} else {
			/* Disconnect, what if u2StatusReason == 0? */
			cfg80211_disconnected(prGlueP2pInfo->aprRoleHandler,
				/* struct net_device * dev, */
				u2StatusReason,
				pucRxIEBuf, u2RxIELen,
				eStatus == WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY,
				GFP_KERNEL);
		}

	} while (FALSE);

}				/* kalP2PGCIndicateConnectionStatus */

#else
void
kalP2PGCIndicateConnectionStatus(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIndex,
		IN struct P2P_CONNECTION_REQ_INFO *prP2pConnInfo,
		IN uint8_t *pucRxIEBuf,
		IN uint16_t u2RxIELen,
		IN uint16_t u2StatusReason)
{
	struct GL_P2P_INFO *prGlueP2pInfo = (struct GL_P2P_INFO *) NULL;
	struct ADAPTER *prAdapter = NULL;

	do {
		if (prGlueInfo == NULL) {
			ASSERT(FALSE);
			break;
		}

		prAdapter = prGlueInfo->prAdapter;
		prGlueP2pInfo = prGlueInfo->prP2PInfo[ucRoleIndex];

		/* FIXME: This exception occurs at wlanRemove. */
		if ((prGlueP2pInfo == NULL) ||
		    (prGlueP2pInfo->aprRoleHandler == NULL) ||
		    (prAdapter->rP2PNetRegState !=
				ENUM_NET_REG_STATE_REGISTERED) ||
		    ((prGlueInfo->ulFlag & GLUE_FLAG_HALT) == 1)) {
			break;
		}

		if (prP2pConnInfo) {
			/* switch netif on */
			netif_carrier_on(prGlueP2pInfo->aprRoleHandler);

			cfg80211_connect_result(prGlueP2pInfo->aprRoleHandler,
				/* struct net_device * dev, */
				prP2pConnInfo->aucBssid,
				prP2pConnInfo->aucIEBuf,
				prP2pConnInfo->u4BufLength,
				pucRxIEBuf, u2RxIELen,
				u2StatusReason,
				/* gfp_t gfp *//* allocation flags */
				GFP_KERNEL);

			prP2pConnInfo->eConnRequest = P2P_CONNECTION_TYPE_IDLE;
		} else {
			/* Disconnect, what if u2StatusReason == 0? */
			cfg80211_disconnected(prGlueP2pInfo->aprRoleHandler,
				/* struct net_device * dev, */
				u2StatusReason, pucRxIEBuf,
				u2RxIELen, GFP_KERNEL);
		}

	} while (FALSE);

}				/* kalP2PGCIndicateConnectionStatus */

#endif

void
kalP2PGOStationUpdate(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIndex,
		IN struct STA_RECORD *prCliStaRec,
		IN u_int8_t fgIsNew)
{
	struct GL_P2P_INFO *prP2pGlueInfo = (struct GL_P2P_INFO *) NULL;

	do {
		if ((prGlueInfo == NULL) || (prCliStaRec == NULL)
			|| (ucRoleIndex >= 2))
			break;

		prP2pGlueInfo = prGlueInfo->prP2PInfo[ucRoleIndex];

		if ((prP2pGlueInfo == NULL) ||
		    (prP2pGlueInfo->aprRoleHandler == NULL)) {
			/* This case may occur when the usb is unplugged */
			break;
		}

		if (fgIsNew) {
			struct station_info rStationInfo;

			if (prCliStaRec->fgIsConnected == TRUE)
				break;
			prCliStaRec->fgIsConnected = TRUE;

			kalMemZero(&rStationInfo, sizeof(rStationInfo));

#if KERNEL_VERSION(4, 0, 0) > CFG80211_VERSION_CODE
			rStationInfo.filled = STATION_INFO_ASSOC_REQ_IES;
#endif
			rStationInfo.generation = ++prP2pGlueInfo->i4Generation;

			rStationInfo.assoc_req_ies = prCliStaRec->pucAssocReqIe;
			rStationInfo.assoc_req_ies_len =
				prCliStaRec->u2AssocReqIeLen;

			cfg80211_new_sta(prP2pGlueInfo->aprRoleHandler,
				/* struct net_device * dev, */
				prCliStaRec->aucMacAddr,
				&rStationInfo, GFP_KERNEL);
		} else {
			++prP2pGlueInfo->i4Generation;

			/* FIXME: The exception occurs at wlanRemove, and
			 *    check GLUE_FLAG_HALT is the temporarily solution.
			 */
			if ((prGlueInfo->ulFlag & GLUE_FLAG_HALT) == 0) {
				if (prCliStaRec->fgIsConnected == FALSE)
					break;
				prCliStaRec->fgIsConnected = FALSE;
				cfg80211_del_sta(prP2pGlueInfo->aprRoleHandler,
					/* struct net_device * dev, */
					prCliStaRec->aucMacAddr, GFP_KERNEL);
			}
		}

	} while (FALSE);

	return;

}				/* kalP2PGOStationUpdate */

#if (CFG_SUPPORT_DFS_MASTER == 1)
void kalP2PRddDetectUpdate(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIndex)
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

		/* cac start disable for next cac slot
		 * if enable in dfs channel
		 */
		prGlueInfo->prP2PInfo[ucRoleIndex]->prWdev->cac_started = FALSE;
		DBGLOG(INIT, INFO,
			"kalP2PRddDetectUpdate: Update to OS\n");
		cfg80211_radar_event(
			prGlueInfo->prP2PInfo[ucRoleIndex]->prWdev->wiphy,
			prGlueInfo->prP2PInfo[ucRoleIndex]->chandef,
			GFP_KERNEL);
		DBGLOG(INIT, INFO,
			"kalP2PRddDetectUpdate: Update to OS Done\n");

		netif_carrier_off(
			prGlueInfo->prP2PInfo[ucRoleIndex]->prDevHandler);
		netif_tx_stop_all_queues(
			prGlueInfo->prP2PInfo[ucRoleIndex]->prDevHandler);

		if (prGlueInfo->prP2PInfo[ucRoleIndex]->chandef->chan)
			cnmMemFree(prGlueInfo->prAdapter,
				prGlueInfo->prP2PInfo
					[ucRoleIndex]->chandef->chan);

		prGlueInfo->prP2PInfo[ucRoleIndex]->chandef->chan = NULL;

		if (prGlueInfo->prP2PInfo[ucRoleIndex]->chandef)
			cnmMemFree(prGlueInfo->prAdapter,
				prGlueInfo->prP2PInfo[ucRoleIndex]->chandef);

		prGlueInfo->prP2PInfo[ucRoleIndex]->chandef = NULL;

	} while (FALSE);

}				/* kalP2PRddDetectUpdate */

void kalP2PCacFinishedUpdate(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIndex)
{
	DBGLOG(INIT, INFO, "CAC Finished event\n");

	do {
		if (prGlueInfo == NULL) {
			ASSERT(FALSE);
			break;
		}

		if (prGlueInfo->prP2PInfo[ucRoleIndex]->chandef == NULL) {
			ASSERT(FALSE);
			break;
		}

		DBGLOG(INIT, INFO, "kalP2PCacFinishedUpdate: Update to OS\n");
#if KERNEL_VERSION(3, 14, 0) <= CFG80211_VERSION_CODE
		cfg80211_cac_event(
			prGlueInfo->prP2PInfo[ucRoleIndex]->prDevHandler,
			prGlueInfo->prP2PInfo[ucRoleIndex]->chandef,
			NL80211_RADAR_CAC_FINISHED, GFP_KERNEL);
#else
		cfg80211_cac_event(
			prGlueInfo->prP2PInfo[ucRoleIndex]->prDevHandler,
			NL80211_RADAR_CAC_FINISHED, GFP_KERNEL);
#endif
		DBGLOG(INIT, INFO,
			"kalP2PCacFinishedUpdate: Update to OS Done\n");

	} while (FALSE);

}				/* kalP2PRddDetectUpdate */
#endif

u_int8_t kalP2pFuncGetChannelType(IN enum ENUM_CHNL_EXT rChnlSco,
		OUT enum nl80211_channel_type *channel_type)
{
	u_int8_t fgIsValid = FALSE;

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

struct ieee80211_channel *kalP2pFuncGetChannelEntry(
		IN struct GL_P2P_INFO *prP2pInfo,
		IN struct RF_CHANNEL_INFO *prChannelInfo)
{
	struct ieee80211_channel *prTargetChannelEntry =
		(struct ieee80211_channel *)NULL;
	uint32_t u4TblSize = 0, u4Idx = 0;
	struct wiphy *wiphy = NULL;

	if ((prP2pInfo == NULL) || (prChannelInfo == NULL))
		return NULL;

	wiphy = prP2pInfo->prWdev->wiphy;

	do {

		switch (prChannelInfo->eBand) {
		case BAND_2G4:
			if (wiphy->bands[KAL_BAND_2GHZ] == NULL)
				DBGLOG(P2P, ERROR,
					"kalP2pFuncGetChannelEntry 2.4G NULL Bands!!\n");
			else {
				prTargetChannelEntry =
					wiphy->bands[KAL_BAND_2GHZ]->channels;
				u4TblSize =
					wiphy->bands[KAL_BAND_2GHZ]->n_channels;
			}
			break;
		case BAND_5G:
			if (wiphy->bands[KAL_BAND_5GHZ] == NULL)
				DBGLOG(P2P, ERROR,
					"kalP2pFuncGetChannelEntry 5G NULL Bands!!\n");
			else {
				prTargetChannelEntry =
					wiphy->bands[KAL_BAND_5GHZ]->channels;
				u4TblSize =
					wiphy->bands[KAL_BAND_5GHZ]->n_channels;
			}
			break;
		default:
			break;
		}

		if (prTargetChannelEntry == NULL)
			break;

		for (u4Idx = 0; u4Idx < u4TblSize
			; u4Idx++, prTargetChannelEntry++) {
			if (prTargetChannelEntry->hw_value
				== prChannelInfo->ucChannelNum)
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

/*---------------------------------------------------------------------------*/
/*!
 * \brief to set the block list of Hotspot
 *
 * \param[in]
 *           prGlueInfo
 *
 * \return
 */
/*---------------------------------------------------------------------------*/
u_int8_t kalP2PSetBlackList(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t rbssid[PARAM_MAC_ADDR_LEN],
		IN u_int8_t fgIsblock,
		IN uint8_t ucRoleIndex)
{
	uint8_t aucNullAddr[] = NULL_MAC_ADDR;
	uint32_t i;

	ASSERT(prGlueInfo);

	/*if only one ap mode register, prGlueInfo->prP2PInfo[1] would be null*/
	if (!prGlueInfo->prP2PInfo[ucRoleIndex])
		return FALSE;

	if (EQUAL_MAC_ADDR(rbssid, aucNullAddr))
		return FALSE;

	if (fgIsblock) {
		for (i = 0; i < P2P_MAXIMUM_CLIENT_COUNT; i++) {
			if (EQUAL_MAC_ADDR(
				&(prGlueInfo->prP2PInfo[ucRoleIndex]
				->aucblackMACList[i]), rbssid)) {
				DBGLOG(P2P, WARN, MACSTR
					" already in black list\n",
					MAC2STR(rbssid));
				return FALSE;
			}
		}
		for (i = 0; i < P2P_MAXIMUM_CLIENT_COUNT; i++) {
			if (EQUAL_MAC_ADDR(
				&(prGlueInfo->prP2PInfo
				[ucRoleIndex]
				->aucblackMACList[i]),
				aucNullAddr)) {
				COPY_MAC_ADDR(
					&(prGlueInfo->prP2PInfo
					[ucRoleIndex]
					->aucblackMACList[i]),
					rbssid);
				return FALSE;
			}
		}
	} else {
		for (i = 0; i < P2P_MAXIMUM_CLIENT_COUNT; i++) {
			if (EQUAL_MAC_ADDR(
					&(prGlueInfo->prP2PInfo[ucRoleIndex]
					->aucblackMACList[i]), rbssid)) {
				COPY_MAC_ADDR(
					&(prGlueInfo->prP2PInfo[ucRoleIndex]
					->aucblackMACList[i]), aucNullAddr);

				return FALSE;
			}
		}
	}

	return FALSE;

}

u_int8_t kalP2PResetBlackList(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIndex)
{
	uint8_t aucNullAddr[] = NULL_MAC_ADDR;
	uint32_t i;

	if (!prGlueInfo || !prGlueInfo->prP2PInfo[ucRoleIndex])
		return FALSE;

	for (i = 0; i < P2P_MAXIMUM_CLIENT_COUNT; i++) {
		COPY_MAC_ADDR(
			&(prGlueInfo->prP2PInfo[ucRoleIndex]
			->aucblackMACList[i]), aucNullAddr);
	}

	return TRUE;
}

/*---------------------------------------------------------------------------*/
/*!
 * \brief to compare the black list of Hotspot
 *
 * \param[in]
 *           prGlueInfo
 *
 * \return
 */
/*---------------------------------------------------------------------------*/
u_int8_t kalP2PCmpBlackList(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t rbssid[PARAM_MAC_ADDR_LEN],
		IN uint8_t ucRoleIndex)
{
	uint8_t aucNullAddr[] = NULL_MAC_ADDR;
	u_int8_t fgIsExsit = FALSE;
	uint32_t i;

	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prP2PInfo[ucRoleIndex]);

	for (i = 0; i < P2P_MAXIMUM_CLIENT_COUNT; i++) {
		if (UNEQUAL_MAC_ADDR(rbssid, aucNullAddr)) {
			if (EQUAL_MAC_ADDR(
				&(prGlueInfo->prP2PInfo
				[ucRoleIndex]->aucblackMACList[i]),
				rbssid)) {
				fgIsExsit = TRUE;
				return fgIsExsit;
			}
		}
	}

	return fgIsExsit;

}

/*---------------------------------------------------------------------------*/
/*!
 * \brief to return the max clients of Hotspot
 *
 * \param[in]
 *           prGlueInfo
 *
 * \return
 */
/*---------------------------------------------------------------------------*/
void kalP2PSetMaxClients(IN struct GLUE_INFO *prGlueInfo,
		IN uint32_t u4MaxClient,
		IN uint8_t ucRoleIndex)
{
	ASSERT(prGlueInfo);

	if (prGlueInfo->prP2PInfo[ucRoleIndex] == NULL)
		return;

	if (u4MaxClient == 0 ||
		u4MaxClient >= P2P_MAXIMUM_CLIENT_COUNT)
		prGlueInfo->prP2PInfo[ucRoleIndex]->ucMaxClients =
			P2P_MAXIMUM_CLIENT_COUNT;
	else
		prGlueInfo->prP2PInfo[ucRoleIndex]->ucMaxClients = u4MaxClient;

	DBGLOG(P2P, TRACE,
		"Role(%d) max client count = %u\n",
		ucRoleIndex,
		prGlueInfo->prP2PInfo[ucRoleIndex]->ucMaxClients);
}

/*---------------------------------------------------------------------------*/
/*!
 * \brief to return the max clients of Hotspot
 *
 * \param[in]
 *           prGlueInfo
 *
 * \return
 */
/*---------------------------------------------------------------------------*/
u_int8_t kalP2PMaxClients(IN struct GLUE_INFO *prGlueInfo,
		IN uint32_t u4NumClient, IN uint8_t ucRoleIndex)
{
	ASSERT(prGlueInfo);

	if (prGlueInfo->prP2PInfo[ucRoleIndex] &&
		prGlueInfo->prP2PInfo[ucRoleIndex]->ucMaxClients) {
		if ((uint8_t) u4NumClient
			>= prGlueInfo->prP2PInfo[ucRoleIndex]->ucMaxClients)
			return TRUE;
		else
			return FALSE;
	}

	return FALSE;
}

#endif

void kalP2pUnlinkBss(IN struct GLUE_INFO *prGlueInfo, IN uint8_t aucBSSID[])
{
	struct GL_P2P_INFO *prGlueP2pInfo = (struct GL_P2P_INFO *) NULL;

	ASSERT(prGlueInfo);
	ASSERT(aucBSSID);

	DBGLOG(P2P, INFO, "bssid: " MACSTR "\n", MAC2STR(aucBSSID));

	prGlueP2pInfo = prGlueInfo->prP2PInfo[0];

	if (prGlueP2pInfo == NULL)
		return;

	if (scanSearchBssDescByBssidAndSsid(prGlueInfo->prAdapter,
			aucBSSID, FALSE, NULL) != NULL)
		scanRemoveBssDescByBssid(prGlueInfo->prAdapter, aucBSSID);
}

void kalP2pIndicateQueuedMgmtFrame(IN struct GLUE_INFO *prGlueInfo,
		IN struct P2P_QUEUED_ACTION_FRAME *prFrame)
{
	struct GL_P2P_INFO *prGlueP2pInfo = (struct GL_P2P_INFO *) NULL;
	struct net_device *prNetdevice = (struct net_device *) NULL;

	if ((prGlueInfo == NULL) || (prFrame == NULL)) {
		ASSERT(FALSE);
		return;
	}

	DBGLOG(P2P, INFO, "Indicate queued p2p action frame.\n");

	if (prFrame->prHeader == NULL || prFrame->u2Length == 0) {
		DBGLOG(P2P, WARN, "Frame pointer is null or length is 0.\n");
		return;
	}

	prGlueP2pInfo = prGlueInfo->prP2PInfo[prFrame->ucRoleIdx];
	prNetdevice = prGlueP2pInfo->prDevHandler;

#if (KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE)
	cfg80211_rx_mgmt(
		/* struct net_device * dev, */
		prNetdevice->ieee80211_ptr,
		prFrame->u4Freq,
		0,
		prFrame->prHeader,
		prFrame->u2Length,
		NL80211_RXMGMT_FLAG_ANSWERED);
#elif (KERNEL_VERSION(3, 12, 0) <= CFG80211_VERSION_CODE)
	cfg80211_rx_mgmt(
		/* struct net_device * dev, */
		prNetdevice->ieee80211_ptr,
		prFrame->u4Freq,
		0,
		prFrame->prHeader,
		prFrame->u2Length,
		NL80211_RXMGMT_FLAG_ANSWERED,
		GFP_ATOMIC);
#else
	cfg80211_rx_mgmt(
		/* struct net_device * dev, */
		prNetdevice->ieee80211_ptr,
		prFrame->u4Freq,
		0,
		prFrame->prHeader,
		prFrame->u2Length,
		GFP_ATOMIC);
#endif
}

void kalP2pIndicateAcsResult(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t ucRoleIndex,
		IN uint8_t ucPrimaryCh,
		IN uint8_t ucSecondCh,
		IN uint8_t ucSeg0Ch,
		IN uint8_t ucSeg1Ch,
		IN enum ENUM_MAX_BANDWIDTH_SETTING eChnlBw)
{
	struct GL_P2P_INFO *prGlueP2pInfo = (struct GL_P2P_INFO *) NULL;
	struct sk_buff *vendor_event = NULL;
	uint16_t ch_width = MAX_BW_20MHZ;

	prGlueP2pInfo = prGlueInfo->prP2PInfo[ucRoleIndex];

	if (!prGlueP2pInfo) {
		DBGLOG(P2P, ERROR, "p2p glue info null.\n");
		return;
	}

	switch (eChnlBw) {
	case MAX_BW_20MHZ:
		ch_width = 20;
		break;
	case MAX_BW_40MHZ:
		ch_width = 40;
		break;
	case MAX_BW_80MHZ:
		ch_width = 80;
		break;
	case MAX_BW_160MHZ:
		ch_width = 160;
		break;
	default:
		DBGLOG(P2P, ERROR, "unsupport width: %d.\n", ch_width);
		break;
	}

	DBGLOG(P2P, INFO, "r=%d, c=%d, s=%d, s0=%d, s1=%d, ch_w=%d\n",
			ucRoleIndex,
			ucPrimaryCh,
			ucSecondCh,
			ucSeg0Ch,
			ucSeg1Ch,
			ch_width);

#if KERNEL_VERSION(3, 14, 0) <= LINUX_VERSION_CODE
	vendor_event = cfg80211_vendor_event_alloc(prGlueP2pInfo->prWdev->wiphy,
#if KERNEL_VERSION(4, 1, 0) <= LINUX_VERSION_CODE
			prGlueP2pInfo->prWdev,
#endif
			4 * sizeof(u8) + 1 * sizeof(u16) + 4 + NLMSG_HDRLEN,
			WIFI_EVENT_ACS,
			GFP_KERNEL);
#endif

	if (!vendor_event) {
		DBGLOG(P2P, ERROR, "allocate vendor event fail.\n");
		goto nla_put_failure;
	}

	if (unlikely(nla_put_u8(vendor_event,
			WIFI_VENDOR_ATTR_ACS_PRIMARY_CHANNEL,
			ucPrimaryCh) < 0)) {
		DBGLOG(P2P, ERROR, "put primary channel fail.\n");
		goto nla_put_failure;
	}

	if (unlikely(nla_put_u8(vendor_event,
			WIFI_VENDOR_ATTR_ACS_SECONDARY_CHANNEL,
			ucSecondCh) < 0)) {
		DBGLOG(P2P, ERROR, "put secondary channel fail.\n");
		goto nla_put_failure;
	}

	if (unlikely(nla_put_u8(vendor_event,
			WIFI_VENDOR_ATTR_ACS_VHT_SEG0_CENTER_CHANNEL,
			ucSeg0Ch) < 0)) {
		DBGLOG(P2P, ERROR, "put vht seg0 fail.\n");
		goto nla_put_failure;
	}

	if (unlikely(nla_put_u8(vendor_event,
			WIFI_VENDOR_ATTR_ACS_VHT_SEG1_CENTER_CHANNEL,
			ucSeg1Ch) < 0)) {
		DBGLOG(P2P, ERROR, "put vht seg1 fail.\n");
		goto nla_put_failure;
	}

	if (unlikely(nla_put_u16(vendor_event,
			WIFI_VENDOR_ATTR_ACS_CHWIDTH,
			ch_width) < 0)) {
		DBGLOG(P2P, ERROR, "put ch width fail.\n");
		goto nla_put_failure;
	}

	if (unlikely(nla_put_u8(vendor_event,
			WIFI_VENDOR_ATTR_ACS_HW_MODE,
			ucPrimaryCh > 14 ?
				P2P_VENDOR_ACS_HW_MODE_11A :
				P2P_VENDOR_ACS_HW_MODE_11G) < 0)) {
		DBGLOG(P2P, ERROR, "put hw mode fail.\n");
		goto nla_put_failure;
	}
#if KERNEL_VERSION(3, 14, 0) <= LINUX_VERSION_CODE
	cfg80211_vendor_event(vendor_event, GFP_KERNEL);
#endif
	return;

nla_put_failure:
	if (vendor_event)
		kfree_skb(vendor_event);
}

void kalP2pNotifyStopApComplete(IN struct ADAPTER *prAdapter,
		IN uint8_t ucRoleIndex)
{
	struct GL_P2P_INFO *prP2PInfo;

	if (!prAdapter)
		return;

	prP2PInfo = prAdapter->prGlueInfo->prP2PInfo[ucRoleIndex];
	if (prP2PInfo && !completion_done(&prP2PInfo->rStopApComp))
		complete(&prP2PInfo->rStopApComp);
}

void kalP2pIndicateChnlSwitch(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prBssInfo)
{
	struct GL_P2P_INFO *prP2PInfo;
	uint8_t role_idx = 0;

	if (!prAdapter || !prBssInfo)
		return;

	role_idx = prBssInfo->u4PrivateData;
	prP2PInfo = prAdapter->prGlueInfo->prP2PInfo[role_idx];

	if (!prP2PInfo) {
		DBGLOG(P2P, WARN, "p2p glue info is not active\n");
		return;
	}

	/* Compose ch info. */
	if (prP2PInfo->chandef == NULL) {
		struct ieee80211_channel *chan;

		prP2PInfo->chandef = (struct cfg80211_chan_def *)
				cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
				sizeof(struct cfg80211_chan_def));
		if (!prP2PInfo->chandef) {
			DBGLOG(P2P, WARN, "cfg80211_chan_def alloc fail\n");
			return;
		}

		prP2PInfo->chandef->chan = (struct ieee80211_channel *)
				cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
				sizeof(struct ieee80211_channel));

		if (!prP2PInfo->chandef->chan) {
			DBGLOG(P2P, WARN, "ieee80211_channel alloc fail\n");
			return;
		}

		chan = ieee80211_get_channel(prP2PInfo->prWdev->wiphy,
				nicChannelNum2Freq(
					prBssInfo->ucPrimaryChannel) / 1000);
		if (!chan) {
			DBGLOG(P2P, WARN,
				"get channel fail\n");
			return;
		}

		/* Fill chan def */
		prP2PInfo->chandef->chan->band =
				(prBssInfo->eBand == BAND_5G) ?
					KAL_BAND_5GHZ : KAL_BAND_2GHZ;
		prP2PInfo->chandef->chan->center_freq = nicChannelNum2Freq(
				prBssInfo->ucPrimaryChannel) / 1000;

		prP2PInfo->chandef->chan->dfs_state = chan->dfs_state;

		switch (prBssInfo->ucVhtChannelWidth) {
		case VHT_OP_CHANNEL_WIDTH_80P80:
			prP2PInfo->chandef->width
				= NL80211_CHAN_WIDTH_80P80;
			prP2PInfo->chandef->center_freq1
				= nicChannelNum2Freq(
				prBssInfo->ucVhtChannelFrequencyS1) / 1000;
			prP2PInfo->chandef->center_freq2
				= nicChannelNum2Freq(
				prBssInfo->ucVhtChannelFrequencyS2) / 1000;
			break;
		case VHT_OP_CHANNEL_WIDTH_160:
			prP2PInfo->chandef->width
				= NL80211_CHAN_WIDTH_160;
			prP2PInfo->chandef->center_freq1
				= nicChannelNum2Freq(
				prBssInfo->ucVhtChannelFrequencyS1) / 1000;
			prP2PInfo->chandef->center_freq2
				= nicChannelNum2Freq(
				prBssInfo->ucVhtChannelFrequencyS2) / 1000;
			break;
		case VHT_OP_CHANNEL_WIDTH_80:
			prP2PInfo->chandef->width
				= NL80211_CHAN_WIDTH_80;
			prP2PInfo->chandef->center_freq1
				= nicChannelNum2Freq(
				prBssInfo->ucVhtChannelFrequencyS1) / 1000;
			prP2PInfo->chandef->center_freq2
				= nicChannelNum2Freq(
				prBssInfo->ucVhtChannelFrequencyS2) / 1000;
			break;
		case VHT_OP_CHANNEL_WIDTH_20_40:
			prP2PInfo->chandef->center_freq1
				= prP2PInfo->chandef->chan->center_freq;
			if (prBssInfo->eBssSCO == CHNL_EXT_SCA) {
				prP2PInfo->chandef->width
					= NL80211_CHAN_WIDTH_40;
				prP2PInfo->chandef->center_freq1 += 10;
			} else if (prBssInfo->eBssSCO == CHNL_EXT_SCB) {
				prP2PInfo->chandef->width
					= NL80211_CHAN_WIDTH_40;
				prP2PInfo->chandef->center_freq1 -= 10;
			} else {
				prP2PInfo->chandef->width
					= NL80211_CHAN_WIDTH_20;
			}
			prP2PInfo->chandef->center_freq2 = 0;
			break;
		default:
			prP2PInfo->chandef->width
				= NL80211_CHAN_WIDTH_20;
			prP2PInfo->chandef->center_freq1
				= prP2PInfo->chandef->chan->center_freq;
			prP2PInfo->chandef->center_freq2 = 0;
			break;
		}

		DBGLOG(P2P, INFO,
			"role(%d) b=%d f=%d w=%d s1=%d s2=%d dfs=%d\n",
			role_idx,
			prP2PInfo->chandef->chan->band,
			prP2PInfo->chandef->chan->center_freq,
			prP2PInfo->chandef->width,
			prP2PInfo->chandef->center_freq1,
			prP2PInfo->chandef->center_freq2,
			prP2PInfo->chandef->chan->dfs_state);
	}

	/* Ch notify */
	cfg80211_ch_switch_notify(
		prP2PInfo->prDevHandler,
		prP2PInfo->chandef);
}
