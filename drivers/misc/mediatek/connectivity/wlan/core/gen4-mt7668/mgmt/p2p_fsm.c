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
** Id: //Department/DaVinci/TRUNK/WiFi_P2P_Driver/mgmt/p2p_fsm.c#61
*/

/*! \file   "p2p_fsm.c"
 *  \brief  This file defines the FSM for P2P Module.
 *
 *  This file defines the FSM for P2P Module.
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 ********************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ********************************************************************************
 */
#include "precomp.h"

#if CFG_ENABLE_WIFI_DIRECT

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

VOID p2pFsmRunEventScanRequest(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_P2P_SCAN_REQUEST_T prP2pScanReqMsg = (P_MSG_P2P_SCAN_REQUEST_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));
		if ((prAdapter == NULL) || (prMsgHdr == NULL))
			break;

		prP2pScanReqMsg = (P_MSG_P2P_SCAN_REQUEST_T) prMsgHdr;

		if (prP2pScanReqMsg->ucBssIdx == P2P_DEV_BSS_INDEX)
			p2pDevFsmRunEventScanRequest(prAdapter, prMsgHdr);
		else
			p2pRoleFsmRunEventScanRequest(prAdapter, prMsgHdr);

		prMsgHdr = NULL;
		/* Both p2pDevFsmRunEventScanRequest and p2pRoleFsmRunEventScanRequest
		 * free prMsgHdr before return, so prMsgHdr is needed to be NULL.
		 */
	} while (FALSE);

	if (prMsgHdr != NULL)
		cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pDevFsmRunEventScanRequest */

/*----------------------------------------------------------------------------*/
/*!
 * \brief    This function is call when channel is granted by CNM module from FW.
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
VOID p2pFsmRunEventChGrant(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_CH_GRANT_T prMsgChGrant = (P_MSG_CH_GRANT_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		prMsgChGrant = (P_MSG_CH_GRANT_T) prMsgHdr;

		prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsgChGrant->ucBssIndex);

		DBGLOG(P2P, TRACE, "P2P Run Event Channel Grant\n");

#if CFG_SUPPORT_DBDC
		if (prP2pBssInfo->eDBDCBand == ENUM_BAND_AUTO)
			prP2pBssInfo->eDBDCBand = prMsgChGrant->eDBDCBand;
#endif

#if CFG_SISO_SW_DEVELOP
		/* Driver record granted CH in BSS info */
		prP2pBssInfo->fgIsGranted = TRUE;
		prP2pBssInfo->eBandGranted = prMsgChGrant->eRfBand;
		prP2pBssInfo->ucPrimaryChannelGranted = prMsgChGrant->ucPrimaryChannel;
#endif

		switch (prP2pBssInfo->eCurrentOPMode) {
		case OP_MODE_P2P_DEVICE:
			ASSERT(prP2pBssInfo->ucBssIndex == P2P_DEV_BSS_INDEX);
			p2pDevFsmRunEventChnlGrant(prAdapter, prMsgHdr, prAdapter->rWifiVar.prP2pDevFsmInfo);
			break;
		case OP_MODE_INFRASTRUCTURE:
		case OP_MODE_ACCESS_POINT:
			ASSERT(prP2pBssInfo->ucBssIndex < P2P_DEV_BSS_INDEX);
			p2pRoleFsmRunEventChnlGrant(prAdapter, prMsgHdr,
						    P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
										   prP2pBssInfo->u4PrivateData));
			break;
		default:
			ASSERT(FALSE);
			break;
		}
	} while (FALSE);
}				/* p2pFsmRunEventChGrant */

VOID p2pFsmRunEventNetDeviceRegister(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_P2P_NETDEV_REGISTER_T prNetDevRegisterMsg = (P_MSG_P2P_NETDEV_REGISTER_T) NULL;

	DBGLOG(P2P, TRACE, "p2pFsmRunEventNetDeviceRegister\n");

	prNetDevRegisterMsg = (P_MSG_P2P_NETDEV_REGISTER_T) prMsgHdr;

	if (prNetDevRegisterMsg->fgIsEnable) {
		p2pSetMode((prNetDevRegisterMsg->ucMode == 1) ? TRUE : FALSE);
		if (p2pLaunch(prAdapter->prGlueInfo))
			ASSERT(prAdapter->fgIsP2PRegistered);
	} else {
		if (prAdapter->fgIsP2PRegistered)
			p2pRemove(prAdapter->prGlueInfo);
	}

	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pFsmRunEventNetDeviceRegister */

VOID p2pFsmRunEventUpdateMgmtFrame(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_P2P_MGMT_FRAME_UPDATE_T prP2pMgmtFrameUpdateMsg = (P_MSG_P2P_MGMT_FRAME_UPDATE_T) NULL;

	DBGLOG(P2P, TRACE, "p2pFsmRunEventUpdateMgmtFrame\n");

	prP2pMgmtFrameUpdateMsg = (P_MSG_P2P_MGMT_FRAME_UPDATE_T) prMsgHdr;

	switch (prP2pMgmtFrameUpdateMsg->eBufferType) {
	case ENUM_FRAME_TYPE_EXTRA_IE_BEACON:
		break;
	case ENUM_FRAME_TYPE_EXTRA_IE_ASSOC_RSP:
		break;
	case ENUM_FRAME_TYPE_EXTRA_IE_PROBE_RSP:
		break;
	case ENUM_FRAME_TYPE_PROBE_RSP_TEMPLATE:
		break;
	case ENUM_FRAME_TYPE_BEACON_TEMPLATE:
		break;
	default:
		break;
	}

	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pFsmRunEventUpdateMgmtFrame */

#if CFG_SUPPORT_WFD
VOID p2pFsmRunEventWfdSettingUpdate(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_WFD_CFG_SETTINGS_T prWfdCfgSettings = (P_WFD_CFG_SETTINGS_T) NULL;
	P_MSG_WFD_CONFIG_SETTINGS_CHANGED_T prMsgWfdCfgSettings = (P_MSG_WFD_CONFIG_SETTINGS_CHANGED_T) NULL;
	UINT_32 i;

	/* WLAN_STATUS rStatus =  WLAN_STATUS_SUCCESS; */

	DBGLOG(P2P, INFO, "p2pFsmRunEventWfdSettingUpdate\n");

	do {
		ASSERT_BREAK((prAdapter != NULL));

		if (prMsgHdr != NULL) {
			prMsgWfdCfgSettings = (P_MSG_WFD_CONFIG_SETTINGS_CHANGED_T) prMsgHdr;
			prWfdCfgSettings = prMsgWfdCfgSettings->prWfdCfgSettings;
		} else {
			prWfdCfgSettings = &prAdapter->rWifiVar.rWfdConfigureSettings;
		}

		DBGLOG(P2P, INFO, "WFD Enalbe %x info %x state %x flag %x adv %x\n",
		       prWfdCfgSettings->ucWfdEnable,
		       prWfdCfgSettings->u2WfdDevInfo,
		       (UINT_32) prWfdCfgSettings->u4WfdState,
		       (UINT_32) prWfdCfgSettings->u4WfdFlag, (UINT_32) prWfdCfgSettings->u4WfdAdvancedFlag);

		if (prWfdCfgSettings->ucWfdEnable == 0)
			for (i = 0; i < KAL_P2P_NUM; i++) {
				if (prAdapter->prGlueInfo->prP2PInfo[i])
					prAdapter->prGlueInfo->prP2PInfo[i]->u2WFDIELen = 0;
			}
	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}

/* p2pFsmRunEventWfdSettingUpdate */

#endif /* CFG_SUPPORT_WFD */


/*----------------------------------------------------------------------------*/
/*!
 * \brief    This function is used to handle scan done event during Device Discovery.
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
VOID p2pFsmRunEventScanDone(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_SCN_SCAN_DONE prScanDoneMsg = (P_MSG_SCN_SCAN_DONE) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	prScanDoneMsg = (P_MSG_SCN_SCAN_DONE) prMsgHdr;

	prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prScanDoneMsg->ucBssIndex);

	if (prAdapter->fgIsP2PRegistered == FALSE) {
		DBGLOG(P2P, TRACE, "P2P BSS Info is removed, break p2pFsmRunEventScanDone\n");

		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	DBGLOG(P2P, TRACE, "P2P Scan Done Event\n");

	switch (prP2pBssInfo->eCurrentOPMode) {
	case OP_MODE_P2P_DEVICE:
		ASSERT(prP2pBssInfo->ucBssIndex == P2P_DEV_BSS_INDEX);
		p2pDevFsmRunEventScanDone(prAdapter, prMsgHdr, prAdapter->rWifiVar.prP2pDevFsmInfo);
		break;
	case OP_MODE_INFRASTRUCTURE:
	case OP_MODE_ACCESS_POINT:
		ASSERT(prP2pBssInfo->ucBssIndex < P2P_DEV_BSS_INDEX);
		p2pRoleFsmRunEventScanDone(prAdapter, prMsgHdr,
					   P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
									  prP2pBssInfo->u4PrivateData));
		break;
	default:
		ASSERT(FALSE);
		break;
	}
}				/* p2pFsmRunEventScanDone */



#endif /* CFG_ENABLE_WIFI_DIRECT */
