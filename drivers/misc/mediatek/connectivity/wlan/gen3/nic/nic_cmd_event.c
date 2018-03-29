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

/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/nic/nic_cmd_event.c#3
*/

/*! \file   nic_cmd_event.c
    \brief  Callback functions for Command packets.

	Various Event packet handlers which will be setup in the callback function of
    a command packet.
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

#ifdef FW_CFG_SUPPORT
#include "fwcfg.h"
#endif
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

VOID nicCmdEventQueryMcrRead(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_PARAM_CUSTOM_MCR_RW_STRUCT_T prMcrRdInfo;
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_ACCESS_REG prCmdAccessReg;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prCmdAccessReg = (P_CMD_ACCESS_REG) (pucEventBuf);

		u4QueryInfoLen = sizeof(PARAM_CUSTOM_MCR_RW_STRUCT_T);

		prMcrRdInfo = (P_PARAM_CUSTOM_MCR_RW_STRUCT_T) prCmdInfo->pvInformationBuffer;
		prMcrRdInfo->u4McrOffset = prCmdAccessReg->u4Address;
		prMcrRdInfo->u4McrData = prCmdAccessReg->u4Data;

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}

	return;

}

VOID nicCmdEventQuerySwCtrlRead(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_PARAM_CUSTOM_SW_CTRL_STRUCT_T prSwCtrlInfo;
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_SW_DBG_CTRL_T prCmdSwCtrl;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prCmdSwCtrl = (P_CMD_SW_DBG_CTRL_T) (pucEventBuf);

		u4QueryInfoLen = sizeof(PARAM_CUSTOM_SW_CTRL_STRUCT_T);

		prSwCtrlInfo = (P_PARAM_CUSTOM_SW_CTRL_STRUCT_T) prCmdInfo->pvInformationBuffer;
		prSwCtrlInfo->u4Id = prCmdSwCtrl->u4Id;
		prSwCtrlInfo->u4Data = prCmdSwCtrl->u4Data;

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}

	return;

}

VOID nicCmdEventQueryChipConfig(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T prChipConfigInfo;
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_CHIP_CONFIG_T prCmdChipConfig;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prCmdChipConfig = (P_CMD_CHIP_CONFIG_T) (pucEventBuf);

		u4QueryInfoLen = sizeof(PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T);

		if (prCmdInfo->u4InformationBufferLength < sizeof(PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T)) {
			DBGLOG(REQ, INFO,
			       "Chip config u4InformationBufferLength %u is not valid (event)\n",
				prCmdInfo->u4InformationBufferLength);
		}
		prChipConfigInfo = (P_PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T) prCmdInfo->pvInformationBuffer;
		prChipConfigInfo->ucRespType = prCmdChipConfig->ucRespType;
		prChipConfigInfo->u2MsgSize = prCmdChipConfig->u2MsgSize;
		DBGLOG(REQ, INFO, "%s: RespTyep  %u\n", __func__, prChipConfigInfo->ucRespType);
		DBGLOG(REQ, INFO, "%s: u2MsgSize %u\n", __func__, prChipConfigInfo->u2MsgSize);

		if (prChipConfigInfo->u2MsgSize > CHIP_CONFIG_RESP_SIZE) {
			DBGLOG(REQ, INFO,
			       "Chip config Msg Size %u is not valid (event)\n", prChipConfigInfo->u2MsgSize);
			prChipConfigInfo->u2MsgSize = CHIP_CONFIG_RESP_SIZE;
		}
		kalMemCopy(prChipConfigInfo->aucCmd, prCmdChipConfig->aucCmd, prChipConfigInfo->u2MsgSize);
		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}

	return;

}

VOID nicCmdEventSetCommon(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	if (prCmdInfo->fgIsOid) {
		/* Update Set Information Length */
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery, prCmdInfo->u4InformationBufferLength, WLAN_STATUS_SUCCESS);
	}

}

VOID nicCmdEventSetDisassociate(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	if (prCmdInfo->fgIsOid) {
		/* Update Set Information Length */
		kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
	}

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);

#if !defined(LINUX)
	prAdapter->fgIsRadioOff = TRUE;
#endif

}

VOID nicCmdEventSetIpAddress(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4Count;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	u4Count = (prCmdInfo->u4SetInfoLen - OFFSET_OF(CMD_SET_NETWORK_ADDRESS_LIST, arNetAddress))
	    / sizeof(IPV4_NETWORK_ADDRESS);

	if (prCmdInfo->fgIsOid) {
		/* Update Set Information Length */
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery,
			       OFFSET_OF(PARAM_NETWORK_ADDRESS_LIST, arAddress) + u4Count *
			       (OFFSET_OF(PARAM_NETWORK_ADDRESS, aucAddress) +
				sizeof(PARAM_NETWORK_ADDRESS_IP)), WLAN_STATUS_SUCCESS);
	}

}

VOID nicCmdEventQueryRfTestATInfo(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_TEST_STATUS prTestStatus, prQueryBuffer;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prTestStatus = (P_EVENT_TEST_STATUS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prQueryBuffer = (P_EVENT_TEST_STATUS) prCmdInfo->pvInformationBuffer;

		/*Memory copy length is depended on upper-layer*/
		kalMemCopy(prQueryBuffer, prTestStatus, prCmdInfo->u4InformationBufferLength);

		u4QueryInfoLen = sizeof(EVENT_TEST_STATUS);

		/* Update Query Information Length */
		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}

}

VOID nicCmdEventQueryLinkQuality(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	PARAM_RSSI rRssi, *prRssi;
	P_EVENT_LINK_QUALITY prLinkQuality;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prLinkQuality = (P_EVENT_LINK_QUALITY) pucEventBuf;

	rRssi = (PARAM_RSSI) prLinkQuality->cRssi;	/* ranged from (-128 ~ 30) in unit of dBm */

	if (prAdapter->prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
		if (rRssi > PARAM_WHQL_RSSI_MAX_DBM)
			rRssi = PARAM_WHQL_RSSI_MAX_DBM;
		else if (rRssi < PARAM_WHQL_RSSI_MIN_DBM)
			rRssi = PARAM_WHQL_RSSI_MIN_DBM;
	} else {
		rRssi = PARAM_WHQL_RSSI_MIN_DBM;
	}

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prRssi = (PARAM_RSSI *) prCmdInfo->pvInformationBuffer;

		kalMemCopy(prRssi, &rRssi, sizeof(PARAM_RSSI));
		u4QueryInfoLen = sizeof(PARAM_RSSI);

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This routine is in response of OID_GEN_LINK_SPEED query request
*
* @param prAdapter      Pointer to the Adapter structure.
* @param prCmdInfo      Pointer to the pending command info
* @param pucEventBuf
*
* @retval none
*/
/*----------------------------------------------------------------------------*/
VOID nicCmdEventQueryLinkSpeed(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_LINK_QUALITY prLinkQuality;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4LinkSpeed;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prLinkQuality = (P_EVENT_LINK_QUALITY) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		pu4LinkSpeed = (PUINT_32) (prCmdInfo->pvInformationBuffer);

		*pu4LinkSpeed = prLinkQuality->u2LinkSpeed * 5000;

		u4QueryInfoLen = sizeof(UINT_32);

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}

VOID nicCmdEventQueryStatistics(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_PARAM_802_11_STATISTICS_STRUCT_T prStatistics;
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		u4QueryInfoLen = sizeof(PARAM_802_11_STATISTICS_STRUCT_T);
		prStatistics = (P_PARAM_802_11_STATISTICS_STRUCT_T) prCmdInfo->pvInformationBuffer;

		prStatistics->rTransmittedFragmentCount = prEventStatistics->rTransmittedFragmentCount;
		prStatistics->rFailedCount = prEventStatistics->rFailedCount;
		prStatistics->rRetryCount = prEventStatistics->rRetryCount;
		prStatistics->rMultipleRetryCount = prEventStatistics->rMultipleRetryCount;
		prStatistics->rACKFailureCount = prEventStatistics->rACKFailureCount;
		prStatistics->rReceivedFragmentCount = prEventStatistics->rReceivedFragmentCount;
		prStatistics->rFCSErrorCount = prEventStatistics->rFCSErrorCount;

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}

VOID nicCmdEventQueryBugReport(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
#define BUG_REPORT_VERSION 1

	P_EVENT_BUG_REPORT_T prStatistics;
	P_EVENT_BUG_REPORT_T prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_BUG_REPORT_T) pucEventBuf;

	if (prCmdInfo->fgIsOid && (prEventStatistics->u4BugReportVersion == BUG_REPORT_VERSION)) {
		prGlueInfo = prAdapter->prGlueInfo;

		u4QueryInfoLen = sizeof(EVENT_BUG_REPORT_T);
		prStatistics = (P_EVENT_BUG_REPORT_T) prCmdInfo->pvInformationBuffer;

		kalMemCopy(prStatistics, prEventStatistics, u4QueryInfoLen);

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}

VOID nicCmdEventEnterRfTest(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
#define WAIT_FW_READY_RETRY_CNT 200

	UINT_32 u4WHISR = 0, u4Value = 0;
	UINT_16 au2TxCount[16];
	UINT_16 u2RetryCnt = 0;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	/* [driver-land] */
	/* prAdapter->fgTestMode = TRUE; */
	if (prAdapter->fgTestMode)
		prAdapter->fgTestMode = FALSE;
	else
		prAdapter->fgTestMode = TRUE;

	/* 0. always indicate disconnection */
	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_DISCONNECTED)
		kalIndicateStatusAndComplete(prAdapter->prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
	/* 1. Remove pending TX */
	nicTxRelease(prAdapter, TRUE);

	/* 1.1 clear pending Security / Management Frames */
	kalClearSecurityFrames(prAdapter->prGlueInfo);
	kalClearMgmtFrames(prAdapter->prGlueInfo);

	/* 1.2 clear pending TX packet queued in glue layer */
	kalFlushPendingTxPackets(prAdapter->prGlueInfo);

	/* 2. Reset driver-domain FSMs */
	nicUninitMGMT(prAdapter);

	nicResetSystemService(prAdapter);
	nicInitMGMT(prAdapter, NULL);

	/* 3. Disable Interrupt */
	HAL_INTR_DISABLE(prAdapter);

	/* 4. Block til firmware completed entering into RF test mode */
	kalMsleep(500);
	while (1) {
		HAL_MCR_RD(prAdapter, MCR_WCIR, &u4Value);

		if (u4Value & WCIR_WLAN_READY) {
			break;
		} else if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE ||
			kalIsResetting() || u2RetryCnt >= WAIT_FW_READY_RETRY_CNT) {
			if (prCmdInfo->fgIsOid) {
				/* Update Set Information Length */
				kalOidComplete(prAdapter->prGlueInfo,
					       prCmdInfo->fgSetQuery,
					       prCmdInfo->u4SetInfoLen, WLAN_STATUS_NOT_SUPPORTED);

			}
			return;
		}
		kalMsleep(10);
		u2RetryCnt++;
	}

	/* 5. Clear Interrupt Status */
	HAL_READ_INTR_STATUS(prAdapter, 4, (PUINT_8)&u4WHISR);
	if (HAL_IS_TX_DONE_INTR(u4WHISR))
		HAL_READ_TX_RELEASED_COUNT(prAdapter, au2TxCount);
	/* 6. Reset TX Counter */
	nicTxResetResource(prAdapter);

	/* 7. Re-enable Interrupt */
	HAL_INTR_ENABLE(prAdapter);

	/* 8. completion indication */
	if (prCmdInfo->fgIsOid) {
		/* Update Set Information Length */
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery, prCmdInfo->u4SetInfoLen, WLAN_STATUS_SUCCESS);
	}
#if CFG_SUPPORT_NVRAM
	/* 9. load manufacture data */
	if (kalIsConfigurationExist(prAdapter->prGlueInfo) == TRUE)
		wlanLoadManufactureData(prAdapter, kalGetConfiguration(prAdapter->prGlueInfo));
	else
		DBGLOG(REQ, WARN, "%s: load manufacture data fail\n", __func__);
#endif

}

VOID nicCmdEventLeaveRfTest(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
#define WAIT_FW_READY_RETRY_CNT 200

	UINT_32 u4WHISR = 0, u4Value = 0;
	UINT_16 au2TxCount[16];
	UINT_16 u2RetryCnt = 0;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	/* 1. Disable Interrupt */
	HAL_INTR_DISABLE(prAdapter);

	/* 2. Block til firmware completed leaving from RF test mode */
	kalMsleep(500);
	while (1) {
		HAL_MCR_RD(prAdapter, MCR_WCIR, &u4Value);

		if (u4Value & WCIR_WLAN_READY) {
			break;
		} else if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE ||
				kalIsResetting() || u2RetryCnt >= WAIT_FW_READY_RETRY_CNT) {
			if (prCmdInfo->fgIsOid) {
				/* Update Set Information Length */
				kalOidComplete(prAdapter->prGlueInfo,
					       prCmdInfo->fgSetQuery,
					       prCmdInfo->u4SetInfoLen, WLAN_STATUS_NOT_SUPPORTED);

			}
			return;
		}
		u2RetryCnt++;
		kalMsleep(10);
	}

	/* 3. Clear Interrupt Status */
	HAL_READ_INTR_STATUS(prAdapter, 4, (PUINT_8)&u4WHISR);
	if (HAL_IS_TX_DONE_INTR(u4WHISR))
		HAL_READ_TX_RELEASED_COUNT(prAdapter, au2TxCount);
	/* 4. Reset TX Counter */
	nicTxResetResource(prAdapter);

	/* 5. Re-enable Interrupt */
	HAL_INTR_ENABLE(prAdapter);

	/* 6. set driver-land variable */
	prAdapter->fgTestMode = FALSE;
	prAdapter->fgIcapMode = FALSE;

	/* 7. completion indication */
	if (prCmdInfo->fgIsOid) {
		/* Update Set Information Length */
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery, prCmdInfo->u4SetInfoLen, WLAN_STATUS_SUCCESS);
	}

	/* 8. Indicate as disconnected */
	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_DISCONNECTED) {

		kalIndicateStatusAndComplete(prAdapter->prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);

		prAdapter->rWlanInfo.u4SysTime = kalGetTimeTick();
	}
#if CFG_SUPPORT_NVRAM
	/* 9. load manufacture data */
	if (kalIsConfigurationExist(prAdapter->prGlueInfo) == TRUE)
		wlanLoadManufactureData(prAdapter, kalGetConfiguration(prAdapter->prGlueInfo));
	else
		DBGLOG(REQ, WARN, "%s: load manufacture data fail\n", __func__);
#endif

	/* 10. Override network address */
	wlanUpdateNetworkAddress(prAdapter);

}

VOID nicCmdEventQueryMcastAddr(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_GLUE_INFO_T prGlueInfo;
	P_EVENT_MAC_MCAST_ADDR prEventMacMcastAddr;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEventMacMcastAddr = (P_EVENT_MAC_MCAST_ADDR) (pucEventBuf);

		u4QueryInfoLen = prEventMacMcastAddr->u4NumOfGroupAddr * MAC_ADDR_LEN;

		/* buffer length check */
		if (prCmdInfo->u4InformationBufferLength < u4QueryInfoLen) {
			kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_BUFFER_TOO_SHORT);
		} else {
			kalMemCopy(prCmdInfo->pvInformationBuffer,
				   prEventMacMcastAddr->arAddress,
				   prEventMacMcastAddr->u4NumOfGroupAddr * MAC_ADDR_LEN);

			kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
		}
	}
}

VOID nicCmdEventQueryEepromRead(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_PARAM_CUSTOM_EEPROM_RW_STRUCT_T prEepromRdInfo;
	P_GLUE_INFO_T prGlueInfo;
	P_EVENT_ACCESS_EEPROM prEventAccessEeprom;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEventAccessEeprom = (P_EVENT_ACCESS_EEPROM) (pucEventBuf);

		u4QueryInfoLen = sizeof(PARAM_CUSTOM_EEPROM_RW_STRUCT_T);

		prEepromRdInfo = (P_PARAM_CUSTOM_EEPROM_RW_STRUCT_T) prCmdInfo->pvInformationBuffer;
		prEepromRdInfo->ucEepromIndex = (UINT_8) (prEventAccessEeprom->u2Offset);
		prEepromRdInfo->u2EepromData = prEventAccessEeprom->u2Data;

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}

	return;

}

VOID nicCmdEventSetMediaStreamMode(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	PARAM_MEDIA_STREAMING_INDICATION rParamMediaStreamIndication;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	if (prCmdInfo->fgIsOid) {
		/* Update Set Information Length */
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery, prCmdInfo->u4SetInfoLen, WLAN_STATUS_SUCCESS);
	}

	rParamMediaStreamIndication.rStatus.eStatusType = ENUM_STATUS_TYPE_MEDIA_STREAM_MODE;
	rParamMediaStreamIndication.eMediaStreamMode =
	    prAdapter->rWlanInfo.eLinkAttr.ucMediaStreamMode == 0 ? ENUM_MEDIA_STREAM_OFF : ENUM_MEDIA_STREAM_ON;

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
				     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
				     (PVOID)&rParamMediaStreamIndication, sizeof(PARAM_MEDIA_STREAMING_INDICATION));
}

VOID nicCmdEventSetStopSchedScan(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	/*
	   DBGLOG(SCN, INFO, "--->nicCmdEventSetStopSchedScan\n" ));
	 */
	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	/*
	   DBGLOG(SCN, INFO, "<--kalSchedScanStopped\n" );
	 */
	if (prCmdInfo->fgIsOid) {
		/* Update Set Information Length */
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery, prCmdInfo->u4InformationBufferLength, WLAN_STATUS_SUCCESS);
	}

	DBGLOG(SCN, INFO, "nicCmdEventSetStopSchedScan OID done, release lock and send event to uplayer\n");
	/*Due to dead lock issue, need to release the IO control before calling kernel APIs */
	kalSchedScanStopped(prAdapter->prGlueInfo, !prCmdInfo->fgIsOid);

}

/* Statistics responder */
VOID nicCmdEventQueryXmitOk(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = (UINT_32) prEventStatistics->rTransmittedFragmentCount.QuadPart;
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = prEventStatistics->rTransmittedFragmentCount.QuadPart;
		}

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}

VOID nicCmdEventQueryRecvOk(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = (UINT_32) prEventStatistics->rReceivedFragmentCount.QuadPart;
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = prEventStatistics->rReceivedFragmentCount.QuadPart;
		}

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}

VOID nicCmdEventQueryXmitError(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = (UINT_32) prEventStatistics->rFailedCount.QuadPart;
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = (UINT_64) prEventStatistics->rFailedCount.QuadPart;
		}

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}

VOID nicCmdEventQueryRecvError(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = (UINT_32) prEventStatistics->rFCSErrorCount.QuadPart;
			/* @FIXME, RX_ERROR_DROP_COUNT/RX_FIFO_FULL_DROP_COUNT is not calculated */
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = prEventStatistics->rFCSErrorCount.QuadPart;
			/* @FIXME, RX_ERROR_DROP_COUNT/RX_FIFO_FULL_DROP_COUNT is not calculated */
		}

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}

VOID nicCmdEventQueryRecvNoBuffer(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = 0;	/* @FIXME? */
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = 0;	/* @FIXME? */
		}

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}

VOID nicCmdEventQueryRecvCrcError(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = (UINT_32) prEventStatistics->rFCSErrorCount.QuadPart;
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = prEventStatistics->rFCSErrorCount.QuadPart;
		}

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}

VOID nicCmdEventQueryRecvErrorAlignment(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = (UINT_32) 0;	/* @FIXME */
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = 0;	/* @FIXME */
		}

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}

VOID nicCmdEventQueryXmitOneCollision(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data =
			    (UINT_32) (prEventStatistics->rMultipleRetryCount.QuadPart -
				       prEventStatistics->rRetryCount.QuadPart);
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data =
			    (UINT_64) (prEventStatistics->rMultipleRetryCount.QuadPart -
				       prEventStatistics->rRetryCount.QuadPart);
		}

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}

VOID nicCmdEventQueryXmitMoreCollisions(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = (UINT_32) prEventStatistics->rMultipleRetryCount.QuadPart;
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = (UINT_64) prEventStatistics->rMultipleRetryCount.QuadPart;
		}

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}

VOID nicCmdEventQueryXmitMaxCollisions(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = (UINT_32) prEventStatistics->rFailedCount.QuadPart;
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = (UINT_64) prEventStatistics->rFailedCount.QuadPart;
		}

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when command by OID/ioctl has been timeout
*
* @param prAdapter          Pointer to the Adapter structure.
* @param prCmdInfo          Pointer to the command information
*
* @return TRUE
*         FALSE
*/
/*----------------------------------------------------------------------------*/
VOID nicOidCmdTimeoutCommon(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
	ASSERT(prAdapter);

	if (prCmdInfo->fgIsOid)
		kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_FAILURE);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when command for entering RF test has
*        failed sending due to timeout (highly possibly by firmware crash)
*
* @param prAdapter          Pointer to the Adapter structure.
* @param prCmdInfo          Pointer to the command information
*
* @return none
*
*/
/*----------------------------------------------------------------------------*/
VOID nicOidCmdEnterRFTestTimeout(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
	ASSERT(prAdapter);

	/* 1. Remove pending TX frames */
	nicTxRelease(prAdapter, TRUE);

	/* 1.1 clear pending Security / Management Frames */
	kalClearSecurityFrames(prAdapter->prGlueInfo);
	kalClearMgmtFrames(prAdapter->prGlueInfo);

	/* 1.2 clear pending TX packet queued in glue layer */
	kalFlushPendingTxPackets(prAdapter->prGlueInfo);

	/* 2. indicate for OID failure */
	kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_FAILURE);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to handle dump burst event
*
* @param prAdapter          Pointer to the Adapter structure.
* @param prCmdInfo         Pointer to the command information
* @param pucEventBuf       Pointer to event buffer
*
* @return none
*
*/
/*----------------------------------------------------------------------------*/

VOID nicEventQueryMemDump(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf)
{
	static UINT_8 aucPath[256];
	static UINT_8 aucPath_done[300];
	static UINT_32 u4CurTimeTick;
	P_EVENT_DUMP_MEM_T prEventDumpMem;

	ASSERT(prAdapter);
	ASSERT(pucEventBuf);

	sprintf(aucPath, "/data/blbist/dump_%05d.hex", g_u2DumpIndex);

	prEventDumpMem = (P_EVENT_DUMP_MEM_T) (pucEventBuf);

	if (kalCheckPath(aucPath) == -1) {
		kalMemSet(aucPath, 0x00, 256);
		sprintf(aucPath, "/data/dump_%05d.hex", g_u2DumpIndex);
	}

	if (prEventDumpMem->ucFragNum == 1) {
		/* Store memory dump into sdcard,
		 * path /sdcard/dump_<current  system tick>_<memory address>_<memory length>.hex
		 */
		u4CurTimeTick = kalGetTimeTick();
#if defined(LINUX)

		/*if blbist mkdir undre /data/blbist, the dump files wouls put on it */
		sprintf(aucPath, "/data/blbist/dump_%05d.hex", g_u2DumpIndex);
		if (kalCheckPath(aucPath) == -1) {
			kalMemSet(aucPath, 0x00, 256);
			sprintf(aucPath, "/data/dump_%05d.hex", g_u2DumpIndex);
		}
#else
		kal_sprintf_ddk(aucPath, sizeof(aucPath),
				u4CurTimeTick,
				prEventDumpMem->u4Address, prEventDumpMem->u4Length + prEventDumpMem->u4RemainLength);
#endif
		kalWriteToFile(aucPath, FALSE, &prEventDumpMem->aucBuffer[0], prEventDumpMem->u4Length);
	} else {
		/* Append current memory dump to the hex file */
		kalWriteToFile(aucPath, TRUE, &prEventDumpMem->aucBuffer[0], prEventDumpMem->u4Length);
	}
	DBGLOG(INIT, INFO,
	       ": ==> (u4RemainLength = %x, u4Address=%x )\n", prEventDumpMem->u4RemainLength,
		prEventDumpMem->u4Address);

	if (prEventDumpMem->u4RemainLength == 0 || prEventDumpMem->u4Address == 0xFFFFFFFF) {

		/* The request is finished or firmware response a error */
		/* Reply time tick to iwpriv */

		g_bIcapEnable = FALSE;
		g_bCaptureDone = TRUE;

		sprintf(aucPath_done, "/data/blbist/file_dump_done.txt");
		if (kalCheckPath(aucPath_done) == -1) {
			kalMemSet(aucPath_done, 0x00, 256);
			sprintf(aucPath_done, "/data/file_dump_done.txt");
		}
		DBGLOG(INIT, INFO, ": ==> gen done_file\n");
		kalWriteToFile(aucPath_done, FALSE, aucPath_done, sizeof(aucPath_done));
		g_u2DumpIndex++;

	}

}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when command for memory dump has
*        replied a event.
*
* @param prAdapter          Pointer to the Adapter structure.
* @param prCmdInfo         Pointer to the command information
* @param pucEventBuf       Pointer to event buffer
*
* @return none
*
*/
/*----------------------------------------------------------------------------*/
VOID nicCmdEventQueryMemDump(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	static UINT_8 aucPath[256];
	static UINT_8 aucPath_done[300];
	static UINT_32 u4CurTimeTick;
	P_PARAM_CUSTOM_MEM_DUMP_STRUCT_T prMemDumpInfo;
	P_GLUE_INFO_T prGlueInfo;
	P_EVENT_DUMP_MEM_T prEventDumpMem;
	UINT_32 u4QueryInfoLen;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (1) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEventDumpMem = (P_EVENT_DUMP_MEM_T) (pucEventBuf);

		u4QueryInfoLen = sizeof(P_PARAM_CUSTOM_MEM_DUMP_STRUCT_T);

		prMemDumpInfo = (P_PARAM_CUSTOM_MEM_DUMP_STRUCT_T) prCmdInfo->pvInformationBuffer;
		prMemDumpInfo->u4Address = prEventDumpMem->u4Address;
		prMemDumpInfo->u4Length = prEventDumpMem->u4Length;
		prMemDumpInfo->u4RemainLength = prEventDumpMem->u4RemainLength;
		prMemDumpInfo->ucFragNum = prEventDumpMem->ucFragNum;

#if 0
		do {
			UINT_32 i = 0;

			DBGLOG(REQ, TRACE, "Rx dump address 0x%X, Length %d, FragNum %d, remain %d\n",
			       prEventDumpMem->u4Address,
			       prEventDumpMem->u4Length, prEventDumpMem->ucFragNum, prEventDumpMem->u4RemainLength);
#if 0
			for (i = 0; i < prEventDumpMem->u4Length; i++) {
				DBGLOG(REQ, TRACE, "%02X ", prEventDumpMem->aucBuffer[i]);
				if (i % 32 == 31)
					DBGLOG(REQ, TRACE, "\n");
			}
#endif
		} while (FALSE);
#endif

		if (prEventDumpMem->ucFragNum == 1) {
			/* Store memory dump into sdcard,
			 * path /sdcard/dump_<current  system tick>_<memory address>_<memory length>.hex
			 */
			u4CurTimeTick = kalGetTimeTick();
#if defined(LINUX)
#if 0
			sprintf(aucPath, "/sdcard/dump_%ld_0x%08lX_%ld.hex",
				u4CurTimeTick,
				prEventDumpMem->u4Address, prEventDumpMem->u4Length + prEventDumpMem->u4RemainLength);
#else

			/*if blbist mkdir undre /data/blbist, the dump files wouls put on it */
			sprintf(aucPath, "/data/blbist/dump_%05d.hex", g_u2DumpIndex);
			if (kalCheckPath(aucPath) == -1) {
				kalMemSet(aucPath, 0x00, 256);
				sprintf(aucPath, "/data/dump_%05d.hex", g_u2DumpIndex);
			}
#endif
#else
			kal_sprintf_ddk(aucPath, sizeof(aucPath),
					u4CurTimeTick,
					prEventDumpMem->u4Address,
					prEventDumpMem->u4Length + prEventDumpMem->u4RemainLength);
			/* strcpy(aucPath, "dump.hex"); */
#endif
			kalWriteToFile(aucPath, FALSE, &prEventDumpMem->aucBuffer[0], prEventDumpMem->u4Length);
		} else {
			/* Append current memory dump to the hex file */
			kalWriteToFile(aucPath, TRUE, &prEventDumpMem->aucBuffer[0], prEventDumpMem->u4Length);
		}

		if (prEventDumpMem->u4RemainLength == 0 || prEventDumpMem->u4Address == 0xFFFFFFFF) {
			/* The request is finished or firmware response a error */
			/* Reply time tick to iwpriv */
			if (prCmdInfo->fgIsOid) {

				/* the oid would be complete only in oid-trigger  mode,
				 * that is no need to if the event-trigger */
				if (g_bIcapEnable == FALSE) {
					*((PUINT_32) prCmdInfo->pvInformationBuffer) = u4CurTimeTick;
					kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery,
						       u4QueryInfoLen, WLAN_STATUS_SUCCESS);
				}
			}
			g_bIcapEnable = FALSE;
			g_bCaptureDone = TRUE;
#if defined(LINUX)
			sprintf(aucPath_done, "/data/blbist/file_dump_done.txt");
			if (kalCheckPath(aucPath_done) == -1) {
				kalMemSet(aucPath_done, 0x00, 256);
				sprintf(aucPath_done, "/data/file_dump_done.txt");
			}
			DBGLOG(INIT, INFO, ": ==> gen done_file\n");
			kalWriteToFile(aucPath_done, FALSE, aucPath_done, sizeof(aucPath_done));
			g_u2DumpIndex++;

#else
			kal_sprintf_done_ddk(aucPath_done, sizeof(aucPath_done));
			kalWriteToFile(aucPath_done, FALSE, aucPath_done, sizeof(aucPath_done));
#endif
		} else {
#if defined(LINUX)

#else /* 2013/05/26 fw would try to send the buffer successfully */
			/* The memory dump request is not finished, Send next command */
			wlanSendMemDumpCmd(prAdapter,
					   prCmdInfo->pvInformationBuffer, prCmdInfo->u4InformationBufferLength);
#endif
		}
	}

	return;

}

#if CFG_SUPPORT_BATCH_SCAN
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when event for SUPPORT_BATCH_SCAN
*
* @param prAdapter          Pointer to the Adapter structure.
* @param prCmdInfo          Pointer to the command information
* @param pucEventBuf        Pointer to the event buffer
*
* @return none
*
*/
/*----------------------------------------------------------------------------*/
VOID nicCmdEventBatchScanResult(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_EVENT_BATCH_RESULT_T prEventBatchResult;
	P_GLUE_INFO_T prGlueInfo;

	DBGLOG(SCN, TRACE, "nicCmdEventBatchScanResult");

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEventBatchResult = (P_EVENT_BATCH_RESULT_T) pucEventBuf;

		u4QueryInfoLen = sizeof(EVENT_BATCH_RESULT_T);
		kalMemCopy(prCmdInfo->pvInformationBuffer, prEventBatchResult, sizeof(EVENT_BATCH_RESULT_T));

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}

}
#endif

#if CFG_SUPPORT_BUILD_DATE_CODE
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when event for build date code information
*        has been retrieved
*
* @param prAdapter          Pointer to the Adapter structure.
* @param prCmdInfo          Pointer to the command information
* @param pucEventBuf        Pointer to the event buffer
*
* @return none
*
*/
/*----------------------------------------------------------------------------*/
VOID nicCmdEventBuildDateCode(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_EVENT_BUILD_DATE_CODE prEvent;
	P_GLUE_INFO_T prGlueInfo;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEvent = (P_EVENT_BUILD_DATE_CODE) pucEventBuf;

		u4QueryInfoLen = sizeof(UINT_8) * 16;
		kalMemCopy(prCmdInfo->pvInformationBuffer, prEvent->aucDateCode, sizeof(UINT_8) * 16);

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}

}
#endif

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when event for query STA link status
*        has been retrieved
*
* @param prAdapter          Pointer to the Adapter structure.
* @param prCmdInfo          Pointer to the command information
* @param pucEventBuf        Pointer to the event buffer
*
* @return none
*
*/
/*----------------------------------------------------------------------------*/
VOID nicCmdEventQueryStaStatistics(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_EVENT_STA_STATISTICS_T prEvent;
	P_GLUE_INFO_T prGlueInfo;
	P_PARAM_GET_STA_STATISTICS prStaStatistics;
	ENUM_WMM_ACI_T eAci;
	P_STA_RECORD_T prStaRec;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEvent = (P_EVENT_STA_STATISTICS_T) pucEventBuf;
		prStaStatistics = (P_PARAM_GET_STA_STATISTICS) prCmdInfo->pvInformationBuffer;

		u4QueryInfoLen = sizeof(PARAM_GET_STA_STA_STATISTICS);

		/* Statistics from FW is valid */
		if (prEvent->u4Flags & BIT(0)) {
			prStaStatistics->ucPer = prEvent->ucPer;
			prStaStatistics->ucRcpi = prEvent->ucRcpi;
			prStaStatistics->u4PhyMode = prEvent->u4PhyMode;
			prStaStatistics->u2LinkSpeed = prEvent->u2LinkSpeed;

			prStaStatistics->u4TxFailCount = prEvent->u4TxFailCount;
			prStaStatistics->u4TxLifeTimeoutCount = prEvent->u4TxLifeTimeoutCount;

			prStaRec = cnmGetStaRecByIndex(prAdapter, prEvent->ucStaRecIdx);

			if (prStaRec) {
				/*link layer statistics */
				for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
					prStaStatistics->arLinkStatistics[eAci].u4TxFailMsdu =
					    prEvent->arLinkStatistics[eAci].u4TxFailMsdu;
					prStaStatistics->arLinkStatistics[eAci].u4TxRetryMsdu =
					    prEvent->arLinkStatistics[eAci].u4TxRetryMsdu;

					/*for dump bss statistics */
					prStaRec->arLinkStatistics[eAci].u4TxFailMsdu =
					    prEvent->arLinkStatistics[eAci].u4TxFailMsdu;
					prStaRec->arLinkStatistics[eAci].u4TxRetryMsdu =
					    prEvent->arLinkStatistics[eAci].u4TxRetryMsdu;
				}
			}
			if (prEvent->u4TxCount) {
				UINT_32 u4TxDoneAirTimeMs = USEC_TO_MSEC(prEvent->u4TxDoneAirTime * 32);

				prStaStatistics->u4TxAverageAirTime = (u4TxDoneAirTimeMs / prEvent->u4TxCount);
			} else {
				prStaStatistics->u4TxAverageAirTime = 0;
			}
		}

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}

}

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when event for query LTE safe channels
*        has been retrieved
*
* @param prAdapter          Pointer to the Adapter structure.
* @param prCmdInfo          Pointer to the command information
* @param pucEventBuf        Pointer to the event buffer
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID nicCmdEventQueryLteSafeChn(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_EVENT_LTE_SAFE_CHN_T prEvent;
	P_GLUE_INFO_T prGlueInfo;
	P_PARAM_GET_CHN_INFO prLteSafeChnInfo;
	UINT_8 ucIdx = 0;

	if ((prAdapter == NULL)
		|| (prCmdInfo == NULL)
		|| (pucEventBuf == NULL)
		|| (prCmdInfo->pvInformationBuffer == NULL)) {
		ASSERT(FALSE);
		return;
	}

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEvent = (P_EVENT_LTE_SAFE_CHN_T) pucEventBuf;	/* FW responsed data */

		prLteSafeChnInfo = (P_PARAM_GET_CHN_INFO) prCmdInfo->pvInformationBuffer;

		u4QueryInfoLen = sizeof(PARAM_GET_CHN_INFO);

		/* Statistics from FW is valid */
		if (prEvent->u4Flags & BIT(0)) {
			for (ucIdx = 0; ucIdx < 5; ucIdx++) {
				prLteSafeChnInfo->rLteSafeChnList.au4SafeChannelBitmask[ucIdx] =
					prEvent->rLteSafeChn.au4SafeChannelBitmask[ucIdx];

				DBGLOG(P2P, INFO,
				       "[ACS]LTE safe channels[%d]=0x%08x\n", ucIdx,
				       prLteSafeChnInfo->rLteSafeChnList.au4SafeChannelBitmask[ucIdx]);
			}
		}
		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}
#endif

#ifdef FW_CFG_SUPPORT
VOID nicCmdEventQueryCfgRead(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	struct _CMD_HEADER_T *prInCfgHeader;
	P_GLUE_INFO_T prGlueInfo;
	struct _CMD_HEADER_T *prOutCfgHeader;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prInCfgHeader = (struct _CMD_HEADER_T *) pucEventBuf;
		u4QueryInfoLen = sizeof(struct _CMD_HEADER_T);
		prOutCfgHeader = (struct _CMD_HEADER_T *) prCmdInfo->pvInformationBuffer;
		kalMemCopy(prOutCfgHeader, prInCfgHeader, sizeof(struct _CMD_HEADER_T));

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}
#endif
