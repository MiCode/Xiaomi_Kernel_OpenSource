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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/common/wlan_bow.c#1
*/

/*! \file wlan_bow.c
*    \brief This file contains the 802.11 PAL commands processing routines for
*	   MediaTek Inc. 802.11 Wireless LAN Adapters.
*/

/******************************************************************************
*                         C O M P I L E R   F L A G S
*******************************************************************************
*/

/******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
*******************************************************************************
*/
#include "precomp.h"

#if CFG_ENABLE_BT_OVER_WIFI

/******************************************************************************
*                              C O N S T A N T S
*******************************************************************************
*/

/******************************************************************************
*                             D A T A   T Y P E S
*******************************************************************************
*/

/******************************************************************************
*                            P U B L I C   D A T A
*******************************************************************************
*/

#if 1				/* Marked for MT6630 */
static uint32_t g_u4LinkCount;
static uint32_t g_u4Beaconing;
static struct BOW_TABLE arBowTable[CFG_BOW_PHYSICAL_LINK_NUM];
#endif

/******************************************************************************
*                           P R I V A T E   D A T A
*******************************************************************************
*/

const struct BOW_CMD arBowCmdTable[] = {
	{BOW_CMD_ID_GET_MAC_STATUS, bowCmdGetMacStatus},
	{BOW_CMD_ID_SETUP_CONNECTION, bowCmdSetupConnection},
	{BOW_CMD_ID_DESTROY_CONNECTION, bowCmdDestroyConnection},
	{BOW_CMD_ID_SET_PTK, bowCmdSetPTK},
	{BOW_CMD_ID_READ_RSSI, bowCmdReadRSSI},
	{BOW_CMD_ID_READ_LINK_QUALITY, bowCmdReadLinkQuality},
	{BOW_CMD_ID_SHORT_RANGE_MODE, bowCmdShortRangeMode},
	{BOW_CMD_ID_GET_CHANNEL_LIST, bowCmdGetChannelList},
};

/******************************************************************************
*                                 M A C R O S
*******************************************************************************
*/

/******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
*******************************************************************************
*/

/******************************************************************************
*                              F U N C T I O N S
*******************************************************************************
*/

#if 1				/* Marked for MT6630 */
/*----------------------------------------------------------------------------*/
/*!
* \brief command packet generation utility
*
* \param[in] prAdapter          Pointer to the Adapter structure.
* \param[in] ucCID              Command ID
* \param[in] fgSetQuery         Set or Query
* \param[in] fgNeedResp         Need for response
* \param[in] pfCmdDoneHandler   Function pointer when command is done
* \param[in] u4SetQueryInfoLen  The length of the set/query buffer
* \param[in] pucInfoBuffer      Pointer to set/query buffer
*
*
* \retval WLAN_STATUS_PENDING
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidSendSetQueryBowCmd(IN struct ADAPTER *prAdapter,
			  IN uint8_t ucCID,
			  IN uint8_t ucBssIdx,
			  IN u_int8_t fgSetQuery,
			  IN u_int8_t fgNeedResp,
			  IN PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
			  IN PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler,
			  IN uint32_t u4SetQueryInfoLen, IN uint8_t *pucInfoBuffer, IN uint8_t ucSeqNumber)
{
	struct GLUE_INFO *prGlueInfo;
	struct CMD_INFO *prCmdInfo;
	u_int8_t *pWifiCmdBufAddr;
	struct mt66xx_chip_info *prChipInfo;
	uint16_t cmd_size;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;
	prChipInfo = prAdapter->chip_info;
	ASSERT(prGlueInfo);

	DBGLOG(REQ, TRACE, "Command ID = 0x%08X\n", ucCID);

	cmd_size = prChipInfo->u2CmdTxHdrSize + u4SetQueryInfoLen;
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, cmd_size);

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	/* Setup common CMD Info Packet */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->u2InfoBufLen = cmd_size;
	prCmdInfo->pfCmdDoneHandler = pfCmdDoneHandler;
	prCmdInfo->pfCmdTimeoutHandler = pfCmdTimeoutHandler;
	prCmdInfo->fgIsOid = FALSE;
	prCmdInfo->ucCID = ucCID;
	prCmdInfo->fgSetQuery = fgSetQuery;
	prCmdInfo->fgNeedResp = fgNeedResp;
	prCmdInfo->u4SetInfoLen = u4SetQueryInfoLen;
	prCmdInfo->pvInformationBuffer = NULL;
	prCmdInfo->u4InformationBufferLength = 0;
	prCmdInfo->u4PrivateData = (uint32_t) ucSeqNumber;

	/* Setup WIFI_CMD (no payload) */
	NIC_FILL_CMD_TX_HDR(prAdapter,
		prCmdInfo->pucInfoBuffer,
		prCmdInfo->u2InfoBufLen,
		prCmdInfo->ucCID,
		CMD_PACKET_TYPE_ID,
		&prCmdInfo->ucCmdSeqNum,
		prCmdInfo->fgSetQuery,
		&pWifiCmdBufAddr, FALSE, 0, S2D_INDEX_CMD_H2N);

	if (u4SetQueryInfoLen > 0 && pucInfoBuffer != NULL)
		kalMemCopy(pWifiCmdBufAddr, pucInfoBuffer, u4SetQueryInfoLen);
	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (struct QUE_ENTRY *) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);
	return WLAN_STATUS_PENDING;
}

#endif /* Marked for MT6630 */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to dispatch command coming from 802.11 PAL
*
* \param[in] prAdapter  Pointer to the Adapter structure.
* \param[in] prCmd      Pointer to the buffer that holds the command
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
uint32_t wlanbowHandleCommand(IN struct ADAPTER *prAdapter, IN struct BT_OVER_WIFI_COMMAND *prCmd)
{
#if 1				/* Marked for MT6630 */
	uint32_t retval = WLAN_STATUS_FAILURE;
	uint16_t i;

	ASSERT(prAdapter);

	for (i = 0; i < sizeof(arBowCmdTable) / sizeof(struct BOW_CMD); i++) {
		if ((arBowCmdTable[i].uCmdID == prCmd->rHeader.ucCommandId) && arBowCmdTable[i].pfCmdHandle) {
			retval = arBowCmdTable[i].pfCmdHandle(prAdapter, prCmd);
			break;
		}
	}

	return retval;

#else
	return 0;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This is command handler for BOW_CMD_ID_GET_MAC_STATUS
*        coming from 802.11 PAL
*
* \param[in] prAdapter  Pointer to the Adapter structure.
* \param[in] prCmd      Pointer to the buffer that holds the command
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
uint32_t bowCmdGetMacStatus(IN struct ADAPTER *prAdapter, IN struct BT_OVER_WIFI_COMMAND *prCmd)
{
#if 1				/* Marked for MT6630 */
	struct BT_OVER_WIFI_EVENT *prEvent;
	struct BOW_MAC_STATUS *prMacStatus;
	uint8_t idx = 0;
	uint8_t ucPrimaryChannel;
	enum ENUM_BAND eBand;
	enum ENUM_CHNL_EXT eBssSCO;
	uint8_t ucNumOfChannel = 0;	/* MAX_BOW_NUMBER_OF_CHANNEL; */

	struct RF_CHANNEL_INFO aucChannelList[MAX_BOW_NUMBER_OF_CHANNEL];

	ASSERT(prAdapter);

	/* 3 <1> If LinkCount != 0 -> OK (optional) */

	eBand = BAND_2G4;
	eBssSCO = CHNL_EXT_SCN;

	/* fill event header */
	prEvent = (struct BT_OVER_WIFI_EVENT *) kalMemAlloc((sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_MAC_STATUS)), VIR_MEM_TYPE);

	prEvent->rHeader.ucEventId = BOW_EVENT_ID_MAC_STATUS;
	prEvent->rHeader.ucSeqNumber = prCmd->rHeader.ucSeqNumber;
	prEvent->rHeader.u2PayloadLength = sizeof(struct BOW_MAC_STATUS);

	/* fill event body */
	prMacStatus = (struct BOW_MAC_STATUS *) (prEvent->aucPayload);
	kalMemZero(prMacStatus, sizeof(struct BOW_MAC_STATUS));

	/* 3 <2> Call CNM to decide if BOW available. */
	if (cnmBowIsPermitted(prAdapter))
		prMacStatus->ucAvailability = TRUE;
	else
		prMacStatus->ucAvailability = FALSE;

	memcpy(prMacStatus->aucMacAddr, prAdapter->rWifiVar.aucDeviceAddress, PARAM_MAC_ADDR_LEN);

	if (cnmPreferredChannel(prAdapter, &eBand, &ucPrimaryChannel, &eBssSCO)) {
		DBGLOG(BOW, EVENT, "bowCmdGetMacStatus, Get preferred channel.\n");

		prMacStatus->ucNumOfChannel = 1;
		prMacStatus->arChannelList[0].ucChannelBand = eBand;
		prMacStatus->arChannelList[0].ucChannelNum = ucPrimaryChannel;
	} else {
		DBGLOG(BOW, EVENT,
		       "bowCmdGetMacStatus, Get channel list. Current number of channel, %d.\n", ucNumOfChannel);

		rlmDomainGetChnlList(prAdapter, BAND_2G4, FALSE, MAX_BOW_NUMBER_OF_CHANNEL_2G4,
				     &ucNumOfChannel, aucChannelList);

		if (ucNumOfChannel > 0) {
			for (idx = 0; idx < ucNumOfChannel /*MAX_BOW_NUMBER_OF_CHANNEL_2G4 */;
			     idx++) {
				prMacStatus->arChannelList[idx].ucChannelBand = aucChannelList[idx].eBand;
				prMacStatus->arChannelList[idx].ucChannelNum = aucChannelList[idx].ucChannelNum;
			}

			prMacStatus->ucNumOfChannel = ucNumOfChannel;
		}

		rlmDomainGetChnlList(prAdapter, BAND_5G, FALSE,
			MAX_BOW_NUMBER_OF_CHANNEL_5G, &ucNumOfChannel, aucChannelList);

		if (ucNumOfChannel > 0) {
			for (idx = 0; idx < ucNumOfChannel /*MAX_BOW_NUMBER_OF_CHANNEL_5G */;
			     idx++) {
				prMacStatus->arChannelList[prMacStatus->ucNumOfChannel +
							   idx].ucChannelBand = aucChannelList[idx].eBand;
				prMacStatus->arChannelList[prMacStatus->ucNumOfChannel +
							   idx].ucChannelNum = aucChannelList[idx].ucChannelNum;
			}

			prMacStatus->ucNumOfChannel = prMacStatus->ucNumOfChannel + ucNumOfChannel;

		}
	}

	DBGLOG(BOW, EVENT,
	       "ucNumOfChannel,eBand,aucChannelList,%x,%x,%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x.\n",
	       ucNumOfChannel, aucChannelList[0].eBand, aucChannelList[0].ucChannelNum,
	       aucChannelList[1].ucChannelNum, aucChannelList[2].ucChannelNum,
	       aucChannelList[3].ucChannelNum, aucChannelList[4].ucChannelNum,
	       aucChannelList[5].ucChannelNum, aucChannelList[6].ucChannelNum,
	       aucChannelList[7].ucChannelNum, aucChannelList[8].ucChannelNum,
	       aucChannelList[9].ucChannelNum, aucChannelList[10].ucChannelNum,
	       aucChannelList[11].ucChannelNum, aucChannelList[12].ucChannelNum,
	       aucChannelList[13].ucChannelNum, aucChannelList[14].ucChannelNum,
	       aucChannelList[15].ucChannelNum, aucChannelList[16].ucChannelNum, aucChannelList[17].ucChannelNum);

	DBGLOG(BOW, EVENT,
	       "prMacStatus->ucNumOfChannel, eBand, %x, %x.\n",
	       prMacStatus->ucNumOfChannel, prMacStatus->arChannelList[0].ucChannelBand);
	DBGLOG(BOW, EVENT,
	       "prMacStatus->arChannelList, %x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x.\n",
	       prMacStatus->arChannelList[0].ucChannelNum,
	       prMacStatus->arChannelList[1].ucChannelNum,
	       prMacStatus->arChannelList[2].ucChannelNum,
	       prMacStatus->arChannelList[3].ucChannelNum,
	       prMacStatus->arChannelList[4].ucChannelNum,
	       prMacStatus->arChannelList[5].ucChannelNum,
	       prMacStatus->arChannelList[6].ucChannelNum,
	       prMacStatus->arChannelList[7].ucChannelNum,
	       prMacStatus->arChannelList[8].ucChannelNum,
	       prMacStatus->arChannelList[9].ucChannelNum,
	       prMacStatus->arChannelList[10].ucChannelNum,
	       prMacStatus->arChannelList[11].ucChannelNum,
	       prMacStatus->arChannelList[12].ucChannelNum,
	       prMacStatus->arChannelList[13].ucChannelNum,
	       prMacStatus->arChannelList[14].ucChannelNum,
	       prMacStatus->arChannelList[15].ucChannelNum,
	       prMacStatus->arChannelList[16].ucChannelNum, prMacStatus->arChannelList[17].ucChannelNum);

	DBGLOG(BOW, EVENT, "prMacStatus->ucNumOfChannel, %x.\n", prMacStatus->ucNumOfChannel);
	DBGLOG(BOW, EVENT,
	       "prMacStatus->arChannelList[0].ucChannelBand, %x.\n", prMacStatus->arChannelList[0].ucChannelBand);
	DBGLOG(BOW, EVENT,
	       "prMacStatus->arChannelList[0].ucChannelNum, %x.\n", prMacStatus->arChannelList[0].ucChannelNum);
	DBGLOG(BOW, EVENT, "prMacStatus->ucAvailability, %x.\n", prMacStatus->ucAvailability);
	DBGLOG(BOW, EVENT, "prMacStatus->aucMacAddr, %x:%x:%x:%x:%x:%x.\n",
	       prMacStatus->aucMacAddr[0],
	       prMacStatus->aucMacAddr[1],
	       prMacStatus->aucMacAddr[2],
	       prMacStatus->aucMacAddr[3], prMacStatus->aucMacAddr[4], prMacStatus->aucMacAddr[5]);

	kalIndicateBOWEvent(prAdapter->prGlueInfo, prEvent);

	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_MAC_STATUS)));

	return WLAN_STATUS_SUCCESS;

#else
	return 0;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This is command handler for BOW_CMD_ID_SETUP_CONNECTION
*        coming from 802.11 PAL
*
* \param[in] prAdapter  Pointer to the Adapter structure.
* \param[in] prCmd      Pointer to the buffer that holds the command
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
uint32_t bowCmdSetupConnection(IN struct ADAPTER *prAdapter, IN struct BT_OVER_WIFI_COMMAND *prCmd)
{
#if 1				/* Marked for MT6630 */
	struct BOW_SETUP_CONNECTION *prBowSetupConnection;
	struct CMD_BT_OVER_WIFI rCmdBtOverWifi;
	struct BOW_FSM_INFO *prBowFsmInfo;
	struct BOW_TABLE rBowTable;

	uint8_t ucBowTableIdx = 0;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBowSetupConnection = (struct BOW_SETUP_CONNECTION *) &(prCmd->aucPayload[0]);

	/* parameter size check */
	if (prCmd->rHeader.u2PayloadLength != sizeof(struct BOW_SETUP_CONNECTION)) {
		wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_INVALID);
		return WLAN_STATUS_INVALID_LENGTH;
	}
	/* 3 <1> If ucLinkCount >= 4 -> Fail. */
	if (g_u4LinkCount >= CFG_BOW_PHYSICAL_LINK_NUM) {
		wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_UNACCEPTED);
		return WLAN_STATUS_NOT_ACCEPTED;
	}
	/* 3 <2> Call CNM, check if BOW is available. */
	if (!cnmBowIsPermitted(prAdapter)) {
		wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_UNACCEPTED);
		return WLAN_STATUS_NOT_ACCEPTED;
	}
	/* 3 <3> Lookup BOW Table, if Peer MAC address exist and valid -> Fail. */
	if (bowCheckBowTableIfVaild(prAdapter, prBowSetupConnection->aucPeerAddress)) {
		wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_UNACCEPTED);
		return WLAN_STATUS_NOT_ACCEPTED;
	}

	if (EQUAL_MAC_ADDR(prBowSetupConnection->aucPeerAddress, prAdapter->rWifiVar.aucDeviceAddress)) {
		wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_INVALID);
		return WLAN_STATUS_NOT_ACCEPTED;
	}

	/* fill CMD_BT_OVER_WIFI */
	rCmdBtOverWifi.ucAction = BOW_SETUP_CMD;
	rCmdBtOverWifi.ucChannelNum = prBowSetupConnection->ucChannelNum;
	COPY_MAC_ADDR(rCmdBtOverWifi.rPeerAddr, prBowSetupConnection->aucPeerAddress);
	rCmdBtOverWifi.u2BeaconInterval = prBowSetupConnection->u2BeaconInterval;
	rCmdBtOverWifi.ucTimeoutDiscovery = prBowSetupConnection->ucTimeoutDiscovery;
	rCmdBtOverWifi.ucTimeoutInactivity = prBowSetupConnection->ucTimeoutInactivity;
	rCmdBtOverWifi.ucRole = prBowSetupConnection->ucRole;
	rCmdBtOverWifi.PAL_Capabilities = prBowSetupConnection->ucPAL_Capabilities;
	rCmdBtOverWifi.cMaxTxPower = prBowSetupConnection->cMaxTxPower;

	if (prBowSetupConnection->ucChannelNum > 14)
		rCmdBtOverWifi.ucChannelBand = BAND_5G;
	else
		rCmdBtOverWifi.ucChannelBand = BAND_2G4;

	COPY_MAC_ADDR(prBowFsmInfo->aucPeerAddress, prBowSetupConnection->aucPeerAddress);

#if CFG_BOW_PHYSICAL_LINK_NUM > 1
	/*Channel check for supporting multiple physical link */
	if (g_u4LinkCount > 0) {
		if (prBowSetupConnection->ucChannelNum != prBowFsmInfo->ucPrimaryChannel) {
			wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_UNACCEPTED);
			return WLAN_STATUS_NOT_ACCEPTED;
		}
	}
#endif

	prBowFsmInfo->ucPrimaryChannel = prBowSetupConnection->ucChannelNum;
	prBowFsmInfo->eBand = rCmdBtOverWifi.ucChannelBand;
	prBowFsmInfo->u2BeaconInterval = prBowSetupConnection->u2BeaconInterval;
	prBowFsmInfo->ucRole = prBowSetupConnection->ucRole;

	if (prBowSetupConnection->ucPAL_Capabilities > 0)
		prBowFsmInfo->fgSupportQoS = TRUE;

	DBGLOG(BOW, EVENT, "bowCmdSetupConnection.\n");
	DBGLOG(BOW, EVENT, "rCmdBtOverWifi Channel Number - 0x%x.\n", rCmdBtOverWifi.ucChannelNum);
	DBGLOG(BOW, EVENT,
	       "rCmdBtOverWifi Peer address - %x:%x:%x:%x:%x:%x.\n", rCmdBtOverWifi.rPeerAddr[0],
	       rCmdBtOverWifi.rPeerAddr[1], rCmdBtOverWifi.rPeerAddr[2],
	       rCmdBtOverWifi.rPeerAddr[3], rCmdBtOverWifi.rPeerAddr[4], rCmdBtOverWifi.rPeerAddr[5]);
	DBGLOG(BOW, EVENT, "rCmdBtOverWifi Beacon interval - 0x%x.\n", rCmdBtOverWifi.u2BeaconInterval);
	DBGLOG(BOW, EVENT, "rCmdBtOverWifi Timeout activity - 0x%x.\n", rCmdBtOverWifi.ucTimeoutDiscovery);
	DBGLOG(BOW, EVENT, "rCmdBtOverWifi Timeout inactivity - 0x%x.\n", rCmdBtOverWifi.ucTimeoutInactivity);
	DBGLOG(BOW, EVENT, "rCmdBtOverWifi Role - 0x%x.\n", rCmdBtOverWifi.ucRole);
	DBGLOG(BOW, EVENT, "rCmdBtOverWifi PAL capability - 0x%x.\n", rCmdBtOverWifi.PAL_Capabilities);
	DBGLOG(BOW, EVENT, "rCmdBtOverWifi Max Tx power - 0x%x.\n", rCmdBtOverWifi.cMaxTxPower);

	/* 3 <4> Get a free BOW entry, mark as Valid, fill in Peer MAC address, LinkCount += 1, state == Starting. */
	if (!bowGetBowTableFreeEntry(prAdapter, &ucBowTableIdx)) {
		wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_UNACCEPTED);
		return WLAN_STATUS_NOT_ACCEPTED;
	}

	prBowFsmInfo->prTargetBssDesc = NULL;

	COPY_MAC_ADDR(rBowTable.aucPeerAddress, prBowSetupConnection->aucPeerAddress);
	/* owTable.eState = BOW_DEVICE_STATE_ACQUIRING_CHANNEL; */
	rBowTable.fgIsValid = TRUE;
	rBowTable.eState = BOW_DEVICE_STATE_NUM; /* Just initiate */
	rBowTable.ucAcquireID = prBowFsmInfo->ucSeqNumOfChReq;
	/* rBowTable.ucRole = prBowSetupConnection->ucRole; */
	/* rBowTable.ucChannelNum = prBowSetupConnection->ucChannelNum; */
	bowSetBowTableContent(prAdapter, ucBowTableIdx, &rBowTable);

	kalSetBowRole(prAdapter->prGlueInfo, rCmdBtOverWifi.ucRole, prBowSetupConnection->aucPeerAddress);

	GLUE_INC_REF_CNT(g_u4LinkCount);

	DBGLOG(BOW, EVENT, "bowStarting, g_u4LinkCount, %x.\n", g_u4LinkCount);

	if (g_u4LinkCount == 1) {
		DBGLOG(BOW, EVENT, "bowStarting, cnmTimerInitTimer.\n");
		DBGLOG(BOW, EVENT, "prBowFsmInfo->u2BeaconInterval, %d.\n", prBowFsmInfo->u2BeaconInterval);

		cnmTimerInitTimer(prAdapter,
				  &prBowFsmInfo->rStartingBeaconTimer,
				  (PFN_MGMT_TIMEOUT_FUNC) bowSendBeacon, (unsigned long) NULL);

		cnmTimerInitTimer(prAdapter,
				  &prBowFsmInfo->rChGrantedTimer,
				  (PFN_MGMT_TIMEOUT_FUNC) bowChGrantedTimeout, (unsigned long) NULL);

		/* Reset Global Variable */
		g_u4Beaconing = 0;

		DBGLOG(BOW, EVENT, "bowCmdSetupConnection, g_u4LinkCount, %x.\n", g_u4LinkCount);
		DBGLOG(BOW, EVENT, "kalInitBowDevice, bow0\n");

#if CFG_BOW_SEPARATE_DATA_PATH
		kalInitBowDevice(prAdapter->prGlueInfo, BOWDEVNAME);
#endif

		/*Active BoW Network */
		SET_NET_ACTIVE(prAdapter, prBowFsmInfo->ucBssIndex);
		SET_NET_PWR_STATE_ACTIVE(prAdapter, prBowFsmInfo->ucBssIndex);
		nicActivateNetwork(prAdapter, prBowFsmInfo->ucBssIndex);

	}

	if (rCmdBtOverWifi.ucRole == BOW_INITIATOR) {
		bowSetBowTableState(prAdapter, prBowSetupConnection->aucPeerAddress,
				    BOW_DEVICE_STATE_ACQUIRING_CHANNEL);
		bowRequestCh(prAdapter);
	} else {
		bowSetBowTableState(prAdapter, prBowSetupConnection->aucPeerAddress, BOW_DEVICE_STATE_SCANNING);
		bowResponderScan(prAdapter);
	}

	wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_SUCCESS);

	return WLAN_STATUS_SUCCESS;

#else
	return 0;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This is command handler for BOW_CMD_ID_DESTROY_CONNECTION
*        coming from 802.11 PAL
*
* \param[in] prAdapter  Pointer to the Adapter structure.
* \param[in] prCmd      Pointer to the buffer that holds the command
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
uint32_t bowCmdDestroyConnection(IN struct ADAPTER *prAdapter, IN struct BT_OVER_WIFI_COMMAND *prCmd)
{
#if 1				/* Marked for MT6630 */
	struct BOW_DESTROY_CONNECTION *prBowDestroyConnection;
	struct CMD_BT_OVER_WIFI rCmdBtOverWifi;
	struct BOW_FSM_INFO *prBowFsmInfo;

	uint8_t ucIdx;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	/* 3 <1> If LinkCount == 0 ->Fail (Optional) */
	if (g_u4LinkCount == 0) {
		wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_UNACCEPTED);
		return WLAN_STATUS_NOT_ACCEPTED;
	}
	/* parameter size check */
	if (prCmd->rHeader.u2PayloadLength != sizeof(struct BOW_DESTROY_CONNECTION)) {
		wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_UNACCEPTED);
		return WLAN_STATUS_INVALID_LENGTH;
	}
	/* 3 <2> Lookup BOW table, check if is not exist (Valid and Peer MAC address) -> Fail */
	prBowDestroyConnection = (struct BOW_DESTROY_CONNECTION *) &(prCmd->aucPayload[0]);

	if (!bowCheckBowTableIfVaild(prAdapter, prBowDestroyConnection->aucPeerAddress)) {
		DBGLOG(BOW, EVENT, "bowCmdDestroyConnection, bowCheckIfVaild, not accepted.\n");
		return WLAN_STATUS_NOT_ACCEPTED;
	}

	DBGLOG(BOW, EVENT,
	       "bowCmdDestroyConnection, destroy Peer address - %x:%x:%x:%x:%x:%x.\n",
	       prBowDestroyConnection->aucPeerAddress[0],
	       prBowDestroyConnection->aucPeerAddress[1],
	       prBowDestroyConnection->aucPeerAddress[2],
	       prBowDestroyConnection->aucPeerAddress[3],
	       prBowDestroyConnection->aucPeerAddress[4], prBowDestroyConnection->aucPeerAddress[5]);

	/* fill CMD_BT_OVER_WIFI */
	rCmdBtOverWifi.ucAction = 2;
	COPY_MAC_ADDR(rCmdBtOverWifi.rPeerAddr, prBowDestroyConnection->aucPeerAddress);
	COPY_MAC_ADDR(prBowFsmInfo->aucPeerAddress, prBowDestroyConnection->aucPeerAddress);

	DBGLOG(BOW, EVENT,
	       "bowCmdDestroyConnection, rCmdBtOverWifi.rPeerAddr - %x:%x:%x:%x:%x:%x.\n",
	       rCmdBtOverWifi.rPeerAddr[0], rCmdBtOverWifi.rPeerAddr[1],
	       rCmdBtOverWifi.rPeerAddr[2], rCmdBtOverWifi.rPeerAddr[3],
	       rCmdBtOverWifi.rPeerAddr[4], rCmdBtOverWifi.rPeerAddr[5]);

	for (ucIdx = 0; ucIdx < 11; ucIdx++) {
		DBGLOG(BOW, EVENT,
		       "BoW receiving PAL packet delta time vs packet number -- %d ms vs %x.\n",
		       ucIdx, g_arBowRevPalPacketTime[ucIdx]);
	}

	wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_SUCCESS);

	return wlanoidSendSetQueryBowCmd(prAdapter,
					 CMD_ID_CMD_BT_OVER_WIFI,
					 prBowFsmInfo->ucBssIndex,
					 TRUE,
					 FALSE,
					 wlanbowCmdEventLinkDisconnected,
					 wlanbowCmdTimeoutHandler,
					 sizeof(struct CMD_BT_OVER_WIFI),
					 (uint8_t *)&rCmdBtOverWifi, prCmd->rHeader.ucSeqNumber);

#else
	return 0;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This is command handler for BOW_CMD_ID_SET_PTK
*        coming from 802.11 PAL
*
* \param[in] prAdapter  Pointer to the Adapter structure.
* \param[in] prCmd      Pointer to the buffer that holds the command
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
uint32_t bowCmdSetPTK(IN struct ADAPTER *prAdapter, IN struct BT_OVER_WIFI_COMMAND *prCmd)
{
#if 1				/* Marked for MT6630 */
	struct BOW_SET_PTK *prBowSetPTK;
	struct CMD_802_11_KEY rCmdKey;
	struct BOW_FSM_INFO *prBowFsmInfo;
	struct STA_RECORD *prStaRec = NULL;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	/* parameter size check */
	if (prCmd->rHeader.u2PayloadLength != sizeof(struct BOW_SET_PTK))
		return WLAN_STATUS_INVALID_LENGTH;

	prBowSetPTK = (struct BOW_SET_PTK *) &(prCmd->aucPayload[0]);

	DBGLOG(BOW, EVENT, "prBowSetPTK->aucPeerAddress, %x:%x:%x:%x:%x:%x.\n",
	       prBowSetPTK->aucPeerAddress[0],
	       prBowSetPTK->aucPeerAddress[1],
	       prBowSetPTK->aucPeerAddress[2],
	       prBowSetPTK->aucPeerAddress[3], prBowSetPTK->aucPeerAddress[4], prBowSetPTK->aucPeerAddress[5]);

	DBGLOG(BOW, EVENT,
	       "rCmdKey.ucIsAuthenticator, %x.\n", kalGetBowRole(prAdapter->prGlueInfo, prBowSetPTK->aucPeerAddress));

	if (!bowCheckBowTableIfVaild(prAdapter, prBowSetPTK->aucPeerAddress)) {
		wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_UNACCEPTED);

		return WLAN_STATUS_NOT_ACCEPTED;
	}

	if (bowGetBowTableState(prAdapter, prBowSetPTK->aucPeerAddress) != BOW_DEVICE_STATE_CONNECTED) {
		wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_FAILURE);

		return WLAN_STATUS_NOT_ACCEPTED;
	}
	/* fill CMD_802_11_KEY */
	rCmdKey.ucAddRemove = 1;	/* add */
	rCmdKey.ucTxKey = 1;
	rCmdKey.ucKeyType = 1;
	rCmdKey.ucIsAuthenticator = kalGetBowRole(prAdapter->prGlueInfo, prBowSetPTK->aucPeerAddress);
	COPY_MAC_ADDR(rCmdKey.aucPeerAddr, prBowSetPTK->aucPeerAddress);
	rCmdKey.ucBssIdx = prBowFsmInfo->ucBssIndex;	/* BT Over Wi-Fi */
	rCmdKey.ucAlgorithmId = CIPHER_SUITE_CCMP;	/* AES */
	rCmdKey.ucKeyId = 0;
	rCmdKey.ucKeyLen = 16;	/* AES = 128bit */
	kalMemCopy(rCmdKey.aucKeyMaterial, prBowSetPTK->aucTemporalKey, 16);

	/* BT Over Wi-Fi */
	prStaRec = cnmGetStaRecByAddress(prAdapter, prBowFsmInfo->ucBssIndex, prBowSetPTK->aucPeerAddress);
	rCmdKey.ucWlanIndex = prStaRec->ucWlanIndex;

	DBGLOG(BOW, EVENT,
	       "prBowSetPTK->aucTemporalKey, %x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x.\n",
	       prBowSetPTK->aucTemporalKey[0], prBowSetPTK->aucTemporalKey[1],
	       prBowSetPTK->aucTemporalKey[2], prBowSetPTK->aucTemporalKey[3],
	       prBowSetPTK->aucTemporalKey[4], prBowSetPTK->aucTemporalKey[5],
	       prBowSetPTK->aucTemporalKey[6], prBowSetPTK->aucTemporalKey[7],
	       prBowSetPTK->aucTemporalKey[8], prBowSetPTK->aucTemporalKey[9],
	       prBowSetPTK->aucTemporalKey[10], prBowSetPTK->aucTemporalKey[11],
	       prBowSetPTK->aucTemporalKey[12], prBowSetPTK->aucTemporalKey[13],
	       prBowSetPTK->aucTemporalKey[14], prBowSetPTK->aucTemporalKey[15]);

	DBGLOG(BOW, EVENT,
	       "rCmdKey.aucKeyMaterial, %x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x.\n",
	       rCmdKey.aucKeyMaterial[0], rCmdKey.aucKeyMaterial[1], rCmdKey.aucKeyMaterial[2],
	       rCmdKey.aucKeyMaterial[3], rCmdKey.aucKeyMaterial[4], rCmdKey.aucKeyMaterial[5],
	       rCmdKey.aucKeyMaterial[6], rCmdKey.aucKeyMaterial[7], rCmdKey.aucKeyMaterial[8],
	       rCmdKey.aucKeyMaterial[9], rCmdKey.aucKeyMaterial[10], rCmdKey.aucKeyMaterial[11],
	       rCmdKey.aucKeyMaterial[12], rCmdKey.aucKeyMaterial[13], rCmdKey.aucKeyMaterial[14],
	       rCmdKey.aucKeyMaterial[15]);

	return wlanoidSendSetQueryBowCmd(prAdapter,
					 CMD_ID_ADD_REMOVE_KEY,
					 prBowFsmInfo->ucBssIndex,
					 TRUE,
					 FALSE,
					 wlanbowCmdEventSetCommon,
					 wlanbowCmdTimeoutHandler,
					 sizeof(struct CMD_802_11_KEY), (uint8_t *)&rCmdKey, prCmd->rHeader.ucSeqNumber);
#else
	return 0;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This is command handler for BOW_CMD_ID_READ_RSSI
*        coming from 802.11 PAL
*
* \param[in] prAdapter  Pointer to the Adapter structure.
* \param[in] prCmd      Pointer to the buffer that holds the command
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
uint32_t bowCmdReadRSSI(IN struct ADAPTER *prAdapter, IN struct BT_OVER_WIFI_COMMAND *prCmd)
{
#if 1				/* Marked for MT6630 */
	struct BOW_READ_RSSI *prBowReadRSSI;
	struct BOW_FSM_INFO *prBowFsmInfo;

	ASSERT(prAdapter);

	if (prAdapter->fgIsEnableLpdvt)
		return WLAN_STATUS_NOT_SUPPORTED;

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	/* parameter size check */
	if (prCmd->rHeader.u2PayloadLength != sizeof(struct BOW_READ_RSSI))
		return WLAN_STATUS_INVALID_LENGTH;

	prBowReadRSSI = (struct BOW_READ_RSSI *) &(prCmd->aucPayload[0]);

	return wlanoidSendSetQueryBowCmd(prAdapter,
					 CMD_ID_GET_LINK_QUALITY,
					 prBowFsmInfo->ucBssIndex,
					 FALSE,
					 TRUE,
					 wlanbowCmdEventReadRssi,
					 wlanbowCmdTimeoutHandler, 0, NULL, prCmd->rHeader.ucSeqNumber);

#else
	return 0;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This is command handler for BOW_CMD_ID_READ_LINK_QUALITY
*        coming from 802.11 PAL
*
* \param[in] prAdapter  Pointer to the Adapter structure.
* \param[in] prCmd      Pointer to the buffer that holds the command
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
uint32_t bowCmdReadLinkQuality(IN struct ADAPTER *prAdapter, IN struct BT_OVER_WIFI_COMMAND *prCmd)
{
#if 1				/* Marked for MT6630 */
	struct BOW_READ_LINK_QUALITY *prBowReadLinkQuality;
	struct BOW_FSM_INFO *prBowFsmInfo;

	ASSERT(prAdapter);

	if (prAdapter->fgIsEnableLpdvt)
		return WLAN_STATUS_NOT_SUPPORTED;

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	/* parameter size check */
	if (prCmd->rHeader.u2PayloadLength != sizeof(struct BOW_READ_LINK_QUALITY *))
		return WLAN_STATUS_INVALID_LENGTH;

	prBowReadLinkQuality = (struct BOW_READ_LINK_QUALITY *) &(prCmd->aucPayload[0]);

	return wlanoidSendSetQueryBowCmd(prAdapter,
					 CMD_ID_GET_LINK_QUALITY,
					 prBowFsmInfo->ucBssIndex,
					 FALSE,
					 TRUE,
					 wlanbowCmdEventReadLinkQuality,
					 wlanbowCmdTimeoutHandler, 0, NULL, prCmd->rHeader.ucSeqNumber);

#else
	return 0;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This is command handler for BOW_CMD_ID_SHORT_RANGE_MODE
*        coming from 802.11 PAL
*
* \param[in] prAdapter  Pointer to the Adapter structure.
* \param[in] prCmd      Pointer to the buffer that holds the command
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
uint32_t bowCmdShortRangeMode(IN struct ADAPTER *prAdapter, IN struct BT_OVER_WIFI_COMMAND *prCmd)
{
#if 1				/* Marked for MT6630 */
	struct BOW_SHORT_RANGE_MODE *prBowShortRangeMode;
	struct CMD_TX_PWR rTxPwrParam;

	ASSERT(prAdapter);

	DBGLOG(BOW, EVENT, "bowCmdShortRangeMode.\n");

	prBowShortRangeMode = (struct BOW_SHORT_RANGE_MODE *) &(prCmd->aucPayload[0]);

	/* parameter size check */
	if (prCmd->rHeader.u2PayloadLength != sizeof(struct BOW_SHORT_RANGE_MODE)) {
		wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_UNACCEPTED);
		return WLAN_STATUS_INVALID_LENGTH;
	}

	if (!bowCheckBowTableIfVaild(prAdapter, prBowShortRangeMode->aucPeerAddress)) {
		wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_UNACCEPTED);
		return WLAN_STATUS_NOT_ACCEPTED;
	}

	if (bowGetBowTableState(prAdapter, prBowShortRangeMode->aucPeerAddress) != BOW_DEVICE_STATE_CONNECTED) {
		wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_FAILURE);
		return WLAN_STATUS_NOT_ACCEPTED;
	}

	DBGLOG(BOW, EVENT, "prBowShortRangeMode->aucPeerAddress, %x:%x:%x:%x:%x:%x.\n",
	       prBowShortRangeMode->aucPeerAddress[0],
	       prBowShortRangeMode->aucPeerAddress[1],
	       prBowShortRangeMode->aucPeerAddress[2],
	       prBowShortRangeMode->aucPeerAddress[3],
	       prBowShortRangeMode->aucPeerAddress[4], prBowShortRangeMode->aucPeerAddress[5]);

	rTxPwrParam.cTxPwr2G4Cck = (prBowShortRangeMode->cTxPower << 1);

	rTxPwrParam.cTxPwr2G4OFDM_BPSK = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr2G4OFDM_QPSK = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr2G4OFDM_16QAM = (prBowShortRangeMode->cTxPower << 1);

	rTxPwrParam.cTxPwr2G4OFDM_48Mbps = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr2G4OFDM_54Mbps = (prBowShortRangeMode->cTxPower << 1);

	rTxPwrParam.cTxPwr2G4HT20_BPSK = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr2G4HT20_QPSK = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr2G4HT20_16QAM = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr2G4HT20_MCS5 = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr2G4HT20_MCS6 = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr2G4HT20_MCS7 = (prBowShortRangeMode->cTxPower << 1);

	rTxPwrParam.cTxPwr2G4HT40_BPSK = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr2G4HT40_QPSK = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr2G4HT40_16QAM = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr2G4HT40_MCS5 = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr2G4HT40_MCS6 = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr2G4HT40_MCS7 = (prBowShortRangeMode->cTxPower << 1);

	rTxPwrParam.cTxPwr5GOFDM_BPSK = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr5GOFDM_QPSK = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr5GOFDM_16QAM = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr5GOFDM_48Mbps = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr5GOFDM_54Mbps = (prBowShortRangeMode->cTxPower << 1);

	rTxPwrParam.cTxPwr5GHT20_BPSK = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr5GHT20_QPSK = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr5GHT20_16QAM = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr5GHT20_MCS5 = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr5GHT20_MCS6 = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr5GHT20_MCS7 = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr5GHT40_BPSK = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr5GHT40_QPSK = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr5GHT40_16QAM = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr5GHT40_MCS5 = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr5GHT40_MCS6 = (prBowShortRangeMode->cTxPower << 1);
	rTxPwrParam.cTxPwr5GHT40_MCS7 = (prBowShortRangeMode->cTxPower << 1);

	if (nicUpdateTxPower(prAdapter, &rTxPwrParam) == WLAN_STATUS_SUCCESS) {
		DBGLOG(BOW, EVENT, "bowCmdShortRangeMode, %x.\n", WLAN_STATUS_SUCCESS);
		wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_SUCCESS);
		return WLAN_STATUS_SUCCESS;
	}
	wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_FAILURE);
	return WLAN_STATUS_FAILURE;

#else
	return 0;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This is command handler for BOW_CMD_ID_GET_CHANNEL_LIST
*        coming from 802.11 PAL
*
* \param[in] prAdapter  Pointer to the Adapter structure.
* \param[in] prCmd      Pointer to the buffer that holds the command
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
uint32_t bowCmdGetChannelList(IN struct ADAPTER *prAdapter, IN struct BT_OVER_WIFI_COMMAND *prCmd)
{
	ASSERT(prAdapter);

	/* not supported yet */
	return WLAN_STATUS_FAILURE;
}

#if 1				/* Marked for MT6630 */

/*----------------------------------------------------------------------------*/
/*!
* \brief This is generic command done handler
*
* \param[in] prAdapter      Pointer to the Adapter structure.
* \param[in] prCmdInfo      Pointer to the buffer that holds the command info
* \param[in] pucEventBuf    Pointer to the set buffer OR event buffer
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
void wlanbowCmdEventSetStatus(IN struct ADAPTER *prAdapter, IN struct BT_OVER_WIFI_COMMAND *prCmd, IN uint8_t ucEventBuf)
{
	struct BT_OVER_WIFI_EVENT *prEvent;
	struct BOW_COMMAND_STATUS *prBowCmdStatus;

	ASSERT(prAdapter);

	/* fill event header */
	prEvent = (struct BT_OVER_WIFI_EVENT *) kalMemAlloc((sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_COMMAND_STATUS)), VIR_MEM_TYPE);
	prEvent->rHeader.ucEventId = BOW_EVENT_ID_COMMAND_STATUS;
	prEvent->rHeader.ucSeqNumber = prCmd->rHeader.ucSeqNumber;
	prEvent->rHeader.u2PayloadLength = sizeof(struct BOW_COMMAND_STATUS);

	/* fill event body */
	prBowCmdStatus = (struct BOW_COMMAND_STATUS *) (prEvent->aucPayload);
	kalMemZero(prBowCmdStatus, sizeof(struct BOW_COMMAND_STATUS));

	prBowCmdStatus->ucStatus = ucEventBuf;

	kalIndicateBOWEvent(prAdapter->prGlueInfo, prEvent);

	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_COMMAND_STATUS)));
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This is generic command done handler
*
* \param[in] prAdapter      Pointer to the Adapter structure.
* \param[in] prCmdInfo      Pointer to the buffer that holds the command info
* \param[in] pucEventBuf    Pointer to the set buffer OR event buffer
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
void wlanbowCmdEventSetCommon(IN struct ADAPTER *prAdapter, IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf)
{
	struct BT_OVER_WIFI_EVENT *prEvent;
	struct BOW_COMMAND_STATUS *prBowCmdStatus;

	ASSERT(prAdapter);

	/* fill event header */
	prEvent = (struct BT_OVER_WIFI_EVENT *) kalMemAlloc((sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_COMMAND_STATUS)), VIR_MEM_TYPE);
	prEvent->rHeader.ucEventId = BOW_EVENT_ID_COMMAND_STATUS;
	prEvent->rHeader.ucSeqNumber = (uint8_t) prCmdInfo->u4PrivateData;
	prEvent->rHeader.u2PayloadLength = sizeof(struct BOW_COMMAND_STATUS);

	/* fill event body */
	prBowCmdStatus = (struct BOW_COMMAND_STATUS *) (prEvent->aucPayload);
	kalMemZero(prBowCmdStatus, sizeof(struct BOW_COMMAND_STATUS));

	prBowCmdStatus->ucStatus = BOWCMD_STATUS_SUCCESS;

	kalIndicateBOWEvent(prAdapter->prGlueInfo, prEvent);

	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_COMMAND_STATUS)));
}

/*----------------------------------------------------------------------------*/
/*!
* \brief command done handler for CMD_ID_CMD_BT_OVER_WIFI
*
* \param[in] prAdapter      Pointer to the Adapter structure.
* \param[in] prCmdInfo      Pointer to the buffer that holds the command info
* \param[in] pucEventBuf    Pointer to the set buffer OR event buffer
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
void wlanbowCmdEventLinkConnected(IN struct ADAPTER *prAdapter, IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf)
{
	struct BT_OVER_WIFI_EVENT *prEvent;
	struct BOW_LINK_CONNECTED *prBowLinkConnected;
	struct BOW_FSM_INFO *prBowFsmInfo;
	struct BSS_INFO *prBssInfo;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

	/* fill event header */
	prEvent = (struct BT_OVER_WIFI_EVENT *) kalMemAlloc((sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_LINK_CONNECTED)), VIR_MEM_TYPE);
	prEvent->rHeader.ucEventId = BOW_EVENT_ID_LINK_CONNECTED;
	prEvent->rHeader.ucSeqNumber = (uint8_t) prCmdInfo->u4PrivateData;
	prEvent->rHeader.u2PayloadLength = sizeof(struct BOW_LINK_CONNECTED);

	/* fill event body */
	prBowLinkConnected = (struct BOW_LINK_CONNECTED *) (prEvent->aucPayload);
	kalMemZero(prBowLinkConnected, sizeof(struct BOW_LINK_CONNECTED));
	prBowLinkConnected->rChannel.ucChannelNum = prBssInfo->ucPrimaryChannel;
	prBowLinkConnected->rChannel.ucChannelBand = prBssInfo->eBand;
	COPY_MAC_ADDR(prBowLinkConnected->aucPeerAddress, prBowFsmInfo->aucPeerAddress);

	DBGLOG(BOW, EVENT, "prEvent->rHeader.ucEventId, 0x%x\n", prEvent->rHeader.ucEventId);
	DBGLOG(BOW, EVENT, "prEvent->rHeader.ucSeqNumber, 0x%x\n", prEvent->rHeader.ucSeqNumber);
	DBGLOG(BOW, EVENT, "prEvent->rHeader.u2PayloadLength, 0x%x\n", prEvent->rHeader.u2PayloadLength);
	DBGLOG(BOW, EVENT,
	       "prBowLinkConnected->rChannel.ucChannelNum, 0x%x\n", prBowLinkConnected->rChannel.ucChannelNum);
	DBGLOG(BOW, EVENT,
	       "prBowLinkConnected->rChannel.ucChannelBand, 0x%x\n", prBowLinkConnected->rChannel.ucChannelBand);
	DBGLOG(BOW, EVENT,
	       "wlanbowCmdEventLinkConnected, prBowFsmInfo->aucPeerAddress, %x:%x:%x:%x:%x:%x.\n",
	       prBowFsmInfo->aucPeerAddress[0], prBowFsmInfo->aucPeerAddress[1],
	       prBowFsmInfo->aucPeerAddress[2], prBowFsmInfo->aucPeerAddress[3],
	       prBowFsmInfo->aucPeerAddress[4], prBowFsmInfo->aucPeerAddress[5]);
	DBGLOG(BOW, EVENT,
	       "wlanbowCmdEventLinkConnected, prBowLinkConnected->aucPeerAddress, %x:%x:%x:%x:%x:%x.\n",
	       prBowLinkConnected->aucPeerAddress[0], prBowLinkConnected->aucPeerAddress[1],
	       prBowLinkConnected->aucPeerAddress[2], prBowLinkConnected->aucPeerAddress[3],
	       prBowLinkConnected->aucPeerAddress[4], prBowLinkConnected->aucPeerAddress[5]);
	DBGLOG(BOW, EVENT, "wlanbowCmdEventLinkConnected, g_u4LinkCount, %x.\n", g_u4LinkCount);

	/*Indicate Event to PAL */
	kalIndicateBOWEvent(prAdapter->prGlueInfo, prEvent);
	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_LINK_CONNECTED)));

	/*Release channel if granted */
	if (prBowFsmInfo->fgIsChannelGranted) {
		cnmTimerStopTimer(prAdapter, &prBowFsmInfo->rChGrantedTimer);
		/* bowReleaseCh(prAdapter); */
		/*Requested, not granted yet */
	} else if (prBowFsmInfo->fgIsChannelRequested) {
		prBowFsmInfo->fgIsChannelRequested = FALSE;
	}

	/* set to connected status */
	bowSetBowTableState(prAdapter, prBowFsmInfo->aucPeerAddress, BOW_DEVICE_STATE_CONNECTED);

}

/*----------------------------------------------------------------------------*/
/*!
* \brief command done handler for CMD_ID_CMD_BT_OVER_WIFI
*
* \param[in] prAdapter      Pointer to the Adapter structure.
* \param[in] prCmdInfo      Pointer to the buffer that holds the command info
* \param[in] pucEventBuf    Pointer to the set buffer OR event buffer
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
void wlanbowCmdEventLinkDisconnected(IN struct ADAPTER *prAdapter, IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf)
{
	struct BT_OVER_WIFI_EVENT *prEvent;
	struct BOW_LINK_DISCONNECTED *prBowLinkDisconnected;
	struct BOW_FSM_INFO *prBowFsmInfo;
	struct BOW_TABLE rBowTable;
	uint8_t ucBowTableIdx;
	enum ENUM_BOW_DEVICE_STATE eFsmState;
	u_int8_t fgSendDeauth = FALSE;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	eFsmState = bowGetBowTableState(prAdapter, prBowFsmInfo->aucPeerAddress);

	if (eFsmState == BOW_DEVICE_STATE_DISCONNECTED) {
		/*do nothing */
		return;
	}
	/*Cancel scan */
	else if (eFsmState == BOW_DEVICE_STATE_SCANNING && !(prBowFsmInfo->fgIsChannelRequested)) {
		bowResponderCancelScan(prAdapter, FALSE);
		bowSetBowTableState(prAdapter, prBowFsmInfo->aucPeerAddress, BOW_DEVICE_STATE_DISCONNECTING);
		return;
	}
	/* fill event header */
	prEvent = (struct BT_OVER_WIFI_EVENT *) kalMemAlloc((sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_LINK_DISCONNECTED)), VIR_MEM_TYPE);
	prEvent->rHeader.ucEventId = BOW_EVENT_ID_LINK_DISCONNECTED;
	if ((prCmdInfo->u4PrivateData))
		prEvent->rHeader.ucSeqNumber = (uint8_t) prCmdInfo->u4PrivateData;
	else
		prEvent->rHeader.ucSeqNumber = 0;

	prEvent->rHeader.u2PayloadLength = sizeof(struct BOW_LINK_DISCONNECTED);

	/* fill event body */
	prBowLinkDisconnected = (struct BOW_LINK_DISCONNECTED *) (prEvent->aucPayload);
	kalMemZero(prBowLinkDisconnected, sizeof(struct BOW_LINK_DISCONNECTED));
	prBowLinkDisconnected->ucReason = 0x0;
	COPY_MAC_ADDR(prBowLinkDisconnected->aucPeerAddress, prBowFsmInfo->aucPeerAddress);

	DBGLOG(BOW, EVENT, "prEvent->rHeader.ucEventId, 0x%x\n", prEvent->rHeader.ucEventId);
	DBGLOG(BOW, EVENT, "prEvent->rHeader.ucSeqNumber, 0x%x\n", prEvent->rHeader.ucSeqNumber);
	DBGLOG(BOW, EVENT, "prEvent->rHeader.u2PayloadLength, 0x%x\n", prEvent->rHeader.u2PayloadLength);

	DBGLOG(BOW, EVENT,
	       "wlanbowCmdEventLinkDisconnected, prBowFsmInfo->aucPeerAddress, %x:%x:%x:%x:%x:%x.\n",
	       prBowFsmInfo->aucPeerAddress[0], prBowFsmInfo->aucPeerAddress[1],
	       prBowFsmInfo->aucPeerAddress[2], prBowFsmInfo->aucPeerAddress[3],
	       prBowFsmInfo->aucPeerAddress[4], prBowFsmInfo->aucPeerAddress[5]);

	DBGLOG(BOW, EVENT,
	       "wlanbowCmdEventLinkDisconnected, prBowLinkDisconnected->aucPeerAddress, %x:%x:%x:%x:%x:%x.\n",
	       prBowLinkDisconnected->aucPeerAddress[0], prBowLinkDisconnected->aucPeerAddress[1],
	       prBowLinkDisconnected->aucPeerAddress[2], prBowLinkDisconnected->aucPeerAddress[3],
	       prBowLinkDisconnected->aucPeerAddress[4], prBowLinkDisconnected->aucPeerAddress[5]);

	DBGLOG(BOW, EVENT, "wlanbowCmdEventLinkDisconnected, g_u4LinkCount, %x.\n", g_u4LinkCount);

	/*Indicate BoW event to PAL */
#if 0
	kalIndicateBOWEvent(prAdapter->prGlueInfo, prEvent);
	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_LINK_DISCONNECTED)));
#endif

	/* set to disconnected status */
	prBowFsmInfo->prTargetStaRec =
	    cnmGetStaRecByAddress(prAdapter, prBowFsmInfo->ucBssIndex, prBowLinkDisconnected->aucPeerAddress);

	/*Release channel if granted */
	if (prBowFsmInfo->fgIsChannelGranted) {
		cnmTimerStopTimer(prAdapter, &prBowFsmInfo->rChGrantedTimer);
		bowReleaseCh(prAdapter);
		/*Requested, not granted yet */
	} else if (prBowFsmInfo->fgIsChannelRequested) {
		prBowFsmInfo->fgIsChannelRequested = FALSE;
		/* bowReleaseCh(prAdapter); */
	}
#if 1
	/*Send Deauth to connected peer */
	if (eFsmState == BOW_DEVICE_STATE_CONNECTED && (prBowFsmInfo->prTargetStaRec->ucStaState == STA_STATE_3)) {
		fgSendDeauth = TRUE;
		DBGLOG(BOW, EVENT,
		       "wlanbowCmdEventLinkDisconnected, bowGetBowTableState, %x.\n",
		       bowGetBowTableState(prAdapter, prBowLinkDisconnected->aucPeerAddress));
		authSendDeauthFrame(prAdapter, NULL, prBowFsmInfo->prTargetStaRec,
				    (struct SW_RFB *) NULL, REASON_CODE_DEAUTH_LEAVING_BSS,
				    (PFN_TX_DONE_HANDLER) bowDisconnectLink);
	}
#endif

#if 0
	/* 3 <3>Stop this link; flush Tx;
	 * send deAuthentication -> abort. SAA, AAA. need to check BOW table state == Connected.
	 */
	if (prAdapter->prGlueInfo->i4TxPendingFrameNum > 0)
		kalFlushPendingTxPackets(prAdapter->prGlueInfo);

	/* flush pending security frames */
	if (prAdapter->prGlueInfo->i4TxPendingSecurityFrameNum > 0)
		kalClearSecurityFrames(prAdapter->prGlueInfo);
#endif

	/*Update BoW table */
	bowGetBowTableEntryByPeerAddress(prAdapter, prBowLinkDisconnected->aucPeerAddress, &ucBowTableIdx);
	rBowTable.fgIsValid = FALSE;
	rBowTable.eState = BOW_DEVICE_STATE_DISCONNECTED;
	rBowTable.ucAcquireID = 0; /* Just initiate */
	bowSetBowTableContent(prAdapter, ucBowTableIdx, &rBowTable);

	/*Indicate BoW event to PAL */
	kalIndicateBOWEvent(prAdapter->prGlueInfo, prEvent);
	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_LINK_DISCONNECTED)));

	/*Decrease link count */
	GLUE_DEC_REF_CNT(g_u4LinkCount);

	/*If no need to send deauth, DO disconnect now */
	/*If need to send deauth, DO disconnect at deauth Tx done */
	if (!fgSendDeauth)
		bowDisconnectLink(prAdapter, NULL, TX_RESULT_SUCCESS);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief command done handler for CMD_ID_CMD_BT_OVER_WIFI
*
* \param[in] prAdapter      Pointer to the Adapter structure.
* \param[in] prCmdInfo      Pointer to the buffer that holds the command info
* \param[in] pucEventBuf    Pointer to the set buffer OR event buffer
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
void wlanbowCmdEventSetSetupConnection(IN struct ADAPTER *prAdapter, IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf)
{
	struct BT_OVER_WIFI_EVENT *prEvent;
	struct BOW_COMMAND_STATUS *prBowCmdStatus;
	struct WIFI_CMD *prWifiCmd;
	struct CMD_BT_OVER_WIFI *prCmdBtOverWifi;
	struct BOW_FSM_INFO *prBowFsmInfo;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	/* restore original command for rPeerAddr */
	prWifiCmd = (struct WIFI_CMD *) (prCmdInfo->pucInfoBuffer);
	prCmdBtOverWifi = (struct CMD_BT_OVER_WIFI *) (prWifiCmd->aucBuffer);

	/* fill event header */
	prEvent = (struct BT_OVER_WIFI_EVENT *) kalMemAlloc((sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_COMMAND_STATUS)), VIR_MEM_TYPE);
	prEvent->rHeader.ucEventId = BOW_EVENT_ID_COMMAND_STATUS;
	prEvent->rHeader.ucSeqNumber = (uint8_t) prCmdInfo->u4PrivateData;
	prEvent->rHeader.u2PayloadLength = sizeof(struct BOW_COMMAND_STATUS);

	/* fill event body */
	prBowCmdStatus = (struct BOW_COMMAND_STATUS *) (prEvent->aucPayload);
	kalMemZero(prBowCmdStatus, sizeof(struct BOW_COMMAND_STATUS));
	prBowCmdStatus->ucStatus = BOWCMD_STATUS_SUCCESS;

	/*Indicate BoW event to PAL */
	kalIndicateBOWEvent(prAdapter->prGlueInfo, prEvent);
	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_COMMAND_STATUS)));

	/* set to starting status */
	kalSetBowState(prAdapter->prGlueInfo, BOW_DEVICE_STATE_STARTING, prCmdBtOverWifi->rPeerAddr);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This is the command done handler for BOW_CMD_ID_READ_LINK_QUALITY
*
* \param[in] prAdapter      Pointer to the Adapter structure.
* \param[in] prCmdInfo      Pointer to the buffer that holds the command info
* \param[in] pucEventBuf    Pointer to the set buffer OR event buffer
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
void wlanbowCmdEventReadLinkQuality(IN struct ADAPTER *prAdapter, IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf)
{
	struct LINK_QUALITY *prLinkQuality;
	struct BT_OVER_WIFI_EVENT *prEvent;
	struct BOW_LINK_QUALITY *prBowLinkQuality;

	ASSERT(prAdapter);

	prLinkQuality = (struct LINK_QUALITY *) pucEventBuf;

	/* fill event header */
	prEvent = (struct BT_OVER_WIFI_EVENT *) kalMemAlloc((sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_LINK_QUALITY)), VIR_MEM_TYPE);
	prEvent->rHeader.ucEventId = BOW_EVENT_ID_LINK_QUALITY;
	prEvent->rHeader.ucSeqNumber = (uint8_t) prCmdInfo->u4PrivateData;
	prEvent->rHeader.u2PayloadLength = sizeof(struct BOW_LINK_QUALITY);

	/* fill event body */
	prBowLinkQuality = (struct BOW_LINK_QUALITY *) (prEvent->aucPayload);
	kalMemZero(prBowLinkQuality, sizeof(struct BOW_LINK_QUALITY));
	prBowLinkQuality->ucLinkQuality = (uint8_t) prLinkQuality->cLinkQuality;

	kalIndicateBOWEvent(prAdapter->prGlueInfo, prEvent);

	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_LINK_QUALITY)));
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This is the command done handler for BOW_CMD_ID_READ_RSSI
*
* \param[in] prAdapter      Pointer to the Adapter structure.
* \param[in] prCmdInfo      Pointer to the buffer that holds the command info
* \param[in] pucEventBuf    Pointer to the set buffer OR event buffer
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
void wlanbowCmdEventReadRssi(IN struct ADAPTER *prAdapter, IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf)
{
	struct LINK_QUALITY *prLinkQuality;
	struct BT_OVER_WIFI_EVENT *prEvent;
	struct BOW_RSSI *prBowRssi;

	ASSERT(prAdapter);

	prLinkQuality = (struct LINK_QUALITY *) pucEventBuf;

	/* fill event header */
	prEvent = (struct BT_OVER_WIFI_EVENT *) kalMemAlloc((sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_LINK_QUALITY)), VIR_MEM_TYPE);
	prEvent->rHeader.ucEventId = BOW_EVENT_ID_RSSI;
	prEvent->rHeader.ucSeqNumber = (uint8_t) prCmdInfo->u4PrivateData;
	prEvent->rHeader.u2PayloadLength = sizeof(struct BOW_RSSI);

	/* fill event body */
	prBowRssi = (struct BOW_RSSI *) (prEvent->aucPayload);
	kalMemZero(prBowRssi, sizeof(struct BOW_RSSI));
	prBowRssi->cRssi = (int8_t) prLinkQuality->cRssi;

	kalIndicateBOWEvent(prAdapter->prGlueInfo, prEvent);

	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_LINK_QUALITY)));

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This is the default command timeout handler
*
* \param[in] prAdapter      Pointer to the Adapter structure.
* \param[in] prCmdInfo      Pointer to the buffer that holds the command info
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
void wlanbowCmdTimeoutHandler(IN struct ADAPTER *prAdapter, IN struct CMD_INFO *prCmdInfo)
{
	struct BT_OVER_WIFI_EVENT *prEvent;
	struct BOW_COMMAND_STATUS *prBowCmdStatus;

	ASSERT(prAdapter);

	/* fill event header */
	prEvent = (struct BT_OVER_WIFI_EVENT *) kalMemAlloc((sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_COMMAND_STATUS)), VIR_MEM_TYPE);
	prEvent->rHeader.ucEventId = BOW_EVENT_ID_COMMAND_STATUS;
	prEvent->rHeader.ucSeqNumber = (uint8_t) prCmdInfo->u4PrivateData;
	prEvent->rHeader.u2PayloadLength = sizeof(struct BOW_COMMAND_STATUS);

	/* fill event body */
	prBowCmdStatus = (struct BOW_COMMAND_STATUS *) (prEvent->aucPayload);
	kalMemZero(prBowCmdStatus, sizeof(struct BOW_COMMAND_STATUS));

	prBowCmdStatus->ucStatus = BOWCMD_STATUS_TIMEOUT;	/* timeout */

	kalIndicateBOWEvent(prAdapter->prGlueInfo, prEvent);

	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(struct BT_OVER_WIFI_EVENT) + sizeof(struct BOW_COMMAND_STATUS)));

}

/* Bruce, 20140224 */
uint8_t bowInit(IN struct ADAPTER *prAdapter)
{
	struct BSS_INFO *prBowBssInfo;
	struct BOW_FSM_INFO *prBowFsmInfo;

	ASSERT(prAdapter);

	prBowBssInfo = cnmGetBssInfoAndInit(prAdapter, NETWORK_TYPE_BOW, TRUE);

	/*Initiate BSS_INFO_T - common part -move from bowstarting */
	BSS_INFO_INIT(prAdapter, prBowBssInfo);

	prBowBssInfo->eCurrentOPMode = OP_MODE_BOW;

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBowFsmInfo->ucBssIndex = prBowBssInfo->ucBssIndex;

	/* Setup Own MAC & BSSID */
	COPY_MAC_ADDR(prBowBssInfo->aucOwnMacAddr, prAdapter->rWifiVar.aucDeviceAddress);
	COPY_MAC_ADDR(prBowBssInfo->aucBSSID, prAdapter->rWifiVar.aucDeviceAddress);

	return prBowBssInfo->ucBssIndex;
}

/* Bruce, 20140224 */
void bowUninit(IN struct ADAPTER *prAdapter)
{
	struct BSS_INFO *prBowBssInfo;
	struct BOW_FSM_INFO *prBowFsmInfo;

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBowBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

	cnmFreeBssInfo(prAdapter, prBowBssInfo);
}

void bowStopping(IN struct ADAPTER *prAdapter)
{
	struct BOW_FSM_INFO *prBowFsmInfo;
	struct BSS_INFO *prBowBssInfo;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBowBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

	DBGLOG(BOW, EVENT, "bowStoping.\n");
	DBGLOG(BOW, EVENT, "bowStoping, SSID %s.\n", prBowBssInfo->aucSSID);
	DBGLOG(BOW, EVENT, "bowStoping, prBowBssInfo->aucBSSID, %x:%x:%x:%x:%x:%x.\n",
	       prBowBssInfo->aucBSSID[0],
	       prBowBssInfo->aucBSSID[1],
	       prBowBssInfo->aucBSSID[2],
	       prBowBssInfo->aucBSSID[3], prBowBssInfo->aucBSSID[4], prBowBssInfo->aucBSSID[5]);
	DBGLOG(BOW, EVENT, "bowStoping, prBssInfo->aucOwnMacAddr, %x:%x:%x:%x:%x:%x.\n",
	       prBowBssInfo->aucOwnMacAddr[0],
	       prBowBssInfo->aucOwnMacAddr[1],
	       prBowBssInfo->aucOwnMacAddr[2],
	       prBowBssInfo->aucOwnMacAddr[3], prBowBssInfo->aucOwnMacAddr[4], prBowBssInfo->aucOwnMacAddr[5]);
	DBGLOG(BOW, EVENT,
	       "bowStoping, prAdapter->rWifiVar.aucDeviceAddress, %x:%x:%x:%x:%x:%x.\n",
	       prAdapter->rWifiVar.aucDeviceAddress[0], prAdapter->rWifiVar.aucDeviceAddress[1],
	       prAdapter->rWifiVar.aucDeviceAddress[2], prAdapter->rWifiVar.aucDeviceAddress[3],
	       prAdapter->rWifiVar.aucDeviceAddress[4], prAdapter->rWifiVar.aucDeviceAddress[5]);
	DBGLOG(BOW, EVENT, "bowStopping, g_u4LinkCount, %x.\n", g_u4LinkCount);
	DBGLOG(BOW, EVENT,
	       "prBowFsmInfo->aucPeerAddress, %x:%x:%x:%x:%x:%x.\n",
	       prBowFsmInfo->aucPeerAddress[0], prBowFsmInfo->aucPeerAddress[1],
	       prBowFsmInfo->aucPeerAddress[2], prBowFsmInfo->aucPeerAddress[3],
	       prBowFsmInfo->aucPeerAddress[4], prBowFsmInfo->aucPeerAddress[5]);
	DBGLOG(BOW, EVENT, "BoW Stoping,[%d,%d]\n", g_u4LinkCount, g_u4Beaconing);

	if (g_u4LinkCount == 0) {
		/*Stop beaconing */
		GLUE_DEC_REF_CNT(g_u4Beaconing);

		/*Deactive BoW network */
		/* prBowBssInfo->fgIsNetActive = FALSE; */
		/* prBowBssInfo->fgIsBeaconActivated = FALSE; */
		nicPmIndicateBssAbort(prAdapter, prBowBssInfo->ucBssIndex);
		bowChangeMediaState(prBowBssInfo, MEDIA_STATE_DISCONNECTED);
		nicUpdateBss(prAdapter, prBowBssInfo->ucBssIndex);
		/*temp solution for FW hal_pwr_mgt.c#3037 ASSERT */
		nicDeactivateNetwork(prAdapter, prBowBssInfo->ucBssIndex);
		SET_NET_PWR_STATE_IDLE(prAdapter, prBowBssInfo->ucBssIndex);
		UNSET_NET_ACTIVE(prAdapter, prBowBssInfo->ucBssIndex);

	}

}

void bowStarting(IN struct ADAPTER *prAdapter)
{
	struct BOW_FSM_INFO *prBowFsmInfo;
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	if (g_u4LinkCount == 1) {
		DBGLOG(BOW, EVENT, "BoW Starting.\n");
		DBGLOG(BOW, EVENT, "BoW channel granted.\n");

		/* 3 <1> Update BSS_INFO_T per Network Basis */
		/* 4 <1.1> Setup Operation Mode */

		/* Bruce, 20140224 */
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

		/* 4 <1.2> Setup SSID */
		prBssInfo->ucSSIDLen = BOW_SSID_LEN;
		bowAssignSsid(prBssInfo->aucSSID, prBssInfo->aucOwnMacAddr);

		DBGLOG(BOW, EVENT, "SSID %s.\n", prBssInfo->aucSSID);
		DBGLOG(BOW, EVENT, "prBssInfo->aucBSSID, %x:%x:%x:%x:%x:%x.\n",
		       prBssInfo->aucBSSID[0],
		       prBssInfo->aucBSSID[1],
		       prBssInfo->aucBSSID[2], prBssInfo->aucBSSID[3], prBssInfo->aucBSSID[4], prBssInfo->aucBSSID[5]);
		DBGLOG(BOW, EVENT, "prBssInfo->aucOwnMacAddr, %x:%x:%x:%x:%x:%x.\n",
		       prBssInfo->aucOwnMacAddr[0],
		       prBssInfo->aucOwnMacAddr[1],
		       prBssInfo->aucOwnMacAddr[2],
		       prBssInfo->aucOwnMacAddr[3], prBssInfo->aucOwnMacAddr[4], prBssInfo->aucOwnMacAddr[5]);
		DBGLOG(BOW, EVENT, "prAdapter->rWifiVar.aucDeviceAddress, %x:%x:%x:%x:%x:%x.\n",
		       prAdapter->rWifiVar.aucDeviceAddress[0],
		       prAdapter->rWifiVar.aucDeviceAddress[1],
		       prAdapter->rWifiVar.aucDeviceAddress[2],
		       prAdapter->rWifiVar.aucDeviceAddress[3],
		       prAdapter->rWifiVar.aucDeviceAddress[4], prAdapter->rWifiVar.aucDeviceAddress[5]);

		/* 4 <1.3> Clear current AP's STA_RECORD_T and current AID */
		prBssInfo->prStaRecOfAP = (struct STA_RECORD *) NULL;
		prBssInfo->u2AssocId = 0;

		/* 4 <1.4> Setup Channel, Band and Phy Attributes */
		prBssInfo->ucPrimaryChannel = prBowFsmInfo->ucPrimaryChannel;
		if (prBowFsmInfo->eBand == BAND_2G4)
			prBssInfo->eBand = BAND_2G4;
		else
			prBssInfo->eBand = BAND_5G;

#if CFG_BOW_SUPPORT_11N
		/* Depend on eBand */
		prBssInfo->ucPhyTypeSet = prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11BGN;
		/* Depend on eCurrentOPMode and ucPhyTypeSet */
		prBssInfo->ucConfigAdHocAPMode = AP_MODE_MIXED_11BG;

		prBssInfo->ucNonHTBasicPhyType = (uint8_t)
		    rNonHTApModeAttributes[prBssInfo->ucConfigAdHocAPMode].ePhyTypeIndex;
		prBssInfo->u2BSSBasicRateSet = rNonHTApModeAttributes[prBssInfo->ucConfigAdHocAPMode].u2BSSBasicRateSet;

		prBssInfo->u2OperationalRateSet =
		    rNonHTPhyAttributes[prBssInfo->ucNonHTBasicPhyType].u2SupportedRateSet;

		rateGetDataRatesFromRateSet(prBssInfo->u2OperationalRateSet,
					    prBssInfo->u2BSSBasicRateSet,
					    prBssInfo->aucAllSupportedRates, &prBssInfo->ucAllSupportedRatesLen);

#else
		if (prBssInfo->eBand == BAND_2G4) {
			/* Depend on eBand */
			prBssInfo->ucPhyTypeSet = PHY_TYPE_SET_802_11BG;
			/* Depend on eCurrentOPMode and ucPhyTypeSet */
			prBssInfo->ucConfigAdHocAPMode = AP_MODE_MIXED_11BG;

			/* RATE_SET_ERP; */
			prBssInfo->u2BSSBasicRateSet = BASIC_RATE_SET_ERP;
			prBssInfo->u2OperationalRateSet = RATE_SET_ERP;
			prBssInfo->ucNonHTBasicPhyType = PHY_TYPE_ERP_INDEX;
		} else {
			/* Depend on eBand */
			/* prBssInfo->ucPhyTypeSet = PHY_TYPE_SET_802_11BG; */
			/* Depend on eCurrentOPMode and ucPhyTypeSet */
			/* prBssInfo->ucConfigAdHocAPMode = AP_MODE_MIXED_11BG; */
			/* Depend on eBand */
			prBssInfo->ucPhyTypeSet = PHY_TYPE_SET_802_11A;
			/* Depend on eCurrentOPMode and ucPhyTypeSet */
			prBssInfo->ucConfigAdHocAPMode = AP_MODE_11A;

			/* RATE_SET_ERP; */
			/* prBssInfo->u2BSSBasicRateSet = BASIC_RATE_SET_ERP; */
			/* prBssInfo->u2OperationalRateSet = RATE_SET_ERP; */

			/* RATE_SET_ERP; */
			prBssInfo->u2BSSBasicRateSet = BASIC_RATE_SET_OFDM;
			prBssInfo->u2OperationalRateSet = RATE_SET_OFDM;
			prBssInfo->ucNonHTBasicPhyType = PHY_TYPE_OFDM_INDEX;
		}

#endif
		prBssInfo->fgErpProtectMode = FALSE;

		/* 4 <1.5> Setup MIB for current BSS */
		prBssInfo->u2BeaconInterval = prBowFsmInfo->u2BeaconInterval;
		prBssInfo->ucDTIMPeriod = DOT11_DTIM_PERIOD_DEFAULT;
		prBssInfo->u2ATIMWindow = 0;
		prBssInfo->ucBeaconTimeoutCount = 0;
		if (prBowFsmInfo->fgSupportQoS) {
			prAdapter->rWifiVar.ucQoS = TRUE;
			prBssInfo->fgIsQBSS = TRUE;
		}

		/* 3 <2> Update BSS_INFO_T common part */
#if CFG_SUPPORT_AAA
		bssInitForAP(prAdapter, prBssInfo, TRUE);
		nicQmUpdateWmmParms(prAdapter, prBssInfo->ucBssIndex);
#endif /* CFG_SUPPORT_AAA */
		prBssInfo->fgIsNetActive = TRUE;
		prBssInfo->fgIsBeaconActivated = TRUE;

		/* 3 <3> Set MAC HW */

		DBGLOG(BOW, EVENT,
		       "prBowFsmInfo->aucPeerAddress, %x:%x:%x:%x:%x:%x.\n",
		       prBowFsmInfo->aucPeerAddress[0], prBowFsmInfo->aucPeerAddress[1],
		       prBowFsmInfo->aucPeerAddress[2], prBowFsmInfo->aucPeerAddress[3],
		       prBowFsmInfo->aucPeerAddress[4], prBowFsmInfo->aucPeerAddress[5]);

		/* 4 <3.1> use command packets to inform firmware */
		rlmBssInitForAPandIbss(prAdapter, prBssInfo);
		nicUpdateBss(prAdapter, prBssInfo->ucBssIndex);

		/* 4 <3.2> Update AdHoc PM parameter */
		nicPmIndicateBssCreated(prAdapter, prBssInfo->ucBssIndex);

		/* 4 <3.1> Reset HW TSF Update Mode and Beacon Mode */

		/* 4 <3.2> Setup BSSID */
		/* TODO: rxmSetRxFilterBSSID0 */
/* rxmSetRxFilterBSSID0(prBssInfo->ucHwBssidId, prBssInfo->aucBSSID); */

		/* 4 <3.3> Setup RX Filter to accept Probe Request */
		/* TODO: f get/set RX filter. */

#if 0
		{
			uint32_t u4RxFilter;

			if (halMacRxGetRxFilters(&u4RxFilter) == HAL_STATUS_SUCCESS) {

				u4RxFilter &= ~BIT(RXFILTER_DROP_PROBE_REQ);

				halMacRxSetRxFilters(u4RxFilter);
			}
		}
#endif
	}

	/*Update BoW Table */
	bowSetBowTableState(prAdapter, prBowFsmInfo->aucPeerAddress, BOW_DEVICE_STATE_STARTING);

	DBGLOG(BOW, EVENT, "BoW Starting,[%d,%d]\n", g_u4LinkCount, g_u4Beaconing);
	DBGLOG(BOW, EVENT, "bowStarting, g_u4LinkCount, %x.\n", g_u4LinkCount);

	/*Start beaconing */
	if (g_u4Beaconing < 1) {
		GLUE_INC_REF_CNT(g_u4Beaconing);
		bssSendBeaconProbeResponse(prAdapter, prBssInfo->ucBssIndex, NULL, 0);
		cnmTimerStartTimer(prAdapter, &prBowFsmInfo->rStartingBeaconTimer, prBowFsmInfo->u2BeaconInterval);
	}
#if 0
	/*Responder: Start to scan Initiator */
	if (prBowFsmInfo->ucRole == BOW_RESPONDER) {
		DBGLOG(BOW, EVENT, "bowStarting responder, start scan result searching.\n");
		cnmTimerStopTimer(prAdapter, &prBowFsmInfo->rChGrantedTimer);
		bowReleaseCh(prAdapter);
		bowResponderScan(prAdapter);
	}
	/*Initiator: Request channel, wait for responder */
	/* else
	*   bowRequestCh(prAdapter);
	*/
#endif

	/* wlanBindBssIdxToNetInterface(prAdapter->prGlueInfo, NET_DEV_BOW_IDX, prBssInfo->ucBssIndex); */

}

void bowAssignSsid(IN uint8_t *pucSsid, IN uint8_t *puOwnMacAddr)
{
	uint8_t i;
	uint8_t aucSSID[] = BOW_WILDCARD_SSID;

	kalMemCopy(pucSsid, aucSSID, BOW_WILDCARD_SSID_LEN);

	for (i = 0; i < 6; i++) {
		pucSsid[(3 * i) + 3] = 0x2D;
		if ((*(puOwnMacAddr + i) >> 4) < 0xA)
			*(pucSsid + (3 * i) + 4) = (*(puOwnMacAddr + i) >> 4) + 0x30;
		else
			*(pucSsid + (3 * i) + 4) = (*(puOwnMacAddr + i) >> 4) + 0x57;

		if ((*(puOwnMacAddr + i) & 0x0F) < 0xA)
			pucSsid[(3 * i) + 5] = (*(puOwnMacAddr + i) & 0x0F) + 0x30;
		else
			pucSsid[(3 * i) + 5] = (*(puOwnMacAddr + i) & 0x0F) + 0x57;
	}

}

#endif /* Marked for MT6630 */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will validate the Rx Probe Request Frame and then return
*        result to BSS to indicate if need to send the corresponding Probe Response
*        Frame if the specified conditions were matched.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prSwRfb            Pointer to SW RFB data structure.
* @param[out] pu4ControlFlags   Control flags for replying the Probe Response
*
* @retval TRUE      Reply the Probe Response
* @retval FALSE     Don't reply the Probe Response
*/
/*----------------------------------------------------------------------------*/
u_int8_t bowValidateProbeReq(IN struct ADAPTER *prAdapter, IN struct SW_RFB *prSwRfb, OUT uint32_t *pu4ControlFlags)
{
#if 1				/* Marked for MT6630 */

	struct WLAN_MAC_MGMT_HEADER *prMgtHdr;
	struct BOW_FSM_INFO *prBowFsmInfo;
	struct BSS_INFO *prBssInfo;
	struct IE_SSID *prIeSsid = (struct IE_SSID *) NULL;
	uint8_t *pucIE;
	uint16_t u2IELength;
	uint16_t u2Offset = 0;
	u_int8_t fgReplyProbeResp = FALSE;

	ASSERT(prSwRfb);
	ASSERT(pu4ControlFlags);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

#if 0
	DBGLOG(BOW, EVENT, "bowValidateProbeReq.\n");
#endif

	/* 4 <1> Parse Probe Req IE and Get IE ptr (SSID, Supported Rate IE, ...) */
	prMgtHdr = (struct WLAN_MAC_MGMT_HEADER *) prSwRfb->pvHeader;

	u2IELength = prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen;
	pucIE = (uint8_t *) (((unsigned long) prSwRfb->pvHeader) + prSwRfb->u2HeaderLen);

	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		if (IE_ID(pucIE) == ELEM_ID_SSID) {
			if ((!prIeSsid) && (IE_LEN(pucIE) <= ELEM_MAX_LEN_SSID))
				prIeSsid = (struct IE_SSID *) pucIE;
			break;
		}
	}			/* end of IE_FOR_EACH */

	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		if (IE_ID(pucIE) == ELEM_ID_SSID) {
			if ((!prIeSsid) && (IE_LEN(pucIE) <= ELEM_MAX_LEN_SSID))
				prIeSsid = (struct IE_SSID *) pucIE;
			break;
		}
	}			/* end of IE_FOR_EACH */

	/* 4 <2> Check network conditions */
	/*If BoW AP is beaconing */
	if (prBssInfo->eCurrentOPMode == OP_MODE_BOW && g_u4Beaconing > 0) {

		/*Check the probe requset sender is our peer */
		if (bowCheckBowTableIfVaild(prAdapter, prMgtHdr->aucSrcAddr))
			fgReplyProbeResp = TRUE;
		/*Check the probe request target SSID is our SSID */
		else if ((prIeSsid) &&
			 EQUAL_SSID(prBssInfo->aucSSID, prBssInfo->ucSSIDLen, prIeSsid->aucSSID, prIeSsid->ucLength))
			fgReplyProbeResp = TRUE;
		else
			fgReplyProbeResp = FALSE;
	}

	return fgReplyProbeResp;
#else
	return 0;
#endif
}

#if 1				/* Marked for MT6630 */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate an Event of "Media Disconnect" to HOST
*
* @param[in] u4Param  Unused timer parameter
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void bowSendBeacon(IN struct ADAPTER *prAdapter, IN unsigned long ulParamPtr)
{
	struct BOW_FSM_INFO *prBowFsmInfo;

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	if ((g_u4Beaconing != 0) && (g_u4LinkCount > 0)
	    && (g_u4LinkCount < CFG_BOW_PHYSICAL_LINK_NUM)) {
		/* Send beacon */
		bssSendBeaconProbeResponse(prAdapter, prBowFsmInfo->ucBssIndex, NULL, 0);
		cnmTimerStartTimer(prAdapter, &prBowFsmInfo->rStartingBeaconTimer, prBowFsmInfo->u2BeaconInterval);
	} else {
		DBGLOG(BOW, EVENT, "BoW Send Beacon,[%d,%d]\n", g_u4LinkCount, g_u4Beaconing);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate an Event of "Media Disconnect" to HOST
*
* @param[in] u4Param  Unused timer parameter
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void bowResponderScan(IN struct ADAPTER *prAdapter)
{
	struct BOW_FSM_INFO *prBowFsmInfo;
	struct MSG_SCN_SCAN_REQ *prScanReqMsg;
	struct BSS_INFO *prBssInfo;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

	DBGLOG(BOW, EVENT, "bowResponderScan.\n");
	DBGLOG(BOW, EVENT, "BOW SCAN [REQ:%d]\n", prBowFsmInfo->ucSeqNumOfScanReq + 1);

	prScanReqMsg = (struct MSG_SCN_SCAN_REQ *) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(struct MSG_SCN_SCAN_REQ));

	if (!prScanReqMsg) {
		ASSERT(0);	/* Can't trigger SCAN FSM */
		return;
	}

	/*Fill scan message */
	prScanReqMsg->rMsgHdr.eMsgId = MID_BOW_SCN_SCAN_REQ;
	prScanReqMsg->ucSeqNum = ++prBowFsmInfo->ucSeqNumOfScanReq;
	prScanReqMsg->ucBssIndex = prBowFsmInfo->ucBssIndex;
	prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;
	prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_SPECIFIED;
	prScanReqMsg->ucSSIDLength = BOW_SSID_LEN;
	bowAssignSsid(prScanReqMsg->aucSSID, prBowFsmInfo->aucPeerAddress);
	prScanReqMsg->ucChannelListNum = 1;

	if (prBowFsmInfo->eBand == BAND_2G4) {
		prScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
		prScanReqMsg->arChnlInfoList[0].eBand = BAND_2G4;
	} else {
		prScanReqMsg->eScanChannel = SCAN_CHANNEL_5G;
		prScanReqMsg->arChnlInfoList[0].eBand = BAND_5G;
	}

	prScanReqMsg->arChnlInfoList[0].ucChannelNum = prBowFsmInfo->ucPrimaryChannel;
	prScanReqMsg->u2IELen = 0;

	/*Send scan message */
	mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *) prScanReqMsg, MSG_SEND_METHOD_BUF);

	/*Change state to SCANNING */
	bowSetBowTableState(prAdapter, prBowFsmInfo->aucPeerAddress, BOW_DEVICE_STATE_SCANNING);

	/* prBowFsmInfo->fgTryScan = FALSE; *//* Will enable background sleep for infrastructure */

}

#endif /* Marked for MT6630 */

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
void bowResponderScanDone(IN struct ADAPTER *prAdapter, IN struct MSG_HDR *prMsgHdr)
{
#if 1				/* Marked for MT6630 */
	struct MSG_SCN_SCAN_DONE *prScanDoneMsg;
	struct BOW_FSM_INFO *prBowFsmInfo;
	struct BSS_DESC *prBssDesc;
	uint8_t ucSeqNumOfCompMsg;
	struct CONNECTION_SETTINGS *prConnSettings;
	enum ENUM_BOW_DEVICE_STATE eFsmState;
	enum ENUM_SCAN_STATUS eScanStatus;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prConnSettings =
		aisGetConnSettings(prAdapter, AIS_DEFAULT_INDEX);
	prScanDoneMsg = (struct MSG_SCN_SCAN_DONE *) prMsgHdr;
	eFsmState = bowGetBowTableState(prAdapter, prBowFsmInfo->aucPeerAddress);

	ucSeqNumOfCompMsg = prScanDoneMsg->ucSeqNum;
	eScanStatus = prScanDoneMsg->eScanStatus;

	cnmMemFree(prAdapter, prMsgHdr);

	DBGLOG(BOW, EVENT, "bowResponderScanDone.\n");
	DBGLOG(BOW, EVENT, "BOW SCAN [DONE:%d]\n", ucSeqNumOfCompMsg);

	if (eScanStatus == SCAN_STATUS_CANCELLED) {
		DBGLOG(BOW, EVENT, "BOW SCAN [CANCELLED:%d]\n", ucSeqNumOfCompMsg);
		if (eFsmState == BOW_DEVICE_STATE_DISCONNECTING) {
			wlanoidSendSetQueryBowCmd(prAdapter,
						  CMD_ID_CMD_BT_OVER_WIFI,
						  prBowFsmInfo->ucBssIndex,
						  TRUE,
						  FALSE,
						  wlanbowCmdEventLinkDisconnected,
						  wlanbowCmdTimeoutHandler, 0, NULL, 0);
		}
		return;
	} else if (eFsmState == BOW_DEVICE_STATE_DISCONNECTED) {
		/* bowDisconnectLink(prAdapter, NULL, TX_RESULT_SUCCESS); */
		return;
	} else if (ucSeqNumOfCompMsg != prBowFsmInfo->ucSeqNumOfScanReq) {
		DBGLOG(BOW, EVENT, "Sequence no. of BOW Responder scan done is not matched.\n");
		return;
	}
	prConnSettings->fgIsScanReqIssued = FALSE;
	prBssDesc = scanSearchBssDescByBssid(prAdapter, prBowFsmInfo->aucPeerAddress);
	DBGLOG(BOW, EVENT, "End scan result searching.\n");
	DBGLOG(BOW, EVENT, "prBowFsmInfo->aucPeerAddress: [" MACSTR "]\n", MAC2STR(prBowFsmInfo->aucPeerAddress));

	/*Initiator is FOUND */
	if (prBssDesc != NULL) {	/* (prBssDesc->aucBSSID != NULL)) */
		DBGLOG(BOW, EVENT,
		       "Search Bow Peer address - %x:%x:%x:%x:%x:%x.\n",
		       prBssDesc->aucBSSID[0], prBssDesc->aucBSSID[1],
		       prBssDesc->aucBSSID[2], prBssDesc->aucBSSID[3], prBssDesc->aucBSSID[4], prBssDesc->aucBSSID[5]);
		DBGLOG(BOW, EVENT, "Starting to join initiator.\n");

		/*Set target BssDesc */
		prBowFsmInfo->prTargetBssDesc = prBssDesc;
		/*Request channel to do JOIN */
		bowSetBowTableState(prAdapter, prBowFsmInfo->aucPeerAddress, BOW_DEVICE_STATE_ACQUIRING_CHANNEL);
		bowRequestCh(prAdapter);
	}
	/*Initiator is NOT FOUND */
	else {
		/*Scan again, until PAL timeout */
		bowResponderScan(prAdapter);
#if 0
		wlanoidSendSetQueryBowCmd(prAdapter,
					  CMD_ID_CMD_BT_OVER_WIFI,
					  TRUE,
					  FALSE, wlanbowCmdEventLinkDisconnected, wlanbowCmdTimeoutHandler, 0, NULL, 0);
#endif
	}

	return;
#endif /* Marked for MT6630 */
}

#if 1				/* Marked for MT6630 */

/*----------------------------------------------------------------------------*/
/*!
* @brief Function for cancelling scan request. There is another option to extend channel privilige
*           for another purpose.
*
* @param fgIsChannelExtention - Keep the channel previlege, but can cancel scan timer.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void bowResponderCancelScan(IN struct ADAPTER *prAdapter, IN u_int8_t fgIsChannelExtention)
{

	struct MSG_SCN_SCAN_CANCEL *prScanCancel = (struct MSG_SCN_SCAN_CANCEL *) NULL;
	struct BOW_FSM_INFO *prBowFsmInfo = (struct BOW_FSM_INFO *) NULL;

	DEBUGFUNC("bowResponderCancelScan()");

	do {
		ASSERT(prAdapter);

		prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

		if (TRUE) {
			DBGLOG(BOW, EVENT, "BOW SCAN [CANCEL:%d]\n", prBowFsmInfo->ucSeqNumOfScanReq);

			/* There is a channel privilege on hand. */

			DBGLOG(BOW, TRACE, "BOW Cancel Scan\n");

			prScanCancel =
			    (struct MSG_SCN_SCAN_CANCEL *) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(struct MSG_SCN_SCAN_CANCEL));
			if (!prScanCancel) {
				/* Buffer not enough, can not cancel scan request. */
				DBGLOG(BOW, TRACE, "Buffer not enough, can not cancel scan.\n");
				ASSERT(FALSE);
				break;
			}

			prScanCancel->rMsgHdr.eMsgId = MID_BOW_SCN_SCAN_CANCEL;
			prScanCancel->ucBssIndex = prBowFsmInfo->ucBssIndex;
			prScanCancel->ucSeqNum = prBowFsmInfo->ucSeqNumOfScanReq;
#if CFG_ENABLE_WIFI_DIRECT
			prScanCancel->fgIsChannelExt = fgIsChannelExtention;
#endif
			mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *) prScanCancel, MSG_SEND_METHOD_BUF);

		}

	} while (FALSE);

}				/* bowResponderCancelScan */

/*----------------------------------------------------------------------------*/
/*!
* @brief Initialization of JOIN STATE
*
* @param[in] prBssDesc  The pointer of BSS_DESC_T which is the BSS we will try to join with.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void bowResponderJoin(IN struct ADAPTER *prAdapter, IN struct BSS_DESC *prBssDesc)
{
	struct BOW_FSM_INFO *prBowFsmInfo;
	struct BSS_INFO *prBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct STA_RECORD *prStaRec;
	struct MSG_SAA_FSM_START *prJoinReqMsg;

	ASSERT(prBssDesc);
	ASSERT(prAdapter);

	DBGLOG(BOW, EVENT, "Starting bowResponderJoin.\n");

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, AIS_DEFAULT_INDEX);

	/* 4 <1> We are going to connect to this BSS. */
	prBssDesc->fgIsConnecting = TRUE;
	bowSetBowTableState(prAdapter, prBowFsmInfo->aucPeerAddress, BOW_DEVICE_STATE_CONNECTING);

	/* 4 <2> Setup corresponding STA_RECORD_T */
	/*Support First JOIN and retry */
	prStaRec = bssCreateStaRecFromBssDesc(prAdapter, STA_TYPE_BOW_AP, prBssInfo->ucBssIndex, prBssDesc);

	prBowFsmInfo->prTargetStaRec = prStaRec;

	/* 4 <3> Update ucAvailableAuthTypes which we can choice during SAA */
	prStaRec->fgIsReAssoc = FALSE;
	prBowFsmInfo->ucAvailableAuthTypes = (uint8_t) AUTH_TYPE_OPEN_SYSTEM;
	prStaRec->ucTxAuthAssocRetryLimit = TX_AUTH_ASSOCI_RETRY_LIMIT;

	/* 4 <4> Use an appropriate Authentication Algorithm Number among the ucAvailableAuthTypes */
	if (prBowFsmInfo->ucAvailableAuthTypes & (uint8_t) AUTH_TYPE_OPEN_SYSTEM) {

		DBGLOG(BOW, LOUD, "JOIN INIT: Try to do Authentication with AuthType == OPEN_SYSTEM.\n");
		prBowFsmInfo->ucAvailableAuthTypes &= ~(uint8_t) AUTH_TYPE_OPEN_SYSTEM;

		prStaRec->ucAuthAlgNum = (uint8_t) AUTH_ALGORITHM_NUM_OPEN_SYSTEM;
	} else {
		ASSERT(0);
	}

	/* 4 <4.1> sync. to firmware domain */
	cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

	/* 4 <5> Overwrite Connection Setting for eConnectionPolicy */
	if (prBssDesc->ucSSIDLen) {
		COPY_SSID(prConnSettings->aucSSID, prConnSettings->ucSSIDLen, prBssDesc->aucSSID, prBssDesc->ucSSIDLen);
		DBGLOG(BOW, EVENT, "bowResponderJoin, SSID %s.\n", prBssDesc->aucSSID);
		DBGLOG(BOW, EVENT, "bowResponderJoin, SSID %s.\n", prConnSettings->aucSSID);
	}
	/* 4 <6> Send a Msg to trigger SAA to start JOIN process. */
	prJoinReqMsg = (struct MSG_SAA_FSM_START *) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(struct MSG_SAA_FSM_START));
	if (!prJoinReqMsg) {

		ASSERT(0);	/* Can't trigger SAA FSM */
		return;
	}

	prJoinReqMsg->rMsgHdr.eMsgId = MID_BOW_SAA_FSM_START;
	prJoinReqMsg->ucSeqNum = ++prBowFsmInfo->ucSeqNumOfReqMsg;
	prJoinReqMsg->prStaRec = prStaRec;

	prBssInfo->prStaRecOfAP = prStaRec;

	DBGLOG(BOW, EVENT, "prStaRec->eStaType, %x.\n", prStaRec->eStaType);
	DBGLOG(BOW, EVENT, "BoW trigger SAA [" MACSTR "]\n", MAC2STR(prStaRec->aucMacAddr));

	mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *) prJoinReqMsg, MSG_SEND_METHOD_BUF);

}

#endif /* Marked for MT6630 */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle the Join Complete Event from SAA FSM for BOW FSM
*
* @param[in] prMsgHdr   Message of Join Complete of SAA FSM.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void bowFsmRunEventJoinComplete(IN struct ADAPTER *prAdapter, IN struct MSG_HDR *prMsgHdr)
{
#if 1				/* Marked for MT6630 */

	struct MSG_SAA_FSM_COMP *prJoinCompMsg;
	struct BOW_FSM_INFO *prBowFsmInfo;
	struct STA_RECORD *prStaRec;
	struct SW_RFB *prAssocRspSwRfb;
	struct WLAN_ASSOC_RSP_FRAME *prAssocRspFrame = (struct WLAN_ASSOC_RSP_FRAME *) NULL;
	uint16_t u2IELength;
	uint8_t *pucIE;
	struct BSS_INFO *prBowBssInfo;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBowBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);
	prJoinCompMsg = (struct MSG_SAA_FSM_COMP *) prMsgHdr;
	prStaRec = prJoinCompMsg->prStaRec;

	DBGLOG(BOW, EVENT, "Start bowfsmRunEventJoinComplete.\n");
	DBGLOG(BOW, EVENT, "bowfsmRunEventJoinComplete ptr check\n");
	DBGLOG(BOW, EVENT, "prMsgHdr %x\n", prMsgHdr);
	DBGLOG(BOW, EVENT, "prAdapter %x\n", prAdapter);
	DBGLOG(BOW, EVENT, "prBowFsmInfo %x\n", prBowFsmInfo);
	DBGLOG(BOW, EVENT, "prStaRec %x\n", prStaRec);

	ASSERT(prStaRec);
	ASSERT(prBowFsmInfo);

	/* Check SEQ NUM */
	if (prJoinCompMsg->ucSeqNum == prBowFsmInfo->ucSeqNumOfReqMsg) {
		COPY_MAC_ADDR(prBowFsmInfo->aucPeerAddress, prStaRec->aucMacAddr);

		/* 4 <1> JOIN was successful */
		if (prJoinCompMsg->rJoinStatus == WLAN_STATUS_SUCCESS) {
			prAssocRspSwRfb = prJoinCompMsg->prSwRfb;
			prAssocRspFrame = (struct WLAN_ASSOC_RSP_FRAME *) prAssocRspSwRfb->pvHeader;

			u2IELength =
			    (uint16_t) ((prAssocRspSwRfb->u2PacketLen -
					prAssocRspSwRfb->u2HeaderLen) -
				       (OFFSET_OF(struct WLAN_ASSOC_RSP_FRAME, aucInfoElem[0]) - WLAN_MAC_MGMT_HEADER_LEN));
			pucIE = prAssocRspFrame->aucInfoElem;

			prStaRec->eStaType = STA_TYPE_BOW_AP;
			prStaRec->u2DesiredNonHTRateSet &= prBowBssInfo->u2OperationalRateSet;
			prStaRec->ucDesiredPhyTypeSet = prStaRec->ucPhyTypeSet & prBowBssInfo->ucPhyTypeSet;
#if CFG_BOW_RATE_LIMITATION
			/* 4 <1.2>Update Rate Set */
			/*Limit Rate Set to 24M,  48M, 54M */
			prStaRec->u2DesiredNonHTRateSet &= (RATE_SET_BIT_24M | RATE_SET_BIT_48M | RATE_SET_BIT_54M);
			/*If peer cannot support the above rate set, fix on the available highest rate */
			if (prStaRec->u2DesiredNonHTRateSet == 0) {
				uint8_t ucHighestRateIndex;

				if (rateGetHighestRateIndexFromRateSet
				    (prBowBssInfo->u2OperationalRateSet, &ucHighestRateIndex)) {
					prStaRec->u2DesiredNonHTRateSet = BIT(ucHighestRateIndex);
				}
			}
#endif

			/* 4 <1.1> Change FW's Media State immediately. */
			bowChangeMediaState(prBowBssInfo,
				MEDIA_STATE_CONNECTED);

			mqmProcessAssocRsp(prAdapter, prAssocRspSwRfb, pucIE, u2IELength);

			/* 4 <1.2> Update HT information and set channel */
			/* Record HT related parameters in rStaRec and rBssInfo
			 * Note: it shall be called before nicUpdateBss()
			 */
#if CFG_BOW_SUPPORT_11N
			rlmProcessAssocRsp(prAdapter, prAssocRspSwRfb, pucIE, u2IELength);
#endif

			/* 4 <1.3> Update BSS_INFO_T */
			nicUpdateBss(prAdapter, prBowBssInfo->ucBssIndex);
			DBGLOG(BOW, EVENT, "Finish bowUpdateBssInfoForJOIN.\n");

			/* 4 <1.4> Activate current AP's STA_RECORD_T in Driver. */
			cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

			DBGLOG(BOW, EVENT, "bowFsmRunEventJoinComplete, qmActivateStaRec.\n");

			/* 4 <1.7> Set the Next State of BOW  FSM */
			wlanoidSendSetQueryBowCmd(prAdapter,
						  CMD_ID_CMD_BT_OVER_WIFI,
						  prBowFsmInfo->ucBssIndex,
						  TRUE,
						  FALSE,
						  wlanbowCmdEventLinkConnected, wlanbowCmdTimeoutHandler, 0, NULL, 0);
		}
		/* 4 <2> JOIN was not successful */
		else {
			/*Retry */
			bowResponderJoin(prAdapter, prBowFsmInfo->prTargetBssDesc);
#if 0
			wlanoidSendSetQueryBowCmd(prAdapter,
						  CMD_ID_CMD_BT_OVER_WIFI,
						  TRUE,
						  FALSE,
						  wlanbowCmdEventLinkDisconnected,
						  wlanbowCmdTimeoutHandler, 0, NULL, 0);
#endif
			DBGLOG(BOW, EVENT, "Start bowfsmRunEventJoinComplete -- Join failed.\n");
			DBGLOG(BOW, EVENT, "BoW trigger SAA REJOIN\n");
		}
	}

	cnmMemFree(prAdapter, prMsgHdr);

#endif /* Marked for MT6630 */
}

#if 1				/* Marked for MT6630 */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate the Media State to HOST
*
* @param[in] eConnectionState   Current Media State
* @param[in] fgDelayIndication  Set TRUE for postponing the Disconnect Indication.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void
bowIndicationOfMediaStateToHost(IN struct ADAPTER *prAdapter,
				IN enum ENUM_PARAM_MEDIA_STATE eConnectionState, IN u_int8_t fgDelayIndication)
{
	struct EVENT_CONNECTION_STATUS rEventConnStatus;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct BSS_INFO *prBssInfo;
	struct BOW_FSM_INFO *prBowFsmInfo;

	prConnSettings = aisGetConnSettings(prAdapter, AIS_DEFAULT_INDEX);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

	/* NOTE(Kevin): Move following line to bowChangeMediaState() macro per CM's request. */
	/* prBowBssInfo->eConnectionState = eConnectionState; */

	/* For indicating the Disconnect Event only if current media state is
	 * disconnected and we didn't do indication yet.
	 */
	if (prBssInfo->eConnectionState == MEDIA_STATE_DISCONNECTED) {
		if (prBssInfo->eConnectionStateIndicated == eConnectionState)
			return;
	}

	if (!fgDelayIndication) {
		/* 4 <0> Cancel Delay Timer */
		cnmTimerStopTimer(prAdapter, &prBowFsmInfo->rIndicationOfDisconnectTimer);

		/* 4 <1> Fill EVENT_CONNECTION_STATUS */
		rEventConnStatus.ucMediaStatus = (uint8_t) eConnectionState;

		if (eConnectionState == MEDIA_STATE_CONNECTED) {
			rEventConnStatus.ucReasonOfDisconnect = DISCONNECT_REASON_CODE_RESERVED;

			if (prBssInfo->eCurrentOPMode == OP_MODE_BOW) {
				rEventConnStatus.ucInfraMode = (uint8_t) NET_TYPE_INFRA;
				rEventConnStatus.u2AID = prBssInfo->u2AssocId;
				rEventConnStatus.u2ATIMWindow = 0;
			} else if (prBssInfo->eCurrentOPMode == OP_MODE_IBSS) {
				rEventConnStatus.ucInfraMode = (uint8_t) NET_TYPE_IBSS;
				rEventConnStatus.u2AID = 0;
				rEventConnStatus.u2ATIMWindow = prBssInfo->u2ATIMWindow;
			} else {
				ASSERT(0);
			}

			COPY_SSID(rEventConnStatus.aucSsid,
				  rEventConnStatus.ucSsidLen, prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

			COPY_MAC_ADDR(rEventConnStatus.aucBssid, prBssInfo->aucBSSID);

			rEventConnStatus.u2BeaconPeriod = prBssInfo->u2BeaconInterval;
			rEventConnStatus.u4FreqInKHz = nicChannelNum2Freq(prBssInfo->ucPrimaryChannel);

			switch (prBssInfo->ucNonHTBasicPhyType) {
			case PHY_TYPE_HR_DSSS_INDEX:
				rEventConnStatus.ucNetworkType = (uint8_t) PARAM_NETWORK_TYPE_DS;
				break;

			case PHY_TYPE_ERP_INDEX:
				rEventConnStatus.ucNetworkType = (uint8_t) PARAM_NETWORK_TYPE_OFDM24;
				break;

			case PHY_TYPE_OFDM_INDEX:
				rEventConnStatus.ucNetworkType = (uint8_t) PARAM_NETWORK_TYPE_OFDM5;
				break;

			default:
				ASSERT(0);
				rEventConnStatus.ucNetworkType = (uint8_t) PARAM_NETWORK_TYPE_DS;
				break;
			}
		} else {

			rEventConnStatus.ucReasonOfDisconnect = prBssInfo->ucReasonOfDisconnect;

		}

		/* 4 <2> Indication */
		nicMediaStateChange(prAdapter, prBssInfo->ucBssIndex, &rEventConnStatus);
		prBssInfo->eConnectionStateIndicated = eConnectionState;
	} else {
		/* NOTE: Only delay the Indication of Disconnect Event */
		ASSERT(eConnectionState == MEDIA_STATE_DISCONNECTED);

		DBGLOG(BOW, INFO, "Postpone the indication of Disconnect for %d seconds\n",
		       prConnSettings->ucDelayTimeOfDisconnectEvent);

		cnmTimerStartTimer(prAdapter,
				   &prBowFsmInfo->rIndicationOfDisconnectTimer,
				   SEC_TO_MSEC(prConnSettings->ucDelayTimeOfDisconnectEvent));
	}

}

#endif /* Marked for MT6630 */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate the Event of Tx Fail of AAA Module.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prStaRec           Pointer to the STA_RECORD_T
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void bowRunEventAAATxFail(IN struct ADAPTER *prAdapter, IN struct STA_RECORD *prStaRec)
{
#if 1				/* Marked for MT6630 */
	struct BSS_INFO *prBssInfo;
	struct BOW_FSM_INFO *prBowFsmInfo;

	ASSERT(prAdapter);
	ASSERT(prStaRec);

	DBGLOG(BOW, EVENT, "bowRunEventAAATxFail , bssRemoveStaRecFromClientList.\n");
	DBGLOG(BOW, EVENT, "BoW AAA TxFail, target state %d\n", prStaRec->ucStaState + 1);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);
	bssRemoveClient(prAdapter, prBssInfo, prStaRec);

	return;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate the Event of Successful Completion of AAA Module.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prStaRec           Pointer to the STA_RECORD_T
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
uint32_t bowRunEventAAAComplete(IN struct ADAPTER *prAdapter, IN struct STA_RECORD *prStaRec)
{
#if 1				/* Marked for MT6630 */
	struct BSS_INFO *prBssInfo;
	struct BOW_FSM_INFO *prBowFsmInfo;

	ASSERT(prStaRec);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

	DBGLOG(BOW, STATE, "bowRunEventAAAComplete, cnmStaRecChangeState, STA_STATE_3.\n");
	DBGLOG(BOW, EVENT, "BoW AAA complete [" MACSTR "]\n", MAC2STR(prStaRec->aucMacAddr));

	/*Update BssInfo to connected */
	bowChangeMediaState(prBssInfo, MEDIA_STATE_CONNECTED);
	nicUpdateBss(prAdapter, prBowFsmInfo->ucBssIndex);

	/*Update StaRec to State3 */
	cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

	/*Connected */
	wlanoidSendSetQueryBowCmd(prAdapter,
				  CMD_ID_CMD_BT_OVER_WIFI,
				  prBowFsmInfo->ucBssIndex,
				  TRUE, FALSE, wlanbowCmdEventLinkConnected, wlanbowCmdTimeoutHandler, 0, NULL, 0);

	return WLAN_STATUS_SUCCESS;

#else
	return 0;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle RxDeauth
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prStaRec           Pointer to the STA_RECORD_T
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/

uint32_t bowRunEventRxDeAuth(IN struct ADAPTER *prAdapter, IN struct STA_RECORD *prStaRec, IN struct SW_RFB *prSwRfb)
{
#if 1				/* Marked for MT6630 */
	struct BSS_INFO *prBowBssInfo;
	struct BOW_FSM_INFO *prBowFsmInfo;
	enum ENUM_BOW_DEVICE_STATE eFsmState;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBowBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

	if (!IS_STA_BOW_TYPE(prStaRec))
		return WLAN_STATUS_NOT_ACCEPTED;

	eFsmState = bowGetBowTableState(prAdapter, prStaRec->aucMacAddr);

	if (eFsmState == BOW_DEVICE_STATE_DISCONNECTED) {
		/*do nothing */
		return WLAN_STATUS_NOT_ACCEPTED;
	}

	if (prStaRec->ucStaState > STA_STATE_1) {

		if (prStaRec->ucStaState == STA_STATE_3) {
			/* P_MSG_AIS_ABORT_T prAisAbortMsg; */

			/* NOTE(Kevin): Change state immediately to avoid starvation of
			 * MSG buffer because of too many deauth frames before changing
			 * the STA state.
			 */
			cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);
		}

		COPY_MAC_ADDR(prBowFsmInfo->aucPeerAddress, prStaRec->aucMacAddr);

		wlanoidSendSetQueryBowCmd(prAdapter,
					  CMD_ID_CMD_BT_OVER_WIFI,
					  prBowFsmInfo->ucBssIndex,
					  TRUE,
					  FALSE, wlanbowCmdEventLinkDisconnected, wlanbowCmdTimeoutHandler, 0, NULL, 0);

		return WLAN_STATUS_SUCCESS;
	}

	return WLAN_STATUS_NOT_ACCEPTED;

#else
	return 0;
#endif
}

#if 1				/* Marked for MT6630 */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function handle BoW Link disconnect.
*
* \param[in] pMsduInfo            Pointer to the Msdu Info
* \param[in] rStatus              The Tx done status
*
* \return -
*
* \note after receive deauth frame, callback function call this
*/
/*----------------------------------------------------------------------------*/
void bowDisconnectLink(IN struct ADAPTER *prAdapter, IN struct MSDU_INFO *prMsduInfo, IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	struct BOW_FSM_INFO *prBowFsmInfo;
	struct STA_RECORD *prStaRec;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	/*Free target StaRec */
	if (prMsduInfo)
		prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
	else
		prStaRec = prBowFsmInfo->prTargetStaRec;

	if (prStaRec) {
		/* cnmStaRecFree(prAdapter, prStaRec); */
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);
	}
	kalPrint("bowDisconnectLink\n");
	/*No one connected */
	if (g_u4LinkCount == 0 && g_u4Beaconing != 0) {
		cnmTimerStopTimer(prAdapter, &prBowFsmInfo->rStartingBeaconTimer);
		bowStopping(prAdapter);
		kalPrint("bowStopping\n");
		/*Restore TxPower from Short range mode */
#if CFG_SUPPORT_NVRAM && 0
		if (kalIsConfigurationExist(prAdapter->prGlueInfo) == TRUE)
			wlanLoadManufactureData(prAdapter, kalGetConfiguration(prAdapter->prGlueInfo));
		else
			DBGLOG(REQ, WARN, "%s: load manufacture data fail\n", __func__);

#endif
		/*Uninit BoW Interface */
#if CFG_BOW_SEPARATE_DATA_PATH
		kalUninitBowDevice(prAdapter->prGlueInfo);
#endif
	}
}

#endif /* Marked for MT6630 */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will validate the Rx Assoc Req Frame and then return
*        the status code to AAA to indicate if need to perform following actions
*        when the specified conditions were matched.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prSwRfb            Pointer to SW RFB data structure.
* @param[out] pu2StatusCode     The Status Code of Validation Result
*
* @retval TRUE      Reply the Assoc Resp
* @retval FALSE     Don't reply the Assoc Resp
*/
/*----------------------------------------------------------------------------*/
u_int8_t bowValidateAssocReq(IN struct ADAPTER *prAdapter, IN struct SW_RFB *prSwRfb, OUT uint16_t *pu2StatusCode)
{
#if 1				/* Marked for MT6630 */

	u_int8_t fgReplyAssocResp = FALSE;
	struct BSS_INFO *prBowBssInfo;
	struct STA_RECORD *prStaRec = (struct STA_RECORD *) NULL;
	struct BOW_FSM_INFO *prBowFsmInfo;
	struct WLAN_ASSOC_REQ_FRAME *prAssocReqFrame = (struct WLAN_ASSOC_REQ_FRAME *) NULL;
	OS_SYSTIME rCurrentTime;
	static OS_SYSTIME rLastRejectAssocTime;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBowBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

	prAssocReqFrame = (struct WLAN_ASSOC_REQ_FRAME *) prSwRfb->pvHeader;
	*pu2StatusCode = STATUS_CODE_REQ_DECLINED;

	DBGLOG(BOW, EVENT,
	       "bowValidateAssocReq, prBowFsmInfo->aucPeerAddress, %x:%x:%x:%x:%x:%x.\n",
	       prBowFsmInfo->aucPeerAddress[0], prBowFsmInfo->aucPeerAddress[1],
	       prBowFsmInfo->aucPeerAddress[2], prBowFsmInfo->aucPeerAddress[3],
	       prBowFsmInfo->aucPeerAddress[4], prBowFsmInfo->aucPeerAddress[5]);
	DBGLOG(BOW, EVENT,
	       "bowValidateAssocReq, prAssocReqFrame->aucSrcAddr, %x:%x:%x:%x:%x:%x.\n",
	       prAssocReqFrame->aucSrcAddr[0], prAssocReqFrame->aucSrcAddr[1],
	       prAssocReqFrame->aucSrcAddr[2], prAssocReqFrame->aucSrcAddr[3],
	       prAssocReqFrame->aucSrcAddr[4], prAssocReqFrame->aucSrcAddr[5]);

	/*Assoc Accept */
	while (EQUAL_MAC_ADDR(prAssocReqFrame->aucSrcAddr, prBowFsmInfo->aucPeerAddress)) {
		DBGLOG(BOW, EVENT, "bowValidateAssocReq, return wlanbowCmdEventLinkConnected.\n");

		/*Update StaRec */
		prStaRec = cnmGetStaRecByAddress(prAdapter, prBowFsmInfo->ucBssIndex, prAssocReqFrame->aucSrcAddr);
		prStaRec->eStaType = STA_TYPE_BOW_CLIENT;
		prStaRec->u2DesiredNonHTRateSet &= prBowBssInfo->u2OperationalRateSet;
		prStaRec->ucDesiredPhyTypeSet = prStaRec->ucPhyTypeSet & prBowBssInfo->ucPhyTypeSet;

#if CFG_BOW_RATE_LIMITATION
		/*Limit Rate Set to 24M,  48M, 54M */
		prStaRec->u2DesiredNonHTRateSet &= (RATE_SET_BIT_24M | RATE_SET_BIT_48M | RATE_SET_BIT_54M);
		/*If peer cannot support the above rate set, fix on the available highest rate */
		if (prStaRec->u2DesiredNonHTRateSet == 0) {
			uint8_t ucHighestRateIndex;

			if (rateGetHighestRateIndexFromRateSet(prBowBssInfo->u2OperationalRateSet, &ucHighestRateIndex))
				prStaRec->u2DesiredNonHTRateSet = BIT(ucHighestRateIndex);
			else {
				/*If no available rate is found, DECLINE the association */
				*pu2StatusCode = STATUS_CODE_ASSOC_DENIED_RATE_NOT_SUPPORTED;
				break;
			}
		}
#endif

		/*Update BssInfo to FW */
		bowChangeMediaState(prBowBssInfo, MEDIA_STATE_CONNECTED);
		nicUpdateBss(prAdapter, prStaRec->ucBssIndex);

		/*reply successful */
		*pu2StatusCode = STATUS_CODE_SUCCESSFUL;
		fgReplyAssocResp = TRUE;
		break;
	}

	/*Reject Assoc */
	if (*pu2StatusCode != STATUS_CODE_SUCCESSFUL) {
		/*Reply Assoc with reject every 5s */
		rCurrentTime = kalGetTimeTick();
		if (CHECK_FOR_TIMEOUT(rCurrentTime, rLastRejectAssocTime, MSEC_TO_SYSTIME(5000)) ||
		    rLastRejectAssocTime == 0) {
			fgReplyAssocResp = TRUE;
			rLastRejectAssocTime = rCurrentTime;
		}
	}

	return fgReplyAssocResp;

#else
	return 0;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will validate the Rx Auth Frame and then return
*        the status code to AAA to indicate if need to perform following actions
*        when the specified conditions were matched.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prSwRfb            Pointer to SW RFB data structure.
* @param[in] pprStaRec          Pointer to pointer of STA_RECORD_T structure.
* @param[out] pu2StatusCode     The Status Code of Validation Result
*
* @retval TRUE      Reply the Auth
* @retval FALSE     Don't reply the Auth
*/
/*----------------------------------------------------------------------------*/
u_int8_t
bowValidateAuth(IN struct ADAPTER *prAdapter,
		IN struct SW_RFB *prSwRfb, IN struct STA_RECORD **pprStaRec, OUT uint16_t *pu2StatusCode)
{
#if 1				/* Marked for MT6630 */
	u_int8_t fgReplyAuth = FALSE;
	struct BSS_INFO *prBowBssInfo;
	struct STA_RECORD *prStaRec = (struct STA_RECORD *) NULL;
	struct BOW_FSM_INFO *prBowFsmInfo;
	struct WLAN_AUTH_FRAME *prAuthFrame = (struct WLAN_AUTH_FRAME *) NULL;
	OS_SYSTIME rCurrentTime;
	static OS_SYSTIME rLastRejectAuthTime;

	/* TODO(Kevin): Call BoW functions to check ..
	 *  1. Check we are BoW now.
	 *  2. Check we can accept connection from thsi peer
	 *  3. Check Black List here.
	 */

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBowBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

	prAuthFrame = (struct WLAN_AUTH_FRAME *) prSwRfb->pvHeader;

	DBGLOG(BOW, EVENT, "bowValidateAuth, prBowFsmInfo->aucPeerAddress, %x:%x:%x:%x:%x:%x.\n",
	       prBowFsmInfo->aucPeerAddress[0],
	       prBowFsmInfo->aucPeerAddress[1],
	       prBowFsmInfo->aucPeerAddress[2],
	       prBowFsmInfo->aucPeerAddress[3], prBowFsmInfo->aucPeerAddress[4], prBowFsmInfo->aucPeerAddress[5]);
	DBGLOG(BOW, EVENT, "bowValidateAuth, prAuthFrame->aucSrcAddr, %x:%x:%x:%x:%x:%x.\n",
	       prAuthFrame->aucSrcAddr[0],
	       prAuthFrame->aucSrcAddr[1],
	       prAuthFrame->aucSrcAddr[2],
	       prAuthFrame->aucSrcAddr[3], prAuthFrame->aucSrcAddr[4], prAuthFrame->aucSrcAddr[5]);

	prStaRec = cnmGetStaRecByAddress(prAdapter, prBowFsmInfo->ucBssIndex, prAuthFrame->aucSrcAddr);
	if (!prStaRec) {
		DBGLOG(BOW, EVENT, "bowValidateAuth, cnmStaRecAlloc.\n");
		prStaRec = cnmStaRecAlloc(prAdapter,
					  STA_TYPE_BOW_CLIENT, prBowFsmInfo->ucBssIndex, prAuthFrame->aucSrcAddr);

		/* TODO(Kevin): Error handling of allocation of struct STA_RECORD for
		 * exhausted case and do removal of unused struct STA_RECORD.
		 */
		ASSERT(prStaRec);
		COPY_MAC_ADDR(prStaRec->aucMacAddr, prAuthFrame->aucSrcAddr);
		prSwRfb->ucStaRecIdx = prStaRec->ucIndex;
		prBowBssInfo->prStaRecOfAP = prStaRec;

		/* NOTE(Kevin): Better to change state here, not at TX Done */
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);
		DBGLOG(BOW, EVENT, "bowValidateAuth, cnmStaRecChangeState.\n");
	} else {
		prSwRfb->ucStaRecIdx = prStaRec->ucIndex;
		DBGLOG(BOW, EVENT, "bowValidateAuth, prStaRec->ucIndex, %x.\n", prStaRec->ucIndex);
		bssRemoveClient(prAdapter, prBowBssInfo, prStaRec);
	}

	if (EQUAL_MAC_ADDR(prAuthFrame->aucSrcAddr, prBowFsmInfo->aucPeerAddress)) {
		DBGLOG(BOW, EVENT, "bowValidateAuth, prStaRec->eStaType, %x.\n", prStaRec->eStaType);
		DBGLOG(BOW, EVENT, "bowValidateAuth, prStaRec->ucBssIndex, %x.\n", prStaRec->ucBssIndex);

		/* Update Station Record - Status/Reason Code */
		prStaRec->u2StatusCode = STATUS_CODE_SUCCESSFUL;
		prStaRec->ucJoinFailureCount = 0;
		*pprStaRec = prStaRec;
		*pu2StatusCode = STATUS_CODE_SUCCESSFUL;
		fgReplyAuth = TRUE;
	} else {
		cnmStaRecFree(prAdapter, prStaRec);
		*pu2StatusCode = STATUS_CODE_REQ_DECLINED;

		/*Reply auth with reject every 5s */
		rCurrentTime = kalGetTimeTick();
		if (CHECK_FOR_TIMEOUT(rCurrentTime, rLastRejectAuthTime, MSEC_TO_SYSTIME(5000)) ||
		    rLastRejectAuthTime == 0) {
			fgReplyAuth = TRUE;
			rLastRejectAuthTime = rCurrentTime;
		}
	}

	DBGLOG(BOW, EVENT, "bowValidateAuth,  fgReplyAuth, %x.\n", fgReplyAuth);
	return fgReplyAuth;

#else
	return 0;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is invoked when CNM granted channel privilege
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
void bowRunEventChGrant(IN struct ADAPTER *prAdapter, IN struct MSG_HDR *prMsgHdr)
{
#if 1				/* Marked for MT6630 */
	struct BSS_INFO *prBowBssInfo;
	struct BOW_FSM_INFO *prBowFsmInfo;
	struct MSG_CH_GRANT *prMsgChGrant;
	uint8_t ucTokenID;
	uint32_t u4GrantInterval;
	enum ENUM_BOW_DEVICE_STATE eFsmState;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBowBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);
	prMsgChGrant = (struct MSG_CH_GRANT *) prMsgHdr;
	ucTokenID = prMsgChGrant->ucTokenID;
	u4GrantInterval = prMsgChGrant->u4GrantInterval;

	/* 1. free message */
	cnmMemFree(prAdapter, prMsgHdr);
	prBowFsmInfo->fgIsChannelGranted = TRUE;

	DBGLOG(BOW, EVENT, "Entering bowRunEventChGrant.\n");

	eFsmState = bowGetBowTableState(prAdapter, prBowFsmInfo->aucPeerAddress);

	/*Release channel */
	if ((!prBowFsmInfo->fgIsChannelRequested) ||
	    (prBowFsmInfo->ucSeqNumOfChReq != ucTokenID) ||
	    (eFsmState == BOW_DEVICE_STATE_DISCONNECTED) || (eFsmState == BOW_DEVICE_STATE_DISCONNECTING)) {
		DBGLOG(BOW, EVENT, "BoW Channel [GIVE UP:%d]\n", ucTokenID);
		DBGLOG(BOW, EVENT, "[Requested:%d][ucSeqNumOfChReq:%d][eFsmState:%d]\n",
		       prBowFsmInfo->fgIsChannelRequested, prBowFsmInfo->ucSeqNumOfChReq, eFsmState);
		bowReleaseCh(prAdapter);
		return;
	}

	/* 2. channel privilege has been approved */
	prBowFsmInfo->u4ChGrantedInterval = u4GrantInterval;

#if 0
	cnmTimerStartTimer(prAdapter,
			   &prBowFsmInfo->rChGrantedTimer,
			   prBowFsmInfo->u4ChGrantedInterval - BOW_JOIN_CH_GRANT_THRESHOLD);
#else
	cnmTimerStartTimer(prAdapter,
			   &prBowFsmInfo->rChGrantedTimer, BOW_JOIN_CH_REQUEST_INTERVAL - BOW_JOIN_CH_GRANT_THRESHOLD);
#endif

	/* 3.2 set local variable to indicate join timer is ticking */

	DBGLOG(BOW, EVENT, "BoW Channel [GRANTED:%d].\n", ucTokenID);

	if (eFsmState == BOW_DEVICE_STATE_ACQUIRING_CHANNEL) {
		bowStarting(prAdapter);
		bowReleaseCh(prAdapter);
		if (prBowFsmInfo->ucRole == BOW_RESPONDER)
			bowResponderJoin(prAdapter, prBowFsmInfo->prTargetBssDesc);
	} else {
		/*update bssinfo */
		nicUpdateBss(prAdapter, prBowFsmInfo->ucBssIndex);
		bowReleaseCh(prAdapter);
	}

	return;
#endif /* Marked for MT6630 */
}				/* end of aisFsmRunEventChGrant() */

#if 1				/* Marked for MT6630 */

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is to inform CNM for channel privilege requesting
*           has been released
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
void bowRequestCh(IN struct ADAPTER *prAdapter)
{
	struct BOW_FSM_INFO *prBowFsmInfo;
	struct MSG_CH_REQ *prMsgChReq;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	if (prBowFsmInfo->fgIsChannelGranted == FALSE) {

		DBGLOG(BOW, EVENT, "BoW channel [REQUEST:%d], %d, %d.\n",
		       prBowFsmInfo->ucSeqNumOfChReq + 1, prBowFsmInfo->ucPrimaryChannel, prBowFsmInfo->eBand);

		prMsgChReq = (struct MSG_CH_REQ *) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(struct MSG_CH_REQ));

		if (!prMsgChReq) {
			ASSERT(0);	/* Can't indicate CNM for channel acquiring */
			return;
		}

		prMsgChReq->rMsgHdr.eMsgId = MID_MNY_CNM_CH_REQ;
		prMsgChReq->ucBssIndex = prBowFsmInfo->ucBssIndex;
		prMsgChReq->ucTokenID = ++prBowFsmInfo->ucSeqNumOfChReq;
		prMsgChReq->eReqType = CH_REQ_TYPE_JOIN;
#if 0
		prMsgChReq->u4MaxInterval = BOW_JOIN_CH_REQUEST_INTERVAL;
#else
		prMsgChReq->u4MaxInterval = 1;
#endif
		/* prBowFsmInfo->prTargetBssDesc->ucChannelNum; */
		prMsgChReq->ucPrimaryChannel = prBowFsmInfo->ucPrimaryChannel;
		/* prBowFsmInfo->prTargetBssDesc->eSco; */
		prMsgChReq->eRfSco = CHNL_EXT_SCN;
		/* prBowFsmInfo->prTargetBssDesc->eBand; */
		prMsgChReq->eRfBand = prBowFsmInfo->eBand;

		/* FIXME : where to call cnmGetDbdcCapability in BOW? */
		/*prMsgChReq->eDBDCBand = (prAdapter->aprBssInfo[prMsgChReq->ucBssIndex])->eDBDCBand;*/
		prMsgChReq->eDBDCBand = ENUM_BAND_AUTO;

		/* To do: check if 80/160MHz bandwidth is needed here */
		prMsgChReq->eRfChannelWidth = 0;
		prMsgChReq->ucRfCenterFreqSeg1 = 0;
		prMsgChReq->ucRfCenterFreqSeg2 = 0;

		prBowFsmInfo->fgIsChannelRequested = TRUE;

		mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *) prMsgChReq, MSG_SEND_METHOD_BUF);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is to inform BOW that channel privilege is granted
*           has been released
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
void bowReleaseCh(IN struct ADAPTER *prAdapter)
{
	struct BOW_FSM_INFO *prBowFsmInfo;
	struct MSG_CH_ABORT *prMsgChAbort;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	if (prBowFsmInfo->fgIsChannelGranted != FALSE || prBowFsmInfo->fgIsChannelRequested != FALSE) {
		DBGLOG(BOW, EVENT,
		       "BoW channel [RELEASE:%d] %d, %d.\n", prBowFsmInfo->ucSeqNumOfChReq,
		       prBowFsmInfo->ucPrimaryChannel, prBowFsmInfo->eBand);

		prBowFsmInfo->fgIsChannelRequested = FALSE;
		prBowFsmInfo->fgIsChannelGranted = FALSE;

		/* 1. return channel privilege to CNM immediately */
		prMsgChAbort = (struct MSG_CH_ABORT *) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(struct MSG_CH_ABORT));
		if (!prMsgChAbort) {
			ASSERT(0);	/* Can't release Channel to CNM */
			return;
		}

		prMsgChAbort->rMsgHdr.eMsgId = MID_MNY_CNM_CH_ABORT;
		prMsgChAbort->ucBssIndex = prBowFsmInfo->ucBssIndex;
		prMsgChAbort->ucTokenID = prBowFsmInfo->ucSeqNumOfChReq;

		/* FIXME : where to call cnmGetDbdcCapability in BOW? */
		/*prMsgChAbort->eDBDCBand = (prAdapter->aprBssInfo[prMsgChAbort->ucBssIndex])->eDBDCBand;*/
		prMsgChAbort->eDBDCBand = ENUM_BAND_AUTO;

		mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *) prMsgChAbort, MSG_SEND_METHOD_BUF);
	}

}				/* end of aisFsmReleaseCh() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate an Event of "Media Disconnect" to HOST
*
* @param[in] u4Param  Unused timer parameter
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void bowChGrantedTimeout(IN struct ADAPTER *prAdapter, IN unsigned long ulParamPtr)
{
	struct BOW_FSM_INFO *prBowFsmInfo;
	enum ENUM_BOW_DEVICE_STATE eFsmState;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	DBGLOG(BOW, EVENT, "BoW Channel [TIMEOUT]\n");

#if 1
	/* bowReleaseCh(prAdapter); */
	eFsmState = bowGetBowTableState(prAdapter, prBowFsmInfo->aucPeerAddress);

	/*If connecting is not completed, request CH again */
	if ((eFsmState == BOW_DEVICE_STATE_CONNECTING) || (eFsmState == BOW_DEVICE_STATE_STARTING))
		bowRequestCh(prAdapter);
#endif
}

#endif /* Marked for MT6630 */

u_int8_t bowNotifyAllLinkDisconnected(IN struct ADAPTER *prAdapter)
{
#if 1				/* Marked for MT6630 */

	uint8_t ucBowTableIdx = 0;
	struct CMD_INFO rCmdInfo;
	struct BOW_FSM_INFO *prBowFsmInfo;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	kalMemZero(&rCmdInfo, sizeof(struct CMD_INFO));

	while (ucBowTableIdx < CFG_BOW_PHYSICAL_LINK_NUM) {
		if (arBowTable[ucBowTableIdx].fgIsValid) {
			COPY_MAC_ADDR(prAdapter->rWifiVar.rBowFsmInfo.aucPeerAddress,
				      arBowTable[ucBowTableIdx].aucPeerAddress);
			DBGLOG(BOW, EVENT,
			       "bowNotifyAllLinkDisconnected, arBowTable[%x].aucPeerAddress, %x:%x:%x:%x:%x:%x.\n",
			       ucBowTableIdx, arBowTable[ucBowTableIdx].aucPeerAddress[0],
			       arBowTable[ucBowTableIdx].aucPeerAddress[1],
			       arBowTable[ucBowTableIdx].aucPeerAddress[2],
			       arBowTable[ucBowTableIdx].aucPeerAddress[3],
			       arBowTable[ucBowTableIdx].aucPeerAddress[4],
			       arBowTable[ucBowTableIdx].aucPeerAddress[5]);
			DBGLOG(BOW, EVENT,
			       "bowNotifyAllLinkDisconnected, arBowTable[%x].fgIsValid, %x.\n",
			       ucBowTableIdx, arBowTable[ucBowTableIdx].fgIsValid);
#if 1
			wlanoidSendSetQueryBowCmd(prAdapter,
						  CMD_ID_CMD_BT_OVER_WIFI,
						  prBowFsmInfo->ucBssIndex,
						  TRUE,
						  FALSE,
						  wlanbowCmdEventLinkDisconnected,
						  wlanbowCmdTimeoutHandler, 0, NULL, 0);
#else
			wlanbowCmdEventLinkDisconnected(prAdapter, &rCmdInfo, NULL);
#endif
		}

		ucBowTableIdx += 1;
	}

	return TRUE;

#else
	return 0;
#endif
}

#if 1				/* Marked for MT6630 */

/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve Bluetooth-over-Wi-Fi state from glue layer
*
* \param[in]
*           prGlueInfo
*           rPeerAddr
* \return
*           ENUM_BOW_DEVICE_STATE
*/
/*----------------------------------------------------------------------------*/

u_int8_t bowCheckBowTableIfVaild(IN struct ADAPTER *prAdapter, IN uint8_t aucPeerAddress[6])
{
	uint8_t idx;

	KAL_SPIN_LOCK_DECLARATION();
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);

	for (idx = 0; idx < CFG_BOW_PHYSICAL_LINK_NUM; idx++) {
		if (arBowTable[idx].fgIsValid && EQUAL_MAC_ADDR(arBowTable[idx].aucPeerAddress, aucPeerAddress)) {

			DBGLOG(BOW, EVENT,
			       "kalCheckBowifVaild, aucPeerAddress %x, %x:%x:%x:%x:%x:%x.\n", idx,
			       aucPeerAddress[0], aucPeerAddress[1], aucPeerAddress[2],
			       aucPeerAddress[3], aucPeerAddress[4], aucPeerAddress[5]);

			DBGLOG(BOW, EVENT,
			       "kalCheckBowifVaild, arBowTable[idx].aucPeerAddress %x, %x:%x:%x:%x:%x:%x.\n",
			       idx, arBowTable[idx].aucPeerAddress[0],
			       arBowTable[idx].aucPeerAddress[1],
			       arBowTable[idx].aucPeerAddress[2],
			       arBowTable[idx].aucPeerAddress[3],
			       arBowTable[idx].aucPeerAddress[4], arBowTable[idx].aucPeerAddress[5]);

			DBGLOG(BOW, EVENT,
			       "kalCheckBowifVaild, arBowTable[idx].fgIsValid, %x, %x.\n", idx,
			       arBowTable[idx].fgIsValid);

			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);
			return TRUE;
		}
	}

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);
	return FALSE;
}

u_int8_t bowGetBowTableContent(IN struct ADAPTER *prAdapter, IN uint8_t ucBowTableIdx, OUT struct BOW_TABLE *prBowTable)
{
	KAL_SPIN_LOCK_DECLARATION();
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);

	if (arBowTable[ucBowTableIdx].fgIsValid) {

		DBGLOG(BOW, EVENT,
		       "bowGetBowTableContent, arBowTable[idx].fgIsValid, %x, %x.\n",
		       ucBowTableIdx, arBowTable[ucBowTableIdx].fgIsValid);
		DBGLOG(BOW, EVENT, "GET State [%d]\n", arBowTable[ucBowTableIdx].eState);
		prBowTable = &(arBowTable[ucBowTableIdx]);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);

		return TRUE;
	}

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);

	return FALSE;
}

u_int8_t bowSetBowTableContent(IN struct ADAPTER *prAdapter, IN uint8_t ucBowTableIdx, IN struct BOW_TABLE *prBowTable)
{
	KAL_SPIN_LOCK_DECLARATION();
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);

	COPY_MAC_ADDR(arBowTable[ucBowTableIdx].aucPeerAddress, prBowTable->aucPeerAddress);
	arBowTable[ucBowTableIdx].eState = prBowTable->eState;
	arBowTable[ucBowTableIdx].fgIsValid = prBowTable->fgIsValid;
	arBowTable[ucBowTableIdx].ucAcquireID = prBowTable->ucAcquireID;

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);

	kalSetBowState(prAdapter->prGlueInfo, prBowTable->eState, prBowTable->aucPeerAddress);
	/* kalSetBowRole(prAdapter->prGlueInfo, prBowTable->ucRole, prBowTable->aucPeerAddress); */

	DBGLOG(BOW, EVENT, "SET State [%d]\n", arBowTable[ucBowTableIdx].eState);
	DBGLOG(BOW, EVENT,
	       "kalCheckBowifVaild, arBowTable[ucBowTableIdx].fgIsValid, %x, %x.\n", ucBowTableIdx,
	       arBowTable[ucBowTableIdx].fgIsValid);

	return TRUE;

}

u_int8_t
bowGetBowTableEntryByPeerAddress(IN struct ADAPTER *prAdapter, IN uint8_t aucPeerAddress[6], OUT uint8_t *pucBowTableIdx)
{
	uint8_t idx;

	KAL_SPIN_LOCK_DECLARATION();
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);

	for (idx = 0; idx < CFG_BOW_PHYSICAL_LINK_NUM; idx++) {
		if (arBowTable[idx].fgIsValid && EQUAL_MAC_ADDR(arBowTable[idx].aucPeerAddress, aucPeerAddress)) {

			DBGLOG(BOW, EVENT,
			       "kalCheckBowifVaild, aucPeerAddress %x, %x:%x:%x:%x:%x:%x.\n", idx,
			       aucPeerAddress[0], aucPeerAddress[1], aucPeerAddress[2],
			       aucPeerAddress[3], aucPeerAddress[4], aucPeerAddress[5]);
			DBGLOG(BOW, EVENT,
			       "kalCheckBowifVaild, arBowTable[idx].aucPeerAddress %x, %x:%x:%x:%x:%x:%x.\n",
			       idx, arBowTable[idx].aucPeerAddress[0],
			       arBowTable[idx].aucPeerAddress[1],
			       arBowTable[idx].aucPeerAddress[2],
			       arBowTable[idx].aucPeerAddress[3],
			       arBowTable[idx].aucPeerAddress[4], arBowTable[idx].aucPeerAddress[5]);
			DBGLOG(BOW, EVENT,
			       "kalCheckBowifVaild, arBowTable[idx].fgIsValid, %x, %x.\n", idx,
			       arBowTable[idx].fgIsValid);
			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);

			*pucBowTableIdx = idx;

			return TRUE;
		}
	}

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);

	return FALSE;
}

u_int8_t bowGetBowTableFreeEntry(IN struct ADAPTER *prAdapter, OUT uint8_t *pucBowTableIdx)
{
	uint8_t idx;

	KAL_SPIN_LOCK_DECLARATION();
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);

	for (idx = 0; idx < CFG_BOW_PHYSICAL_LINK_NUM; idx++) {
		if (!arBowTable[idx].fgIsValid) {
			DBGLOG(BOW, EVENT,
			       "bowGetBowTableFreeEntry, arBowTable[idx].fgIsValid, %x, %x.\n",
			       idx, arBowTable[idx].fgIsValid);
			*pucBowTableIdx = idx;
			arBowTable[idx].fgIsValid = TRUE;

			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);

			return TRUE;
		}
	}

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);

	return FALSE;
}

enum ENUM_BOW_DEVICE_STATE bowGetBowTableState(IN struct ADAPTER *prAdapter, IN uint8_t aucPeerAddress[6])
{
	uint8_t idx;

	KAL_SPIN_LOCK_DECLARATION();
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);

	for (idx = 0; idx < CFG_BOW_PHYSICAL_LINK_NUM; idx++) {
		if (arBowTable[idx].fgIsValid && EQUAL_MAC_ADDR(arBowTable[idx].aucPeerAddress, aucPeerAddress)) {
			DBGLOG(BOW, EVENT,
			       "bowGetState, aucPeerAddress %x, %x:%x:%x:%x:%x:%x.\n", idx,
			       aucPeerAddress[0], aucPeerAddress[1], aucPeerAddress[2],
			       aucPeerAddress[3], aucPeerAddress[4], aucPeerAddress[5]);
			DBGLOG(BOW, EVENT,
			       "bowGetState, arBowTable[idx].aucPeerAddress %x, %x:%x:%x:%x:%x:%x.\n",
			       idx, arBowTable[idx].aucPeerAddress[0],
			       arBowTable[idx].aucPeerAddress[1],
			       arBowTable[idx].aucPeerAddress[2],
			       arBowTable[idx].aucPeerAddress[3],
			       arBowTable[idx].aucPeerAddress[4], arBowTable[idx].aucPeerAddress[5]);
			DBGLOG(BOW, EVENT,
			       "bowGetState, arBowTable[idx].fgIsValid, %x, %x.\n", idx, arBowTable[idx].fgIsValid);
			DBGLOG(BOW, EVENT,
			       "bowGetState, arBowTable[idx].eState;, %x, %x.\n", idx, arBowTable[idx].eState);
			DBGLOG(BOW, EVENT, "GET State [%d]\n", arBowTable[idx].eState);

			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);

			return arBowTable[idx].eState;
		}
	}

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);

	return BOW_DEVICE_STATE_DISCONNECTED;
}

u_int8_t bowSetBowTableState(IN struct ADAPTER *prAdapter, IN uint8_t aucPeerAddress[6], IN enum ENUM_BOW_DEVICE_STATE eState)
{
	uint8_t ucBowTableIdx;

	if (bowGetBowTableEntryByPeerAddress(prAdapter, aucPeerAddress, &ucBowTableIdx)) {
		KAL_SPIN_LOCK_DECLARATION();
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);

		arBowTable[ucBowTableIdx].eState = eState;
		DBGLOG(BOW, EVENT, "SET State [%d]\n", eState);

		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_BOW_TABLE);

		kalSetBowState(prAdapter->prGlueInfo, eState, aucPeerAddress);
		return TRUE;
	}
	return FALSE;
}

#endif

#endif /* Marked for MT6630 */
