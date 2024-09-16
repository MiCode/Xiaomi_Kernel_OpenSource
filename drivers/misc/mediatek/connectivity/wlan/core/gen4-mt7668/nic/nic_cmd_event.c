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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/nic/nic_cmd_event.c#3
*/

/*! \file   nic_cmd_event.c
*    \brief  Callback functions for Command packets.
*
*	Various Event packet handlers which will be setup in the callback function of
*    a command packet.
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
#include "gl_ate_agent.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
const NIC_CAPABILITY_V2_REF_TABLE_T gNicCapabilityV2InfoTable[] = {
	{TAG_CAP_TX_RESOURCE, nicCmdEventQueryNicTxResource},
	{TAG_CAP_TX_EFUSEADDRESS, nicCmdEventQueryNicEfuseAddr},
	{TAG_CAP_COEX_FEATURE, nicCmdEventQueryNicCoexFeature},
	{TAG_CAP_SINGLE_SKU, rlmDomainExtractSingleSkuInfoFromFirmware},
#if CFG_TCP_IP_CHKSUM_OFFLOAD
	{TAG_CAP_CSUM_OFFLOAD, nicCmdEventQueryNicCsumOffload},
#endif
	{TAG_CAP_MAC_EFUSE_OFFSET, nicCmdEventQueryEfuseOffset},
	{TAG_CAP_R_MODE_CAP, nicCmdEventQueryRModeCapability}
};

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                            F U N C T I O N   D A T A
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

VOID nicCmdEventQueryCoexGetInfo(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_GLUE_INFO_T prGlueInfo;

	struct CMD_COEX_CTRL *prCmdCoexCtrl;
	struct CMD_COEX_GET_INFO *prCmdCoexGetInfo;
	struct PARAM_COEX_CTRL *prCoexCtrl;
	struct PARAM_COEX_GET_INFO *prCoexGetInfo;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prCmdCoexCtrl = (struct CMD_COEX_CTRL *) (pucEventBuf);
		u4QueryInfoLen  = sizeof(struct PARAM_COEX_CTRL);
		prCmdCoexGetInfo = (struct CMD_COEX_GET_INFO *) &prCmdCoexCtrl->aucBuffer[0];

		prCoexCtrl = (struct PARAM_COEX_CTRL *) prCmdInfo->pvInformationBuffer;
		prCoexGetInfo  = (struct PARAM_COEX_GET_INFO *) &prCoexCtrl->aucBuffer[0];

		kalMemCopy(prCoexGetInfo->u4CoexInfo, prCmdCoexGetInfo->u4CoexInfo,
			sizeof(prCmdCoexGetInfo->u4CoexInfo));
		DBGLOG(REQ, INFO, "nicCmdEventQueryCoexGetInfo!!\n");
		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}

}

VOID nicCmdEventQueryCoexIso(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_GLUE_INFO_T prGlueInfo;

	struct CMD_COEX_CTRL *prCmdCoexCtrl;
	struct CMD_COEX_ISO_DETECT *prCmdCoexIsoDetect;
	struct PARAM_COEX_CTRL *prCoexCtrl;
	struct PARAM_COEX_ISO_DETECT *prCoexIsoDetect;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prCmdCoexCtrl = (struct CMD_COEX_CTRL *) (pucEventBuf);
		u4QueryInfoLen  = sizeof(struct PARAM_COEX_CTRL);
		prCmdCoexIsoDetect = (struct CMD_COEX_ISO_DETECT *) &prCmdCoexCtrl->aucBuffer[0];

		prCoexCtrl = (struct PARAM_COEX_CTRL *) prCmdInfo->pvInformationBuffer;
		prCoexIsoDetect  = (struct PARAM_COEX_ISO_DETECT *) &prCoexCtrl->aucBuffer[0];
		prCoexIsoDetect->u4IsoPath = prCmdCoexIsoDetect->u4IsoPath;
		prCoexIsoDetect->u4Channel = prCmdCoexIsoDetect->u4Channel;
		/*prCoexIsoDetect->u4Band = prCmdCoexIsoDetect->u4Band;*/
		prCoexIsoDetect->u4Isolation = prCmdCoexIsoDetect->u4Isolation;

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}

}

#if CFG_SUPPORT_QA_TOOL
VOID nicCmdEventQueryRxStatistics(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_PARAM_CUSTOM_ACCESS_RX_STAT prRxStatistics;
	P_EVENT_ACCESS_RX_STAT prEventAccessRxStat;
	UINT_32 u4QueryInfoLen, i;
	P_GLUE_INFO_T prGlueInfo;
	PUINT_32 prElement;
	UINT_32 u4Temp;
	/* P_CMD_ACCESS_RX_STAT                  prCmdRxStat, prRxStat; */

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEventAccessRxStat = (P_EVENT_ACCESS_RX_STAT) (pucEventBuf);

		prRxStatistics = (P_PARAM_CUSTOM_ACCESS_RX_STAT) prCmdInfo->pvInformationBuffer;
		prRxStatistics->u4SeqNum = prEventAccessRxStat->u4SeqNum;
		prRxStatistics->u4TotalNum = prEventAccessRxStat->u4TotalNum;

		u4QueryInfoLen = sizeof(CMD_ACCESS_RX_STAT);

		if (prRxStatistics->u4SeqNum == u4RxStatSeqNum) {
			prElement = &g_HqaRxStat.MAC_FCS_Err;
			for (i = 0; i < HQA_RX_STATISTIC_NUM; i++) {
				u4Temp = ntohl(prEventAccessRxStat->au4Buffer[i]);
				kalMemCopy(prElement, &u4Temp, 4);

				if (i < (HQA_RX_STATISTIC_NUM - 1))
					prElement++;
			}

			g_HqaRxStat.AllMacMdrdy0 = ntohl(prEventAccessRxStat->au4Buffer[i]);
			i++;
			g_HqaRxStat.AllMacMdrdy1 = ntohl(prEventAccessRxStat->au4Buffer[i]);
			/* i++; */
			/* g_HqaRxStat.AllFCSErr0 = ntohl(prEventAccessRxStat->au4Buffer[i]); */
			/* i++; */
			/* g_HqaRxStat.AllFCSErr1 = ntohl(prEventAccessRxStat->au4Buffer[i]); */
		}

		DBGLOG(INIT, ERROR,
			"MT6632 : RX Statistics Test SeqNum = %d, TotalNum = %d\n",
			 (unsigned int)prEventAccessRxStat->u4SeqNum, (unsigned int)prEventAccessRxStat->u4TotalNum);

		DBGLOG(INIT, ERROR, "MAC_FCS_ERR = %d, MAC_MDRDY = %d, MU_RX_CNT = %d, RX_FIFO_FULL = %d\n",
			 (unsigned int)prEventAccessRxStat->au4Buffer[0],
				(unsigned int)prEventAccessRxStat->au4Buffer[1],
				(unsigned int)prEventAccessRxStat->au4Buffer[65],
				(unsigned int)prEventAccessRxStat->au4Buffer[22]);

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}

}

#if CFG_SUPPORT_TX_BF
VOID nicCmdEventPfmuDataRead(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_GLUE_INFO_T prGlueInfo;
	P_PFMU_DATA prEventPfmuDataRead = NULL;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEventPfmuDataRead = (P_PFMU_DATA) (pucEventBuf);

		u4QueryInfoLen = sizeof(PFMU_DATA);

		g_rPfmuData = *prEventPfmuDataRead;

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}

	DBGLOG(INIT, INFO, "=========== Before ===========\n");
	if (prEventPfmuDataRead != NULL) {
		DBGLOG(INIT, INFO, "u2Phi11 = 0x%x\n", prEventPfmuDataRead->rField.u2Phi11);
		DBGLOG(INIT, INFO, "ucPsi21 = 0x%x\n", prEventPfmuDataRead->rField.ucPsi21);
		DBGLOG(INIT, INFO, "u2Phi21 = 0x%x\n", prEventPfmuDataRead->rField.u2Phi21);
		DBGLOG(INIT, INFO, "ucPsi31 = 0x%x\n", prEventPfmuDataRead->rField.ucPsi31);
		DBGLOG(INIT, INFO, "u2Phi31 = 0x%x\n", prEventPfmuDataRead->rField.u2Phi31);
		DBGLOG(INIT, INFO, "ucPsi41 = 0x%x\n", prEventPfmuDataRead->rField.ucPsi41);
		DBGLOG(INIT, INFO, "u2Phi22 = 0x%x\n", prEventPfmuDataRead->rField.u2Phi22);
		DBGLOG(INIT, INFO, "ucPsi32 = 0x%x\n", prEventPfmuDataRead->rField.ucPsi32);
		DBGLOG(INIT, INFO, "u2Phi32 = 0x%x\n", prEventPfmuDataRead->rField.u2Phi32);
		DBGLOG(INIT, INFO, "ucPsi42 = 0x%x\n", prEventPfmuDataRead->rField.ucPsi42);
		DBGLOG(INIT, INFO, "u2Phi33 = 0x%x\n", prEventPfmuDataRead->rField.u2Phi33);
		DBGLOG(INIT, INFO, "ucPsi43 = 0x%x\n", prEventPfmuDataRead->rField.ucPsi43);
		DBGLOG(INIT, INFO, "u2dSNR00 = 0x%x\n", prEventPfmuDataRead->rField.u2dSNR00);
		DBGLOG(INIT, INFO, "u2dSNR01 = 0x%x\n", prEventPfmuDataRead->rField.u2dSNR01);
		DBGLOG(INIT, INFO, "u2dSNR02 = 0x%x\n", prEventPfmuDataRead->rField.u2dSNR02);
		DBGLOG(INIT, INFO, "u2dSNR03 = 0x%x\n", prEventPfmuDataRead->rField.u2dSNR03);
	}
}

VOID nicCmdEventPfmuTagRead(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_GLUE_INFO_T prGlueInfo;
	P_EVENT_PFMU_TAG_READ_T prEventPfmuTagRead = NULL;
	P_PARAM_CUSTOM_PFMU_TAG_READ_STRUCT_T prPfumTagRead = NULL;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	if (!pucEventBuf) {
		DBGLOG(INIT, ERROR, "pucEventBuf is NULL.\n");
		return;
	}
	if (!prCmdInfo->pvInformationBuffer) {
		DBGLOG(INIT, ERROR, "prCmdInfo->pvInformationBuffer is NULL.\n");
		return;
	}
	/* 4 <2> Update information of OID */
	if (!prCmdInfo->fgIsOid) {
		DBGLOG(INIT, ERROR, "cmd %u seq #%u not oid!",
		       prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum);
		return;
	}
	prGlueInfo = prAdapter->prGlueInfo;
	prEventPfmuTagRead = (P_EVENT_PFMU_TAG_READ_T) (pucEventBuf);

	prPfumTagRead = (P_PARAM_CUSTOM_PFMU_TAG_READ_STRUCT_T) prCmdInfo->pvInformationBuffer;

	kalMemCopy(prPfumTagRead, prEventPfmuTagRead, sizeof(EVENT_PFMU_TAG_READ_T));

	u4QueryInfoLen = sizeof(CMD_TXBF_ACTION_T);

	g_rPfmuTag1 = prPfumTagRead->ru4TxBfPFMUTag1;
	g_rPfmuTag2 = prPfumTagRead->ru4TxBfPFMUTag2;

	kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);

	DBGLOG(INIT, INFO, "========================== (R)Tag1 info ==========================\n");

	DBGLOG(INIT, INFO, " Row data0 : %x, Row data1 : %x, Row data2 : %x, Row data3 : %x\n",
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.au4RawData[0],
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.au4RawData[1],
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.au4RawData[2], prEventPfmuTagRead->ru4TxBfPFMUTag1.au4RawData[3]);
	DBGLOG(INIT, INFO, "ProfileID = %d Invalid status = %d\n",
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucProfileID,
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucInvalidProf);
	DBGLOG(INIT, INFO, "0:iBF / 1:eBF = %d\n", prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucTxBf);
	DBGLOG(INIT, INFO, "0:SU / 1:MU = %d\n", prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucSU_MU);
	DBGLOG(INIT, INFO, "DBW(0/1/2/3 BW20/40/80/160NC) = %d\n", prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucDBW);
	DBGLOG(INIT, INFO, "RMSD = %d\n", prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucRMSD);
	DBGLOG(INIT, INFO, "Nrow = %d, Ncol = %d, Ng = %d, LM = %d\n",
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucNrow,
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucNcol,
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucNgroup, prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucLM);
	DBGLOG(INIT, INFO, "Mem1(%d, %d), Mem2(%d, %d), Mem3(%d, %d), Mem4(%d, %d)\n",
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucMemAddr1ColIdx,
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucMemAddr1RowIdx,
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucMemAddr2ColIdx,
	       (prEventPfmuTagRead->ru4TxBfPFMUTag1.
		rField.ucMemAddr2RowIdx | (prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucMemAddr2RowIdxMsb << 5)),
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucMemAddr3ColIdx,
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucMemAddr3RowIdx,
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucMemAddr4ColIdx,
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucMemAddr4RowIdx);
	DBGLOG(INIT, INFO, "SNR STS0=0x%x, SNR STS1=0x%x, SNR STS2=0x%x, SNR STS3=0x%x\n",
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucSNR_STS0,
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucSNR_STS1,
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucSNR_STS2,
	       prEventPfmuTagRead->ru4TxBfPFMUTag1.rField.ucSNR_STS3);
	DBGLOG(INIT, INFO, "===============================================================\n");

	DBGLOG(INIT, INFO, "========================== (R)Tag2 info ==========================\n");
	DBGLOG(INIT, INFO, " Row data0 : %x, Row data1 : %x, Row data2 : %x\n",
	       prEventPfmuTagRead->ru4TxBfPFMUTag2.au4RawData[0],
	       prEventPfmuTagRead->ru4TxBfPFMUTag2.au4RawData[1], prEventPfmuTagRead->ru4TxBfPFMUTag2.au4RawData[2]);
	DBGLOG(INIT, INFO, "Smart Ant Cfg = %d\n", prEventPfmuTagRead->ru4TxBfPFMUTag2.rField.u2SmartAnt);
	DBGLOG(INIT, INFO, "SE index = %d\n", prEventPfmuTagRead->ru4TxBfPFMUTag2.rField.ucSEIdx);
	DBGLOG(INIT, INFO, "RMSD Threshold = %d\n", prEventPfmuTagRead->ru4TxBfPFMUTag2.rField.ucRMSDThd);
	DBGLOG(INIT, INFO, "MCS TH L1SS = %d, S1SS = %d, L2SS = %d, S2SS = %d\n"
	       "L3SS = %d, S3SS = %d\n",
	       prEventPfmuTagRead->ru4TxBfPFMUTag2.rField.ucMCSThL1SS,
	       prEventPfmuTagRead->ru4TxBfPFMUTag2.rField.ucMCSThS1SS,
	       prEventPfmuTagRead->ru4TxBfPFMUTag2.rField.ucMCSThL2SS,
	       prEventPfmuTagRead->ru4TxBfPFMUTag2.rField.ucMCSThS2SS,
	       prEventPfmuTagRead->ru4TxBfPFMUTag2.rField.ucMCSThL3SS,
	       prEventPfmuTagRead->ru4TxBfPFMUTag2.rField.ucMCSThS3SS);
	DBGLOG(INIT, INFO, "iBF lifetime limit(unit:4ms) = 0x%x\n",
	       prEventPfmuTagRead->ru4TxBfPFMUTag2.rField.uciBfTimeOut);
	DBGLOG(INIT, INFO, "iBF desired DBW = %d\n  0/1/2/3 : BW20/40/80/160NC\n",
	       prEventPfmuTagRead->ru4TxBfPFMUTag2.rField.uciBfDBW);
	DBGLOG(INIT, INFO, "iBF desired Ncol = %d\n  0/1/2 : Ncol = 1 ~ 3\n",
	       prEventPfmuTagRead->ru4TxBfPFMUTag2.rField.uciBfNcol);
	DBGLOG(INIT, INFO, "iBF desired Nrow = %d\n  0/1/2/3 : Nrow = 1 ~ 4\n",
	       prEventPfmuTagRead->ru4TxBfPFMUTag2.rField.uciBfNrow);
	DBGLOG(INIT, INFO, "===============================================================\n");

}

#endif /* CFG_SUPPORT_TX_BF */
#if CFG_SUPPORT_MU_MIMO
VOID nicCmdEventGetQd(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_GLUE_INFO_T prGlueInfo;
	P_EVENT_HQA_GET_QD prEventHqaGetQd;
	UINT_32 i;

	P_PARAM_CUSTOM_GET_QD_STRUCT_T prGetQd = NULL;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);
	if (!pucEventBuf) {
		DBGLOG(INIT, ERROR, "pucEventBuf is NULL.\n");
		return;
	}
	if (!prCmdInfo->pvInformationBuffer) {
		DBGLOG(INIT, ERROR, "prCmdInfo->pvInformationBuffer is NULL.\n");
		return;
	}
	/* 4 <2> Update information of OID */
	if (!prCmdInfo->fgIsOid) {
		DBGLOG(INIT, ERROR, "cmd %u seq #%u not oid!\n",
		       prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum);
		return;
	}
	prGlueInfo = prAdapter->prGlueInfo;
	prEventHqaGetQd = (P_EVENT_HQA_GET_QD) (pucEventBuf);

	prGetQd = (P_PARAM_CUSTOM_GET_QD_STRUCT_T) prCmdInfo->pvInformationBuffer;

	kalMemCopy(prGetQd, prEventHqaGetQd, sizeof(EVENT_HQA_GET_QD));

	u4QueryInfoLen = sizeof(CMD_MUMIMO_ACTION_T);

	/* g_rPfmuTag1 = prPfumTagRead->ru4TxBfPFMUTag1; */
	/* g_rPfmuTag2 = prPfumTagRead->ru4TxBfPFMUTag2; */

	kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);

	DBGLOG(INIT, INFO, " event id : %x\n", prGetQd->u4EventId);
	for (i = 0; i < 14; i++)
		DBGLOG(INIT, INFO, "au4RawData[%d]: %x\n", i, prGetQd->au4RawData[i]);

}

VOID nicCmdEventGetCalcLq(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_GLUE_INFO_T prGlueInfo;
	P_EVENT_HQA_GET_MU_CALC_LQ prEventHqaGetMuCalcLq;
	UINT_32 i, j;

	P_PARAM_CUSTOM_GET_MU_CALC_LQ_STRUCT_T prGetMuCalcLq = NULL;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);
	if (!pucEventBuf) {
		DBGLOG(INIT, ERROR, "pucEventBuf is NULL.\n");
		return;
	}
	if (!prCmdInfo->pvInformationBuffer) {
		DBGLOG(INIT, ERROR, "prCmdInfo->pvInformationBuffer is NULL.\n");
		return;
	}
	/* 4 <2> Update information of OID */
	if (!prCmdInfo->fgIsOid) {
		DBGLOG(INIT, ERROR, "cmd %u seq #%u not oid!\n",
		       prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum);
		return;
	}
	prGlueInfo = prAdapter->prGlueInfo;
	prEventHqaGetMuCalcLq = (P_EVENT_HQA_GET_MU_CALC_LQ) (pucEventBuf);

	prGetMuCalcLq = (P_PARAM_CUSTOM_GET_MU_CALC_LQ_STRUCT_T) prCmdInfo->pvInformationBuffer;

	kalMemCopy(prGetMuCalcLq, prEventHqaGetMuCalcLq, sizeof(EVENT_HQA_GET_MU_CALC_LQ));

	u4QueryInfoLen = sizeof(CMD_MUMIMO_ACTION_T);

	/* g_rPfmuTag1 = prPfumTagRead->ru4TxBfPFMUTag1; */
	/* g_rPfmuTag2 = prPfumTagRead->ru4TxBfPFMUTag2; */

	kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);


	DBGLOG(INIT, INFO, " event id : %x\n", prGetMuCalcLq->u4EventId);
	for (i = 0; i < NUM_OF_USER; i++)
		for (j = 0; j < NUM_OF_MODUL; j++)
			DBGLOG(INIT, INFO, " lq_report[%d][%d]: %x\n", i, j, prGetMuCalcLq->rEntry.lq_report[i][j]);

}

VOID nicCmdEventGetCalcInitMcs(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_GLUE_INFO_T prGlueInfo;
	P_EVENT_SHOW_GROUP_TBL_ENTRY prEventShowGroupTblEntry = NULL;

	P_PARAM_CUSTOM_SHOW_GROUP_TBL_ENTRY_STRUCT_T prShowGroupTbl;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);
	if (!pucEventBuf) {
		DBGLOG(INIT, ERROR, "pucEventBuf is NULL.\n");
		return;
	}
	if (!prCmdInfo->pvInformationBuffer) {
		DBGLOG(INIT, ERROR, "prCmdInfo->pvInformationBuffer is NULL.\n");
		return;
	}
	/* 4 <2> Update information of OID */
	if (!prCmdInfo->fgIsOid) {
		DBGLOG(INIT, ERROR, "cmd %u seq #%u not oid!\n",
		       prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum);
		return;
	}
	prGlueInfo = prAdapter->prGlueInfo;
	prEventShowGroupTblEntry = (P_EVENT_SHOW_GROUP_TBL_ENTRY) (pucEventBuf);

	prShowGroupTbl = (P_PARAM_CUSTOM_SHOW_GROUP_TBL_ENTRY_STRUCT_T) prCmdInfo->pvInformationBuffer;

	kalMemCopy(prShowGroupTbl, prEventShowGroupTblEntry, sizeof(EVENT_SHOW_GROUP_TBL_ENTRY));

	u4QueryInfoLen = sizeof(CMD_MUMIMO_ACTION_T);

	/* g_rPfmuTag1 = prPfumTagRead->ru4TxBfPFMUTag1; */
	/* g_rPfmuTag2 = prPfumTagRead->ru4TxBfPFMUTag2; */

	kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);


	DBGLOG(INIT, INFO, "========================== (R)Group table info ==========================\n");
	DBGLOG(INIT, INFO, " event id : %x\n", prEventShowGroupTblEntry->u4EventId);
	DBGLOG(INIT, INFO, "index = %x numUser = %x\n", prEventShowGroupTblEntry->index,
	       prEventShowGroupTblEntry->numUser);
	DBGLOG(INIT, INFO, "BW = %x NS0/1/ = %x/%x\n", prEventShowGroupTblEntry->BW, prEventShowGroupTblEntry->NS0,
	       prEventShowGroupTblEntry->NS1);
	DBGLOG(INIT, INFO, "PFIDUser0/1 = %x/%x\n", prEventShowGroupTblEntry->PFIDUser0,
	       prEventShowGroupTblEntry->PFIDUser1);
	DBGLOG(INIT, INFO, "fgIsShortGI = %x, fgIsUsed = %x, fgIsDisable = %x\n", prEventShowGroupTblEntry->fgIsShortGI,
	       prEventShowGroupTblEntry->fgIsUsed, prEventShowGroupTblEntry->fgIsDisable);
	DBGLOG(INIT, INFO, "initMcsUser0/1 = %x/%x\n", prEventShowGroupTblEntry->initMcsUser0,
	       prEventShowGroupTblEntry->initMcsUser1);
	DBGLOG(INIT, INFO, "dMcsUser0: 0/1/ = %x/%x\n", prEventShowGroupTblEntry->dMcsUser0,
	       prEventShowGroupTblEntry->dMcsUser1);

}
#endif /* CFG_SUPPORT_MU_MIMO */
#endif /* CFG_SUPPORT_QA_TOOL */

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

#if 0
		if (prChipConfigInfo->u2MsgSize > CHIP_CONFIG_RESP_SIZE) {
			DBGLOG(REQ, INFO,
			       "Chip config Msg Size %u is not valid (event)\n", prChipConfigInfo->u2MsgSize);
			prChipConfigInfo->u2MsgSize = CHIP_CONFIG_RESP_SIZE;
		}
#endif
		kalMemCopy(prChipConfigInfo->aucCmd, prCmdChipConfig->aucCmd, prChipConfigInfo->u2MsgSize);
		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}

}

VOID nicCmdEventSetCommon(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	if (prCmdInfo->fgIsOid) {
		/* Update Set Information Length */
		kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery,
			       prCmdInfo->u4InformationBufferLength, WLAN_STATUS_SUCCESS);
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

		kalMemCopy(prQueryBuffer, prTestStatus, sizeof(EVENT_TEST_STATUS));

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

	prGlueInfo = prAdapter->prGlueInfo;
	prRssi = (PARAM_RSSI *) prCmdInfo->pvInformationBuffer;

	kalMemCopy(prRssi, &rRssi, sizeof(PARAM_RSSI));
	u4QueryInfoLen = sizeof(PARAM_RSSI);

	if (prCmdInfo->fgIsOid)
		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery,
			u4QueryInfoLen, WLAN_STATUS_SUCCESS);
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

		prStatistics->u4Length = sizeof(PARAM_802_11_STATISTICS_STRUCT_T);
		prStatistics->rTransmittedFragmentCount = prEventStatistics->rTransmittedFragmentCount;
		prStatistics->rMulticastTransmittedFrameCount = prEventStatistics->rMulticastTransmittedFrameCount;
		prStatistics->rFailedCount = prEventStatistics->rFailedCount;
		prStatistics->rRetryCount = prEventStatistics->rRetryCount;
		prStatistics->rMultipleRetryCount = prEventStatistics->rMultipleRetryCount;
		prStatistics->rRTSSuccessCount = prEventStatistics->rRTSSuccessCount;
		prStatistics->rRTSFailureCount = prEventStatistics->rRTSFailureCount;
		prStatistics->rACKFailureCount = prEventStatistics->rACKFailureCount;
		prStatistics->rFrameDuplicateCount = prEventStatistics->rFrameDuplicateCount;
		prStatistics->rReceivedFragmentCount = prEventStatistics->rReceivedFragmentCount;
		prStatistics->rMulticastReceivedFrameCount = prEventStatistics->rMulticastReceivedFrameCount;
		prStatistics->rFCSErrorCount = prEventStatistics->rFCSErrorCount;
		prStatistics->rTKIPLocalMICFailures.QuadPart = 0;
		prStatistics->rTKIPICVErrors.QuadPart = 0;
		prStatistics->rTKIPCounterMeasuresInvoked.QuadPart = 0;
		prStatistics->rTKIPReplays.QuadPart = 0;
		prStatistics->rCCMPFormatErrors.QuadPart = 0;
		prStatistics->rCCMPReplays.QuadPart = 0;
		prStatistics->rCCMPDecryptErrors.QuadPart = 0;
		prStatistics->rFourWayHandshakeFailures.QuadPart = 0;
		prStatistics->rWEPUndecryptableCount.QuadPart = 0;
		prStatistics->rWEPICVErrorCount.QuadPart = 0;
		prStatistics->rDecryptSuccessCount.QuadPart = 0;
		prStatistics->rDecryptFailureCount.QuadPart = 0;

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}

VOID nicCmdEventEnterRfTest(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
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

#if defined(_HIF_SDIO) && 0
	/* 3. Disable Interrupt */
	HAL_INTR_DISABLE(prAdapter);

	/* 4. Block til firmware completed entering into RF test mode */
	kalMsleep(500);
	while (1) {
		UINT_32 u4Value;

		HAL_MCR_RD(prAdapter, MCR_WCIR, &u4Value);

		if (u4Value & WCIR_WLAN_READY) {
			break;
		} else if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
			if (prCmdInfo->fgIsOid) {
				/* Update Set Information Length */
				kalOidComplete(prAdapter->prGlueInfo,
					       prCmdInfo->fgSetQuery,
					       prCmdInfo->u4SetInfoLen, WLAN_STATUS_NOT_SUPPORTED);

			}
			return;
		}
			kalMsleep(10);
	}

	/* 5. Clear Interrupt Status */
	{
		UINT_32 u4WHISR = 0;
		UINT_16 au2TxCount[16];

		HAL_READ_INTR_STATUS(prAdapter, 4, (PUINT_8)&u4WHISR);
		if (HAL_IS_TX_DONE_INTR(u4WHISR))
			HAL_READ_TX_RELEASED_COUNT(prAdapter, au2TxCount);
	}
	/* 6. Reset TX Counter */
	nicTxResetResource(prAdapter);

	/* 7. Re-enable Interrupt */
	HAL_INTR_ENABLE(prAdapter);
#endif

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
#if defined(_HIF_SDIO) && 0
	UINT_32 u4WHISR = 0;
	UINT_16 au2TxCount[16];
	UINT_32 u4Value;

	/* 1. Disable Interrupt */
	HAL_INTR_DISABLE(prAdapter);

	/* 2. Block until firmware completed leaving from RF test mode */
	kalMsleep(500);
	while (1) {
		HAL_MCR_RD(prAdapter, MCR_WCIR, &u4Value);

		if (u4Value & WCIR_WLAN_READY) {
			break;
		} else if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
			if (prCmdInfo->fgIsOid) {
				/* Update Set Information Length */
				kalOidComplete(prAdapter->prGlueInfo,
					       prCmdInfo->fgSetQuery,
					       prCmdInfo->u4SetInfoLen, WLAN_STATUS_NOT_SUPPORTED);

			}
			return;
		}
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
#endif

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
	 *  DBGLOG(SCN, INFO, "--->nicCmdEventSetStopSchedScan\n" ));
	 */
	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	/*
	 *  DBGLOG(SCN, INFO, "<--kalSchedScanStopped\n" );
	 */
	if (prCmdInfo->fgIsOid) {
		/* Update Set Information Length */
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery, prCmdInfo->u4InformationBufferLength, WLAN_STATUS_SUCCESS);
	}

	DBGLOG(SCN, INFO, "nicCmdEventSetStopSchedScan OID done, release lock and send event to uplayer\n");
	/*Due to dead lock issue, need to release the IO control before calling kernel APIs */
	kalSchedScanStopped(prAdapter->prGlueInfo);

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
* @brief This function is a generic command timeout handler
*
* @param pfnOidHandler      Pointer to the OID handler
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID nicCmdTimeoutCommon(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
	ASSERT(prAdapter);
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

#if CFG_SUPPORT_QA_TOOL
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when received dump memory event packet.
*        transfer the memory data to the IQ format data and write into file
*
* @param prIQAry     Pointer to the array store I or Q data.
*		 prDataLen   The return data length - bytes
*        u4IQ        0: get I data
*				 1 : get Q data
*
* @return -1: open file error
*
*/
/*----------------------------------------------------------------------------*/
INT_32 GetIQData(INT_32 **prIQAry, UINT_32 *prDataLen, UINT_32 u4IQ, UINT_32 u4GetWf1)
{
	UINT_8 aucPath[50];	/* the path for iq data dump out */
	UINT_8 aucData[50];	/* iq data in string format */
	UINT_32 i = 0, j = 0, count = 0;
	INT_32 ret = -1;
	INT_32 rv;
	struct file *file = NULL;

	*prIQAry = g_au4IQData;

	/* sprintf(aucPath, "/pattern.txt");             // CSD's Pattern */
	snprintf(aucPath,
			sizeof(aucPath), "/tmp/dump_out_%05hu_WF%d.txt",
			(g_u2DumpIndex - 1), u4GetWf1);
	if (kalCheckPath(aucPath) == -1)
		snprintf(aucPath,
			sizeof(aucPath), "/data/dump_out_%05hu_WF%d.txt",
			(g_u2DumpIndex - 1), u4GetWf1);

	DBGLOG(INIT, INFO,
		"iCap Read Dump File dump_out_%05hu_WF%u.txt\n",
		(g_u2DumpIndex - 1), u4GetWf1);

	file = kalFileOpen(aucPath, O_RDONLY, 0);

	if ((file != NULL) && !IS_ERR(file)) {
		/* read 1K data per time */
		for (i = 0; i < RTN_IQ_DATA_LEN / sizeof(UINT_32);
		     i++, g_au4Offset[u4GetWf1][u4IQ] += IQ_FILE_LINE_OFFSET) {
			if (kalFileRead(file, g_au4Offset[u4GetWf1][u4IQ], aucData, IQ_FILE_IQ_STR_LEN) == 0)
				break;

			count = 0;

			for (j = 0; j < 8; j++) {
				if (aucData[j] != ' ')
					aucData[count++] = aucData[j];
			}

		    aucData[count] = '\0';

			rv = kstrtoint(aucData, 0, &g_au4IQData[i]);	/* transfer data format (string to int) */
		}
		*prDataLen = i * sizeof(UINT_32);
		kalFileClose(file);
		ret = 0;
	}

	DBGLOG(INIT, INFO, "MT6632 : QA_AGENT GetIQData prDataLen = %d\n", *prDataLen);
	DBGLOG(INIT, INFO, "MT6632 : QA_AGENT GetIQData i = %d\n", i);

	return ret;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when received dump memory event packet.
*        transfer the memory data to the IQ format data and write into file
*
* @param prEventDumpMem     Pointer to the event dump memory structure.
*
* @return 0: SUCCESS, -1: FAIL
*
*/
/*----------------------------------------------------------------------------*/

UINT_32 TsfRawData2IqFmt(P_EVENT_DUMP_MEM_T prEventDumpMem)
{
	static UINT_8 aucPathWF0[40];	/* the path for iq data dump out */
	static UINT_8 aucPathWF1[40];	/* the path for iq data dump out */
	static UINT_8 aucPathRAWWF0[40];	/* the path for iq data dump out */
	static UINT_8 aucPathRAWWF1[40];	/* the path for iq data dump out */
	PUINT_8 pucDataWF0 = NULL;	/* the data write into file */
	PUINT_8 pucDataWF1 = NULL;	/* the data write into file */
	PUINT_8 pucDataRAWWF0 = NULL;	/* the data write into file */
	PUINT_8 pucDataRAWWF1 = NULL;	/* the data write into file */
	UINT_32 u4SrcOffset;	/* record the buffer offset */
	UINT_32 u4FmtLen = 0;	/* bus format length */
	UINT_32 u4CpyLen = 0;
	UINT_32 u4RemainByte;
	UINT_32 u4DataWBufSize = 150;
	UINT_32 u4DataRAWWBufSize = 150;
	UINT_32 u4DataWLenF0 = 0;
	UINT_32 u4DataWLenF1 = 0;
	UINT_32 u4DataRAWWLenF0 = 0;
	UINT_32 u4DataRAWWLenF1 = 0;

	BOOLEAN fgAppend;
	INT_32 u4Iqc160WF0Q0, u4Iqc160WF1I1;

	static UINT_8 ucDstOffset;	/* for alignment. bcs we send 2KB data per packet,*/
								/*the data will not align in 12 bytes case. */
	static UINT_32 u4CurTimeTick;

	static ICAP_BUS_FMT icapBusData;
	UINT_32 *ptr;

	pucDataWF0 = kmalloc(u4DataWBufSize, GFP_KERNEL);
	pucDataWF1 = kmalloc(u4DataWBufSize, GFP_KERNEL);
	pucDataRAWWF0 = kmalloc(u4DataRAWWBufSize, GFP_KERNEL);
	pucDataRAWWF1 = kmalloc(u4DataRAWWBufSize, GFP_KERNEL);


	if ((!pucDataWF0) || (!pucDataWF1) || (!pucDataRAWWF0) || (!pucDataRAWWF1)) {
		DBGLOG(INIT, ERROR, "kmalloc failed.\n");
		kfree(pucDataWF0);
		kfree(pucDataWF1);
		kfree(pucDataRAWWF0);
		kfree(pucDataRAWWF1);
		ASSERT(-1);
		return -1;
	}

	fgAppend = TRUE;
	if (prEventDumpMem->ucFragNum == 1) {

		u4CurTimeTick = kalGetTimeTick();
		/* Store memory dump into sdcard,
		 * path /sdcard/dump_<current  system tick>_<memory address>_<memory length>.hex
		 */
#if defined(LINUX)

		/*if blbist mkdir undre /data/blbist, the dump files wouls put on it */
		scnprintf(aucPathWF0,
			sizeof(aucPathWF0), "/tmp/dump_out_%05hu_WF0.txt",
			g_u2DumpIndex);
		scnprintf(aucPathWF1,
			sizeof(aucPathWF1), "/tmp/dump_out_%05hu_WF1.txt",
			g_u2DumpIndex);
		if (kalCheckPath(aucPathWF0) == -1) {
			kalMemSet(aucPathWF0, 0x00, sizeof(aucPathWF0));
			scnprintf(aucPathWF0,
				sizeof(aucPathWF0),
				"/data/dump_out_%05hu_WF0.txt",
				g_u2DumpIndex);
		} else
			kalTrunkPath(aucPathWF0);

		if (kalCheckPath(aucPathWF1) == -1) {
			kalMemSet(aucPathWF1, 0x00, sizeof(aucPathWF1));
			scnprintf(aucPathWF1,
				sizeof(aucPathWF1),
				"/data/dump_out_%05hu_WF1.txt",
				g_u2DumpIndex);
		} else
			kalTrunkPath(aucPathWF1);

		scnprintf(aucPathRAWWF0,
			sizeof(aucPathRAWWF0), "/dump_RAW_%05hu_WF0.txt",
			g_u2DumpIndex);
		scnprintf(aucPathRAWWF1,
			sizeof(aucPathRAWWF1), "/dump_RAW_%05hu_WF1.txt",
			g_u2DumpIndex);
		if (kalCheckPath(aucPathRAWWF0) == -1) {
			kalMemSet(aucPathRAWWF0, 0x00, sizeof(aucPathRAWWF0));
			scnprintf(aucPathRAWWF0,
				sizeof(aucPathRAWWF0),
					"/data/dump_RAW_%05hu_WF0.txt",
					g_u2DumpIndex);
		} else
			kalTrunkPath(aucPathRAWWF0);

		if (kalCheckPath(aucPathRAWWF1) == -1) {
			kalMemSet(aucPathRAWWF1, 0x00, sizeof(aucPathRAWWF1));
			scnprintf(aucPathRAWWF1, sizeof(aucPathRAWWF1),
				"/data/dump_RAW_%05hu_WF1.txt", g_u2DumpIndex);
		} else
			kalTrunkPath(aucPathRAWWF1);

#else
		kal_sprintf_ddk(aucPathWF0, sizeof(aucPathWF0),
				u4CurTimeTick,
				prEventDumpMem->u4Address, prEventDumpMem->u4Length + prEventDumpMem->u4RemainLength);
		kal_sprintf_ddk(aucPathWF1, sizeof(aucPathWF1),
				u4CurTimeTick,
				prEventDumpMem->u4Address, prEventDumpMem->u4Length + prEventDumpMem->u4RemainLength);
#endif
		/* fgAppend = FALSE; */
	}

	ptr = (PUINT_32)(&prEventDumpMem->aucBuffer[0]);
	/*DBGLOG(INIT, INFO, ": ==> (prEventDumpMem = %08x %08x %08x)\n", *(ptr), *(ptr + 4), *(ptr + 8));*/
	/*DBGLOG(INIT, INFO, ": ==> (prEventDumpMem->eIcapContent = %x)\n", prEventDumpMem->eIcapContent);*/

	for (u4SrcOffset = 0, u4RemainByte = prEventDumpMem->u4Length; u4RemainByte > 0;) {
		u4FmtLen =
(prEventDumpMem->eIcapContent == ICAP_CONTENT_SPECTRUM) ? sizeof(SPECTRUM_BUS_FMT_T) : sizeof(ICAP_BUS_FMT);
		/* 4 bytes : 12 bytes */
		u4CpyLen = (u4RemainByte - u4FmtLen >= 0) ? u4FmtLen : u4RemainByte;

		if ((ucDstOffset + u4CpyLen) > sizeof(icapBusData)) {
			DBGLOG(INIT, ERROR,
			       "ucDstOffset(%u) + u4CpyLen(%u) exceed bound of icapBusData\n",
			       ucDstOffset, u4CpyLen);
			kfree(pucDataWF0);
			kfree(pucDataWF1);
			kfree(pucDataRAWWF0);
			kfree(pucDataRAWWF1);
			ASSERT(-1);
			return -1;
		}
		memcpy((UINT_8 *)&icapBusData + ucDstOffset, &prEventDumpMem->aucBuffer[0] + u4SrcOffset, u4CpyLen);
#if 0
		if (prEventDumpMem->eIcapContent == ICAP_CONTENT_ADC) {
			sprintf(aucDataWF0, "%8d,%8d\n", icapBusData.rAdcBusData.u4Dcoc0I,
				icapBusData.rAdcBusData.u4Dcoc0Q);
			sprintf(aucDataWF1, "%8d,%8d\n", icapBusData.rAdcBusData.u4Dcoc1I,
				icapBusData.rAdcBusData.u4Dcoc1Q);
		}
#endif
		if (prEventDumpMem->eIcapContent == ICAP_CONTENT_FIIQ ||
		    prEventDumpMem->eIcapContent == ICAP_CONTENT_FDIQ) {
			u4DataWLenF0 = scnprintf(pucDataWF0, u4DataWBufSize, "%8d,%8d\n",
						 icapBusData.rIqcBusData.u4Iqc0I,
						 icapBusData.rIqcBusData.u4Iqc0Q);
			u4DataWLenF1 = scnprintf(pucDataWF1, u4DataWBufSize, "%8d,%8d\n",
						 icapBusData.rIqcBusData.u4Iqc1I,
						 icapBusData.rIqcBusData.u4Iqc1Q);
		} else if (prEventDumpMem->eIcapContent - 1000 == ICAP_CONTENT_FIIQ
			   || prEventDumpMem->eIcapContent - 1000 == ICAP_CONTENT_FDIQ) {
			u4Iqc160WF0Q0 =
			    icapBusData.rIqc160BusData.u4Iqc0Q0P1 | (icapBusData.rIqc160BusData.u4Iqc0Q0P2 << 8);
			u4Iqc160WF1I1 =
			    icapBusData.rIqc160BusData.u4Iqc1I1P1 | (icapBusData.rIqc160BusData.u4Iqc1I1P2 << 4);

			u4DataWLenF0 = scnprintf(pucDataWF0, u4DataWBufSize, "%8d,%8d\n%8d,%8d\n",
						 icapBusData.rIqc160BusData.u4Iqc0I0, u4Iqc160WF0Q0,
						 icapBusData.rIqc160BusData.u4Iqc0I1,
						 icapBusData.rIqc160BusData.u4Iqc0Q1);

			u4DataWLenF1 = scnprintf(pucDataWF1, u4DataWBufSize, "%8d,%8d\n%8d,%8d\n",
						 icapBusData.rIqc160BusData.u4Iqc1I0,
						 icapBusData.rIqc160BusData.u4Iqc1Q0, u4Iqc160WF1I1,
						 icapBusData.rIqc160BusData.u4Iqc1Q1);

		} else if (prEventDumpMem->eIcapContent == ICAP_CONTENT_SPECTRUM) {
			u4DataWLenF0 = scnprintf(pucDataWF0, u4DataWBufSize, "%8d,%8d\n",
						 icapBusData.rSpectrumBusData.u4DcocI,
						 icapBusData.rSpectrumBusData.u4DcocQ);
		} else if (prEventDumpMem->eIcapContent == ICAP_CONTENT_ADC) {
			u4DataWLenF0 = scnprintf(pucDataWF0, u4DataWBufSize,
						 "%8d,%8d\n%8d,%8d\n%8d,%8d\n%8d,%8d\n%8d,%8d\n%8d,%8d\n",
						 icapBusData.rPackedAdcBusData.u4AdcI0T0,
						 icapBusData.rPackedAdcBusData.u4AdcQ0T0,
						 icapBusData.rPackedAdcBusData.u4AdcI0T1,
						 icapBusData.rPackedAdcBusData.u4AdcQ0T1,
						 icapBusData.rPackedAdcBusData.u4AdcI0T2,
						 icapBusData.rPackedAdcBusData.u4AdcQ0T2,
						 icapBusData.rPackedAdcBusData.u4AdcI0T3,
						 icapBusData.rPackedAdcBusData.u4AdcQ0T3,
						 icapBusData.rPackedAdcBusData.u4AdcI0T4,
						 icapBusData.rPackedAdcBusData.u4AdcQ0T4,
						 icapBusData.rPackedAdcBusData.u4AdcI0T5,
						 icapBusData.rPackedAdcBusData.u4AdcQ0T5);

			u4DataWLenF1 = scnprintf(pucDataWF1, u4DataWBufSize,
						 "%8d,%8d\n%8d,%8d\n%8d,%8d\n%8d,%8d\n%8d,%8d\n%8d,%8d\n",
						 icapBusData.rPackedAdcBusData.u4AdcI1T0,
						 icapBusData.rPackedAdcBusData.u4AdcQ1T0,
						 icapBusData.rPackedAdcBusData.u4AdcI1T1,
						 icapBusData.rPackedAdcBusData.u4AdcQ1T1,
						 icapBusData.rPackedAdcBusData.u4AdcI1T2,
						 icapBusData.rPackedAdcBusData.u4AdcQ1T2,
						 icapBusData.rPackedAdcBusData.u4AdcI1T3,
						 icapBusData.rPackedAdcBusData.u4AdcQ1T3,
						 icapBusData.rPackedAdcBusData.u4AdcI1T4,
						 icapBusData.rPackedAdcBusData.u4AdcQ1T4,
						 icapBusData.rPackedAdcBusData.u4AdcI1T5,
						 icapBusData.rPackedAdcBusData.u4AdcQ1T5);
		} else if (prEventDumpMem->eIcapContent - 2000 == ICAP_CONTENT_ADC) {
			u4DataWLenF0 = scnprintf(pucDataWF0, u4DataWBufSize, "%8d,%8d\n%8d,%8d\n%8d,%8d\n",
						 icapBusData.rPackedAdcBusData.u4AdcI0T0,
						 icapBusData.rPackedAdcBusData.u4AdcQ0T0,
						 icapBusData.rPackedAdcBusData.u4AdcI0T1,
						 icapBusData.rPackedAdcBusData.u4AdcQ0T1,
						 icapBusData.rPackedAdcBusData.u4AdcI0T2,
						 icapBusData.rPackedAdcBusData.u4AdcQ0T2);

			u4DataWLenF1 = scnprintf(pucDataWF1, u4DataWBufSize, "%8d,%8d\n%8d,%8d\n%8d,%8d\n",
						 icapBusData.rPackedAdcBusData.u4AdcI1T0,
						 icapBusData.rPackedAdcBusData.u4AdcQ1T0,
						 icapBusData.rPackedAdcBusData.u4AdcI1T1,
						 icapBusData.rPackedAdcBusData.u4AdcQ1T1,
						 icapBusData.rPackedAdcBusData.u4AdcI1T2,
						 icapBusData.rPackedAdcBusData.u4AdcQ1T2);
		} else if (prEventDumpMem->eIcapContent == ICAP_CONTENT_TOAE) {
			/* actually, this is DCOC. we take TOAE as DCOC */
			u4DataWLenF0 = scnprintf(pucDataWF0, u4DataWBufSize, "%8d,%8d\n",
						 icapBusData.rAdcBusData.u4Dcoc0I, icapBusData.rAdcBusData.u4Dcoc0Q);
			u4DataWLenF1 = scnprintf(pucDataWF1, u4DataWBufSize, "%8d,%8d\n",
						 icapBusData.rAdcBusData.u4Dcoc1I, icapBusData.rAdcBusData.u4Dcoc1Q);
		}
		if (u4CpyLen == u4FmtLen) {	/* the data format is complete */
			kalWriteToFile(aucPathWF0, fgAppend, pucDataWF0, u4DataWLenF0);
			kalWriteToFile(aucPathWF1, fgAppend, pucDataWF1, u4DataWLenF1);
		}
		ptr = (PUINT_32)(&prEventDumpMem->aucBuffer[0] + u4SrcOffset);
		u4DataRAWWLenF0 = scnprintf(pucDataRAWWF0, u4DataWBufSize, "%08x%08x%08x\n",
					    *(ptr + 2), *(ptr + 1), *ptr);
		kalWriteToFile(aucPathRAWWF0, fgAppend, pucDataRAWWF0, u4DataRAWWLenF0);
		kalWriteToFile(aucPathRAWWF1, fgAppend, pucDataRAWWF1, u4DataRAWWLenF1);

		u4RemainByte -= u4CpyLen;
		u4SrcOffset += u4CpyLen;	/* shift offset */
		ucDstOffset = 0;	/* only use ucDstOffset at first packet for align 2KB */
	}
	/* if this is a last packet, we can't transfer the remain data.
	 *  bcs we can't guarantee the data is complete align data format
	 */
	if (u4CpyLen != u4FmtLen) {	/* the data format is complete */
		ucDstOffset = u4CpyLen;	/* not align 2KB, keep the data and next packet data will append it */
	}

	kfree(pucDataWF0);
	kfree(pucDataWF1);
	kfree(pucDataRAWWF0);
	kfree(pucDataRAWWF1);

	if (u4RemainByte < 0) {
		ASSERT(-1);
		return -1;
	}

	return 0;
}
#endif /* CFG_SUPPORT_QA_TOOL */

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
VOID nicCmdEventQueryCalBackupV2(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_PARAM_CAL_BACKUP_STRUCT_V2_T prCalBackupDataV2Info;
	P_CMD_CAL_BACKUP_STRUCT_V2_T prEventCalBackupDataV2;
	UINT_32 u4QueryInfoLen, u4QueryInfo, u4TempAddress;
	P_GLUE_INFO_T prGlueInfo;

	DBGLOG(RFTEST, INFO, "%s\n", __func__);

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);


	prGlueInfo = prAdapter->prGlueInfo;
	prEventCalBackupDataV2 = (P_CMD_CAL_BACKUP_STRUCT_V2_T) (pucEventBuf);

	u4QueryInfoLen = sizeof(CMD_CAL_BACKUP_STRUCT_V2_T);

	prCalBackupDataV2Info = (P_PARAM_CAL_BACKUP_STRUCT_V2_T) prCmdInfo->pvInformationBuffer;
#if 0
	DBGLOG(RFTEST, INFO, "============ Receive a Cal Data EVENT (Info) ============\n");
	DBGLOG(RFTEST, INFO, "Reason = %d\n", prEventCalBackupDataV2->ucReason);
	DBGLOG(RFTEST, INFO, "Action = %d\n", prEventCalBackupDataV2->ucAction);
	DBGLOG(RFTEST, INFO, "NeedResp = %d\n", prEventCalBackupDataV2->ucNeedResp);
	DBGLOG(RFTEST, INFO, "FragNum = %d\n", prEventCalBackupDataV2->ucFragNum);
	DBGLOG(RFTEST, INFO, "RomRam = %d\n", prEventCalBackupDataV2->ucRomRam);
	DBGLOG(RFTEST, INFO, "ThermalValue = %d\n", prEventCalBackupDataV2->u4ThermalValue);
	DBGLOG(RFTEST, INFO, "Address = 0x%08x\n", prEventCalBackupDataV2->u4Address);
	DBGLOG(RFTEST, INFO, "Length = %d\n", prEventCalBackupDataV2->u4Length);
	DBGLOG(RFTEST, INFO, "RemainLength = %d\n", prEventCalBackupDataV2->u4RemainLength);
	DBGLOG(RFTEST, INFO, "=========================================================\n");
#endif

	if (prEventCalBackupDataV2->ucReason == 0 && prEventCalBackupDataV2->ucAction == 0) {
		DBGLOG(RFTEST, INFO, "Received an EVENT for Query Thermal Temp.\n");
		prCalBackupDataV2Info->u4ThermalValue = prEventCalBackupDataV2->u4ThermalValue;
		g_rBackupCalDataAllV2.u4ThermalInfo = prEventCalBackupDataV2->u4ThermalValue;
		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	} else if (prEventCalBackupDataV2->ucReason == 0 && prEventCalBackupDataV2->ucAction == 1) {
		DBGLOG(RFTEST, INFO, "Received an EVENT for Query Total Cal Data Length.\n");
		prCalBackupDataV2Info->u4Length = prEventCalBackupDataV2->u4Length;

		if (prEventCalBackupDataV2->ucRomRam == 0)
			g_rBackupCalDataAllV2.u4ValidRomCalDataLength = prEventCalBackupDataV2->u4Length;
		else if (prEventCalBackupDataV2->ucRomRam == 1)
			g_rBackupCalDataAllV2.u4ValidRamCalDataLength = prEventCalBackupDataV2->u4Length;

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	} else if (prEventCalBackupDataV2->ucReason == 2 && prEventCalBackupDataV2->ucAction == 4) {
		DBGLOG(RFTEST, INFO, "Received an EVENT for Query All Cal (%s) Data. FragNum = %d\n",
			prCalBackupDataV2Info->ucRomRam == 0 ? "ROM" : "RAM",
			prEventCalBackupDataV2->ucFragNum);
		prCalBackupDataV2Info->u4Address = prEventCalBackupDataV2->u4Address;
		prCalBackupDataV2Info->u4Length = prEventCalBackupDataV2->u4Length;
		prCalBackupDataV2Info->u4RemainLength = prEventCalBackupDataV2->u4RemainLength;
		prCalBackupDataV2Info->ucFragNum = prEventCalBackupDataV2->ucFragNum;

		/* Copy Cal Data From FW to Driver Array */
		if (prEventCalBackupDataV2->ucRomRam == 0) {
			u4TempAddress = prEventCalBackupDataV2->u4Address;
			kalMemCopy((PUINT_8)(g_rBackupCalDataAllV2.au4RomCalData) + u4TempAddress,
			(PUINT_8)(prEventCalBackupDataV2->au4Buffer),
			prEventCalBackupDataV2->u4Length);
		} else if (prEventCalBackupDataV2->ucRomRam == 1) {
			u4TempAddress = prEventCalBackupDataV2->u4Address;
			kalMemCopy((PUINT_8)(g_rBackupCalDataAllV2.au4RamCalData) + u4TempAddress,
			(PUINT_8)(prEventCalBackupDataV2->au4Buffer),
			prEventCalBackupDataV2->u4Length);
		}

		if (prEventCalBackupDataV2->u4Address == 0xFFFFFFFF) {
			DBGLOG(RFTEST, INFO, "RLM CMD : Address Error!!!!!!!!!!!\n");
		} else if (prEventCalBackupDataV2->u4RemainLength == 0
					&& prEventCalBackupDataV2->ucRomRam == 1) {
			DBGLOG(RFTEST, INFO, "RLM CMD : Get Cal Data from FW (%s). Finish!!!!!!!!!!!\n",
				prCalBackupDataV2Info->ucRomRam == 0 ? "ROM" : "RAM");
		} else if (prEventCalBackupDataV2->u4RemainLength == 0
					&& prEventCalBackupDataV2->ucRomRam == 0) {
			DBGLOG(RFTEST, INFO, "RLM CMD : Get Cal Data from FW (%s). Finish!!!!!!!!!!!\n",
				prCalBackupDataV2Info->ucRomRam == 0 ? "ROM" : "RAM");
			prCalBackupDataV2Info->ucFragNum = 0;
			prCalBackupDataV2Info->ucRomRam = 1;
			prCalBackupDataV2Info->u4ThermalValue = 0;
			prCalBackupDataV2Info->u4Address = 0;
			prCalBackupDataV2Info->u4Length = 0;
			prCalBackupDataV2Info->u4RemainLength = 0;
			DBGLOG(RFTEST, INFO, "RLM CMD : Get Cal Data from FW (%s). Start!!!!!!!!!!!!!!!!\n",
				prCalBackupDataV2Info->ucRomRam == 0 ? "ROM" : "RAM");
			DBGLOG(RFTEST, INFO, "Thermal Temp = %d\n", g_rBackupCalDataAllV2.u4ThermalInfo);
			wlanoidQueryCalBackupV2(prAdapter,
				prCalBackupDataV2Info,
				sizeof(PARAM_CAL_BACKUP_STRUCT_V2_T),
				&u4QueryInfo);
		} else {
			wlanoidSendCalBackupV2Cmd(prAdapter,
				   prCmdInfo->pvInformationBuffer, prCmdInfo->u4InformationBufferLength);
		}
	} else if (prEventCalBackupDataV2->ucReason == 3 && prEventCalBackupDataV2->ucAction == 5) {
		DBGLOG(RFTEST, INFO,
			"Received an EVENT for Send All Cal Data. FragNum = %d\n",
			prEventCalBackupDataV2->ucFragNum);
		prCalBackupDataV2Info->u4Address = prEventCalBackupDataV2->u4Address;
		prCalBackupDataV2Info->u4Length = prEventCalBackupDataV2->u4Length;
		prCalBackupDataV2Info->u4RemainLength = prEventCalBackupDataV2->u4RemainLength;
		prCalBackupDataV2Info->ucFragNum = prEventCalBackupDataV2->ucFragNum;

		if (prEventCalBackupDataV2->u4RemainLength == 0
			|| prEventCalBackupDataV2->u4Address == 0xFFFFFFFF) {
			kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
		} else {
			wlanoidSendCalBackupV2Cmd(prAdapter,
				   prCmdInfo->pvInformationBuffer, prCmdInfo->u4InformationBufferLength);
		}
	} else {
		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}

}
#endif

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
	P_EVENT_DUMP_MEM_T prEventDumpMem;
	static UINT_8 aucPath[256];
	static UINT_8 aucPath_done[300];
	static UINT_32 u4CurTimeTick;

	ASSERT(prAdapter);
	ASSERT(pucEventBuf);

	snprintf(aucPath, sizeof(aucPath), "/dump_%05hu.hex", g_u2DumpIndex);

	prEventDumpMem = (P_EVENT_DUMP_MEM_T) (pucEventBuf);

	if (kalCheckPath(aucPath) == -1) {
		kalMemSet(aucPath, 0x00, 256);
		snprintf(aucPath, sizeof(aucPath),
				"/data/dump_%05hu.hex", g_u2DumpIndex);
	}

	if (prEventDumpMem->ucFragNum == 1) {
		/* Store memory dump into sdcard,
		 * path /sdcard/dump_<current  system tick>_<memory address>_<memory length>.hex
		 */
		u4CurTimeTick = kalGetTimeTick();
#if defined(LINUX)

		/*if blbist mkdir undre /data/blbist, the dump files wouls put on it */
		snprintf(aucPath, sizeof(aucPath),
				"/dump_%05hu.hex", g_u2DumpIndex);
		if (kalCheckPath(aucPath) == -1) {
			kalMemSet(aucPath, 0x00, 256);
			snprintf(aucPath, sizeof(aucPath),
				"/data/dump_%05hu.hex", g_u2DumpIndex);
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
#if CFG_SUPPORT_QA_TOOL
	TsfRawData2IqFmt(prEventDumpMem);
#endif /* CFG_SUPPORT_QA_TOOL */
	DBGLOG(INIT, INFO,
	       "iCap : ==> (u4RemainLength = %x, u4Address=%x )\n", prEventDumpMem->u4RemainLength,
	       prEventDumpMem->u4Address);

	if (prEventDumpMem->u4RemainLength == 0 || prEventDumpMem->u4Address == 0xFFFFFFFF) {

		/* The request is finished or firmware response a error */
		/* Reply time tick to iwpriv */

		g_bIcapEnable = FALSE;
		g_bCaptureDone = TRUE;

		snprintf(aucPath_done, sizeof(aucPath_done), "/file_dump_done.txt");
		if (kalCheckPath(aucPath_done) == -1) {
			kalMemSet(aucPath_done, 0x00, 256);
			snprintf(aucPath_done, sizeof(aucPath_done), "/data/file_dump_done.txt");
		}
		DBGLOG(INIT, INFO, ": ==> gen done_file\n");
		kalWriteToFile(aucPath_done, FALSE, aucPath_done, sizeof(aucPath_done));
#if CFG_SUPPORT_QA_TOOL
		g_au4Offset[0][0] = 0;
		g_au4Offset[0][1] = 9;
		g_au4Offset[1][0] = 0;
		g_au4Offset[1][1] = 9;
#endif /* CFG_SUPPORT_QA_TOOL */

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
	UINT_32 u4QueryInfoLen;
	P_GLUE_INFO_T prGlueInfo;
	P_EVENT_DUMP_MEM_T prEventDumpMem;
	static UINT_8 aucPath[256];
/*	static UINT_8 aucPath_done[300]; */
	static UINT_32 u4CurTimeTick;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (1) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEventDumpMem = (P_EVENT_DUMP_MEM_T) (pucEventBuf);

		u4QueryInfoLen = sizeof(P_PARAM_CUSTOM_MEM_DUMP_STRUCT_T);

		if (prEventDumpMem->ucFragNum == 1) {
			/* Store memory dump into sdcard,
			 * path /sdcard/dump_<current  system tick>_<memory address>_<memory length>.hex
			 */
			u4CurTimeTick = kalGetTimeTick();
#if defined(LINUX)

			/* PeiHsuan add for avoiding out of memory 20160801 */
			if (g_u2DumpIndex >= 20)
				g_u2DumpIndex = 0;

			/*if blbist mkdir undre /data/blbist, the dump files wouls put on it */
			snprintf(aucPath, sizeof(aucPath),
				"/dump_%05hu.hex", g_u2DumpIndex);
			if (kalCheckPath(aucPath) == -1) {
				kalMemSet(aucPath, 0x00, 256);
				snprintf(aucPath, sizeof(aucPath),
					"/data/dump_%05hu.hex", g_u2DumpIndex);
			} else
				kalTrunkPath(aucPath);

			DBGLOG(INIT, INFO,
				"iCap Create New Dump File dump_%05hu.hex\n",
				g_u2DumpIndex);
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
#if CFG_SUPPORT_QA_TOOL
		TsfRawData2IqFmt(prEventDumpMem);
#endif /* CFG_SUPPORT_QA_TOOL */
		if (prEventDumpMem->u4RemainLength == 0 || prEventDumpMem->u4Address == 0xFFFFFFFF) {
			/* The request is finished or firmware response a error */
			/* Reply time tick to iwpriv */
			if (prCmdInfo->fgIsOid) {

				/* the oid would be complete only in oid-trigger  mode,
				 * that is no need to if the event-trigger
				 */
				if (g_bIcapEnable == FALSE) {
					*((PUINT_32) prCmdInfo->pvInformationBuffer) = u4CurTimeTick;
					kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery,
						       u4QueryInfoLen, WLAN_STATUS_SUCCESS);
				}
			}
			g_bIcapEnable = FALSE;
			g_bCaptureDone = TRUE;
#if defined(LINUX)

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

VOID nicEventHifCtrl(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
#if defined(_HIF_USB)
	P_EVENT_HIF_CTRL_T prEventHifCtrl;

	prEventHifCtrl = (P_EVENT_HIF_CTRL_T) (prEvent->aucBuffer);

	DBGLOG(HAL, INFO, "%s: EVENT_ID_HIF_CTRL\n", __func__);
	DBGLOG(HAL, INFO, "prEventHifCtrl->ucHifType = %hhu\n", prEventHifCtrl->ucHifType);
	DBGLOG(HAL, INFO, "prEventHifCtrl->ucHifTxTrafficStatus, prEventHifCtrl->ucHifRxTrafficStatus = %hhu, %hhu\n",
	       prEventHifCtrl->ucHifTxTrafficStatus, prEventHifCtrl->ucHifRxTrafficStatus);

	if (prEventHifCtrl->ucHifTxTrafficStatus == ENUM_HIF_TRAFFIC_IDLE &&
	    prEventHifCtrl->ucHifRxTrafficStatus == ENUM_HIF_TRAFFIC_IDLE) {	/* success */
		halUSBPreSuspendDone(prAdapter, NULL, prEvent->aucBuffer);
	} else if (prEventHifCtrl->ucHifTxTrafficStatus == ENUM_HIF_TRAFFIC_BUSY ||
		   prEventHifCtrl->ucHifRxTrafficStatus == ENUM_HIF_TRAFFIC_BUSY) {	/* busy */
		halUSBPreSuspendTimeout(prAdapter, NULL);
	} else if (prEventHifCtrl->ucHifTxTrafficStatus == ENUM_HIF_TRAFFIC_INVALID ||
		   prEventHifCtrl->ucHifRxTrafficStatus == ENUM_HIF_TRAFFIC_INVALID) {	/* invalid */
		halUSBPreSuspendTimeout(prAdapter, NULL);
	}
#endif
}

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
	ASSERT(prCmdInfo->pvInformationBuffer);


	prGlueInfo = prAdapter->prGlueInfo;
	prEvent = (P_EVENT_STA_STATISTICS_T) pucEventBuf;
	prStaStatistics =
		(P_PARAM_GET_STA_STATISTICS) prCmdInfo->pvInformationBuffer;

	u4QueryInfoLen = sizeof(PARAM_GET_STA_STA_STATISTICS);

	/* Statistics from FW is valid */
	if (prEvent->u4Flags & BIT(0)) {
		prStaStatistics->ucPer = prEvent->ucPer;
		prStaStatistics->ucRcpi = prEvent->ucRcpi;
		prStaStatistics->u4PhyMode = prEvent->u4PhyMode;
		prStaStatistics->u2LinkSpeed = prEvent->u2LinkSpeed;

		prStaStatistics->u4TxFailCount = prEvent->u4TxFailCount;
		prStaStatistics->u4TxLifeTimeoutCount =
			prEvent->u4TxLifeTimeoutCount;
		prStaStatistics->u4TransmitCount = prEvent->u4TransmitCount;
		prStaStatistics->u4TransmitFailCount =
			prEvent->u4TransmitFailCount;
		prStaStatistics->u4Rate1TxCnt = prEvent->u4Rate1TxCnt;
		prStaStatistics->u4Rate1FailCnt = prEvent->u4Rate1FailCnt;

		prStaStatistics->ucTemperature = prEvent->ucTemperature;
		prStaStatistics->ucSkipAr = prEvent->ucSkipAr;
		prStaStatistics->ucArTableIdx = prEvent->ucArTableIdx;
		prStaStatistics->ucRateEntryIdx = prEvent->ucRateEntryIdx;
		prStaStatistics->ucRateEntryIdxPrev =
			prEvent->ucRateEntryIdxPrev;
		prStaStatistics->ucTxSgiDetectPassCnt =
			prEvent->ucTxSgiDetectPassCnt;
		prStaStatistics->ucAvePer = prEvent->ucAvePer;
		kalMemCopy(prStaStatistics->aucArRatePer, prEvent->aucArRatePer,
			sizeof(prEvent->aucArRatePer));
		kalMemCopy(prStaStatistics->aucRateEntryIndex,
			prEvent->aucRateEntryIndex,
			sizeof(prEvent->aucRateEntryIndex));
		prStaStatistics->ucArStateCurr = prEvent->ucArStateCurr;
		prStaStatistics->ucArStatePrev = prEvent->ucArStatePrev;
		prStaStatistics->ucArActionType = prEvent->ucArActionType;
		prStaStatistics->ucHighestRateCnt = prEvent->ucHighestRateCnt;
		prStaStatistics->ucLowestRateCnt = prEvent->ucLowestRateCnt;
		prStaStatistics->u2TrainUp = prEvent->u2TrainUp;
		prStaStatistics->u2TrainDown = prEvent->u2TrainDown;
		kalMemCopy(&prStaStatistics->rTxVector, &prEvent->rTxVector,
			sizeof(prEvent->rTxVector));
		kalMemCopy(&prStaStatistics->rMibInfo, &prEvent->rMibInfo,
			sizeof(prEvent->rMibInfo));
		prStaStatistics->fgIsForceTxStream = prEvent->fgIsForceTxStream;
		prStaStatistics->fgIsForceSeOff = prEvent->fgIsForceSeOff;

		prStaRec = cnmGetStaRecByIndex(prAdapter, prEvent->ucStaRecIdx);

		if (prStaRec) {
			/*link layer statistics */
			for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
				prStaStatistics->arLinkStatistics[eAci].
				u4TxFailMsdu =
				prEvent->arLinkStatistics[eAci].u4TxFailMsdu;

				prStaStatistics->arLinkStatistics[eAci].
				u4TxRetryMsdu =
				prEvent->arLinkStatistics[eAci].u4TxRetryMsdu;

				/*for dump bss statistics */
				prStaRec->arLinkStatistics[eAci].u4TxFailMsdu =
				prEvent->arLinkStatistics[eAci].u4TxFailMsdu;
				prStaRec->arLinkStatistics[eAci].u4TxRetryMsdu =
				prEvent->arLinkStatistics[eAci].u4TxRetryMsdu;
			}
		}
		if (prEvent->u4TxCount) {
			UINT_32 u4TxDoneAirTimeMs =
				USEC_TO_MSEC(prEvent->u4TxDoneAirTime * 32);

			prStaStatistics->u4TxAverageAirTime =
				(u4TxDoneAirTimeMs / prEvent->u4TxCount);
		} else {
			prStaStatistics->u4TxAverageAirTime = 0;
		}
	}

	if (prCmdInfo->fgIsOid)
		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery,
			u4QueryInfoLen, WLAN_STATUS_SUCCESS);
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
			for (ucIdx = 0; ucIdx < NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_MAX; ucIdx++) {
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

VOID nicEventRddPulseDump(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf)
{
	UINT_16 u2Idx, u2PulseCnt;
	P_EVENT_WIFI_RDD_TEST_T prRddPulseEvent;

	ASSERT(prAdapter);
	ASSERT(pucEventBuf);

	prRddPulseEvent = (P_EVENT_WIFI_RDD_TEST_T) (pucEventBuf);

	u2PulseCnt = (prRddPulseEvent->u4FuncLength - RDD_EVENT_HDR_SIZE) / RDD_ONEPLUSE_SIZE;

	DBGLOG(INIT, INFO, "[RDD]0x%08x %08d[RDD%d]\n", prRddPulseEvent->u4Prefix
			, prRddPulseEvent->u4Count, prRddPulseEvent->ucRddIdx);

	for (u2Idx = 0; u2Idx < u2PulseCnt; u2Idx++) {
		DBGLOG(INIT, INFO, "[RDD]0x%02x%02x%02x%02x %02x%02x%02x%02x[RDD%d]\n"
			, prRddPulseEvent->aucBuffer[RDD_ONEPLUSE_SIZE*u2Idx+RDD_PULSE_OFFSET3]
			, prRddPulseEvent->aucBuffer[RDD_ONEPLUSE_SIZE*u2Idx+RDD_PULSE_OFFSET2]
			, prRddPulseEvent->aucBuffer[RDD_ONEPLUSE_SIZE*u2Idx+RDD_PULSE_OFFSET1]
			, prRddPulseEvent->aucBuffer[RDD_ONEPLUSE_SIZE*u2Idx+RDD_PULSE_OFFSET0]
			, prRddPulseEvent->aucBuffer[RDD_ONEPLUSE_SIZE*u2Idx+RDD_PULSE_OFFSET7]
			, prRddPulseEvent->aucBuffer[RDD_ONEPLUSE_SIZE*u2Idx+RDD_PULSE_OFFSET6]
			, prRddPulseEvent->aucBuffer[RDD_ONEPLUSE_SIZE*u2Idx+RDD_PULSE_OFFSET5]
			, prRddPulseEvent->aucBuffer[RDD_ONEPLUSE_SIZE*u2Idx+RDD_PULSE_OFFSET4]
			, prRddPulseEvent->ucRddIdx
			);
	}

	DBGLOG(INIT, INFO, "[RDD]0x%08x %08x[RDD%d]\n", prRddPulseEvent->u4SubBandRssi0
			, prRddPulseEvent->u4SubBandRssi1, prRddPulseEvent->ucRddIdx);

}


#if CFG_SUPPORT_ADVANCE_CONTROL
VOID nicCmdEventQueryAdvCtrl(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	PUINT_8 query;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 query_len;
	P_CMD_ADV_CONFIG_HEADER_T hdr;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	if (!pucEventBuf) {
		DBGLOG(REQ, ERROR, "pucEventBuf is null.\n");
		return;
	}
	hdr = (P_CMD_ADV_CONFIG_HEADER_T) pucEventBuf;
	DBGLOG(REQ, LOUD, "%s type %x len %d>\n", __func__, hdr->u2Type, hdr->u2Len);

	prGlueInfo = prAdapter->prGlueInfo;
	query_len = hdr->u2Len;
	query = prCmdInfo->pvInformationBuffer;
	if (query && (query_len == prCmdInfo->u4InformationBufferLength))
		kalMemCopy(query, hdr, query_len);
	else
		DBGLOG(REQ, LOUD, "%s type %x, len %d != buflen %d>\n",
			 __func__, hdr->u2Type, hdr->u2Len,
			prCmdInfo->u4InformationBufferLength);

	if (prCmdInfo->fgIsOid)
		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery,
			query_len, WLAN_STATUS_SUCCESS);
}
#endif

#if CFG_SUPPORT_MSP
VOID nicCmdEventQueryWlanInfo(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_PARAM_HW_WLAN_INFO_T prWlanInfo;
	P_EVENT_WLAN_INFO prEventWlanInfo;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventWlanInfo = (P_EVENT_WLAN_INFO) pucEventBuf;

	DBGLOG(RSN, INFO, "MT6632 : nicCmdEventQueryWlanInfo\n");

	prGlueInfo = prAdapter->prGlueInfo;

	u4QueryInfoLen = sizeof(PARAM_HW_WLAN_INFO_T);
	prWlanInfo = (P_PARAM_HW_WLAN_INFO_T) prCmdInfo->pvInformationBuffer;

	/* prWlanInfo->u4Length = sizeof(PARAM_HW_WLAN_INFO_T); */
	if (prEventWlanInfo && prWlanInfo) {
		kalMemCopy(&prWlanInfo->rWtblTxConfig,
			   &prEventWlanInfo->rWtblTxConfig,
			   sizeof(PARAM_TX_CONFIG_T));
		kalMemCopy(&prWlanInfo->rWtblSecConfig,
			   &prEventWlanInfo->rWtblSecConfig,
			   sizeof(PARAM_SEC_CONFIG_T));
		kalMemCopy(&prWlanInfo->rWtblKeyConfig,
			   &prEventWlanInfo->rWtblKeyConfig,
			   sizeof(PARAM_KEY_CONFIG_T));
		kalMemCopy(&prWlanInfo->rWtblRateInfo,
			   &prEventWlanInfo->rWtblRateInfo,
			   sizeof(PARAM_PEER_RATE_INFO_T));
		kalMemCopy(&prWlanInfo->rWtblBaConfig,
			   &prEventWlanInfo->rWtblBaConfig,
			   sizeof(PARAM_PEER_BA_CONFIG_T));
		kalMemCopy(&prWlanInfo->rWtblPeerCap,
			   &prEventWlanInfo->rWtblPeerCap,
			   sizeof(PARAM_PEER_CAP_T));
		kalMemCopy(&prWlanInfo->rWtblRxCounter,
			   &prEventWlanInfo->rWtblRxCounter,
			   sizeof(PARAM_PEER_RX_COUNTER_ALL_T));
		kalMemCopy(&prWlanInfo->rWtblTxCounter,
				&prEventWlanInfo->rWtblTxCounter,
				sizeof(PARAM_PEER_TX_COUNTER_ALL_T));
	}

	if (prCmdInfo->fgIsOid)
		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery,
			u4QueryInfoLen, WLAN_STATUS_SUCCESS);
}


VOID nicCmdEventQueryMibInfo(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_PARAM_HW_MIB_INFO_T prMibInfo;
	P_EVENT_MIB_INFO prEventMibInfo;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventMibInfo = (P_EVENT_MIB_INFO) pucEventBuf;

	DBGLOG(RSN, INFO, "MT6632 : nicCmdEventQueryMibInfo\n");

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		u4QueryInfoLen = sizeof(PARAM_HW_MIB_INFO_T);
		prMibInfo = (P_PARAM_HW_MIB_INFO_T) prCmdInfo->pvInformationBuffer;
		if (prEventMibInfo && prMibInfo) {
			kalMemCopy(&prMibInfo->rHwMibCnt,
				   &prEventMibInfo->rHwMibCnt,
				   sizeof(HW_MIB_COUNTER_T));
			kalMemCopy(&prMibInfo->rHwMib2Cnt,
				   &prEventMibInfo->rHwMib2Cnt,
				   sizeof(HW_MIB2_COUNTER_T));
			kalMemCopy(&prMibInfo->rHwTxAmpduMts,
				   &prEventMibInfo->rHwTxAmpduMts,
				   sizeof(HW_TX_AMPDU_METRICS_T));
		}
		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}
#endif

#if CFG_SUPPORT_LAST_SEC_MCS_INFO
VOID nicCmdEventTxMcsInfo(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_GLUE_INFO_T prGlueInfo;
	struct EVENT_TX_MCS_INFO *prTxMcsEvent;
	struct PARAM_TX_MCS_INFO *prTxMcsInfo;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);
	ASSERT(prCmdInfo->pvInformationBuffer);

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prTxMcsEvent = (struct EVENT_TX_MCS_INFO *) pucEventBuf;
		prTxMcsInfo = (struct PARAM_TX_MCS_INFO *) prCmdInfo->pvInformationBuffer;

		u4QueryInfoLen = sizeof(struct EVENT_TX_MCS_INFO);

		kalMemCopy(prTxMcsInfo->au2TxRateCode, prTxMcsEvent->au2TxRateCode,
				sizeof(prTxMcsEvent->au2TxRateCode));
		kalMemCopy(prTxMcsInfo->aucTxRatePer, prTxMcsEvent->aucTxRatePer,
				sizeof(prTxMcsEvent->aucTxRatePer));

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}
#endif

#if CFG_TCP_IP_CHKSUM_OFFLOAD
WLAN_STATUS nicCmdEventQueryNicCsumOffload(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf)
{
	P_NIC_CSUM_OFFLOAD_T prChecksumOffload = (P_NIC_CSUM_OFFLOAD_T)pucEventBuf;

	prAdapter->fgIsSupportCsumOffload = prChecksumOffload->ucIsSupportCsumOffload;

	DBGLOG(INIT, INFO, "nicCmdEventQueryNicCsumOffload: ucIsSupportCsumOffload = %x\n",
						prAdapter->fgIsSupportCsumOffload);

	return WLAN_STATUS_SUCCESS;
}
#endif

WLAN_STATUS nicCmdEventQueryNicCoexFeature(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf)
{
	P_NIC_COEX_FEATURE_T prCoexFeature = (P_NIC_COEX_FEATURE_T)pucEventBuf;

	prAdapter->u4FddMode = prCoexFeature->u4FddMode;

	DBGLOG(INIT, INFO, "nicCmdEventQueryNicCoexFeature: u4FddMode = %x\n",
						prAdapter->u4FddMode);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS nicCmdEventQueryNicEfuseAddr(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf)
{
	P_NIC_EFUSE_ADDRESS_T prTxResource = (P_NIC_EFUSE_ADDRESS_T)pucEventBuf;

	prAdapter->u4EfuseStartAddress = prTxResource->u4EfuseStartAddress;
	prAdapter->u4EfuseEndAddress = prTxResource->u4EfuseEndAddress;

	DBGLOG(INIT, INFO, "nicCmdEventQueryNicEfuseAddr: u4EfuseStartAddress = %x\n",
						prAdapter->u4EfuseStartAddress);
	DBGLOG(INIT, INFO, "nicCmdEventQueryNicEfuseAddr: u4EfuseEndAddress = %x\n",
						prAdapter->u4EfuseEndAddress);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS nicCmdEventQueryEfuseOffset(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf)
{
	struct _NIC_EFUSE_OFFSET_T *prEfuseOffset = (struct _NIC_EFUSE_OFFSET_T *)pucEventBuf;

	if (prEfuseOffset->u4TotalItem > 0)
		prAdapter->u4EfuseMacAddrOffset = prEfuseOffset->u4WlanMacAddr;

	return WLAN_STATUS_SUCCESS;
}


WLAN_STATUS nicCmdEventQueryRModeCapability(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf)
{
	struct _CAP_R_MODE_CAP_T *prRModeOffset = (struct _CAP_R_MODE_CAP_T *)pucEventBuf;

	prAdapter->ucRModeOnlyFlag = prRModeOffset->ucRModeOnlyFlag;

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS nicCmdEventQueryNicTxResource(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf)
{
	P_NIC_TX_RESOURCE_T prTxResource = (P_NIC_TX_RESOURCE_T)pucEventBuf;

	prAdapter->fgIsNicTxReousrceValid = TRUE;
	prAdapter->nicTxReousrce.u4McuTotalResource = prTxResource->u4McuTotalResource;
	prAdapter->nicTxReousrce.u4McuResourceUnit = prTxResource->u4McuResourceUnit;
	prAdapter->nicTxReousrce.u4LmacTotalResource = prTxResource->u4LmacTotalResource;
	prAdapter->nicTxReousrce.u4LmacResourceUnit = prTxResource->u4LmacResourceUnit;

	DBGLOG(INIT, INFO, "nicCmdEventQueryNicTxResource: u4McuTotalResource = %x\n",
						prAdapter->nicTxReousrce.u4McuTotalResource);
	DBGLOG(INIT, INFO, "nicCmdEventQueryNicTxResource: u4McuResourceUnit = %x\n",
						prAdapter->nicTxReousrce.u4McuResourceUnit);
	DBGLOG(INIT, INFO, "nicCmdEventQueryNicTxResource: u4LmacTotalResource = %x\n",
						prAdapter->nicTxReousrce.u4LmacTotalResource);
	DBGLOG(INIT, INFO, "nicCmdEventQueryNicTxResource: u4LmacResourceUnit = %x\n",
						prAdapter->nicTxReousrce.u4LmacResourceUnit);

	return WLAN_STATUS_SUCCESS;
}

VOID nicCmdEventQueryNicCapabilityV2(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf)
{
	P_EVENT_NIC_CAPABILITY_V2_T prEventNicV2 = (P_EVENT_NIC_CAPABILITY_V2_T)pucEventBuf;
	P_NIC_CAPABILITY_V2_ELEMENT prElement;
	UINT_32 tag_idx, table_idx, offset;

	offset = 0;

	/* process each element */
	for (tag_idx = 0; tag_idx < prEventNicV2->u2TotalElementNum; tag_idx++) {

		prElement = (P_NIC_CAPABILITY_V2_ELEMENT)(prEventNicV2->aucBuffer + offset);

		for (table_idx = 0;
			 table_idx < (sizeof(gNicCapabilityV2InfoTable)/sizeof(NIC_CAPABILITY_V2_REF_TABLE_T));
			 table_idx++) {

			/* find the corresponding tag's handler */
			if (gNicCapabilityV2InfoTable[table_idx].tag_type == prElement->tag_type) {
				gNicCapabilityV2InfoTable[table_idx].hdlr(prAdapter, prElement->aucbody);
				break;
			}
		}

		/* move to the next tag */
		offset += prElement->body_len + (UINT_16) OFFSET_OF(NIC_CAPABILITY_V2_ELEMENT, aucbody);
	}

}

VOID nicEventLinkQuality(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_CMD_INFO_T prCmdInfo;

#if CFG_ENABLE_WIFI_DIRECT && CFG_SUPPORT_P2P_RSSI_QUERY
	if (prEvent->u2PacketLen == EVENT_HDR_WITHOUT_RXD_SIZE + sizeof(EVENT_LINK_QUALITY_EX)) {
		P_EVENT_LINK_QUALITY_EX prLqEx = (P_EVENT_LINK_QUALITY_EX) (prEvent->aucBuffer);

		if (prLqEx->ucIsLQ0Rdy)
			nicUpdateLinkQuality(prAdapter, 0, (P_EVENT_LINK_QUALITY) prLqEx);
		if (prLqEx->ucIsLQ1Rdy)
			nicUpdateLinkQuality(prAdapter, 1, (P_EVENT_LINK_QUALITY) prLqEx);
	} else {
		/* For old FW, P2P may invoke link quality query, and make driver flag becone TRUE. */
		DBGLOG(P2P, WARN, "Old FW version, not support P2P RSSI query.\n");

		/* Must not use NETWORK_TYPE_P2P_INDEX, cause the structure is mismatch. */
		nicUpdateLinkQuality(prAdapter, 0, (P_EVENT_LINK_QUALITY) (prEvent->aucBuffer));
	}
#else
	/*only support ais query */
	{
		UINT_8 ucBssIndex;
		P_BSS_INFO_T prBssInfo;

		for (ucBssIndex = 0; ucBssIndex < BSS_INFO_NUM; ucBssIndex++) {
			prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

			if (prBssInfo->eNetworkType == NETWORK_TYPE_AIS && prBssInfo->fgIsInUse)
				break;
		}

		if (ucBssIndex >= BSS_INFO_NUM)
			ucBssIndex = 1;	/* No hit(bss1 for default ais network) */
		/* printk("=======> rssi with bss%d ,%d\n",ucBssIndex,
		 * ((P_EVENT_LINK_QUALITY_V2)(prEvent->aucBuffer))->rLq[ucBssIndex].cRssi);
		 */
		nicUpdateLinkQuality(prAdapter, ucBssIndex, (P_EVENT_LINK_QUALITY_V2) (prEvent->aucBuffer));
	}

#endif

	/* command response handling */
	prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

	if (prCmdInfo != NULL) {
		if (prCmdInfo->pfCmdDoneHandler)
			prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
		else if (prCmdInfo->fgIsOid)
			kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
		/* return prCmdInfo */
		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
	}
#ifndef LINUX
	if (prAdapter->rWlanInfo.eRssiTriggerType == ENUM_RSSI_TRIGGER_GREATER &&
		prAdapter->rWlanInfo.rRssiTriggerValue >= (PARAM_RSSI) (prAdapter->rLinkQuality.cRssi)) {

		prAdapter->rWlanInfo.eRssiTriggerType = ENUM_RSSI_TRIGGER_TRIGGERED;

		kalIndicateStatusAndComplete(prAdapter->prGlueInfo, WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
			(PVOID)&(prAdapter->rWlanInfo.rRssiTriggerValue), sizeof(PARAM_RSSI));
	} else if (prAdapter->rWlanInfo.eRssiTriggerType == ENUM_RSSI_TRIGGER_LESS &&
		prAdapter->rWlanInfo.rRssiTriggerValue <= (PARAM_RSSI) (prAdapter->rLinkQuality.cRssi)) {

		prAdapter->rWlanInfo.eRssiTriggerType = ENUM_RSSI_TRIGGER_TRIGGERED;

		kalIndicateStatusAndComplete(prAdapter->prGlueInfo, WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
			(PVOID)&(prAdapter->rWlanInfo.rRssiTriggerValue), sizeof(PARAM_RSSI));
	}
#endif
}

VOID nicEventLayer0ExtMagic(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	UINT_32 u4QueryInfoLen;
	P_CMD_INFO_T prCmdInfo;
	P_EVENT_ACCESS_EFUSE prEventEfuseAccess;
	P_EVENT_EFUSE_FREE_BLOCK_T prEventGetFreeBlock;
	P_EVENT_GET_TX_POWER_T prEventGetTXPower;

	if ((prEvent->ucExtenEID) == EXT_EVENT_ID_CMD_RESULT) {

		u4QueryInfoLen = sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T);

		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			if ((prCmdInfo->fgIsOid) != 0) {
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery,
					u4QueryInfoLen, WLAN_STATUS_SUCCESS);
				/* return prCmdInfo */
				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			}
		}
	} else if ((prEvent->ucExtenEID) == EXT_EVENT_ID_CMD_EFUSE_ACCESS) {
		u4QueryInfoLen = sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T);
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);
		prEventEfuseAccess = (P_EVENT_ACCESS_EFUSE) (prEvent->aucBuffer);

		/* Efuse block size 16 */
		kalMemCopy(prAdapter->aucEepromVaule, prEventEfuseAccess->aucData, 16);

		if (prCmdInfo != NULL) {
			if ((prCmdInfo->fgIsOid) != 0) {
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery,
					u4QueryInfoLen, WLAN_STATUS_SUCCESS);
				/* return prCmdInfo */
				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

			}
		}
	} else if ((prEvent->ucExtenEID) == EXT_EVENT_ID_EFUSE_FREE_BLOCK) {
		u4QueryInfoLen = sizeof(PARAM_CUSTOM_EFUSE_FREE_BLOCK_T);
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);
		prEventGetFreeBlock = (P_EVENT_EFUSE_FREE_BLOCK_T) (prEvent->aucBuffer);
		prAdapter->u4FreeBlockNum = prEventGetFreeBlock->u2FreeBlockNum;

		if (prCmdInfo != NULL) {
			if ((prCmdInfo->fgIsOid) != 0) {
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery,
					u4QueryInfoLen, WLAN_STATUS_SUCCESS);
				/* return prCmdInfo */
				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			}
		}
	} else if ((prEvent->ucExtenEID) == EXT_EVENT_ID_GET_TX_POWER) {
		u4QueryInfoLen = sizeof(PARAM_CUSTOM_GET_TX_POWER_T);
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);
		prEventGetTXPower = (P_EVENT_GET_TX_POWER_T) (prEvent->aucBuffer);

		prAdapter->u4GetTxPower = prEventGetTXPower->ucTx0TargetPower;

		if (prCmdInfo != NULL) {
			if ((prCmdInfo->fgIsOid) != 0) {
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery,
					u4QueryInfoLen, WLAN_STATUS_SUCCESS);
				/* return prCmdInfo */
				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

			}
		}
	}
}

VOID nicEventMicErrorInfo(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_EVENT_MIC_ERR_INFO prMicError;
	/* P_PARAM_AUTH_EVENT_T prAuthEvent; */
	P_STA_RECORD_T prStaRec;

	DBGLOG(RSN, EVENT, "EVENT_ID_MIC_ERR_INFO\n");

	prMicError = (P_EVENT_MIC_ERR_INFO) (prEvent->aucBuffer);
	prStaRec = cnmGetStaRecByAddress(prAdapter, prAdapter->prAisBssInfo->ucBssIndex,
					prAdapter->rWlanInfo.rCurrBssId.arMacAddress);
	ASSERT(prStaRec);

	if (prStaRec)
		rsnTkipHandleMICFailure(prAdapter, prStaRec, (BOOLEAN) prMicError->u4Flags);
	else
		DBGLOG(RSN, INFO, "No STA rec!!\n");
#if 0
	prAuthEvent = (P_PARAM_AUTH_EVENT_T) prAdapter->aucIndicationEventBuffer;

	/* Status type: Authentication Event */
	prAuthEvent->rStatus.eStatusType = ENUM_STATUS_TYPE_AUTHENTICATION;

	/* Authentication request */
	prAuthEvent->arRequest[0].u4Length = sizeof(PARAM_AUTH_REQUEST_T);
	kalMemCopy((PVOID) prAuthEvent->arRequest[0].arBssid,
			(PVOID) prAdapter->rWlanInfo.rCurrBssId.arMacAddress,	/* whsu:Todo? */
		   PARAM_MAC_ADDR_LEN);

	if (prMicError->u4Flags != 0)
		prAuthEvent->arRequest[0].u4Flags = PARAM_AUTH_REQUEST_GROUP_ERROR;
	else
		prAuthEvent->arRequest[0].u4Flags = PARAM_AUTH_REQUEST_PAIRWISE_ERROR;

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
				     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
				     (PVOID) prAuthEvent,
				     sizeof(PARAM_STATUS_INDICATION_T) + sizeof(PARAM_AUTH_REQUEST_T));
#endif
}

VOID nicEventScanDone(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	scnEventScanDone(prAdapter, (P_EVENT_SCAN_DONE) (prEvent->aucBuffer), TRUE);
}

VOID nicEventNloDone(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	DBGLOG(INIT, INFO, "EVENT_ID_NLO_DONE\n");
	scnEventNloDone(prAdapter, (P_EVENT_NLO_DONE_T) (prEvent->aucBuffer));
#if CFG_SUPPORT_PNO
	prAdapter->prAisBssInfo->fgIsPNOEnable = FALSE;
	if (prAdapter->prAisBssInfo->fgIsNetRequestInActive && prAdapter->prAisBssInfo->fgIsPNOEnable) {
		UNSET_NET_ACTIVE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
		DBGLOG(INIT, INFO, "INACTIVE  AIS from  ACTIVEto disable PNO\n");
		/* sync with firmware */
		nicDeactivateNetwork(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
	}
#endif
}

VOID nicEventSleepyNotify(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
#if !defined(_HIF_USB)
	P_EVENT_SLEEPY_INFO_T prEventSleepyNotify;

	prEventSleepyNotify = (P_EVENT_SLEEPY_INFO_T) (prEvent->aucBuffer);

	/* DBGLOG(RX, INFO, ("ucSleepyState = %d\n", prEventSleepyNotify->ucSleepyState)); */

	prAdapter->fgWiFiInSleepyState = (BOOLEAN) (prEventSleepyNotify->ucSleepyState);

#if CFG_SUPPORT_MULTITHREAD
	if (prEventSleepyNotify->ucSleepyState)
		kalSetFwOwnEvent2Hif(prAdapter->prGlueInfo);
#endif
#endif
}

VOID nicEventBtOverWifi(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
#if CFG_ENABLE_BT_OVER_WIFI
	UINT_8 aucTmp[sizeof(AMPC_EVENT) + sizeof(BOW_LINK_DISCONNECTED)];
	P_EVENT_BT_OVER_WIFI prEventBtOverWifi;
	P_AMPC_EVENT prBowEvent;
	P_BOW_LINK_CONNECTED prBowLinkConnected;
	P_BOW_LINK_DISCONNECTED prBowLinkDisconnected;

	prEventBtOverWifi = (P_EVENT_BT_OVER_WIFI) (prEvent->aucBuffer);

	/* construct event header */
	prBowEvent = (P_AMPC_EVENT) aucTmp;

	if (prEventBtOverWifi->ucLinkStatus == 0) {
		/* Connection */
		prBowEvent->rHeader.ucEventId = BOW_EVENT_ID_LINK_CONNECTED;
		prBowEvent->rHeader.ucSeqNumber = 0;
		prBowEvent->rHeader.u2PayloadLength = sizeof(BOW_LINK_CONNECTED);

		/* fill event body */
		prBowLinkConnected = (P_BOW_LINK_CONNECTED) (prBowEvent->aucPayload);
		prBowLinkConnected->rChannel.ucChannelNum = prEventBtOverWifi->ucSelectedChannel;
		kalMemZero(prBowLinkConnected->aucPeerAddress, MAC_ADDR_LEN);	/* @FIXME */

		kalIndicateBOWEvent(prAdapter->prGlueInfo, prBowEvent);
	} else {
		/* Disconnection */
		prBowEvent->rHeader.ucEventId = BOW_EVENT_ID_LINK_DISCONNECTED;
		prBowEvent->rHeader.ucSeqNumber = 0;
		prBowEvent->rHeader.u2PayloadLength = sizeof(BOW_LINK_DISCONNECTED);

		/* fill event body */
		prBowLinkDisconnected = (P_BOW_LINK_DISCONNECTED) (prBowEvent->aucPayload);
		prBowLinkDisconnected->ucReason = 0;	/* @FIXME */
		kalMemZero(prBowLinkDisconnected->aucPeerAddress, MAC_ADDR_LEN);	/* @FIXME */

		kalIndicateBOWEvent(prAdapter->prGlueInfo, prBowEvent);
	}
#endif
}

VOID nicEventStatistics(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_CMD_INFO_T prCmdInfo;

	/* buffer statistics for further query */
	prAdapter->fgIsStatValid = TRUE;
	prAdapter->rStatUpdateTime = kalGetTimeTick();
	kalMemCopy(&prAdapter->rStatStruct, prEvent->aucBuffer, sizeof(EVENT_STATISTICS));

	/* command response handling */
	prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

	if (prCmdInfo != NULL) {
		if (prCmdInfo->pfCmdDoneHandler)
			prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
		else if (prCmdInfo->fgIsOid)
			kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
		/* return prCmdInfo */
		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
	}
}
VOID nicEventWlanInfo(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_CMD_INFO_T prCmdInfo;

	/* buffer statistics for further query */
	prAdapter->fgIsStatValid = TRUE;
	prAdapter->rStatUpdateTime = kalGetTimeTick();
	kalMemCopy(&prAdapter->rEventWlanInfo, prEvent->aucBuffer, sizeof(EVENT_WLAN_INFO));

	DBGLOG(RSN, INFO, "EVENT_ID_WLAN_INFO");
	/* command response handling */
	prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

	if (prCmdInfo != NULL) {
		if (prCmdInfo->pfCmdDoneHandler)
			prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
		else if (prCmdInfo->fgIsOid)
			kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
		/* return prCmdInfo */
		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
	}
}

VOID nicEventMibInfo(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_CMD_INFO_T prCmdInfo;


	/* buffer statistics for further query */
	prAdapter->fgIsStatValid = TRUE;
	prAdapter->rStatUpdateTime = kalGetTimeTick();

	DBGLOG(RSN, INFO, "EVENT_ID_MIB_INFO");
	/* command response handling */
	prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

	if (prCmdInfo != NULL) {
		if (prCmdInfo->pfCmdDoneHandler)
			prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
		else if (prCmdInfo->fgIsOid)
			kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
		/* return prCmdInfo */
		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
	}

}

#if CFG_SUPPORT_LAST_SEC_MCS_INFO
VOID nicEventTxMcsInfo(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_CMD_INFO_T prCmdInfo;

	DBGLOG(RSN, INFO, "EVENT_ID_TX_MCS_INFO");
	/* command response handling */
	prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

	if (prCmdInfo != NULL) {
		if (prCmdInfo->pfCmdDoneHandler)
			prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
		else if (prCmdInfo->fgIsOid)
			kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
		/* return prCmdInfo */
		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
	}

}
#endif

VOID nicEventBeaconTimeout(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	DBGLOG(NIC, INFO, "EVENT_ID_BSS_BEACON_TIMEOUT\n");

	if (prAdapter->fgDisBcnLostDetection == FALSE) {
		P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;
		P_EVENT_BSS_BEACON_TIMEOUT_T prEventBssBeaconTimeout;

		prEventBssBeaconTimeout = (P_EVENT_BSS_BEACON_TIMEOUT_T) (prEvent->aucBuffer);

		if (prEventBssBeaconTimeout->ucBssIndex >= MAX_BSS_INDEX)
			return;

		DBGLOG(NIC, INFO, "Reason code: %d\n", prEventBssBeaconTimeout->ucReasonCode);

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prEventBssBeaconTimeout->ucBssIndex);

		if (prEventBssBeaconTimeout->ucBssIndex ==
			prAdapter->prAisBssInfo->ucBssIndex) {
#if CFG_DISCONN_DEBUG_FEATURE
			g_rDisconnInfoTemp.ucBcnTimeoutReason =
				prEventBssBeaconTimeout->ucReasonCode;
#endif
			aisBssBeaconTimeout(prAdapter);
		}
#if CFG_ENABLE_WIFI_DIRECT
		else if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P)
			p2pRoleFsmRunEventBeaconTimeout(prAdapter, prBssInfo);
#endif
#if CFG_ENABLE_BT_OVER_WIFI
		else if (GET_BSS_INFO_BY_INDEX(prAdapter, prEventBssBeaconTimeout->ucBssIndex)->eNetworkType ==
			 NETWORK_TYPE_BOW) {
			/* ToDo:: Nothing */
		}
#endif
		else {
			DBGLOG(RX, ERROR, "EVENT_ID_BSS_BEACON_TIMEOUT: (ucBssIndex = %d)\n",
				prEventBssBeaconTimeout->ucBssIndex);
		}
	}

}

VOID nicEventUpdateNoaParams(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
#if CFG_ENABLE_WIFI_DIRECT
	if (prAdapter->fgIsP2PRegistered) {
		P_EVENT_UPDATE_NOA_PARAMS_T prEventUpdateNoaParam;

		prEventUpdateNoaParam = (P_EVENT_UPDATE_NOA_PARAMS_T) (prEvent->aucBuffer);

		if (GET_BSS_INFO_BY_INDEX(prAdapter, prEventUpdateNoaParam->ucBssIndex)->eNetworkType ==
			NETWORK_TYPE_P2P) {

			p2pProcessEvent_UpdateNOAParam(prAdapter, prEventUpdateNoaParam->ucBssIndex,
				prEventUpdateNoaParam);
		} else {
			ASSERT(0);
		}
	}
#else
	ASSERT(0);
#endif
}

VOID nicEventStaAgingTimeout(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	if (prAdapter->fgDisStaAgingTimeoutDetection == FALSE) {
		P_EVENT_STA_AGING_TIMEOUT_T prEventStaAgingTimeout;
		P_STA_RECORD_T prStaRec;
		P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;

		prEventStaAgingTimeout = (P_EVENT_STA_AGING_TIMEOUT_T) (prEvent->aucBuffer);
		prStaRec = cnmGetStaRecByIndex(prAdapter, prEventStaAgingTimeout->ucStaRecIdx);
		if (prStaRec == NULL)
			return;

		DBGLOG(NIC, INFO, "EVENT_ID_STA_AGING_TIMEOUT %u " MACSTR "\n",
			prEventStaAgingTimeout->ucStaRecIdx, MAC2STR(prStaRec->aucMacAddr));

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

		bssRemoveClient(prAdapter, prBssInfo, prStaRec);

		/* Call False Auth */
		if (prAdapter->fgIsP2PRegistered) {
			p2pFuncDisconnect(prAdapter, prBssInfo, prStaRec, TRUE,
				REASON_CODE_DISASSOC_INACTIVITY);
		}

	}
	/* gDisStaAgingTimeoutDetection */
}

VOID nicEventApObssStatus(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
#if CFG_ENABLE_WIFI_DIRECT
	if (prAdapter->fgIsP2PRegistered)
		rlmHandleObssStatusEventPkt(prAdapter, (P_EVENT_AP_OBSS_STATUS_T) prEvent->aucBuffer);
#endif
}

VOID nicEventRoamingStatus(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
#if CFG_SUPPORT_ROAMING
	P_CMD_ROAMING_TRANSIT_T prTransit;

	prTransit = (P_CMD_ROAMING_TRANSIT_T) (prEvent->aucBuffer);

	roamingFsmProcessEvent(prAdapter, prTransit);
#endif /* CFG_SUPPORT_ROAMING */
}

VOID nicEventSendDeauth(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	SW_RFB_T rSwRfb;

#if DBG
	P_WLAN_MAC_HEADER_T prWlanMacHeader;

	prWlanMacHeader = (P_WLAN_MAC_HEADER_T) &prEvent->aucBuffer[0];
	DBGLOG(RX, TRACE, "nicRx: aucAddr1: " MACSTR "\n", MAC2STR(prWlanMacHeader->aucAddr1));
	DBGLOG(RX, TRACE, "nicRx: aucAddr2: " MACSTR "\n", MAC2STR(prWlanMacHeader->aucAddr2));
#endif

	/* receive packets without StaRec */
	rSwRfb.pvHeader = (P_WLAN_MAC_HEADER_T) &prEvent->aucBuffer[0];
	if (authSendDeauthFrame(prAdapter, NULL, NULL, &rSwRfb, REASON_CODE_CLASS_3_ERR,
		(PFN_TX_DONE_HANDLER) NULL) == WLAN_STATUS_SUCCESS) {

		DBGLOG(RX, TRACE, "Send Deauth Error\n");
	}
}

VOID nicEventUpdateRddStatus(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
#if CFG_SUPPORT_RDD_TEST_MODE
	P_EVENT_RDD_STATUS_T prEventRddStatus;

	prEventRddStatus = (P_EVENT_RDD_STATUS_T) (prEvent->aucBuffer);

	prAdapter->ucRddStatus = prEventRddStatus->ucRddStatus;
#endif
}

VOID nicEventUpdateBwcsStatus(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_PTA_IPC_T prEventBwcsStatus;

	prEventBwcsStatus = (P_PTA_IPC_T) (prEvent->aucBuffer);

#if CFG_SUPPORT_BCM_BWCS_DEBUG
	DBGLOG(RSN, EVENT, "BCM BWCS Event: %02x%02x%02x%02x\n",
	       prEventBwcsStatus->u.aucBTPParams[0],
	       prEventBwcsStatus->u.aucBTPParams[1],
	       prEventBwcsStatus->u.aucBTPParams[2], prEventBwcsStatus->u.aucBTPParams[3]);
#endif

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo, WLAN_STATUS_BWCS_UPDATE,
		(PVOID) prEventBwcsStatus, sizeof(PTA_IPC_T));
}

VOID nicEventUpdateBcmDebug(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_PTA_IPC_T prEventBwcsStatus;

	prEventBwcsStatus = (P_PTA_IPC_T) (prEvent->aucBuffer);

#if CFG_SUPPORT_BCM_BWCS_DEBUG
	DBGLOG(RSN, EVENT, "BCM FW status: %02x%02x%02x%02x\n",
	       prEventBwcsStatus->u.aucBTPParams[0],
	       prEventBwcsStatus->u.aucBTPParams[1],
	       prEventBwcsStatus->u.aucBTPParams[2], prEventBwcsStatus->u.aucBTPParams[3]);
#endif
}

VOID nicEventAddPkeyDone(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_EVENT_ADD_KEY_DONE_INFO prAddKeyDone;
	P_STA_RECORD_T prStaRec;

	prAddKeyDone = (P_EVENT_ADD_KEY_DONE_INFO) (prEvent->aucBuffer);

	DBGLOG(RSN, EVENT, "EVENT_ID_ADD_PKEY_DONE BSSIDX=%d " MACSTR "\n",
		prAddKeyDone->ucBSSIndex, MAC2STR(prAddKeyDone->aucStaAddr));

	prStaRec = cnmGetStaRecByAddress(prAdapter, prAddKeyDone->ucBSSIndex, prAddKeyDone->aucStaAddr);

	if (prStaRec) {
		DBGLOG(RSN, EVENT, "STA " MACSTR " Add Key Done!!\n", MAC2STR(prStaRec->aucMacAddr));
		prStaRec->fgIsTxKeyReady = TRUE;
		qmUpdateStaRec(prAdapter, prStaRec);
	}
}

VOID nicEventIcapDone(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_EVENT_ICAP_STATUS_T prEventIcapStatus;
	PARAM_CUSTOM_MEM_DUMP_STRUCT_T rMemDumpInfo;
	UINT_32 u4QueryInfo;

	prEventIcapStatus = (P_EVENT_ICAP_STATUS_T) (prEvent->aucBuffer);

	rMemDumpInfo.u4Address = prEventIcapStatus->u4StartAddress;
	rMemDumpInfo.u4Length = prEventIcapStatus->u4IcapSieze;
#if CFG_SUPPORT_QA_TOOL
	rMemDumpInfo.u4IcapContent = prEventIcapStatus->u4IcapContent;
#endif

	wlanoidQueryMemDump(prAdapter, &rMemDumpInfo, sizeof(rMemDumpInfo), &u4QueryInfo);
}

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
PARAM_CAL_BACKUP_STRUCT_V2_T	g_rCalBackupDataV2;

VOID nicEventCalAllDone(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_CMD_CAL_BACKUP_STRUCT_V2_T prEventCalBackupDataV2;
	UINT_32 u4QueryInfo;

	DBGLOG(RFTEST, INFO, "%s\n", __func__);

	memset(&g_rCalBackupDataV2, 0, sizeof(PARAM_CAL_BACKUP_STRUCT_V2_T));

	prEventCalBackupDataV2 = (P_CMD_CAL_BACKUP_STRUCT_V2_T) (prEvent->aucBuffer);

	if (prEventCalBackupDataV2->ucReason == 1 && prEventCalBackupDataV2->ucAction == 2) {
		DBGLOG(RFTEST, INFO, "Received an EVENT for Trigger Do All Cal Function.\n");

		g_rCalBackupDataV2.ucReason = 2;
		g_rCalBackupDataV2.ucAction = 4;
		g_rCalBackupDataV2.ucNeedResp = 1;
		g_rCalBackupDataV2.ucFragNum = 0;
		g_rCalBackupDataV2.ucRomRam = 0;
		g_rCalBackupDataV2.u4ThermalValue = 0;
		g_rCalBackupDataV2.u4Address = 0;
		g_rCalBackupDataV2.u4Length = 0;
		g_rCalBackupDataV2.u4RemainLength = 0;

		DBGLOG(RFTEST, INFO, "RLM CMD : Get Cal Data from FW (%s). Start!!!!!!!!!!!!!!!!\n",
			g_rCalBackupDataV2.ucRomRam == 0 ? "ROM" : "RAM");
		DBGLOG(RFTEST, INFO, "Thermal Temp = %d\n", g_rBackupCalDataAllV2.u4ThermalInfo);
		wlanoidQueryCalBackupV2(prAdapter,
			&g_rCalBackupDataV2,
			sizeof(PARAM_CAL_BACKUP_STRUCT_V2_T),
			&u4QueryInfo);
	}

}
#endif

VOID nicEventDebugMsg(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_EVENT_DEBUG_MSG_T prEventDebugMsg;
	UINT_16 u2DebugMsgId;
	UINT_8 ucMsgType;
	UINT_8 ucFlags;
	UINT_32 u4Value;
	UINT_16 u2MsgSize;
	P_UINT_8 pucMsg;

	prEventDebugMsg = (P_EVENT_DEBUG_MSG_T) (prEvent->aucBuffer);

	u2DebugMsgId = prEventDebugMsg->u2DebugMsgId;
	ucMsgType = prEventDebugMsg->ucMsgType;
	ucFlags = prEventDebugMsg->ucFlags;
	u4Value = prEventDebugMsg->u4Value;
	u2MsgSize = prEventDebugMsg->u2MsgSize;
	pucMsg = prEventDebugMsg->aucMsg;

	DBGLOG(SW4, TRACE, "DEBUG_MSG Id %u Type %u Fg 0x%x Val 0x%x Size %u\n",
		u2DebugMsgId, ucMsgType, ucFlags, u4Value, u2MsgSize);

	if (u2MsgSize <= DEBUG_MSG_SIZE_MAX) {
		if (ucMsgType >= DEBUG_MSG_TYPE_END)
			ucMsgType = DEBUG_MSG_TYPE_MEM32;

		if (ucMsgType == DEBUG_MSG_TYPE_ASCII) {
			PUINT_8 pucChr;

			pucMsg[u2MsgSize] = '\0';

			/* skip newline */
			pucChr = kalStrChr(pucMsg, '\0');
			if (*(pucChr - 1) == '\n')
				*(pucChr - 1) = '\0';

			DBGLOG(SW4, EVENT, "<FW>%s\n", pucMsg);
		} else if (ucMsgType == DEBUG_MSG_TYPE_MEM8) {
			DBGLOG(SW4, INFO, "<FW>Dump MEM8\n");
			DBGLOG_MEM8(SW4, INFO, pucMsg, u2MsgSize);
		} else {
			DBGLOG(SW4, INFO, "<FW>Dump MEM32\n");
			DBGLOG_MEM32(SW4, INFO, pucMsg, u2MsgSize);
		}
	} /* DEBUG_MSG_SIZE_MAX */
	else
		DBGLOG(SW4, INFO, "Debug msg size %u is too large.\n", u2MsgSize);
}

VOID nicEventTdls(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
#if CFG_SUPPORT_TDLS
	TdlsexEventHandle(prAdapter->prGlueInfo, (PUINT_8)prEvent->aucBuffer,
		(UINT_32)(prEvent->u2PacketLength - 8));
#endif
}

VOID nicEventDumpMem(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_CMD_INFO_T prCmdInfo;

	DBGLOG(SW4, INFO, "%s: EVENT_ID_DUMP_MEM\n", __func__);

	prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

	if (prCmdInfo != NULL) {
		DBGLOG(NIC, INFO, ": ==> 1\n");
		if (prCmdInfo->pfCmdDoneHandler)
			prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
		else if (prCmdInfo->fgIsOid)
			kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
		/* return prCmdInfo */
		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
	} else {
		/* Burst mode */
		DBGLOG(NIC, INFO, ": ==> 2\n");
		nicEventQueryMemDump(prAdapter, prEvent->aucBuffer);
	}
}

VOID nicEventAssertDump(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{

	if (wlanIsChipRstRecEnabled(prAdapter))
		wlanChipRstPreAct(prAdapter);

	if (prEvent->ucS2DIndex == S2D_INDEX_EVENT_N2H) {
		if (!prAdapter->fgN9AssertDumpOngoing) {
			DBGLOG(NIC, ERROR, "%s: EVENT_ID_ASSERT_DUMP\n", __func__);
			DBGLOG(NIC, ERROR, "\n[DUMP_N9]====N9 ASSERT_DUMPSTART====\n");
			prAdapter->fgKeepPrintCoreDump = TRUE;
			if (kalOpenCorDumpFile(TRUE) != WLAN_STATUS_SUCCESS)
				DBGLOG(NIC, ERROR, "kalOpenCorDumpFile fail\n");
			else
				prAdapter->fgN9CorDumpFileOpend = TRUE;

			prAdapter->fgN9AssertDumpOngoing = TRUE;
				}
		if (prAdapter->fgN9AssertDumpOngoing) {

			if (prAdapter->fgKeepPrintCoreDump)
				DBGLOG(NIC, ERROR, "[DUMP_N9]%s:\n", prEvent->aucBuffer);
			if (!kalStrnCmp(prEvent->aucBuffer, ";more log added here", 5)
			    || !kalStrnCmp(prEvent->aucBuffer, ";[core dump start]", 5))
				prAdapter->fgKeepPrintCoreDump = FALSE;

			if (prAdapter->fgN9CorDumpFileOpend) {
				if (kalWriteCorDumpFile(prEvent->aucBuffer,
						prEvent->u2PacketLength - EVENT_HDR_WITHOUT_RXD_SIZE, TRUE) !=
						WLAN_STATUS_SUCCESS) {
					DBGLOG(NIC, INFO, "kalWriteN9CorDumpFile fail\n");
				}
			}
			wlanCorDumpTimerReset(prAdapter, TRUE);
		}
	} else {
		/* prEvent->ucS2DIndex == S2D_INDEX_EVENT_C2H */
		if (!prAdapter->fgCr4AssertDumpOngoing) {
			DBGLOG(NIC, ERROR, "%s: EVENT_ID_ASSERT_DUMP\n", __func__);
			DBGLOG(NIC, ERROR, "\n[DUMP_Cr4]====CR4 ASSERT_DUMPSTART====\n");
			prAdapter->fgKeepPrintCoreDump = TRUE;
			if (kalOpenCorDumpFile(FALSE) != WLAN_STATUS_SUCCESS)
				DBGLOG(NIC, ERROR, "kalOpenCorDumpFile fail\n");
			else
				prAdapter->fgCr4CorDumpFileOpend = TRUE;

			prAdapter->fgCr4AssertDumpOngoing = TRUE;
		}
		if (prAdapter->fgCr4AssertDumpOngoing) {
			if (prAdapter->fgKeepPrintCoreDump)
				DBGLOG(NIC, ERROR, "[DUMP_CR4]%s:\n", prEvent->aucBuffer);
			if (!kalStrnCmp(prEvent->aucBuffer, ";more log added here", 5))
				prAdapter->fgKeepPrintCoreDump = FALSE;

			if (prAdapter->fgCr4CorDumpFileOpend) {
				if (kalWriteCorDumpFile(prEvent->aucBuffer,
					prEvent->u2PacketLength - EVENT_HDR_WITHOUT_RXD_SIZE, FALSE) !=
					WLAN_STATUS_SUCCESS) {
					DBGLOG(NIC, ERROR, "kalWriteN9CorDumpFile fail\n");
				}
			}
			wlanCorDumpTimerReset(prAdapter, FALSE);
		}
	}
}

VOID nicEventRddSendPulse(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	DBGLOG(RLM, INFO, "%s: EVENT_ID_RDD_SEND_PULSE\n", __func__);

	nicEventRddPulseDump(prAdapter, prEvent->aucBuffer);
}

VOID nicEventUpdateCoexPhyrate(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	UINT_8 i;
	P_EVENT_UPDATE_COEX_PHYRATE_T prEventUpdateCoexPhyrate;

	DBGLOG(NIC, LOUD, "%s\n", __func__);

	prEventUpdateCoexPhyrate = (P_EVENT_UPDATE_COEX_PHYRATE_T)(prEvent->aucBuffer);

	for (i = 0; i < (HW_BSSID_NUM+1); i++) {
		prAdapter->aprBssInfo[i]->u4CoexPhyRateLimit = prEventUpdateCoexPhyrate->au4PhyRateLimit[i];
		DBGLOG(NIC, INFO, "Coex:BSS[%d]R:%d\n", i, prAdapter->aprBssInfo[i]->u4CoexPhyRateLimit);
	}
}

#if (CFG_WOW_SUPPORT == 1)
VOID nicEventWakeUpReason(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	struct _EVENT_WAKEUP_REASON_INFO *prWakeUpReason;
	P_GLUE_INFO_T prGlueInfo;

	DBGLOG(NIC, INFO, "nicEventWakeUpReason\n");
	prGlueInfo = prAdapter->prGlueInfo;

	/* Driver receives EVENT_ID_WOW_WAKEUP_REASON after firmware wake up host
	 * The possible Wakeup Reason define in FW as following
	 * 0:  MAGIC PACKET
	 * 1:  BITMAP
	 * 2:  ARPNS
	 * 3:  GTK_REKEY
	 * 4:  COALESCING_FILTER
	 * 5:  HW_GLOBAL_ENABLE
	 * 6:  TCP_SYN PACKET
	 * 7:  TDLS
	 * 8:  DISCONNECT
	 * 9:  IPV4_UDP PACKET
	 * 10: IPV4_TCP PACKET
	 * 11: IPV6_UDP PACKET
	 * 12: IPV6_TCP PACKET
	 */
	prWakeUpReason = (struct _EVENT_WAKEUP_REASON_INFO *) (prEvent->aucBuffer);
	prGlueInfo->prAdapter->rWowCtrl.ucReason = prWakeUpReason->reason;
	DBGLOG(NIC, INFO, "nicEventWakeUpReason:%d\n", prGlueInfo->prAdapter->rWowCtrl.ucReason);
}
#endif

#if CFG_SUPPORT_CSI
VOID nicEventCSIData(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	struct TLV_ELEMENT *prCSITlvData;
	INT_32 i4EventLen =
		prEvent->u2PacketLength - EVENT_HDR_WITHOUT_RXD_SIZE;
	INT_16 i2Idx = 0;
	PINT_8 prBuf = NULL;
	UINT_32 u4IsCck = 0;
	PUINT_16 pru2Tmp = NULL;
	struct CSI_DATA_T *prCSIData = NULL;
	struct CSI_INFO_T *prCSIInfo = &(prAdapter->rCSIInfo);
	/* u2Offset is 8 bytes currently, tag 4 bytes + length 4 bytes */
	UINT_16 u2Offset = OFFSET_OF(struct TLV_ELEMENT, aucbody);

#define CSI_EVENT_MAX_SIZE 1500

	DBGLOG(NIC, INFO, "nicEventCSIData\n");

	if (i4EventLen > CSI_EVENT_MAX_SIZE) {
		DBGLOG(NIC, WARN, "Invalid CSI event size %u\n",
			i4EventLen);
		return;
	}
	prCSIData = kalMemAlloc(sizeof(struct CSI_DATA_T), VIR_MEM_TYPE);

	if (!prCSIData) {
		DBGLOG(NIC, WARN, "Alloc prCSIData failed!");
		return;
	}

	prCSIData->u8TimeStamp = kalDivU64(kalGetBootTime(), USEC_PER_MSEC);

	prBuf = (PINT_8) (prEvent->aucBuffer);

	while (i4EventLen >= u2Offset) {
		prCSITlvData = (struct TLV_ELEMENT *) prBuf;

		switch (prCSITlvData->tag_type) {
		case CSI_EVENT_IS_CCK:
			if (prCSITlvData->body_len != sizeof(UINT_32)) {
				DBGLOG(NIC, WARN,
					"Invalid IS_CCK len %u",
					prCSITlvData->body_len);
				goto out;
			}

			u4IsCck =
				le32_to_cpup((PUINT_32) prCSITlvData->aucbody);
			prCSIData->bIsCck = (u4IsCck) ? TRUE : FALSE;
			break;
		case CSI_EVENT_CBW:
			if (prCSITlvData->body_len != sizeof(UINT_32)) {
				DBGLOG(NIC, WARN,
					"Invalid CBW len %u",
					prCSITlvData->body_len);
				goto out;
			}

			prCSIData->ucBw =
				le32_to_cpup((PUINT_32) prCSITlvData->aucbody);
			break;
		case CSI_EVENT_RSSI:
			if (prCSITlvData->body_len != sizeof(UINT_32)) {
				DBGLOG(NIC, WARN,
					"Invalid RSSI len %u",
					prCSITlvData->body_len);
				goto out;
			}

			prCSIData->cRssi =
				le32_to_cpup((PUINT_32) prCSITlvData->aucbody);
			break;
		case CSI_EVENT_SNR:
			if (prCSITlvData->body_len != sizeof(UINT_32)) {
				DBGLOG(NIC, WARN,
					"Invalid SNR len %u",
					prCSITlvData->body_len);
				goto out;
			}

			prCSIData->ucSNR =
				le32_to_cpup((PUINT_32) prCSITlvData->aucbody);
			break;
		case CSI_EVENT_BAND:
			if (prCSITlvData->body_len != sizeof(UINT_32)) {
				DBGLOG(NIC, WARN,
					"Invalid BAND len %u",
					prCSITlvData->body_len);
				goto out;
			}

			prCSIData->ucDbdcIdx =
				le32_to_cpup((PUINT_32) prCSITlvData->aucbody);
			break;
		case CSI_EVENT_CSI_NUM:
			if (prCSITlvData->body_len != sizeof(UINT_32)) {
				DBGLOG(NIC, WARN,
					"Invalid CSI num len %u",
					prCSITlvData->body_len);
				goto out;
			}

			prCSIData->u2DataCount =
				le32_to_cpup((PUINT_32) prCSITlvData->aucbody);
			break;
		case CSI_EVENT_CSI_I_DATA:
			if (prCSIData->u2DataCount > CSI_MAX_DATA_COUNT) {
				DBGLOG(NIC, WARN,
					"Invalid CSI count %u\n",
					prCSIData->u2DataCount);
				goto out;
			}

			if (prCSITlvData->body_len !=
				sizeof(INT_16) * CSI_MAX_DATA_COUNT) {
				DBGLOG(NIC, WARN,
					"Invalid CSI num len %u",
					prCSITlvData->body_len);
				goto out;
			}

			kalMemZero(prCSIData->ac2IData,
				sizeof(prCSIData->ac2IData));

			pru2Tmp = (PINT_16) prCSITlvData->aucbody;
			for (i2Idx = 0; i2Idx < prCSIData->u2DataCount; i2Idx++)
				prCSIData->ac2IData[i2Idx] =
					le16_to_cpup(pru2Tmp + i2Idx);
			break;
		case CSI_EVENT_CSI_Q_DATA:
			if (prCSIData->u2DataCount > CSI_MAX_DATA_COUNT) {
				DBGLOG(NIC, WARN,
					"Invalid CSI count %u\n",
					prCSIData->u2DataCount);
				goto out;
			}

			if (prCSITlvData->body_len !=
				sizeof(INT_16) * CSI_MAX_DATA_COUNT) {
				DBGLOG(NIC, WARN,
					"Invalid CSI num len %u",
					prCSITlvData->body_len);
				goto out;
			}

			kalMemZero(prCSIData->ac2QData,
				sizeof(prCSIData->ac2QData));

			pru2Tmp = (PINT_16) prCSITlvData->aucbody;
			for (i2Idx = 0; i2Idx < prCSIData->u2DataCount; i2Idx++)
				prCSIData->ac2QData[i2Idx] =
					le16_to_cpup(pru2Tmp + i2Idx);
			break;
		case CSI_EVENT_DBW:
			if (prCSITlvData->body_len != sizeof(UINT_32)) {
				DBGLOG(NIC, WARN,
					"Invalid DBW len %u",
					prCSITlvData->body_len);
				goto out;
			}

			prCSIData->ucDataBw =
				le32_to_cpup((PINT_32) prCSITlvData->aucbody);
			break;
		case CSI_EVENT_CH_IDX:
			if (prCSITlvData->body_len != sizeof(UINT_32)) {
				DBGLOG(NIC, WARN,
					"Invalid CH IDX len %u",
					prCSITlvData->body_len);
				goto out;
			}

			prCSIData->ucPrimaryChIdx =
				le32_to_cpup((PINT_32) prCSITlvData->aucbody);
			break;
		case CSI_EVENT_TA:
			/*
			 * TA length is 8-byte long (MAC addr 6 bytes +
			 * 2 bytes padding), the 2-byte padding keeps
			 * the next Tag at a 4-byte aligned address.
			 */
			if (prCSITlvData->body_len !=
				ALIGN_4(sizeof(prCSIData->aucTA))) {
				DBGLOG(NIC, WARN,
					"Invalid TA len %u",
					prCSITlvData->body_len);
				goto out;
			}
			kalMemCopy(prCSIData->aucTA, prCSITlvData->aucbody,
				sizeof(prCSIData->aucTA));
			break;
		case CSI_EVENT_EXTRA_INFO:
			if (prCSITlvData->body_len != sizeof(UINT_32)) {
				DBGLOG(NIC, WARN,
					"Invalid Error len %u",
					prCSITlvData->body_len);
				goto out;
			}
			prCSIData->u4ExtraInfo =
				le32_to_cpup((PINT_32) prCSITlvData->aucbody);
			break;
		case CSI_EVENT_RX_MODE:
			if (prCSITlvData->body_len != sizeof(UINT_32)) {
				DBGLOG(NIC, WARN,
					"Invalid Rx Mode len %u",
					prCSITlvData->body_len);
				goto out;
			}

			prCSIData->ucRxMode =
				le32_to_cpup((PINT_32) prCSITlvData->aucbody);
			break;
		case CSI_EVENT_RSVD1:
			if (prCSITlvData->body_len >
				sizeof(INT_32) * CSI_MAX_RSVD1_COUNT) {
				DBGLOG(NIC, WARN,
					"Invalid RSVD1 len %u",
					prCSITlvData->body_len);
				goto out;
			}

			kalMemCopy(prCSIData->ai4Rsvd1,
				prCSITlvData->aucbody,
				prCSITlvData->body_len);

			prCSIData->ucRsvd1Cnt =
				prCSITlvData->body_len / sizeof(INT_32);
			break;
		case CSI_EVENT_RSVD2:
			if (prCSITlvData->body_len >
				sizeof(INT_32) * CSI_MAX_RSVD1_COUNT) {
				DBGLOG(NIC, WARN,
					"Invalid RSVD2 len %u",
					prCSITlvData->body_len);
				goto out;
			}

			kalMemCopy(prCSIData->au4Rsvd2,
				prCSITlvData->aucbody,
				prCSITlvData->body_len);

			prCSIData->ucRsvd1Cnt =
				prCSITlvData->body_len / sizeof(INT_32);
			break;
		case CSI_EVENT_RSVD3:
			if (prCSITlvData->body_len != sizeof(UINT_32)) {
				DBGLOG(NIC, WARN,
					"Invalid RSVD3 len %u",
					prCSITlvData->body_len);
				goto out;
			}

			prCSIData->i4Rsvd3 =
				le32_to_cpup((PINT_32) prCSITlvData->aucbody);
			break;
		case CSI_EVENT_RSVD4:
			if (prCSITlvData->body_len != sizeof(UINT_32)) {
				DBGLOG(NIC, WARN,
					"Invalid RSVD4 len %u",
					prCSITlvData->body_len);
				goto out;
			}

			prCSIData->ucRsvd4 =
				le32_to_cpup((PUINT_32) prCSITlvData->aucbody);
			break;
		default:
			DBGLOG(NIC, INFO, "Unsupported CSI tag %d\n",
				prCSITlvData->tag_type);
		};

		i4EventLen -= (u2Offset + prCSITlvData->body_len);

		if (i4EventLen >= u2Offset)
			prBuf += (u2Offset + prCSITlvData->body_len);
	}

	if ((prCSIInfo->ucValue1[CSI_CONFIG_OUTPUT_FORMAT] ==
		CSI_OUTPUT_TONE_MASKED ||
		prCSIInfo->ucValue1[CSI_CONFIG_OUTPUT_FORMAT] ==
		CSI_OUTPUT_TONE_MASKED_SHIFTED) &&
		!prCSIData->bIsCck) {
		wlanApplyCSIToneMask(prCSIData->ucRxMode,
			prCSIData->ucBw, prCSIData->ucDataBw,
			prCSIData->ucPrimaryChIdx,
			prCSIData->ac2IData, prCSIData->ac2QData);
	}
	if (prCSIInfo->ucValue1[CSI_CONFIG_OUTPUT_FORMAT] ==
		CSI_OUTPUT_TONE_MASKED_SHIFTED &&
		!prCSIData->bIsCck) {
		kalMemCopy(prCSIInfo->ai2TempIData,
			prCSIData->ac2IData,
			sizeof(INT_16) * prCSIData->u2DataCount);
		kalMemCopy(prCSIInfo->ai2TempQData,
			prCSIData->ac2QData,
			sizeof(INT_16) * prCSIData->u2DataCount);
		wlanShiftCSI(prCSIData->ucRxMode,
			prCSIData->ucBw, prCSIData->ucDataBw,
			prCSIData->ucPrimaryChIdx,
			prCSIInfo->ai2TempIData,
			prCSIInfo->ai2TempQData,
			prCSIData->ac2IData,
			prCSIData->ac2QData);

		if (prCSIData->ucDataBw == RX_VT_FR_MODE_20)
			prCSIData->u2DataCount = 64;
		else if (prCSIData->ucDataBw == RX_VT_FR_MODE_40)
			prCSIData->u2DataCount = 128;
		else
			prCSIData->u2DataCount = 256;
	}

	wlanPushCSIData(prAdapter, prCSIData);
	wake_up_interruptible(&(prAdapter->rCSIInfo.waitq));

out:
	kalMemFree(prCSIData, VIR_MEM_TYPE, sizeof(struct CSI_DATA_T));
}
#endif

#if CFG_SUPPORT_REPLAY_DETECTION
VOID nicCmdEventSetAddKey(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_WIFI_CMD_T prWifiCmd = NULL;
	P_CMD_802_11_KEY prCmdKey = NULL;
	struct SEC_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
	UINT_8 ucBssIndex = 0;
	P_BSS_INFO_T prBssInfo = NULL;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	if (prCmdInfo->fgIsOid) {
		/* Update Set Information Length */
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery, prCmdInfo->u4InformationBufferLength, WLAN_STATUS_SUCCESS);
	}

	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
	prCmdKey = (P_CMD_802_11_KEY) (prWifiCmd->aucBuffer);
	ucBssIndex = prCmdKey->ucBssIdx;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	ASSERT(prBssInfo);

	prDetRplyInfo = &prBssInfo->rDetRplyInfo;

	if (pucEventBuf) {
		prWifiCmd = (P_WIFI_CMD_T) (pucEventBuf);
		prCmdKey = (P_CMD_802_11_KEY) (prWifiCmd->aucBuffer);
		if (!prCmdKey->ucKeyType) {
			prDetRplyInfo->ucCurKeyId = prCmdKey->ucKeyId;
			prDetRplyInfo->ucKeyType = prCmdKey->ucKeyType;
			prDetRplyInfo->arReplayPNInfo[prCmdKey->ucKeyId].fgRekey = TRUE;
			prDetRplyInfo->arReplayPNInfo[prCmdKey->ucKeyId].fgFirstPkt = TRUE;
			DBGLOG(NIC, TRACE, "Keyid is %d, ucKeyType is %d\n",
				prCmdKey->ucKeyId, prCmdKey->ucKeyType);
		}
	}
}

VOID nicOidCmdTimeoutSetAddKey(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
	ASSERT(prAdapter);

	DBGLOG(NIC, WARN, "Wlan setaddkey timeout.\n");
	if (prCmdInfo->fgIsOid)
		kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_FAILURE);
}


VOID nicEventGetGtkDataSync(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_PARAM_GTK_REKEY_DATA prGtkData = NULL;
	struct SEC_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
	P_BSS_INFO_T prBssInfo = NULL;
	UINT_8 ucCurKeyId;

	prGtkData = (P_PARAM_GTK_REKEY_DATA) (prEvent->aucBuffer);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prAdapter->prAisBssInfo->ucBssIndex);

	prDetRplyInfo = &prBssInfo->rDetRplyInfo;
	prDetRplyInfo->ucCurKeyId = prGtkData->ucCurKeyId;
	ucCurKeyId = prDetRplyInfo->ucCurKeyId;

	kalMemZero(prDetRplyInfo->arReplayPNInfo[ucCurKeyId].auPN, NL80211_REPLAY_CTR_LEN);

#if 0
	/* if Drv alread rx a new PN value large than fw PN, then skip PN update */
	if (qmRxDetectReplay(prGtkData->aucReplayCtr,
		prDetRplyInfo->arReplayPNInfo[ucCurKeyId].auPN))
		return;
#endif

	kalMemCopy(prDetRplyInfo->arReplayPNInfo[ucCurKeyId].auPN,
		prGtkData->aucReplayCtr, 6);

	DBGLOG(RSN, INFO, "Get BC/MC PN update from fw.\n");

	DBGLOG_MEM8(RSN, INFO, (PUINT_8)prDetRplyInfo->arReplayPNInfo[ucCurKeyId].auPN, NL80211_REPLAY_CTR_LEN);
}

#endif

VOID nicCmdEventGetTxPwrTbl(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo,
			    IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_GLUE_INFO_T prGlueInfo;
	struct EVENT_GET_TXPWR_TBL *prTxPwrTblEvent = NULL;
	struct PARAM_CMD_GET_TXPWR_TBL *prTxPwrTbl = NULL;
	void *info_buf = NULL;

	if (!prAdapter) {
		DBGLOG(NIC, ERROR, "NULL prAdapter!\n");
		return;
	}

	if (!prCmdInfo) {
		DBGLOG(NIC, ERROR, "NULL prCmdInfo!\n");
		return;
	}

	if (!pucEventBuf || !prCmdInfo->pvInformationBuffer) {
		if (prCmdInfo->fgIsOid) {
			kalOidComplete(prAdapter->prGlueInfo,
				       prCmdInfo->fgSetQuery,
				       0,
				       WLAN_STATUS_FAILURE);
		}

		if (!pucEventBuf)
			DBGLOG(NIC, WARN, "NULL pucEventBuf!\n");

		if (!prCmdInfo->pvInformationBuffer)
			DBGLOG(NIC, WARN, "NULL pvInformationBuffer!\n");

		return;
	}

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		info_buf = prCmdInfo->pvInformationBuffer;
		prTxPwrTblEvent = (struct EVENT_GET_TXPWR_TBL *) pucEventBuf;
		prTxPwrTbl = (struct PARAM_CMD_GET_TXPWR_TBL *) info_buf;

		u4QueryInfoLen = sizeof(struct PARAM_CMD_GET_TXPWR_TBL);

		prTxPwrTbl->ucCenterCh = prTxPwrTblEvent->ucCenterCh;

		kalMemCopy(prTxPwrTbl->tx_pwr_tbl,
			   prTxPwrTblEvent->tx_pwr_tbl,
			   sizeof(prTxPwrTblEvent->tx_pwr_tbl));

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery,
			       u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}
