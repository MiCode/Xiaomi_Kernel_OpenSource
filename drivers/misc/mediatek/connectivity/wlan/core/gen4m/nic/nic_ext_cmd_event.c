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

/*! \file   nic_ext_cmd_event.c
 *    \brief  Callback functions for Command packets.
 *
 *	Various Event packet handlers which will be setup in the callback
 *  function of a command packet.
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */
#if (CFG_SUPPORT_CONNAC2X == 1)

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"
#include "gl_ate_agent.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                            F U N C T I O N   D A T A
 *******************************************************************************
 */
static uint32_t
wlanSendSetQueryExtCmd2WA(
	struct ADAPTER *prAdapter,
	uint8_t ucCID,
	uint8_t ucExtCID,
	u_int8_t fgSetQuery,
	u_int8_t fgNeedResp,
	u_int8_t fgIsOid,
	PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
	PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler,
	uint32_t u4SetQueryInfoLen,
	uint8_t *pucInfoBuffer,
	void *pvSetQueryBuffer,
	uint32_t u4SetQueryBufferLen)
{
	struct GLUE_INFO *prGlueInfo;
	struct CMD_INFO *prCmdInfo;
	uint8_t *pucCmdBuf;
	struct mt66xx_chip_info *prChipInfo;

	prGlueInfo = prAdapter->prGlueInfo;
	prChipInfo = prAdapter->chip_info;
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
		(prChipInfo->u2CmdTxHdrSize + u4SetQueryInfoLen));

	DEBUGFUNC("wlanSendSetQueryCmd");

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	/* Setup common CMD Info Packet */
	prCmdInfo->eCmdType = COMMAND_TYPE_NETWORK_IOCTL;
	prCmdInfo->u2InfoBufLen =
		(uint16_t)(prChipInfo->u2CmdTxHdrSize + u4SetQueryInfoLen);
	prCmdInfo->pfCmdDoneHandler = pfCmdDoneHandler;
	prCmdInfo->pfCmdTimeoutHandler = pfCmdTimeoutHandler;
	prCmdInfo->fgIsOid = fgIsOid;
	prCmdInfo->ucCID = ucCID;
	prCmdInfo->fgSetQuery = fgSetQuery;
	prCmdInfo->fgNeedResp = fgNeedResp;
	prCmdInfo->u4SetInfoLen = u4SetQueryInfoLen;
	prCmdInfo->pvInformationBuffer = pvSetQueryBuffer;
	prCmdInfo->u4InformationBufferLength = u4SetQueryBufferLen;

	/* Setup WIFI_CMD_T (no payload) */
	NIC_FILL_CMD_TX_HDR(prAdapter,
		prCmdInfo->pucInfoBuffer,
		prCmdInfo->u2InfoBufLen,
		prCmdInfo->ucCID,
		CMD_PACKET_TYPE_ID,
		&prCmdInfo->ucCmdSeqNum,
		prCmdInfo->fgSetQuery,
		&pucCmdBuf, FALSE, ucExtCID, S2D_INDEX_CMD_H2C);
	if (u4SetQueryInfoLen > 0 && pucInfoBuffer != NULL)
		kalMemCopy(pucCmdBuf, pucInfoBuffer, u4SetQueryInfoLen);

	/* insert into prCmdQueue */
	kalEnqueueCommand(prGlueInfo, (struct QUE_ENTRY *) prCmdInfo);

	/* wakeup txServiceThread later */
	GLUE_SET_EVENT(prGlueInfo);
	return WLAN_STATUS_PENDING;
}

static uint32_t StaRecUpdateBasic(
	struct ADAPTER *pAd,
	uint8_t *pMsgBuf,
	void *args)
{
	struct STA_RECORD *pStaRecCfg = (struct STA_RECORD *)args;
	struct STAREC_COMMON_T StaRecCommon = {0};

	/* Fill TLV format */
	StaRecCommon.u2Tag = STA_REC_BASIC;
	StaRecCommon.u2Length = sizeof(struct STAREC_COMMON_T);
	StaRecCommon.u4ConnectionType = CPU_TO_LE32(CONNECTION_INFRA_STA);
	StaRecCommon.ucConnectionState = pStaRecCfg->ucStaState;
	/* New info to indicate this is new way to update STAREC */
	StaRecCommon.u2ExtraInfo = STAREC_COMMON_EXTRAINFO_V2;

	if (pStaRecCfg->ucStaState == STA_STATE_1)
		StaRecCommon.u2ExtraInfo |= STAREC_COMMON_EXTRAINFO_NEWSTAREC;

#if 0 /* TODO: soft ap mode */
#ifdef CONFIG_AP_SUPPORT
	if (pEntry) {
		StaRecCommon.ucIsQBSS =
			CLIENT_STATUS_TEST_FLAG(pEntry,
				fCLIENT_STATUS_WMM_CAPABLE) ?
			TRUE : FALSE;
		StaRecCommon.u2AID = cpu2le16(pEntry->Aid);
	}

#endif /* CONFIG_AP_SUPPORT */
#endif

	StaRecCommon.ucIsQBSS = pStaRecCfg->fgIsQoS;

	StaRecCommon.u2AID = CPU_TO_LE16(pStaRecCfg->u2AssocId);
	kalMemCopy(&StaRecCommon.aucPeerMacAddr[0], &pStaRecCfg->aucMacAddr[0],
		MAC_ADDR_LEN);

	/* Append this feature */
#if 0 /* TODO: big endian platform */
#ifdef RT_BIG_ENDIAN
	StaRecCommon.u2Tag = cpu2le16(StaRecCommon.u2Tag);
	StaRecCommon.u2Length = cpu2le16(StaRecCommon.u2Length);
	StaRecCommon.u2ExtraInfo = cpu2le16(StaRecCommon.u2ExtraInfo);
#endif
#endif
	kalMemCopy(pMsgBuf,
		(char *)&StaRecCommon,
		sizeof(struct STAREC_COMMON_T));
	return 0;
}

static uint32_t BssInfoUpdateBasic(
	struct ADAPTER *pAd,
	uint8_t *pMsgBuf,
	uint8_t ucBssIdx)
{
	struct BSSINFO_BASIC_T rBssInfo = {0};
	struct BSS_INFO *prBssInfo;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo =
	    aisGetAisSpecBssInfo(pAd, ucBssIdx);

	prBssInfo = pAd->aprBssInfo[ucBssIdx];
	/* Tag assignment */
	rBssInfo.u2Tag = BSS_INFO_BASIC;
	rBssInfo.u2Length = sizeof(struct BSSINFO_BASIC_T);
	/* content */
	kalMemCopy(rBssInfo.aucBSSID, prBssInfo->aucBSSID, MAC_ADDR_LEN);
	rBssInfo.ucBcMcWlanidx = ucBssIdx;
	rBssInfo.ucActive = TRUE;
	rBssInfo.ucWmmIdx = prBssInfo->ucWmmQueSet;
	rBssInfo.ucCipherSuit =
		prAisSpecBssInfo->ucKeyAlgorithmId;

/* WA didn't use */
/*
*	rBssInfo.u4NetworkType = NETWORK_TYPE_AIS;
*	rBssInfo.u2BcnInterval = 100;
*	rBssInfo.ucDtimPeriod = 1;
*/

	/* Append this feature */
	kalMemCopy(pMsgBuf,
		(char *)&rBssInfo,
		sizeof(struct BSSINFO_BASIC_T));
	return 0;
}

static uint32_t BssInfoUpdateConnectOwnDev(
	struct ADAPTER *pAd,
	uint8_t *pMsgBuf,
	uint8_t ucBssIdx)
{
	struct BSSINFO_CONNECT_OWN_DEV_T rBssInfo = {0};
	struct BSS_INFO *prBssInfo;

	prBssInfo = pAd->aprBssInfo[ucBssIdx];
	/* Tag assignment */
	rBssInfo.u2Tag = BSS_INFO_OWN_MAC;
	rBssInfo.u2Length = sizeof(struct BSSINFO_CONNECT_OWN_DEV_T);
	/* content */
	rBssInfo.ucHwBSSIndex = ucBssIdx;
	rBssInfo.ucOwnMacIdx = prBssInfo->ucOwnMacIndex;
	rBssInfo.ucConnectionType = CPU_TO_LE32(CONNECTION_INFRA_STA);

	/* Append this feature */
	kalMemCopy(pMsgBuf,
		(char *)&rBssInfo,
		sizeof(struct BSSINFO_CONNECT_OWN_DEV_T));
	return 0;
}

static uint32_t DevInfoUpdateBasic(
	struct ADAPTER *pAd,
	uint8_t *pMsgBuf,
	uint8_t ucBssIdx)
{
	struct CMD_DEVINFO_ACTIVE_T rDevInfo = {0};
	struct BSS_INFO *prBssInfo;

	prBssInfo = pAd->aprBssInfo[ucBssIdx];

	/* Tag assignment */
	rDevInfo.u2Tag = DEV_INFO_ACTIVE;
	rDevInfo.u2Length = sizeof(struct CMD_DEVINFO_ACTIVE_T);
	/* content */
	kalMemCopy(rDevInfo.aucOwnMacAddr,
		prBssInfo->aucOwnMacAddr, MAC_ADDR_LEN);
	rDevInfo.ucActive = TRUE;
	rDevInfo.ucDbdcIdx = 0;

	/* Append this feature */
	kalMemCopy(pMsgBuf,
		(char *)&rDevInfo,
		sizeof(struct CMD_DEVINFO_ACTIVE_T));
	return 0;
}

uint32_t CmdExtStaRecUpdate2WA(
	struct ADAPTER *pAd,
	struct STA_RECORD *pStaRecCfg)
{
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	uint32_t size;
	struct CMD_STAREC_UPDATE_T *prCmdContent;

	size = sizeof(struct CMD_STAREC_UPDATE_T);
	size += sizeof(struct STAREC_COMMON_T);

	prCmdContent = cnmMemAlloc(pAd, RAM_TYPE_BUF, size);
	if (!prCmdContent) {
		log_dbg(MEM, WARN,
			"%s: command allocation failed\n",
			__func__);

		return WLAN_STATUS_RESOURCES;
	}

	prCmdContent->ucBssIndex = pStaRecCfg->ucBssIndex;
	prCmdContent->ucWlanIdx = pStaRecCfg->ucWlanIndex;
	prCmdContent->ucMuarIdx = 0;

	StaRecUpdateBasic(pAd,
		(uint8_t *)prCmdContent+sizeof(struct CMD_STAREC_UPDATE_T),
		(void *)pStaRecCfg);

	rWlanStatus = wlanSendSetQueryExtCmd2WA(pAd,
						CMD_ID_LAYER_0_EXT_MAGIC_NUM,
						EXT_CMD_ID_STAREC_UPDATE,
						TRUE,
						FALSE,
						TRUE,
						NULL,
						nicOidCmdTimeoutCommon,
						size,
						(uint8_t *) (prCmdContent),
						NULL, 0);
	cnmMemFree(pAd, prCmdContent);
	return rWlanStatus;
}

#if (CFG_SUPPORT_DMASHDL_SYSDVT)
/* Inform DVT item to WA */
uint32_t CmdExtDmaShdlDvt2WA(
	struct ADAPTER *pAd,
	uint8_t ucItemNo,
	uint8_t ucSubItemNo)
{
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	uint32_t size;
	struct EXT_CMD_CR4_DMASHDL_DVT_T *prCmdContent;

	size = sizeof(struct EXT_CMD_CR4_DMASHDL_DVT_T);

	prCmdContent = cnmMemAlloc(pAd, RAM_TYPE_BUF, size);
	if (!prCmdContent) {
		log_dbg(MEM, WARN,
			"%s: command allocation failed\n",
			__func__);

		return WLAN_STATUS_RESOURCES;
	}

	prCmdContent->ucItemNo = ucItemNo;
	prCmdContent->ucSubItemNo = ucSubItemNo;

	rWlanStatus = wlanSendSetQueryExtCmd2WA(pAd,
						CMD_ID_LAYER_0_EXT_MAGIC_NUM,
						EXT_CMD_ID_CR4_DMASHDL_DVT,
						TRUE,
						FALSE,
						TRUE,
						NULL,
						nicOidCmdTimeoutCommon,
						size,
						(uint8_t *) (prCmdContent),
						NULL, 0);
	cnmMemFree(pAd, prCmdContent);
	return rWlanStatus;
}
#endif /* CFG_SUPPORT_DMASHDL_SYSDVT */

uint32_t CmdExtBssInfoUpdate2WA(
	struct ADAPTER *pAd,
	uint8_t ucBssIndex)
{
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	uint32_t size;
	struct BSS_INFO *prBssInfo;
	struct CMD_BSSINFO_UPDATE_T *prCmdContent;

	ASSERT(pAd);
	ASSERT(ucBssIndex <= pAd->ucHwBssIdNum);

	prBssInfo = pAd->aprBssInfo[ucBssIndex];

	size = sizeof(struct CMD_BSSINFO_UPDATE_T);
	size += sizeof(struct BSSINFO_BASIC_T);
	size += sizeof(struct BSSINFO_CONNECT_OWN_DEV_T);

	prCmdContent = cnmMemAlloc(pAd, RAM_TYPE_BUF, size);
	if (!prCmdContent) {
		log_dbg(MEM, WARN,
			"%s: command allocation failed\n",
			__func__);
		return WLAN_STATUS_RESOURCES;
	}

	prCmdContent->ucBssIndex = ucBssIndex;
	prCmdContent->u2TotalElementNum = 1;

	BssInfoUpdateBasic(pAd,
		(uint8_t *)prCmdContent+sizeof(struct CMD_BSSINFO_UPDATE_T),
		ucBssIndex);

	BssInfoUpdateConnectOwnDev(pAd,
		(uint8_t *)prCmdContent +
		sizeof(struct CMD_BSSINFO_UPDATE_T) +
		sizeof(struct BSSINFO_BASIC_T),
		ucBssIndex);

	rWlanStatus = wlanSendSetQueryExtCmd2WA(pAd,
						CMD_ID_LAYER_0_EXT_MAGIC_NUM,
						EXT_CMD_ID_BSSINFO_UPDATE,
						TRUE,
						FALSE,
						TRUE,
						NULL,
						nicOidCmdTimeoutCommon,
						size,
						(uint8_t *) (prCmdContent),
						NULL, 0);
	cnmMemFree(pAd, prCmdContent);
	return rWlanStatus;
}

uint32_t CmdExtDevInfoUpdate2WA(
	struct ADAPTER *pAd,
	uint8_t ucBssIndex)
{
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	uint32_t size;
	struct CMD_DEVINFO_UPDATE_T *prCmdContent;
	struct BSS_INFO *prBssInfo;

	prBssInfo = pAd->aprBssInfo[ucBssIndex];

	ASSERT(pAd);

	size = sizeof(struct CMD_DEVINFO_UPDATE_T);
	size += sizeof(struct CMD_DEVINFO_ACTIVE_T);

	prCmdContent = cnmMemAlloc(pAd, RAM_TYPE_BUF, size);
	if (!prCmdContent) {
		log_dbg(MEM, WARN,
			"%s: command allocation failed\n",
			__func__);

		return WLAN_STATUS_RESOURCES;
	}

	prCmdContent->ucOwnMacIdx = prBssInfo->ucOwnMacIndex;
	prCmdContent->u2TotalElementNum = 1;

	DevInfoUpdateBasic(pAd,
		(uint8_t *)prCmdContent+sizeof(struct CMD_DEVINFO_UPDATE_T),
		ucBssIndex);

	rWlanStatus = wlanSendSetQueryExtCmd2WA(pAd,
						CMD_ID_LAYER_0_EXT_MAGIC_NUM,
						EXT_CMD_ID_DEVINFO_UPDATE,
						TRUE,
						FALSE,
						TRUE,
						NULL,
						nicOidCmdTimeoutCommon,
						size,
						(uint8_t *) (prCmdContent),
						NULL, 0);
	cnmMemFree(pAd, prCmdContent);
	return rWlanStatus;
}

#endif /* CFG_SUPPORT_CONNAC2X == 1 */
