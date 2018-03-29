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

/*! \file wlan_bow.c
    \brief This file contains the 802.11 PAL commands processing routines for
	   MediaTek Inc. 802.11 Wireless LAN Adapters.
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
static UINT_32 g_u4LinkCount;
static UINT_32 g_u4Beaconing;
static BOW_TABLE_T arBowTable[CFG_BOW_PHYSICAL_LINK_NUM];
#endif

/******************************************************************************
*                           P R I V A T E   D A T A
*******************************************************************************
*/

const BOW_CMD_T arBowCmdTable[] = {
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
WLAN_STATUS
wlanoidSendSetQueryBowCmd(IN P_ADAPTER_T prAdapter,
			  IN UINT_8 ucCID,
			  IN UINT_8 ucBssIdx,
			  IN BOOLEAN fgSetQuery,
			  IN BOOLEAN fgNeedResp,
			  IN PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
			  IN PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler,
			  IN UINT_32 u4SetQueryInfoLen, IN PUINT_8 pucInfoBuffer, IN UINT_8 ucSeqNumber)
{
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_CMD_T prWifiCmd;
	UINT_8 ucCmdSeqNum;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;
	ASSERT(prGlueInfo);

	DBGLOG(REQ, TRACE, "Command ID = 0x%08X\n", ucCID);

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + u4SetQueryInfoLen));

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}
	/* increase command sequence number */
	ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);
	DBGLOG(REQ, TRACE, "ucCmdSeqNum =%d\n", ucCmdSeqNum);

	/* Setup common CMD Info Packet */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->ucBssIndex = ucBssIdx;
	prCmdInfo->u2InfoBufLen = (UINT_16) (CMD_HDR_SIZE + u4SetQueryInfoLen);
	prCmdInfo->pfCmdDoneHandler = pfCmdDoneHandler;
	prCmdInfo->pfCmdTimeoutHandler = pfCmdTimeoutHandler;
	prCmdInfo->fgIsOid = FALSE;
	prCmdInfo->ucCID = ucCID;
	prCmdInfo->fgSetQuery = fgSetQuery;
	prCmdInfo->fgNeedResp = fgNeedResp;
	prCmdInfo->fgDriverDomainMCR = FALSE;
	prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
	prCmdInfo->u4SetInfoLen = u4SetQueryInfoLen;
	prCmdInfo->pvInformationBuffer = NULL;
	prCmdInfo->u4InformationBufferLength = 0;
	prCmdInfo->u4PrivateData = (UINT_32) ucSeqNumber;

	/* Setup WIFI_CMD_T (no payload) */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
	prWifiCmd->ucCID = prCmdInfo->ucCID;
	prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
	prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	if (u4SetQueryInfoLen > 0 && pucInfoBuffer != NULL)
		kalMemCopy(prWifiCmd->aucBuffer, pucInfoBuffer, u4SetQueryInfoLen);
	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

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
WLAN_STATUS wlanbowHandleCommand(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd)
{
#if 1				/* Marked for MT6630 */
	WLAN_STATUS retval = WLAN_STATUS_FAILURE;
	UINT_16 i;

	ASSERT(prAdapter);

	for (i = 0; i < sizeof(arBowCmdTable) / sizeof(BOW_CMD_T); i++) {
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
WLAN_STATUS bowCmdGetMacStatus(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd)
{
#if 1				/* Marked for MT6630 */
	P_AMPC_EVENT prEvent;
	P_BOW_MAC_STATUS prMacStatus;
	UINT_8 idx = 0;
	UINT_8 ucPrimaryChannel;
	ENUM_BAND_T eBand;
	ENUM_CHNL_EXT_T eBssSCO;
	UINT_8 ucNumOfChannel = 0;	/* MAX_BOW_NUMBER_OF_CHANNEL; */

	RF_CHANNEL_INFO_T aucChannelList[MAX_BOW_NUMBER_OF_CHANNEL];

	ASSERT(prAdapter);

	/* 3 <1> If LinkCount != 0 -> OK (optional) */

	eBand = BAND_2G4;
	eBssSCO = CHNL_EXT_SCN;

	/* fill event header */
	prEvent = (P_AMPC_EVENT) kalMemAlloc((sizeof(AMPC_EVENT) + sizeof(BOW_MAC_STATUS)), VIR_MEM_TYPE);

	if (prEvent == NULL)
		return WLAN_STATUS_FAILURE;

	prEvent->rHeader.ucEventId = BOW_EVENT_ID_MAC_STATUS;
	prEvent->rHeader.ucSeqNumber = prCmd->rHeader.ucSeqNumber;
	prEvent->rHeader.u2PayloadLength = sizeof(BOW_MAC_STATUS);

	/* fill event body */
	prMacStatus = (P_BOW_MAC_STATUS) (prEvent->aucPayload);
	kalMemZero(prMacStatus, sizeof(BOW_MAC_STATUS));

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
			for (idx = 0; idx < ucNumOfChannel; idx++) {
				prMacStatus->arChannelList[idx].ucChannelBand = aucChannelList[idx].eBand;
				prMacStatus->arChannelList[idx].ucChannelNum = aucChannelList[idx].ucChannelNum;
			}

			prMacStatus->ucNumOfChannel = ucNumOfChannel;
		}

		rlmDomainGetChnlList(prAdapter, BAND_5G, FALSE, MAX_BOW_NUMBER_OF_CHANNEL_5G,
				     &ucNumOfChannel, aucChannelList);

		if (ucNumOfChannel > 0) {
			for (idx = 0; idx < ucNumOfChannel; idx++) {
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
		prMacStatus->arChannelList[16].ucChannelNum,
		prMacStatus->arChannelList[17].ucChannelNum);

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

	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(AMPC_EVENT) + sizeof(BOW_MAC_STATUS)));

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
WLAN_STATUS bowCmdSetupConnection(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd)
{
#if 1				/* Marked for MT6630 */
	P_BOW_SETUP_CONNECTION prBowSetupConnection;
	CMD_BT_OVER_WIFI rCmdBtOverWifi;
	P_BOW_FSM_INFO_T prBowFsmInfo;
	BOW_TABLE_T rBowTable;

	UINT_8 ucBowTableIdx = 0;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBowSetupConnection = (P_BOW_SETUP_CONNECTION) &(prCmd->aucPayload[0]);

	/* parameter size check */
	if (prCmd->rHeader.u2PayloadLength != sizeof(BOW_SETUP_CONNECTION)) {
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
				  (PFN_MGMT_TIMEOUT_FUNC) bowSendBeacon, (ULONG) NULL);

		cnmTimerInitTimer(prAdapter,
				  &prBowFsmInfo->rChGrantedTimer,
				  (PFN_MGMT_TIMEOUT_FUNC) bowChGrantedTimeout, (ULONG) NULL);

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
WLAN_STATUS bowCmdDestroyConnection(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd)
{
#if 1				/* Marked for MT6630 */
	P_BOW_DESTROY_CONNECTION prBowDestroyConnection;
	CMD_BT_OVER_WIFI rCmdBtOverWifi;
	P_BOW_FSM_INFO_T prBowFsmInfo;

	UINT_8 ucIdx;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	/* 3 <1> If LinkCount == 0 ->Fail (Optional) */
	if (g_u4LinkCount == 0) {
		wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_UNACCEPTED);
		return WLAN_STATUS_NOT_ACCEPTED;
	}
	/* parameter size check */
	if (prCmd->rHeader.u2PayloadLength != sizeof(BOW_DESTROY_CONNECTION)) {
		wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_UNACCEPTED);
		return WLAN_STATUS_INVALID_LENGTH;
	}
	/* 3 <2> Lookup BOW table, check if is not exist (Valid and Peer MAC address) -> Fail */
	prBowDestroyConnection = (P_BOW_DESTROY_CONNECTION) &(prCmd->aucPayload[0]);

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
					 sizeof(CMD_BT_OVER_WIFI),
					 (PUINT_8)&rCmdBtOverWifi, prCmd->rHeader.ucSeqNumber);

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
WLAN_STATUS bowCmdSetPTK(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd)
{
#if 1				/* Marked for MT6630 */
	P_BOW_SET_PTK prBowSetPTK;
	CMD_802_11_KEY rCmdKey;
	P_BOW_FSM_INFO_T prBowFsmInfo;
	P_STA_RECORD_T prStaRec = NULL;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	/* parameter size check */
	if (prCmd->rHeader.u2PayloadLength != sizeof(BOW_SET_PTK))
		return WLAN_STATUS_INVALID_LENGTH;

	prBowSetPTK = (P_BOW_SET_PTK) &(prCmd->aucPayload[0]);

	DBGLOG(BOW, EVENT, "prBowSetPTK->aucPeerAddress, %x:%x:%x:%x:%x:%x.\n",
			    prBowSetPTK->aucPeerAddress[0],
			    prBowSetPTK->aucPeerAddress[1],
			    prBowSetPTK->aucPeerAddress[2],
			    prBowSetPTK->aucPeerAddress[3],
			    prBowSetPTK->aucPeerAddress[4], prBowSetPTK->aucPeerAddress[5]);

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

	if (prStaRec == NULL)
		return WLAN_STATUS_FAILURE;

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
					 sizeof(CMD_802_11_KEY), (PUINT_8)&rCmdKey, prCmd->rHeader.ucSeqNumber);
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
WLAN_STATUS bowCmdReadRSSI(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd)
{
#if 1				/* Marked for MT6630 */
	P_BOW_READ_RSSI prBowReadRSSI;
	P_BOW_FSM_INFO_T prBowFsmInfo;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	/* parameter size check */
	if (prCmd->rHeader.u2PayloadLength != sizeof(BOW_READ_RSSI))
		return WLAN_STATUS_INVALID_LENGTH;

	prBowReadRSSI = (P_BOW_READ_RSSI) &(prCmd->aucPayload[0]);

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
WLAN_STATUS bowCmdReadLinkQuality(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd)
{
#if 1				/* Marked for MT6630 */
	P_BOW_READ_LINK_QUALITY prBowReadLinkQuality;
	P_BOW_FSM_INFO_T prBowFsmInfo;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	/* parameter size check */
	if (prCmd->rHeader.u2PayloadLength != sizeof(P_BOW_READ_LINK_QUALITY))
		return WLAN_STATUS_INVALID_LENGTH;

	prBowReadLinkQuality = (P_BOW_READ_LINK_QUALITY) &(prCmd->aucPayload[0]);

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
WLAN_STATUS bowCmdShortRangeMode(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd)
{
#if 1				/* Marked for MT6630 */
	P_BOW_SHORT_RANGE_MODE prBowShortRangeMode;
	CMD_TX_PWR_T rTxPwrParam;

	ASSERT(prAdapter);

	DBGLOG(BOW, EVENT, "bowCmdShortRangeMode.\n");

	prBowShortRangeMode = (P_BOW_SHORT_RANGE_MODE) &(prCmd->aucPayload[0]);

	/* parameter size check */
	if (prCmd->rHeader.u2PayloadLength != sizeof(BOW_SHORT_RANGE_MODE)) {
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

	if (nicUpdateTxPower(prAdapter, &rTxPwrParam) != WLAN_STATUS_SUCCESS) {
		wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_FAILURE);
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(BOW, EVENT, "bowCmdShortRangeMode, %x.\n", WLAN_STATUS_SUCCESS);
	wlanbowCmdEventSetStatus(prAdapter, prCmd, BOWCMD_STATUS_SUCCESS);
	return WLAN_STATUS_SUCCESS;
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
WLAN_STATUS bowCmdGetChannelList(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd)
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
VOID wlanbowCmdEventSetStatus(IN P_ADAPTER_T prAdapter, IN P_AMPC_COMMAND prCmd, IN UINT_8 ucEventBuf)
{
	P_AMPC_EVENT prEvent;
	P_BOW_COMMAND_STATUS prBowCmdStatus;

	ASSERT(prAdapter);

	/* fill event header */
	prEvent = (P_AMPC_EVENT) kalMemAlloc((sizeof(AMPC_EVENT) + sizeof(BOW_COMMAND_STATUS)), VIR_MEM_TYPE);
	if (prEvent == NULL)
		return;
	prEvent->rHeader.ucEventId = BOW_EVENT_ID_COMMAND_STATUS;
	prEvent->rHeader.ucSeqNumber = prCmd->rHeader.ucSeqNumber;
	prEvent->rHeader.u2PayloadLength = sizeof(BOW_COMMAND_STATUS);

	/* fill event body */
	prBowCmdStatus = (P_BOW_COMMAND_STATUS) (prEvent->aucPayload);
	kalMemZero(prBowCmdStatus, sizeof(BOW_COMMAND_STATUS));

	prBowCmdStatus->ucStatus = ucEventBuf;

	kalIndicateBOWEvent(prAdapter->prGlueInfo, prEvent);

	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(AMPC_EVENT) + sizeof(BOW_COMMAND_STATUS)));
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
VOID wlanbowCmdEventSetCommon(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_AMPC_EVENT prEvent;
	P_BOW_COMMAND_STATUS prBowCmdStatus;

	ASSERT(prAdapter);

	/* fill event header */
	prEvent = (P_AMPC_EVENT) kalMemAlloc((sizeof(AMPC_EVENT) + sizeof(BOW_COMMAND_STATUS)), VIR_MEM_TYPE);
	if (prEvent == NULL)
		return;
	prEvent->rHeader.ucEventId = BOW_EVENT_ID_COMMAND_STATUS;
	prEvent->rHeader.ucSeqNumber = (UINT_8) prCmdInfo->u4PrivateData;
	prEvent->rHeader.u2PayloadLength = sizeof(BOW_COMMAND_STATUS);

	/* fill event body */
	prBowCmdStatus = (P_BOW_COMMAND_STATUS) (prEvent->aucPayload);
	kalMemZero(prBowCmdStatus, sizeof(BOW_COMMAND_STATUS));

	prBowCmdStatus->ucStatus = BOWCMD_STATUS_SUCCESS;

	kalIndicateBOWEvent(prAdapter->prGlueInfo, prEvent);

	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(AMPC_EVENT) + sizeof(BOW_COMMAND_STATUS)));
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
VOID wlanbowCmdEventLinkConnected(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_AMPC_EVENT prEvent;
	P_BOW_LINK_CONNECTED prBowLinkConnected;
	P_BOW_FSM_INFO_T prBowFsmInfo;
	P_BSS_INFO_T prBssInfo;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

	/* fill event header */
	prEvent = (P_AMPC_EVENT) kalMemAlloc((sizeof(AMPC_EVENT) + sizeof(BOW_LINK_CONNECTED)), VIR_MEM_TYPE);
	if (prEvent == NULL)
		return;
	prEvent->rHeader.ucEventId = BOW_EVENT_ID_LINK_CONNECTED;
	prEvent->rHeader.ucSeqNumber = (UINT_8) prCmdInfo->u4PrivateData;
	prEvent->rHeader.u2PayloadLength = sizeof(BOW_LINK_CONNECTED);

	/* fill event body */
	prBowLinkConnected = (P_BOW_LINK_CONNECTED) (prEvent->aucPayload);
	kalMemZero(prBowLinkConnected, sizeof(BOW_LINK_CONNECTED));
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
	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(AMPC_EVENT) + sizeof(BOW_LINK_CONNECTED)));

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
VOID wlanbowCmdEventLinkDisconnected(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_AMPC_EVENT prEvent;
	P_BOW_LINK_DISCONNECTED prBowLinkDisconnected;
	P_BOW_FSM_INFO_T prBowFsmInfo;
	BOW_TABLE_T rBowTable;
	UINT_8 ucBowTableIdx;
	ENUM_BOW_DEVICE_STATE eFsmState;
	BOOL fgSendDeauth = FALSE;

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
	prEvent = (P_AMPC_EVENT) kalMemAlloc((sizeof(AMPC_EVENT) + sizeof(BOW_LINK_DISCONNECTED)), VIR_MEM_TYPE);
	if (prEvent == NULL)
		return;
	prEvent->rHeader.ucEventId = BOW_EVENT_ID_LINK_DISCONNECTED;
	if ((prCmdInfo->u4PrivateData))
		prEvent->rHeader.ucSeqNumber = (UINT_8) prCmdInfo->u4PrivateData;
	else
		prEvent->rHeader.ucSeqNumber = 0;

	prEvent->rHeader.u2PayloadLength = sizeof(BOW_LINK_DISCONNECTED);

	/* fill event body */
	prBowLinkDisconnected = (P_BOW_LINK_DISCONNECTED) (prEvent->aucPayload);
	kalMemZero(prBowLinkDisconnected, sizeof(BOW_LINK_DISCONNECTED));
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
	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(AMPC_EVENT) + sizeof(BOW_LINK_DISCONNECTED)));
#endif

	/* set to disconnected status */
	prBowFsmInfo->prTargetStaRec =
	    cnmGetStaRecByAddress(prAdapter, prBowFsmInfo->ucBssIndex, prBowLinkDisconnected->aucPeerAddress);

	if (prBowFsmInfo->prTargetStaRec == NULL) {
		kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(AMPC_EVENT) + sizeof(BOW_LINK_DISCONNECTED)));
		return;
	}

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
				    (P_SW_RFB_T) NULL, REASON_CODE_DEAUTH_LEAVING_BSS,
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
	bowSetBowTableContent(prAdapter, ucBowTableIdx, &rBowTable);

	/*Indicate BoW event to PAL */
	kalIndicateBOWEvent(prAdapter->prGlueInfo, prEvent);
	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(AMPC_EVENT) + sizeof(BOW_LINK_DISCONNECTED)));

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
VOID wlanbowCmdEventSetSetupConnection(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_AMPC_EVENT prEvent;
	P_BOW_COMMAND_STATUS prBowCmdStatus;
	P_WIFI_CMD_T prWifiCmd;
	P_CMD_BT_OVER_WIFI prCmdBtOverWifi;
	P_BOW_FSM_INFO_T prBowFsmInfo;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	/* restore original command for rPeerAddr */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prCmdBtOverWifi = (P_CMD_BT_OVER_WIFI) (prWifiCmd->aucBuffer);

	/* fill event header */
	prEvent = (P_AMPC_EVENT) kalMemAlloc((sizeof(AMPC_EVENT) + sizeof(BOW_COMMAND_STATUS)), VIR_MEM_TYPE);
	if (prEvent == NULL)
		return;
	prEvent->rHeader.ucEventId = BOW_EVENT_ID_COMMAND_STATUS;
	prEvent->rHeader.ucSeqNumber = (UINT_8) prCmdInfo->u4PrivateData;
	prEvent->rHeader.u2PayloadLength = sizeof(BOW_COMMAND_STATUS);

	/* fill event body */
	prBowCmdStatus = (P_BOW_COMMAND_STATUS) (prEvent->aucPayload);
	kalMemZero(prBowCmdStatus, sizeof(BOW_COMMAND_STATUS));
	prBowCmdStatus->ucStatus = BOWCMD_STATUS_SUCCESS;

	/*Indicate BoW event to PAL */
	kalIndicateBOWEvent(prAdapter->prGlueInfo, prEvent);
	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(AMPC_EVENT) + sizeof(BOW_COMMAND_STATUS)));

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
VOID wlanbowCmdEventReadLinkQuality(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_LINK_QUALITY prLinkQuality;
	P_AMPC_EVENT prEvent;
	P_BOW_LINK_QUALITY prBowLinkQuality;

	ASSERT(prAdapter);

	prLinkQuality = (P_EVENT_LINK_QUALITY) pucEventBuf;

	/* fill event header */
	prEvent = (P_AMPC_EVENT) kalMemAlloc((sizeof(AMPC_EVENT) + sizeof(BOW_LINK_QUALITY)), VIR_MEM_TYPE);
	if (prEvent == NULL)
		return;
	prEvent->rHeader.ucEventId = BOW_EVENT_ID_LINK_QUALITY;
	prEvent->rHeader.ucSeqNumber = (UINT_8) prCmdInfo->u4PrivateData;
	prEvent->rHeader.u2PayloadLength = sizeof(BOW_LINK_QUALITY);

	/* fill event body */
	prBowLinkQuality = (P_BOW_LINK_QUALITY) (prEvent->aucPayload);
	kalMemZero(prBowLinkQuality, sizeof(BOW_LINK_QUALITY));
	prBowLinkQuality->ucLinkQuality = (UINT_8) prLinkQuality->cLinkQuality;

	kalIndicateBOWEvent(prAdapter->prGlueInfo, prEvent);

	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(AMPC_EVENT) + sizeof(BOW_LINK_QUALITY)));
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
VOID wlanbowCmdEventReadRssi(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_LINK_QUALITY prLinkQuality;
	P_AMPC_EVENT prEvent;
	P_BOW_RSSI prBowRssi;

	ASSERT(prAdapter);

	prLinkQuality = (P_EVENT_LINK_QUALITY) pucEventBuf;

	/* fill event header */
	prEvent = (P_AMPC_EVENT) kalMemAlloc((sizeof(AMPC_EVENT) + sizeof(BOW_LINK_QUALITY)), VIR_MEM_TYPE);
	if (prEvent == NULL)
		return;
	prEvent->rHeader.ucEventId = BOW_EVENT_ID_RSSI;
	prEvent->rHeader.ucSeqNumber = (UINT_8) prCmdInfo->u4PrivateData;
	prEvent->rHeader.u2PayloadLength = sizeof(BOW_RSSI);

	/* fill event body */
	prBowRssi = (P_BOW_RSSI) (prEvent->aucPayload);
	kalMemZero(prBowRssi, sizeof(BOW_RSSI));
	prBowRssi->cRssi = (INT_8) prLinkQuality->cRssi;

	kalIndicateBOWEvent(prAdapter->prGlueInfo, prEvent);

	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(AMPC_EVENT) + sizeof(BOW_LINK_QUALITY)));

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
VOID wlanbowCmdTimeoutHandler(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
	P_AMPC_EVENT prEvent;
	P_BOW_COMMAND_STATUS prBowCmdStatus;

	ASSERT(prAdapter);

	/* fill event header */
	prEvent = (P_AMPC_EVENT) kalMemAlloc((sizeof(AMPC_EVENT) + sizeof(BOW_COMMAND_STATUS)), VIR_MEM_TYPE);
	if (prEvent == NULL)
		return;
	prEvent->rHeader.ucEventId = BOW_EVENT_ID_COMMAND_STATUS;
	prEvent->rHeader.ucSeqNumber = (UINT_8) prCmdInfo->u4PrivateData;
	prEvent->rHeader.u2PayloadLength = sizeof(BOW_COMMAND_STATUS);

	/* fill event body */
	prBowCmdStatus = (P_BOW_COMMAND_STATUS) (prEvent->aucPayload);
	kalMemZero(prBowCmdStatus, sizeof(BOW_COMMAND_STATUS));

	prBowCmdStatus->ucStatus = BOWCMD_STATUS_TIMEOUT;	/* timeout */

	kalIndicateBOWEvent(prAdapter->prGlueInfo, prEvent);

	kalMemFree(prEvent, VIR_MEM_TYPE, (sizeof(AMPC_EVENT) + sizeof(BOW_COMMAND_STATUS)));
}

/* Bruce, 20140224 */
UINT_8 bowInit(IN P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prBowBssInfo;
	P_BOW_FSM_INFO_T prBowFsmInfo;

	ASSERT(prAdapter);

	prBowBssInfo = cnmGetBssInfoAndInit(prAdapter, NETWORK_TYPE_BOW, TRUE);
	if (prBowBssInfo == NULL)
		return BSS_INFO_NUM;

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
VOID bowUninit(IN P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prBowBssInfo;
	P_BOW_FSM_INFO_T prBowFsmInfo;

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBowBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

	cnmFreeBssInfo(prAdapter, prBowBssInfo);
}

VOID bowStopping(IN P_ADAPTER_T prAdapter)
{
	P_BOW_FSM_INFO_T prBowFsmInfo;
	P_BSS_INFO_T prBowBssInfo;

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
			    prBowBssInfo->aucOwnMacAddr[3],
			    prBowBssInfo->aucOwnMacAddr[4], prBowBssInfo->aucOwnMacAddr[5]);
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
		bowChangeMediaState(prBowBssInfo, PARAM_MEDIA_STATE_DISCONNECTED);
		nicUpdateBss(prAdapter, prBowBssInfo->ucBssIndex);
		/*temp solution for FW hal_pwr_mgt.c#3037 ASSERT */
		nicDeactivateNetwork(prAdapter, prBowBssInfo->ucBssIndex);
		SET_NET_PWR_STATE_IDLE(prAdapter, prBowBssInfo->ucBssIndex);
		UNSET_NET_ACTIVE(prAdapter, prBowBssInfo->ucBssIndex);

	}

}

VOID bowStarting(IN P_ADAPTER_T prAdapter)
{
	P_BOW_FSM_INFO_T prBowFsmInfo;
	P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;

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
				    prBssInfo->aucBSSID[2],
				    prBssInfo->aucBSSID[3], prBssInfo->aucBSSID[4], prBssInfo->aucBSSID[5]);
		DBGLOG(BOW, EVENT, "prBssInfo->aucOwnMacAddr, %x:%x:%x:%x:%x:%x.\n",
				    prBssInfo->aucOwnMacAddr[0],
				    prBssInfo->aucOwnMacAddr[1],
				    prBssInfo->aucOwnMacAddr[2],
				    prBssInfo->aucOwnMacAddr[3],
				    prBssInfo->aucOwnMacAddr[4], prBssInfo->aucOwnMacAddr[5]);
		DBGLOG(BOW, EVENT, "prAdapter->rWifiVar.aucDeviceAddress, %x:%x:%x:%x:%x:%x.\n",
				    prAdapter->rWifiVar.aucDeviceAddress[0],
				    prAdapter->rWifiVar.aucDeviceAddress[1],
				    prAdapter->rWifiVar.aucDeviceAddress[2],
				    prAdapter->rWifiVar.aucDeviceAddress[3],
				    prAdapter->rWifiVar.aucDeviceAddress[4], prAdapter->rWifiVar.aucDeviceAddress[5]);

		/* 4 <1.3> Clear current AP's STA_RECORD_T and current AID */
		prBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;
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

		prBssInfo->ucNonHTBasicPhyType = (UINT_8)
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
			UINT_32 u4RxFilter;

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
	if ((g_u4Beaconing < 1) && (prBssInfo)) {
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
		 bowRequestCh(prAdapter); */
#endif

	/* wlanBindBssIdxToNetInterface(prAdapter->prGlueInfo, NET_DEV_BOW_IDX, prBssInfo->ucBssIndex); */
}

VOID bowAssignSsid(IN PUINT_8 pucSsid, IN PUINT_8 puOwnMacAddr)
{
	UINT_8 i;
	UINT_8 aucSSID[] = BOW_WILDCARD_SSID;

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
BOOLEAN bowValidateProbeReq(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, OUT PUINT_32 pu4ControlFlags)
{
#if 1				/* Marked for MT6630 */

	P_WLAN_MAC_MGMT_HEADER_T prMgtHdr;
	P_BOW_FSM_INFO_T prBowFsmInfo;
	P_BSS_INFO_T prBssInfo;
	P_IE_SSID_T prIeSsid = (P_IE_SSID_T) NULL;
	PUINT_8 pucIE;
	UINT_16 u2IELength;
	UINT_16 u2Offset = 0;
	BOOLEAN fgReplyProbeResp = FALSE;

	ASSERT(prSwRfb);
	ASSERT(pu4ControlFlags);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

#if 0
	DBGLOG(BOW, EVENT, "bowValidateProbeReq.\n");
#endif

	/* 4 <1> Parse Probe Req IE and Get IE ptr (SSID, Supported Rate IE, ...) */
	prMgtHdr = (P_WLAN_MAC_MGMT_HEADER_T) prSwRfb->pvHeader;

	u2IELength = prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen;
	pucIE = (PUINT_8) (((ULONG) prSwRfb->pvHeader) + prSwRfb->u2HeaderLen);

	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		if (ELEM_ID_SSID == IE_ID(pucIE)) {
			if ((!prIeSsid) && (IE_LEN(pucIE) <= ELEM_MAX_LEN_SSID))
				prIeSsid = (P_IE_SSID_T) pucIE;
			break;
		}
	}			/* end of IE_FOR_EACH */

	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		if (ELEM_ID_SSID == IE_ID(pucIE)) {
			if ((!prIeSsid) && (IE_LEN(pucIE) <= ELEM_MAX_LEN_SSID))
				prIeSsid = (P_IE_SSID_T) pucIE;
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
VOID bowSendBeacon(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr)
{
	P_BOW_FSM_INFO_T prBowFsmInfo;

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
VOID bowResponderScan(IN P_ADAPTER_T prAdapter)
{
	P_BOW_FSM_INFO_T prBowFsmInfo;
	P_MSG_SCN_SCAN_REQ prScanReqMsg;
	P_BSS_INFO_T prBssInfo;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

	DBGLOG(BOW, EVENT, "bowResponderScan.\n");
	DBGLOG(BOW, EVENT, "BOW SCAN [REQ:%d]\n", prBowFsmInfo->ucSeqNumOfScanReq + 1);

	prScanReqMsg = (P_MSG_SCN_SCAN_REQ) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_SCN_SCAN_REQ));

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
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prScanReqMsg, MSG_SEND_METHOD_BUF);

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
VOID bowResponderScanDone(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
#if 1				/* Marked for MT6630 */
	P_MSG_SCN_SCAN_DONE prScanDoneMsg;
	P_BOW_FSM_INFO_T prBowFsmInfo;
	P_BSS_DESC_T prBssDesc;
	UINT_8 ucSeqNumOfCompMsg;
	P_CONNECTION_SETTINGS_T prConnSettings;
	ENUM_BOW_DEVICE_STATE eFsmState;
	ENUM_SCAN_STATUS eScanStatus;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prScanDoneMsg = (P_MSG_SCN_SCAN_DONE) prMsgHdr;
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
	} else if (ucSeqNumOfCompMsg != prBowFsmInfo->ucSeqNumOfScanReq) {
		DBGLOG(BOW, EVENT, "Sequence no. of BOW Responder scan done is not matched.\n");
	} else {
		prConnSettings->fgIsScanReqIssued = FALSE;
		prBssDesc = scanSearchBssDescByBssid(prAdapter, prBowFsmInfo->aucPeerAddress);
		DBGLOG(BOW, EVENT, "End scan result searching.\n");
		DBGLOG(BOW, EVENT,
		       "prBowFsmInfo->aucPeerAddress: [" MACSTR "]\n", MAC2STR(prBowFsmInfo->aucPeerAddress));

		/*Initiator is FOUND */
		if (prBssDesc != NULL) {	/* (prBssDesc->aucBSSID != NULL)) */
			DBGLOG(BOW, EVENT,
			       "Search Bow Peer address - %x:%x:%x:%x:%x:%x.\n",
				prBssDesc->aucBSSID[0], prBssDesc->aucBSSID[1],
				prBssDesc->aucBSSID[2], prBssDesc->aucBSSID[3],
				prBssDesc->aucBSSID[4], prBssDesc->aucBSSID[5]);
			DBGLOG(BOW, EVENT, "Starting to join initiator.\n");

			/*Set target BssDesc */
			prBowFsmInfo->prTargetBssDesc = prBssDesc;
			/*Request channel to do JOIN */
			bowSetBowTableState(prAdapter, prBowFsmInfo->aucPeerAddress,
					    BOW_DEVICE_STATE_ACQUIRING_CHANNEL);
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
						  FALSE,
						  wlanbowCmdEventLinkDisconnected,
						  wlanbowCmdTimeoutHandler, 0, NULL, 0);
#endif
		}
	}
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
VOID bowResponderCancelScan(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsChannelExtention)
{

	P_MSG_SCN_SCAN_CANCEL prScanCancel = (P_MSG_SCN_SCAN_CANCEL) NULL;
	P_BOW_FSM_INFO_T prBowFsmInfo = (P_BOW_FSM_INFO_T) NULL;

	DEBUGFUNC("bowResponderCancelScan()");

	do {
		ASSERT(prAdapter);

		prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

		if (TRUE) {
			DBGLOG(BOW, EVENT, "BOW SCAN [CANCEL:%d]\n", prBowFsmInfo->ucSeqNumOfScanReq);

			/* There is a channel privilege on hand. */

			DBGLOG(BOW, TRACE, "BOW Cancel Scan\n");

			prScanCancel =
			    (P_MSG_SCN_SCAN_CANCEL) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_SCN_SCAN_CANCEL));
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
			mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prScanCancel, MSG_SEND_METHOD_BUF);

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
VOID bowResponderJoin(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc)
{
	P_BOW_FSM_INFO_T prBowFsmInfo;
	P_BSS_INFO_T prBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_STA_RECORD_T prStaRec;
	P_MSG_JOIN_REQ_T prJoinReqMsg;

	ASSERT(prBssDesc);
	ASSERT(prAdapter);

	DBGLOG(BOW, EVENT, "Starting bowResponderJoin.\n");

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* 4 <1> We are going to connect to this BSS. */
	prBssDesc->fgIsConnecting = TRUE;
	bowSetBowTableState(prAdapter, prBowFsmInfo->aucPeerAddress, BOW_DEVICE_STATE_CONNECTING);

	/* 4 <2> Setup corresponding STA_RECORD_T */
	/*Support First JOIN and retry */
	prStaRec = bssCreateStaRecFromBssDesc(prAdapter, STA_TYPE_BOW_AP, prBssInfo->ucBssIndex, prBssDesc);
	if (prStaRec == NULL)
		return;
	prBowFsmInfo->prTargetStaRec = prStaRec;

	/* 4 <3> Update ucAvailableAuthTypes which we can choice during SAA */
	prStaRec->fgIsReAssoc = FALSE;
	prBowFsmInfo->ucAvailableAuthTypes = (UINT_8) AUTH_TYPE_OPEN_SYSTEM;
	prStaRec->ucTxAuthAssocRetryLimit = TX_AUTH_ASSOCI_RETRY_LIMIT;

	/* 4 <4> Use an appropriate Authentication Algorithm Number among the ucAvailableAuthTypes */
	if (prBowFsmInfo->ucAvailableAuthTypes & (UINT_8) AUTH_TYPE_OPEN_SYSTEM) {

		DBGLOG(BOW, LOUD, "JOIN INIT: Try to do Authentication with AuthType == OPEN_SYSTEM.\n");
		prBowFsmInfo->ucAvailableAuthTypes &= ~(UINT_8) AUTH_TYPE_OPEN_SYSTEM;

		prStaRec->ucAuthAlgNum = (UINT_8) AUTH_ALGORITHM_NUM_OPEN_SYSTEM;
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
	prJoinReqMsg = (P_MSG_JOIN_REQ_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_JOIN_REQ_T));
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

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prJoinReqMsg, MSG_SEND_METHOD_BUF);
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
VOID bowFsmRunEventJoinComplete(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
#if 1				/* Marked for MT6630 */

	P_MSG_JOIN_COMP_T prJoinCompMsg;
	P_BOW_FSM_INFO_T prBowFsmInfo;
	P_STA_RECORD_T prStaRec;
	P_SW_RFB_T prAssocRspSwRfb;
	P_WLAN_ASSOC_RSP_FRAME_T prAssocRspFrame = (P_WLAN_ASSOC_RSP_FRAME_T) NULL;
	UINT_16 u2IELength;
	PUINT_8 pucIE;
	P_BSS_INFO_T prBowBssInfo;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBowBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);
	prJoinCompMsg = (P_MSG_JOIN_COMP_T) prMsgHdr;
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
			prAssocRspFrame = (P_WLAN_ASSOC_RSP_FRAME_T) prAssocRspSwRfb->pvHeader;

			u2IELength =
			    (UINT_16) ((prAssocRspSwRfb->u2PacketLen -
					prAssocRspSwRfb->u2HeaderLen) -
				       (OFFSET_OF(WLAN_ASSOC_RSP_FRAME_T, aucInfoElem[0]) - WLAN_MAC_MGMT_HEADER_LEN));
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
				UINT_8 ucHighestRateIndex;

				if (rateGetHighestRateIndexFromRateSet
				    (prBowBssInfo->u2OperationalRateSet, &ucHighestRateIndex)) {
					prStaRec->u2DesiredNonHTRateSet = BIT(ucHighestRateIndex);
				}
			}
#endif

			/* 4 <1.1> Change FW's Media State immediately. */
			bowChangeMediaState(prBowBssInfo, PARAM_MEDIA_STATE_CONNECTED);

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
VOID
bowIndicationOfMediaStateToHost(IN P_ADAPTER_T prAdapter,
				IN ENUM_PARAM_MEDIA_STATE_T eConnectionState, IN BOOLEAN fgDelayIndication)
{
	EVENT_CONNECTION_STATUS rEventConnStatus;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prBssInfo;
	P_BOW_FSM_INFO_T prBowFsmInfo;

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

	/* NOTE(Kevin): Move following line to bowChangeMediaState() macro per CM's request. */
	/* prBowBssInfo->eConnectionState = eConnectionState; */

	/* For indicating the Disconnect Event only if current media state is
	 * disconnected and we didn't do indication yet.
	 */
	if (prBssInfo->eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED) {
		if (prBssInfo->eConnectionStateIndicated == eConnectionState)
			return;
	}

	if (!fgDelayIndication) {
		/* 4 <0> Cancel Delay Timer */
		cnmTimerStopTimer(prAdapter, &prBowFsmInfo->rIndicationOfDisconnectTimer);

		/* 4 <1> Fill EVENT_CONNECTION_STATUS */
		rEventConnStatus.ucMediaStatus = (UINT_8) eConnectionState;

		if (eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
			rEventConnStatus.ucReasonOfDisconnect = DISCONNECT_REASON_CODE_RESERVED;

			if (prBssInfo->eCurrentOPMode == OP_MODE_BOW) {
				rEventConnStatus.ucInfraMode = (UINT_8) NET_TYPE_INFRA;
				rEventConnStatus.u2AID = prBssInfo->u2AssocId;
				rEventConnStatus.u2ATIMWindow = 0;
			} else if (prBssInfo->eCurrentOPMode == OP_MODE_IBSS) {
				rEventConnStatus.ucInfraMode = (UINT_8) NET_TYPE_IBSS;
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
				rEventConnStatus.ucNetworkType = (UINT_8) PARAM_NETWORK_TYPE_DS;
				break;

			case PHY_TYPE_ERP_INDEX:
				rEventConnStatus.ucNetworkType = (UINT_8) PARAM_NETWORK_TYPE_OFDM24;
				break;

			case PHY_TYPE_OFDM_INDEX:
				rEventConnStatus.ucNetworkType = (UINT_8) PARAM_NETWORK_TYPE_OFDM5;
				break;

			default:
				ASSERT(0);
				rEventConnStatus.ucNetworkType = (UINT_8) PARAM_NETWORK_TYPE_DS;
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
		ASSERT(eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED);

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
VOID bowRunEventAAATxFail(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
#if 1				/* Marked for MT6630 */
	P_BSS_INFO_T prBssInfo;
	P_BOW_FSM_INFO_T prBowFsmInfo;

	ASSERT(prAdapter);
	ASSERT(prStaRec);

	DBGLOG(BOW, EVENT, "bowRunEventAAATxFail , bssRemoveStaRecFromClientList.\n");
	DBGLOG(BOW, EVENT, "BoW AAA TxFail, target state %d\n", prStaRec->ucStaState + 1);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);
	bssRemoveClient(prAdapter, prBssInfo, prStaRec);
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
WLAN_STATUS bowRunEventAAAComplete(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
#if 1				/* Marked for MT6630 */
	P_BSS_INFO_T prBssInfo;
	P_BOW_FSM_INFO_T prBowFsmInfo;

	ASSERT(prStaRec);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

	DBGLOG(BOW, STATE, "bowRunEventAAAComplete, cnmStaRecChangeState, STA_STATE_3.\n");
	DBGLOG(BOW, EVENT, "BoW AAA complete [" MACSTR "]\n", MAC2STR(prStaRec->aucMacAddr));

	/*Update BssInfo to connected */
	bowChangeMediaState(prBssInfo, PARAM_MEDIA_STATE_CONNECTED);
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

WLAN_STATUS bowRunEventRxDeAuth(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prSwRfb)
{
#if 1				/* Marked for MT6630 */
	P_BSS_INFO_T prBowBssInfo;
	P_BOW_FSM_INFO_T prBowFsmInfo;
	ENUM_BOW_DEVICE_STATE eFsmState;

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

		if (STA_STATE_3 == prStaRec->ucStaState) {
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
VOID bowDisconnectLink(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	P_BOW_FSM_INFO_T prBowFsmInfo;
	P_STA_RECORD_T prStaRec;

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
		if (kalIsConfigurationExist(prAdapter->prGlueInfo) == TRUE) {
			wlanLoadManufactureData(prAdapter, kalGetConfiguration(prAdapter->prGlueInfo));
		} else {
			DBGLOG(REQ, WARN,
				"%s: load manufacture data fail\n", __func__);
		}
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
BOOLEAN bowValidateAssocReq(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, OUT PUINT_16 pu2StatusCode)
{
#if 1				/* Marked for MT6630 */

	BOOLEAN fgReplyAssocResp = FALSE;
	P_BSS_INFO_T prBowBssInfo;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;
	P_BOW_FSM_INFO_T prBowFsmInfo;
	P_WLAN_ASSOC_REQ_FRAME_T prAssocReqFrame = (P_WLAN_ASSOC_REQ_FRAME_T) NULL;
	OS_SYSTIME rCurrentTime;
	static OS_SYSTIME rLastRejectAssocTime;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBowBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

	prAssocReqFrame = (P_WLAN_ASSOC_REQ_FRAME_T) prSwRfb->pvHeader;
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
		if (prStaRec == NULL)
			break;
		prStaRec->eStaType = STA_TYPE_BOW_CLIENT;
		prStaRec->u2DesiredNonHTRateSet &= prBowBssInfo->u2OperationalRateSet;
		prStaRec->ucDesiredPhyTypeSet = prStaRec->ucPhyTypeSet & prBowBssInfo->ucPhyTypeSet;

#if CFG_BOW_RATE_LIMITATION
		/*Limit Rate Set to 24M,  48M, 54M */
		prStaRec->u2DesiredNonHTRateSet &= (RATE_SET_BIT_24M | RATE_SET_BIT_48M | RATE_SET_BIT_54M);
		/*If peer cannot support the above rate set, fix on the available highest rate */
		if (prStaRec->u2DesiredNonHTRateSet == 0) {
			UINT_8 ucHighestRateIndex;

			if (rateGetHighestRateIndexFromRateSet(prBowBssInfo->u2OperationalRateSet, &ucHighestRateIndex))
				prStaRec->u2DesiredNonHTRateSet = BIT(ucHighestRateIndex);
			else {
				/*If no available rate is found, DECLINE the association */
				*pu2StatusCode = STATUS_CODE_ASSOC_DENIED_RATE_NOT_SUPPORTED;
				break;
			}
		}
#endif

		/*Undpate BssInfo to FW */
		bowChangeMediaState(prBowBssInfo, PARAM_MEDIA_STATE_CONNECTED);
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
BOOLEAN
bowValidateAuth(IN P_ADAPTER_T prAdapter,
		IN P_SW_RFB_T prSwRfb, IN PP_STA_RECORD_T pprStaRec, OUT PUINT_16 pu2StatusCode)
{
#if 1				/* Marked for MT6630 */
	BOOLEAN fgReplyAuth = FALSE;
	P_BSS_INFO_T prBowBssInfo;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;
	P_BOW_FSM_INFO_T prBowFsmInfo;
	P_WLAN_AUTH_FRAME_T prAuthFrame = (P_WLAN_AUTH_FRAME_T) NULL;
	OS_SYSTIME rCurrentTime;
	static OS_SYSTIME rLastRejectAuthTime;

	/* TODO(Kevin): Call BoW functions to check ..
	   1. Check we are BoW now.
	   2. Check we can accept connection from thsi peer
	   3. Check Black List here.
	 */

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBowBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);

	prAuthFrame = (P_WLAN_AUTH_FRAME_T) prSwRfb->pvHeader;

	DBGLOG(BOW, EVENT, "bowValidateAuth, prBowFsmInfo->aucPeerAddress, %x:%x:%x:%x:%x:%x.\n",
			    prBowFsmInfo->aucPeerAddress[0],
			    prBowFsmInfo->aucPeerAddress[1],
			    prBowFsmInfo->aucPeerAddress[2],
			    prBowFsmInfo->aucPeerAddress[3],
			    prBowFsmInfo->aucPeerAddress[4], prBowFsmInfo->aucPeerAddress[5]);
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

		/* TODO(Kevin): Error handling of allocation of STA_RECORD_T for
		 * exhausted case and do removal of unused STA_RECORD_T.
		 */
		if (prStaRec == NULL)
			return fgReplyAuth;
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
VOID bowRunEventChGrant(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
#if 1				/* Marked for MT6630 */
	P_BSS_INFO_T prBowBssInfo;
	P_BOW_FSM_INFO_T prBowFsmInfo;
	P_MSG_CH_GRANT_T prMsgChGrant;
	UINT_8 ucTokenID;
	UINT_32 u4GrantInterval;
	ENUM_BOW_DEVICE_STATE eFsmState;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);
	prBowBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prBowFsmInfo->ucBssIndex);
	prMsgChGrant = (P_MSG_CH_GRANT_T) prMsgHdr;
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
VOID bowRequestCh(IN P_ADAPTER_T prAdapter)
{
	P_BOW_FSM_INFO_T prBowFsmInfo;
	P_MSG_CH_REQ_T prMsgChReq;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	if (prBowFsmInfo->fgIsChannelGranted == FALSE) {

		DBGLOG(BOW, EVENT, "BoW channel [REQUEST:%d], %d, %d.\n",
				    prBowFsmInfo->ucSeqNumOfChReq + 1,
				    prBowFsmInfo->ucPrimaryChannel, prBowFsmInfo->eBand);

		prMsgChReq = (P_MSG_CH_REQ_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_CH_REQ_T));

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

		/* To do: check if 80/160MHz bandwidth is needed here */
		prMsgChReq->eRfChannelWidth = 0;
		prMsgChReq->ucRfCenterFreqSeg1 = 0;
		prMsgChReq->ucRfCenterFreqSeg2 = 0;

		prBowFsmInfo->fgIsChannelRequested = TRUE;

		mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChReq, MSG_SEND_METHOD_BUF);
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
VOID bowReleaseCh(IN P_ADAPTER_T prAdapter)
{
	P_BOW_FSM_INFO_T prBowFsmInfo;
	P_MSG_CH_ABORT_T prMsgChAbort;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	if (prBowFsmInfo->fgIsChannelGranted != FALSE || prBowFsmInfo->fgIsChannelRequested != FALSE) {
		DBGLOG(BOW, EVENT,
		       "BoW channel [RELEASE:%d] %d, %d.\n", prBowFsmInfo->ucSeqNumOfChReq,
			prBowFsmInfo->ucPrimaryChannel, prBowFsmInfo->eBand);

		prBowFsmInfo->fgIsChannelRequested = FALSE;
		prBowFsmInfo->fgIsChannelGranted = FALSE;

		/* 1. return channel privilege to CNM immediately */
		prMsgChAbort = (P_MSG_CH_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_CH_ABORT_T));
		if (!prMsgChAbort) {
			ASSERT(0);	/* Can't release Channel to CNM */
			return;
		}

		prMsgChAbort->rMsgHdr.eMsgId = MID_MNY_CNM_CH_ABORT;
		prMsgChAbort->ucBssIndex = prBowFsmInfo->ucBssIndex;
		prMsgChAbort->ucTokenID = prBowFsmInfo->ucSeqNumOfChReq;

		mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChAbort, MSG_SEND_METHOD_BUF);
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
VOID bowChGrantedTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr)
{
	P_BOW_FSM_INFO_T prBowFsmInfo;
	ENUM_BOW_DEVICE_STATE eFsmState;

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

BOOLEAN bowNotifyAllLinkDisconnected(IN P_ADAPTER_T prAdapter)
{
#if 1				/* Marked for MT6630 */

	UINT_8 ucBowTableIdx = 0;
	CMD_INFO_T rCmdInfo;
	P_BOW_FSM_INFO_T prBowFsmInfo;

	ASSERT(prAdapter);

	prBowFsmInfo = &(prAdapter->rWifiVar.rBowFsmInfo);

	kalMemZero(&rCmdInfo, sizeof(CMD_INFO_T));

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

BOOLEAN bowCheckBowTableIfVaild(IN P_ADAPTER_T prAdapter, IN UINT_8 aucPeerAddress[6])
{
	UINT_8 idx;

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

BOOLEAN bowGetBowTableContent(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBowTableIdx, OUT P_BOW_TABLE_T prBowTable)
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

BOOLEAN bowSetBowTableContent(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBowTableIdx, IN P_BOW_TABLE_T prBowTable)
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

BOOLEAN
bowGetBowTableEntryByPeerAddress(IN P_ADAPTER_T prAdapter, IN UINT_8 aucPeerAddress[6], OUT PUINT_8 pucBowTableIdx)
{
	UINT_8 idx;

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

BOOLEAN bowGetBowTableFreeEntry(IN P_ADAPTER_T prAdapter, OUT PUINT_8 pucBowTableIdx)
{
	UINT_8 idx;

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

ENUM_BOW_DEVICE_STATE bowGetBowTableState(IN P_ADAPTER_T prAdapter, IN UINT_8 aucPeerAddress[6])
{
	UINT_8 idx;

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

BOOLEAN bowSetBowTableState(IN P_ADAPTER_T prAdapter, IN UINT_8 aucPeerAddress[6], IN ENUM_BOW_DEVICE_STATE eState)
{
	UINT_8 ucBowTableIdx;

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
