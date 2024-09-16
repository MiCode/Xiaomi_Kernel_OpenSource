/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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
ENUM_PARAM_MEDIA_STATE_T kalP2PGetState(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->prP2PInfo->eState;
}				/* end of kalP2PGetState() */

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
		      IN PUINT_8 pucFrameBody, IN UINT_32 u4FrameBodyLen, IN BOOLEAN fgReassocRequest)
{
	union iwreq_data wrqu;
	unsigned char *pucExtraInfo = NULL;
	unsigned char *pucDesiredIE = NULL;
/* unsigned char aucExtraInfoBuf[200]; */
	PUINT_8 cp;

	memset(&wrqu, 0, sizeof(wrqu));

	if (fgReassocRequest) {
		if (u4FrameBodyLen < 15) {
			/*
			 * printk(KERN_WARNING "frameBodyLen too short:%ld\n", frameBodyLen);
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
		goto skip_indicate_event;
	}

	/* IWEVASSOCREQIE, indicate binary string */
	pucExtraInfo = pucDesiredIE;
	wrqu.data.length = pucDesiredIE[1] + 2;

	/* Send event to user space */
	wireless_send_event(prGlueInfo->prP2PInfo->prDevHandler, IWEVASSOCREQIE, &wrqu, pucExtraInfo);

skip_indicate_event:
	return;

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
VOID
kalP2PSetState(IN P_GLUE_INFO_T prGlueInfo,
	       IN ENUM_PARAM_MEDIA_STATE_T eState, IN PARAM_MAC_ADDRESS rPeerAddr, IN UINT_8 ucRole)
{
	union iwreq_data evt;
	UINT_8 aucBuffer[IW_CUSTOM_MAX];

	ASSERT(prGlueInfo);

	memset(&evt, 0, sizeof(evt));

	if (eState == PARAM_MEDIA_STATE_CONNECTED) {
		prGlueInfo->prP2PInfo->eState = PARAM_MEDIA_STATE_CONNECTED;

		snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_STA_CONNECT=%pM ", rPeerAddr);
		evt.data.length = strlen(aucBuffer);

		/* indicate in IWECUSTOM event */
		wireless_send_event(prGlueInfo->prP2PInfo->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);

	} else if (eState == PARAM_MEDIA_STATE_DISCONNECTED) {
		snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_STA_DISCONNECT=%pM ", rPeerAddr);
		evt.data.length = strlen(aucBuffer);

		/* indicate in IWECUSTOM event */
		wireless_send_event(prGlueInfo->prP2PInfo->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);
	} else {
		ASSERT(0);
	}

}				/* end of kalP2PSetState() */

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
UINT_32 kalP2PGetFreqInKHz(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->prP2PInfo->u4FreqInKHz;
}				/* end of kalP2PGetFreqInKHz() */

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
UINT_8 kalP2PGetRole(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->prP2PInfo->ucRole;
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
		prGlueInfo->prP2PInfo->ucRole = ucRole;

	if (pucSSID)
		snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_FORMATION_RST=%d%d%d%c%c", ucResult, ucRole,
			 1 /* persistence or not */, pucSSID[7], pucSSID[8]);
	else
		snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_FORMATION_RST=%d%d%d%c%c", ucResult, ucRole,
			 1 /* persistence or not */, '0', '0');

	evt.data.length = strlen(aucBuffer);

	/* if (pucSSID) */
	/* printk("P2P GO SSID DIRECT-%c%c\n", pucSSID[7], pucSSID[8]); */

	/* indicate in IWECUSTOM event */
	wireless_send_event(prGlueInfo->prP2PInfo->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);

}				/* end of kalP2PSetRole() */

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
VOID kalP2PSetCipher(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Cipher)
{
	if ((!prGlueInfo) || (!prGlueInfo->prP2PInfo)) {
		ASSERT(FALSE);
		return;
	}

	prGlueInfo->prP2PInfo->u4CipherPairwise = u4Cipher;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief to get the cipher, return for cipher is ccmp
*
* \param[in]
*           prGlueInfo
*
* \return
*           TRUE: cipher is ccmp
*           FALSE: cipher is none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalP2PGetCipher(IN P_GLUE_INFO_T prGlueInfo)
{
	if ((!prGlueInfo) || (!prGlueInfo->prP2PInfo)) {
		ASSERT(FALSE);
		return FALSE;
	}

	if (prGlueInfo->prP2PInfo->u4CipherPairwise == IW_AUTH_CIPHER_CCMP)
		return TRUE;

	if (prGlueInfo->prP2PInfo->u4CipherPairwise == IW_AUTH_CIPHER_TKIP)
		return TRUE;

	return FALSE;
}

BOOLEAN kalP2PGetCcmpCipher(IN P_GLUE_INFO_T prGlueInfo)
{
	if ((!prGlueInfo) || (!prGlueInfo->prP2PInfo)) {
		ASSERT(FALSE);
		return FALSE;
	}

	if (prGlueInfo->prP2PInfo->u4CipherPairwise == IW_AUTH_CIPHER_CCMP)
		return TRUE;

	if (prGlueInfo->prP2PInfo->u4CipherPairwise == IW_AUTH_CIPHER_TKIP)
		return FALSE;

	return FALSE;
}

BOOLEAN kalP2PGetTkipCipher(IN P_GLUE_INFO_T prGlueInfo)
{
	if ((!prGlueInfo) || (!prGlueInfo->prP2PInfo)) {
		ASSERT(FALSE);
		return FALSE;
	}

	if (prGlueInfo->prP2PInfo->u4CipherPairwise == IW_AUTH_CIPHER_CCMP)
		return FALSE;

	if (prGlueInfo->prP2PInfo->u4CipherPairwise == IW_AUTH_CIPHER_TKIP)
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
	if ((!prGlueInfo) || (!prGlueInfo->prP2PInfo)) {
		ASSERT(FALSE);
		return;
	}

	prGlueInfo->prP2PInfo->ucWSCRunning = ucWscMode;
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
	if ((!prGlueInfo) || (!prGlueInfo->prP2PInfo)) {
		ASSERT(FALSE);
		return 0;
	}

	return prGlueInfo->prP2PInfo->ucWSCRunning;
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
UINT_16 kalP2PCalWSC_IELen(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucType)
{
	ASSERT(prGlueInfo);

	ASSERT(ucType < 3);

	return prGlueInfo->prP2PInfo->u2WSCIELen[ucType];
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to get the p2p ie length
*
* \param[in]
*           prGlueInfo
*
*
* \return
*           The P2P IE length
*/
/*----------------------------------------------------------------------------*/
UINT_16 kalP2PCalP2P_IELen(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucIndex)
{
	ASSERT(prGlueInfo);

	ASSERT(ucIndex < MAX_P2P_IE_SIZE);

	return prGlueInfo->prP2PInfo->u2P2PIELen[ucIndex];
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
VOID kalP2PGenWSC_IE(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucType, IN PUINT_8 pucBuffer)
{
	P_GL_P2P_INFO_T prGlP2pInfo = (P_GL_P2P_INFO_T) NULL;

	do {
		if ((prGlueInfo == NULL) || (ucType >= 3) || (pucBuffer == NULL))
			break;

		prGlP2pInfo = prGlueInfo->prP2PInfo;

		kalMemCopy(pucBuffer, prGlP2pInfo->aucWSCIE[ucType], prGlP2pInfo->u2WSCIELen[ucType]);

	} while (FALSE);

}

VOID kalP2PUpdateWSC_IE(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucType, IN PUINT_8 pucBuffer, IN UINT_16 u2BufferLength)
{
	P_GL_P2P_INFO_T prGlP2pInfo = (P_GL_P2P_INFO_T) NULL;

	do {
		if ((prGlueInfo == NULL) || (ucType >= 3) || ((u2BufferLength > 0) && (pucBuffer == NULL)))
			break;

		if (u2BufferLength > 400) {
			DBGLOG(P2P, ERROR,
			       "Buffer length is not enough, GLUE only 400 bytes but %d received\n", u2BufferLength);
			ASSERT(FALSE);
			break;
		}

		prGlP2pInfo = prGlueInfo->prP2PInfo;

		kalMemCopy(prGlP2pInfo->aucWSCIE[ucType], pucBuffer, u2BufferLength);

		prGlP2pInfo->u2WSCIELen[ucType] = u2BufferLength;

	} while (FALSE);

}				/* kalP2PUpdateWSC_IE */

/*----------------------------------------------------------------------------*/
/*!
* \brief to copy the p2p ie setting from p2p supplicant
*
* \param[in]
*           prGlueInfo
*
* \return
*
*/
/*----------------------------------------------------------------------------*/
VOID kalP2PGenP2P_IE(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucIndex, IN PUINT_8 pucBuffer)
{
	P_GL_P2P_INFO_T prGlP2pInfo = (P_GL_P2P_INFO_T) NULL;

	do {
		if ((prGlueInfo == NULL) || (ucIndex >= MAX_P2P_IE_SIZE) || (pucBuffer == NULL))
			break;

		prGlP2pInfo = prGlueInfo->prP2PInfo;

		kalMemCopy(pucBuffer, prGlP2pInfo->aucP2PIE[ucIndex], prGlP2pInfo->u2P2PIELen[ucIndex]);

	} while (FALSE);

}

VOID kalP2PUpdateP2P_IE(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucIndex, IN PUINT_8 pucBuffer, IN UINT_16 u2BufferLength)
{
	P_GL_P2P_INFO_T prGlP2pInfo = (P_GL_P2P_INFO_T) NULL;

	do {
		if ((prGlueInfo == NULL) ||
			(ucIndex >= MAX_P2P_IE_SIZE) ||
			((u2BufferLength > 0) && (pucBuffer == NULL)))
			break;

		if (u2BufferLength > 400) {
			DBGLOG(P2P, ERROR,
			       "kalP2PUpdateP2P_IE > Buffer length is not enough, GLUE only 400 bytes but %d received\n",
					u2BufferLength);
			ASSERT(FALSE);
			break;
		}

		prGlP2pInfo = prGlueInfo->prP2PInfo;

		kalMemCopy(prGlP2pInfo->aucP2PIE[ucIndex], pucBuffer, u2BufferLength);

		prGlP2pInfo->u2P2PIELen[ucIndex] = u2BufferLength;

	} while (FALSE);

}				/* kalP2PUpdateWSC_IE */

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
			   IN PARAM_MAC_ADDRESS rPeerAddr, IN UINT_8 ucDevType,	/* 0: P2P Device / 1: GC / 2: GO */
			   IN INT_32 i4ConfigMethod, IN INT_32 i4ActiveConfigMethod)
{
	union iwreq_data evt;
	UINT_8 aucBuffer[IW_CUSTOM_MAX];

	ASSERT(prGlueInfo);

	/* buffer peer information for later IOC_P2P_GET_REQ_DEVICE_INFO access */
	prGlueInfo->prP2PInfo->u4ConnReqNameLength = u4NameLength > 32 ? 32 : u4NameLength;
	kalMemCopy(prGlueInfo->prP2PInfo->aucConnReqDevName, pucDevName, prGlueInfo->prP2PInfo->u4ConnReqNameLength);
	COPY_MAC_ADDR(prGlueInfo->prP2PInfo->rConnReqPeerAddr, rPeerAddr);
	prGlueInfo->prP2PInfo->ucConnReqDevType = ucDevType;
	prGlueInfo->prP2PInfo->i4ConnReqConfigMethod = i4ConfigMethod;
	prGlueInfo->prP2PInfo->i4ConnReqActiveConfigMethod = i4ActiveConfigMethod;

	/* prepare event structure */
	memset(&evt, 0, sizeof(evt));

	snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_DVC_REQ");
	evt.data.length = strlen(aucBuffer);

	/* indicate in IWEVCUSTOM event */
	wireless_send_event(prGlueInfo->prP2PInfo->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);

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
	prGlueInfo->prP2PInfo->u4ConnReqNameLength =
	    (UINT_32) ((prP2pDevDesc->u2NameLength > 32) ? 32 : prP2pDevDesc->u2NameLength);
	kalMemCopy(prGlueInfo->prP2PInfo->aucConnReqDevName, prP2pDevDesc->aucName,
		   prGlueInfo->prP2PInfo->u4ConnReqNameLength);
	COPY_MAC_ADDR(prGlueInfo->prP2PInfo->rConnReqPeerAddr, prP2pDevDesc->aucDeviceAddr);
	COPY_MAC_ADDR(prGlueInfo->prP2PInfo->rConnReqGroupAddr, pucGroupBssid);
	prGlueInfo->prP2PInfo->i4ConnReqConfigMethod = (INT_32) (prP2pDevDesc->u2ConfigMethod);
	prGlueInfo->prP2PInfo->ucOperatingChnl = ucOperatingChnl;
	prGlueInfo->prP2PInfo->ucInvitationType = ucInvitationType;

	/* prepare event structure */
	memset(&evt, 0, sizeof(evt));

	snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_INV_INDICATE");
	evt.data.length = strlen(aucBuffer);

	/* indicate in IWEVCUSTOM event */
	wireless_send_event(prGlueInfo->prP2PInfo->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);
	return;

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
			prP2pConnReq->fgIsTobeGO = (prGlueInfo->prP2PInfo->ucRole == 2) ? TRUE : FALSE;

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

	return;
#endif

}				/* kalP2PInvitationIndication */

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
	prGlueInfo->prP2PInfo->u4InvStatus = u4InvStatus;

	/* prepare event structure */
	memset(&evt, 0, sizeof(evt));

	snprintf(aucBuffer, IW_CUSTOM_MAX - 1, "P2P_INV_STATUS");
	evt.data.length = strlen(aucBuffer);

	/* indicate in IWEVCUSTOM event */
	wireless_send_event(prGlueInfo->prP2PInfo->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);

}				/* kalP2PInvitationStatus */

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
	wireless_send_event(prGlueInfo->prP2PInfo->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);

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
	wireless_send_event(prGlueInfo->prP2PInfo->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);

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
	wireless_send_event(prGlueInfo->prP2PInfo->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);

}				/* end of kalP2PIndicateSDResponse() */

struct net_device *kalP2PGetDevHdlr(P_GLUE_INFO_T prGlueInfo)
{
	if ((!prGlueInfo) || (!prGlueInfo->prP2PInfo)) {
		ASSERT(FALSE);
		return NULL;
	}

	return prGlueInfo->prP2PInfo->prDevHandler;
}

#if CFG_SUPPORT_ANTI_PIRACY
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

	kalMemCopy(prGlueInfo->prP2PInfo->aucSecCheckRsp, pucRsp, u2RspLen);
	evt.data.length = strlen(aucBuffer);

#if DBG
	DBGLOG_MEM8(SEC, LOUD, prGlueInfo->prP2PInfo->aucSecCheckRsp, u2RspLen);
#endif
	/* indicate in IWECUSTOM event */
	wireless_send_event(prGlueInfo->prP2PInfo->prDevHandler, IWEVCUSTOM, &evt, aucBuffer);
}				/* p2pFsmRunEventRxDisassociation */
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
	rlmDomainGetChnlList(prGlueInfo->prAdapter, eSpecificBand, FALSE, ucMaxChannelNum,
			     pucNumOfChannel, paucChannelList);
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

		prIEEE80211ChnlStruct = kalP2pFuncGetChannelEntry(prGlueInfo->prP2PInfo, &rChannelInfo);

		kalP2pFuncGetChannelType(eSco, &eChnlType);

		if (!prIEEE80211ChnlStruct) {
			DBGLOG(P2P, WARN, "prIEEE80211ChnlStruct is NULL\n");
			break;
		}
		cfg80211_ready_on_channel(prGlueInfo->prP2PInfo->prWdev,	/* struct wireless_dev, */
					  u8SeqNum,	/* u64 cookie, */
					  prIEEE80211ChnlStruct,	/* struct ieee80211_channel * chan, */
					  u4Duration,	/* unsigned int duration, */
					  GFP_KERNEL);	/* gfp_t gfp */    /* allocation flags */
	} while (FALSE);

}				/* kalP2PIndicateChannelReady */

VOID kalP2PIndicateChannelExpired(IN P_GLUE_INFO_T prGlueInfo, IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo)
{

	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;
	struct ieee80211_channel *prIEEE80211ChnlStruct = (struct ieee80211_channel *)NULL;
	enum nl80211_channel_type eChnlType = NL80211_CHAN_NO_HT;
	RF_CHANNEL_INFO_T rRfChannelInfo;

	do {
		if ((prGlueInfo == NULL) || (prChnlReqInfo == NULL)) {

			ASSERT(FALSE);
			break;
		}

		prGlueP2pInfo = prGlueInfo->prP2PInfo;

		if (prGlueP2pInfo == NULL) {
			ASSERT(FALSE);
			break;
		}

		DBGLOG(P2P, TRACE, "kalP2PIndicateChannelExpired\n");

		rRfChannelInfo.eBand = prChnlReqInfo->eBand;
		rRfChannelInfo.ucChannelNum = prChnlReqInfo->ucReqChnlNum;

		prIEEE80211ChnlStruct = kalP2pFuncGetChannelEntry(prGlueP2pInfo, &rRfChannelInfo);

		kalP2pFuncGetChannelType(prChnlReqInfo->eChnlSco, &eChnlType);

		if (!prIEEE80211ChnlStruct) {
			DBGLOG(P2P, WARN, "prIEEE80211ChnlStruct is NULL\n");
			break;
		}
		cfg80211_remain_on_channel_expired(prGlueP2pInfo->prWdev,	/* struct wireless_dev, */
						   prChnlReqInfo->u8Cookie, prIEEE80211ChnlStruct, GFP_KERNEL);
	} while (FALSE);

}				/* kalP2PIndicateChannelExpired */

VOID kalP2PIndicateScanDone(IN P_GLUE_INFO_T prGlueInfo, IN BOOLEAN fgIsAbort)
{
	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;
	struct cfg80211_scan_request *prScanRequest = NULL;

	GLUE_SPIN_LOCK_DECLARATION();

	do {
		if (prGlueInfo == NULL) {

			ASSERT(FALSE);
			break;
		}

		prGlueP2pInfo = prGlueInfo->prP2PInfo;

		if (prGlueP2pInfo == NULL) {
			ASSERT(FALSE);
			break;
		}

		DBGLOG(INIT, TRACE, "[p2p] scan complete %p\n", prGlueP2pInfo->prScanRequest);

		GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
		if (prGlueP2pInfo->prScanRequest != NULL) {
			prScanRequest = prGlueP2pInfo->prScanRequest;
			prGlueP2pInfo->prScanRequest = NULL;
		}
		GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

		/* 2. then CFG80211 Indication */

		if (prScanRequest != NULL) {

			/* report all queued beacon/probe response frames  to upper layer */
			scanReportBss2Cfg80211(prGlueInfo->prAdapter, BSS_TYPE_P2P_DEVICE, NULL);

			DBGLOG(INIT, TRACE, "DBG:p2p_cfg_scan_done\n");
			kalCfg80211ScanDone(prScanRequest, fgIsAbort);
		}

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

		prGlueP2pInfo = prGlueInfo->prP2PInfo;

		if (prGlueP2pInfo == NULL) {
			ASSERT(FALSE);
			break;
		}

		prChannelEntry = kalP2pFuncGetChannelEntry(prGlueP2pInfo, prChannelInfo);

		if (prChannelEntry == NULL) {
			DBGLOG(P2P, WARN, "Unknown channel info\n");
			break;
		}
		/* rChannelInfo.center_freq = nicChannelNum2Freq((UINT_32)prChannelInfo->ucChannelNum) / 1000; */

		if (u4BufLen > 0) {
			prCfg80211Bss = cfg80211_inform_bss_frame(
				/* struct wiphy * wiphy, */
				prGlueP2pInfo->prWdev->wiphy,
				prChannelEntry,
				prBcnProbeRspFrame,
				u4BufLen, i4SignalStrength, GFP_KERNEL);
		}

		/* Return this structure. */
		if (!prCfg80211Bss) {
			DBGLOG(P2P, WARN,
				"inform bss[%pM]: to cfg80211 failed, bss channel %d, rcpi %d, len %d\n",
				prBcnProbeRspFrame->bssid,
				prChannelInfo->ucChannelNum, i4SignalStrength, u4BufLen);
		} else {
			cfg80211_put_bss(prGlueP2pInfo->prWdev->wiphy, prCfg80211Bss);
			DBGLOG(P2P, TRACE, "inform bss to cfg80211, bss channel %d, rcpi %d\n",
				prChannelInfo->ucChannelNum, i4SignalStrength);
		}
	} while (FALSE);

}				/* kalP2PIndicateBssInfo */

VOID
kalP2PIndicateCompleteBssInfo(IN P_GLUE_INFO_T prGlueInfo, IN P_BSS_DESC_T prSpecificBssDesc)
{
	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;
	struct ieee80211_channel *prChannelEntry = (struct ieee80211_channel *)NULL;
	struct cfg80211_bss *prCfg80211Bss = (struct cfg80211_bss *)NULL;
	RF_CHANNEL_INFO_T rChannelInfo;

	if ((prGlueInfo == NULL) || (prSpecificBssDesc == NULL)) {
		ASSERT(FALSE);
		return;
	}

	prGlueP2pInfo = prGlueInfo->prP2PInfo;

	if (prGlueP2pInfo == NULL) {
		DBGLOG(P2P, WARN, "kalP2PIndicateCompleteBssInfo: prGlueP2pInfo is NULL\n");
		ASSERT(FALSE);
		return;
	}

	rChannelInfo.ucChannelNum = prSpecificBssDesc->ucChannelNum;
	rChannelInfo.eBand = prSpecificBssDesc->eBand;
	prChannelEntry = kalP2pFuncGetChannelEntry(prGlueP2pInfo, &rChannelInfo);

	if (prChannelEntry == NULL) {
		DBGLOG(P2P, WARN, "kalP2PIndicateCompleteBssInfo: Unknown channel info\n");
		return;
	}

	prCfg80211Bss = cfg80211_inform_bss(prGlueP2pInfo->prWdev->wiphy, prChannelEntry,
						((prSpecificBssDesc->fgSeenProbeResp == TRUE) ?
							CFG80211_BSS_FTYPE_PRESP : CFG80211_BSS_FTYPE_BEACON),
						prSpecificBssDesc->aucBSSID, 0,	/* TSF */
						prSpecificBssDesc->u2CapInfo,
						prSpecificBssDesc->u2BeaconInterval,	/* beacon interval */
						prSpecificBssDesc->aucIEBuf,	/* IE */
						prSpecificBssDesc->u2IELength,	/* IE Length */
						RCPI_TO_dBm(prSpecificBssDesc->ucRCPI) * 100,	/* MBM */
						GFP_KERNEL);

	if (!prCfg80211Bss) {
		DBGLOG(P2P, WARN,
			"kalP2PIndicateCompleteBssInfo fail, BSSID[%pM] SSID[%s] Chnl[%d] RCPI[%d] IELng[%d] Bcn[%d]\n",
			prSpecificBssDesc->aucBSSID,
			HIDE(prSpecificBssDesc->aucSSID),
			prSpecificBssDesc->ucChannelNum,
			RCPI_TO_dBm(prSpecificBssDesc->ucRCPI),
			prSpecificBssDesc->u2IELength,
			prSpecificBssDesc->u2BeaconInterval);
		dumpMemory8IEOneLine(prSpecificBssDesc->aucBSSID,
			prSpecificBssDesc->aucIEBuf, prSpecificBssDesc->u2IELength);
	} else
		DBGLOG(P2P, TRACE,
			"kalP2PIndicateCompleteBssInfo, BSS[%p] BSSID[%pM] SSID[%s] Chnl[%d] RCPI[%d] IELng[%d] Bcn[%d]\n",
			prCfg80211Bss,
			prSpecificBssDesc->aucBSSID,
			HIDE(prSpecificBssDesc->aucSSID),
			prSpecificBssDesc->ucChannelNum,
			RCPI_TO_dBm(prSpecificBssDesc->ucRCPI),
			prSpecificBssDesc->u2IELength,
			prSpecificBssDesc->u2BeaconInterval);
}


VOID
kalP2PIndicateMgmtTxStatus(IN P_GLUE_INFO_T prGlueInfo,
			   IN UINT_64 u8Cookie, IN BOOLEAN fgIsAck, IN PUINT_8 pucFrameBuf, IN UINT_32 u4FrameLen)
{
	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;

	do {
		if ((prGlueInfo == NULL) || (pucFrameBuf == NULL) || (u4FrameLen == 0)) {
			DBGLOG(P2P, TRACE, "Unexpected pointer PARAM. %p, %p, %u.",
					    prGlueInfo, pucFrameBuf, u4FrameLen);
			ASSERT(FALSE);
			break;
		}

		prGlueP2pInfo = prGlueInfo->prP2PInfo;

		cfg80211_mgmt_tx_status(prGlueP2pInfo->prWdev,	/* struct net_device * dev, */
					u8Cookie, pucFrameBuf, u4FrameLen, fgIsAck, GFP_KERNEL);

	} while (FALSE);

}				/* kalP2PIndicateMgmtTxStatus */

VOID kalP2PIndicateRxMgmtFrame(IN P_GLUE_INFO_T prGlueInfo, IN P_SW_RFB_T prSwRfb)
{
#define DBG_P2P_MGMT_FRAME_INDICATION 0
	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;
	INT_32 i4Freq = 0;
	UINT_8 ucChnlNum = 0;
#if DBG_P2P_MGMT_FRAME_INDICATION
	P_WLAN_MAC_HEADER_T prWlanHeader = (P_WLAN_MAC_HEADER_T) NULL;
#endif

	do {
		if ((prGlueInfo == NULL) || (prSwRfb == NULL)) {
			ASSERT(FALSE);
			break;
		}

		prGlueP2pInfo = prGlueInfo->prP2PInfo;

		ucChnlNum = prSwRfb->prHifRxHdr->ucHwChannelNum;

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

		DBGLOG(P2P, TRACE, "from: %pM\n", prWlanHeader->aucAddr2);
#endif
		i4Freq = nicChannelNum2Freq(ucChnlNum) / 1000;

		cfg80211_rx_mgmt(prGlueP2pInfo->prWdev,	/* struct net_device * dev, */
				 i4Freq,
				 RCPI_TO_dBm(prSwRfb->prHifRxHdr->ucRcpi),
				 prSwRfb->pvHeader, prSwRfb->u2PacketLen, GFP_ATOMIC);
	} while (FALSE);

}				/* kalP2PIndicateRxMgmtFrame */

VOID
kalP2PGCIndicateConnectionStatus(IN P_GLUE_INFO_T prGlueInfo,
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

		prGlueP2pInfo = prGlueInfo->prP2PInfo;
		DBGLOG(P2P, INFO, "%s: Reason code: %d eStatus=0x%x\n", __func__, u2StatusReason, eStatus);

		if (prP2pConnInfo) {
			/* switch netif on */
			netif_carrier_on(prGlueP2pInfo->prDevHandler);

			cfg80211_connect_result(prGlueP2pInfo->prDevHandler,	/* struct net_device * dev, */
						prP2pConnInfo->aucBssid, prP2pConnInfo->aucIEBuf,
						prP2pConnInfo->u4BufLength,
						pucRxIEBuf, u2RxIELen, u2StatusReason, GFP_KERNEL);
						/* gfp_t gfp */   /* allocation flags */
			prP2pConnInfo->fgIsConnRequest = FALSE;
		} else {
			/* Disconnect, what if u2StatusReason == 0? */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
			cfg80211_disconnected(prGlueP2pInfo->prDevHandler,	/* struct net_device * dev, */
					      u2StatusReason, pucRxIEBuf, u2RxIELen,
					      eStatus == WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY ? true : false,
					      GFP_KERNEL);
#else
			cfg80211_disconnected(prGlueP2pInfo->prDevHandler,	/* struct net_device * dev, */
					      u2StatusReason, pucRxIEBuf, u2RxIELen, GFP_KERNEL);
#endif
		}

	} while (FALSE);

}				/* kalP2PGCIndicateConnectionStatus */

VOID kalP2PGOStationUpdate(IN P_GLUE_INFO_T prGlueInfo, IN P_STA_RECORD_T prCliStaRec, IN BOOLEAN fgIsNew)
{
	P_GL_P2P_INFO_T prP2pGlueInfo = (P_GL_P2P_INFO_T) NULL;

	do {
		if ((prGlueInfo == NULL) || (prCliStaRec == NULL))
			break;

		prP2pGlueInfo = prGlueInfo->prP2PInfo;

		if (fgIsNew) {
			struct station_info rStationInfo;

			kalMemZero(&rStationInfo, sizeof(rStationInfo));
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0))
			rStationInfo.filled = STATION_INFO_ASSOC_REQ_IES;
#endif
			rStationInfo.generation = ++prP2pGlueInfo->i4Generation;

			rStationInfo.assoc_req_ies = prCliStaRec->pucAssocReqIe;
			rStationInfo.assoc_req_ies_len = prCliStaRec->u2AssocReqIeLen;

			cfg80211_new_sta(prGlueInfo->prP2PInfo->prDevHandler,	/* struct net_device * dev, */
					 prCliStaRec->aucMacAddr, &rStationInfo, GFP_KERNEL);
		} else {
			++prP2pGlueInfo->i4Generation;

			cfg80211_del_sta(prGlueInfo->prP2PInfo->prDevHandler,	/* struct net_device * dev, */
					 prCliStaRec->aucMacAddr, GFP_KERNEL);
		}

	} while (FALSE);

	return;

}				/* kalP2PGOStationUpdate */

BOOLEAN kalP2pFuncGetChannelType(IN ENUM_CHNL_EXT_T rChnlSco, OUT enum nl80211_channel_type *channel_type)
{
	BOOLEAN fgIsValid = FALSE;

	do {
		if (channel_type) {

			switch (rChnlSco) {
			case CHNL_EXT_SCN:
#if CFG_SUPPORT_P2P_ECSA
				*channel_type = NL80211_CHAN_HT20;
#else
				*channel_type = NL80211_CHAN_NO_HT;
#endif
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
	UINT_32 u4TblSize = 0, u4Idx = 0;
	struct ieee80211_supported_band **bands;

	do {
		if ((prP2pInfo == NULL) || (prChannelInfo == NULL))
			break;
		bands = &prP2pInfo->prWdev->wiphy->bands[0];
		switch (prChannelInfo->eBand) {
		case BAND_2G4:
			if (bands[IEEE80211_BAND_2GHZ] == NULL)
				DBGLOG(P2P, ERROR, "kalP2pFuncGetChannelEntry 2.4G NULL Bands!!\n");
			else {
				prTargetChannelEntry = bands[IEEE80211_BAND_2GHZ]->channels;
				u4TblSize = bands[IEEE80211_BAND_2GHZ]->n_channels;
			}
			break;
		case BAND_5G:
			if (bands[IEEE80211_BAND_5GHZ] == NULL)
				DBGLOG(P2P, ERROR, "kalP2pFuncGetChannelEntry 5G NULL Bands!!\n");
			else {
				prTargetChannelEntry = bands[IEEE80211_BAND_5GHZ]->channels;
				u4TblSize = bands[IEEE80211_BAND_5GHZ]->n_channels;
			}
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
#if CFG_SUPPORT_P2P_ECSA
VOID kalP2pUpdateECSA(IN P_ADAPTER_T prAdapter, IN P_EVENT_ECSA_RESULT prECSA)
{
	P_BSS_INFO_T prBssInfo = &(prAdapter->rWifiVar.arBssInfo[prECSA->ucNetTypeIndex]);
	UINT_32 u4Freq = 0;
	struct cfg80211_chan_def chandef;
	RF_CHANNEL_INFO_T rChannelInfo;
	enum nl80211_channel_type chantype;
	struct ieee80211_channel *channel;


	if (prECSA == NULL) {
		DBGLOG(P2P, ERROR, "ECSA_RESULT is null!\n");
		return;
	}

	if (prECSA->ucStatus == ECSA_EVENT_STATUS_UPDATE_BEACON) {
		DBGLOG(P2P, INFO, "ECSA FW upeate beacon success!\n");
		return;
	}

	u4Freq = nicChannelNum2Freq(prECSA->ucPrimaryChannel);

	if (!u4Freq) {
		DBGLOG(P2P, ERROR, "channel number invalid: %d\n", prECSA->ucPrimaryChannel);
		return;
	}

	rChannelInfo.ucChannelNum = prECSA->ucPrimaryChannel;
	rChannelInfo.eBand = prECSA->ucPrimaryChannel > 14 ? BAND_5G : BAND_2G4;

	channel = kalP2pFuncGetChannelEntry(prAdapter->prGlueInfo->prP2PInfo, &rChannelInfo);
	if (!channel) {
		DBGLOG(P2P, ERROR, "invalid channel:band %d:%d\n",
				rChannelInfo.ucChannelNum,
				rChannelInfo.eBand);
		return;
	}
	kalP2pFuncGetChannelType(prECSA->ucRfSco, &chantype);
	prBssInfo->ucPrimaryChannel = prECSA->ucPrimaryChannel;
	prBssInfo->eBssSCO = prECSA->ucRfSco;
	prBssInfo->eBand = prBssInfo->ucPrimaryChannel > 14 ? BAND_5G : BAND_2G4;
	prBssInfo->fgChanSwitching = FALSE;

	/* sync with firmware */
	nicUpdateBss(prAdapter, prECSA->ucNetTypeIndex);

	/* only indicate to host when we AP/GO */
	if (prBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT) {
		DBGLOG(P2P, INFO, "Do not indicate to host\n");
		return;
	}

	if (prBssInfo->ucPrimaryChannel == 14)
		chantype = NL80211_CHAN_NO_HT;

	cfg80211_chandef_create(&chandef, channel, chantype);
	/* indicate to host */
	cfg80211_ch_switch_notify(prAdapter->prGlueInfo->prP2PInfo->prDevHandler,
			&chandef);
}
#endif
/*----------------------------------------------------------------------------*/
/*!
* \brief to set/clear the MAC address to/from the black list of Hotspot
*
* \param[in]
*           prGlueInfo
*
* \return
*/
/*----------------------------------------------------------------------------*/
INT_32 kalP2PSetBlackList(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS bssid, IN BOOLEAN block)
{
	UINT_8 aucNullAddr[] = NULL_MAC_ADDR;
	UINT_32 i;

	if ((!prGlueInfo) || (!prGlueInfo->prP2PInfo)) {
		ASSERT(FALSE);
		return -EFAULT;
	}

	if (EQUAL_MAC_ADDR(bssid, aucNullAddr))
		return -EINVAL;

	if (block) {
		/* Set the bssid to the black list to block the STA */
		for (i = 0; i < P2P_MAXIMUM_CLIENT_COUNT; i++) {
			if (EQUAL_MAC_ADDR(&(prGlueInfo->prP2PInfo->aucBlackMACList[i]), bssid))
				break;
		}
		if (i >= P2P_MAXIMUM_CLIENT_COUNT) {
			for (i = 0; i < P2P_MAXIMUM_CLIENT_COUNT; i++) {
				if (EQUAL_MAC_ADDR(&(prGlueInfo->prP2PInfo->aucBlackMACList[i]), aucNullAddr)) {
					COPY_MAC_ADDR(&(prGlueInfo->prP2PInfo->aucBlackMACList[i]), bssid);
					break;
				}
			}
			if (i >= P2P_MAXIMUM_CLIENT_COUNT) {
				DBGLOG(P2P, ERROR, "AP black list full, cannot block more STA!!\n");
				return -ENOBUFS;
			}
		} else
			DBGLOG(P2P, WARN, MACSTR " already in black list\n", MAC2STR(bssid));

	} else {
		/* Clear the bssid from the black list to unblock the STA */
		for (i = 0; i < P2P_MAXIMUM_CLIENT_COUNT; i++) {
			if (EQUAL_MAC_ADDR(&(prGlueInfo->prP2PInfo->aucBlackMACList[i]), bssid)) {
				COPY_MAC_ADDR(&(prGlueInfo->prP2PInfo->aucBlackMACList[i]), aucNullAddr);
				break;
			}
		}
		if (i >= P2P_MAXIMUM_CLIENT_COUNT)
			DBGLOG(P2P, ERROR, MACSTR " is not found in black list!!\n", MAC2STR(bssid));
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to compare and check whether the MAC address is in the black list of Hotspot
*
* \param[in]
*           prGlueInfo
*
* \return
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalP2PCmpBlackList(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS bssid)
{
	UINT_8 aucNullAddr[] = NULL_MAC_ADDR;
	UINT_32 i;

	if ((!prGlueInfo) || (!prGlueInfo->prP2PInfo))
		return FALSE;

	if (EQUAL_MAC_ADDR(bssid, aucNullAddr))
		return FALSE;

	for (i = 0; i < P2P_MAXIMUM_CLIENT_COUNT; i++) {
		if (EQUAL_MAC_ADDR(&(prGlueInfo->prP2PInfo->aucBlackMACList[i]), bssid))
			return TRUE;
	}

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to set the max clients of Hotspot or P2P GO
*
* \param[in]
*           prGlueInfo
*
* \return
*/
/*----------------------------------------------------------------------------*/
VOID kalP2PSetMaxClients(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4MaxClient)
{
	if ((!prGlueInfo) || (!prGlueInfo->prP2PInfo)) {
		ASSERT(FALSE);
		return;
	}

	if (u4MaxClient == 0 || u4MaxClient >= P2P_MAXIMUM_CLIENT_COUNT)
		prGlueInfo->prP2PInfo->ucMaxClients = P2P_MAXIMUM_CLIENT_COUNT;
	else
		prGlueInfo->prP2PInfo->ucMaxClients = (UINT_8)u4MaxClient;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to check whether reaches the max clients of Hotspot or P2P GO
*
* \param[in]
*           prGlueInfo
*
* \return
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalP2PReachMaxClients(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4NumClient)
{
	if ((!prGlueInfo) || (!prGlueInfo->prP2PInfo)) {
		ASSERT(FALSE);
		return FALSE;
	}

	if (prGlueInfo->prP2PInfo->ucMaxClients) {
		if ((UINT_8)u4NumClient >= prGlueInfo->prP2PInfo->ucMaxClients)
			return TRUE;
		else
			return FALSE;
	}

	return FALSE;
}

VOID kalP2pUnlinkBss(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 aucBSSID[])
{
	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;
	struct cfg80211_bss *bss = NULL;

	ASSERT(prGlueInfo);
	ASSERT(aucBSSID);

	DBGLOG(P2P, INFO, "bssid: " MACSTR "\n", MAC2STR(aucBSSID));

	prGlueP2pInfo = prGlueInfo->prP2PInfo;

	if (prGlueP2pInfo == NULL)
		return;

#if (KERNEL_VERSION(4, 1, 0) <= CFG80211_VERSION_CODE)
	bss = cfg80211_get_bss(prGlueP2pInfo->prWdev->wiphy,
			NULL, /* channel */
			aucBSSID,
			NULL, /* ssid */
			0, /* ssid length */
			IEEE80211_BSS_TYPE_ESS,
			IEEE80211_PRIVACY_ANY);
#else
	bss = cfg80211_get_bss(prGlueP2pInfo->prWdev->wiphy,
			NULL, /* channel */
			aucBSSID,
			NULL, /* ssid */
			0, /* ssid length */
			WLAN_CAPABILITY_ESS,
			WLAN_CAPABILITY_ESS);
#endif

	if (bss != NULL) {
		cfg80211_unlink_bss(prGlueP2pInfo->prWdev->wiphy, bss);
		cfg80211_put_bss(prGlueP2pInfo->prWdev->wiphy, bss);
	}

	if (scanSearchBssDescByBssidAndSsid(prGlueInfo->prAdapter,
			aucBSSID, FALSE, NULL) != NULL)
		scanRemoveBssDescByBssid(prGlueInfo->prAdapter, aucBSSID);
}

void kalP2pIndicateQueuedMgmtFrame(IN P_GLUE_INFO_T prGlueInfo,
		IN struct P2P_QUEUED_ACTION_FRAME *prFrame)
{
	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;

	if ((prGlueInfo == NULL) || (prFrame == NULL))
		return;

	if (prFrame->prHeader == NULL || prFrame->u2Length == 0) {
		DBGLOG(P2P, WARN, "Frame pointer is null or length is 0.\n");
		return;
	}

	DBGLOG(P2P, INFO, "Indicate queued p2p action frame.\n");

	prGlueP2pInfo = prGlueInfo->prP2PInfo;

	cfg80211_rx_mgmt(
		/* struct net_device * dev, */
		prGlueP2pInfo->prWdev,
		prFrame->u4Freq,
		0,
		prFrame->prHeader,
		prFrame->u2Length,
		GFP_ATOMIC);
}

void kalP2pIndicateAcsResult(IN P_GLUE_INFO_T prGlueInfo,
		IN uint8_t ucPrimaryCh,
		IN uint8_t ucSecondCh,
		IN uint8_t ucSeg0Ch,
		IN uint8_t ucSeg1Ch,
		IN enum ENUM_MAX_BANDWIDTH_SETTING eChnlBw)
{
	struct sk_buff *vendor_event = NULL;
	uint16_t ch_width = MAX_BW_20MHZ;

	switch (eChnlBw) {
	case MAX_BW_20MHZ:
		ch_width = 20;
		break;
	case MAX_BW_40MHZ:
		ch_width = 40;
		break;
	default:
		DBGLOG(P2P, ERROR, "unsupport width: %d.\n", ch_width);
		break;
	}

	DBGLOG(P2P, INFO, "pri: %d, sec: %d, seg0: %d, seg1: %d, ch_width: %d\n",
			ucPrimaryCh,
			ucSecondCh,
			ucSeg0Ch,
			ucSeg1Ch,
			ch_width);

#if KERNEL_VERSION(3, 14, 0) <= CFG80211_VERSION_CODE
	vendor_event = cfg80211_vendor_event_alloc(
			prGlueInfo->prP2PInfo->prWdev->wiphy,
#if KERNEL_VERSION(4, 1, 0) <= CFG80211_VERSION_CODE
			prGlueInfo->prP2PInfo->prWdev,
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
#if KERNEL_VERSION(3, 14, 0) <= CFG80211_VERSION_CODE
	cfg80211_vendor_event(vendor_event, GFP_KERNEL);
#endif
	return;

nla_put_failure:
	if (vendor_event)
		kfree_skb(vendor_event);
}

