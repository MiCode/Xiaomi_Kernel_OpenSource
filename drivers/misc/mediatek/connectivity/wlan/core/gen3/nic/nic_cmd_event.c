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
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/nic/nic_cmd_event.c#3
 */

/*
 * ! \file   nic_cmd_event.c
 *  \brief  Callback functions for Command packets.
 *
 *  Various Event packet handlers which will be setup in the callback function of
 *  a command packet.
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
UINT_32 u4IQDataIndex;
UINT_32 u4IQDataOffset;
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
			for (i = 0; i < sizeof(PARAM_RX_STAT_T)/4; i++) {
				u4Temp = ntohl(prEventAccessRxStat->au4Buffer[i]);
				kalMemCopy(prElement, &u4Temp, 4);

				if (i < ((sizeof(PARAM_RX_STAT_T)/4)  - 1))
					prElement++;
			}
			/*
			* for (i = 0; i < HQA_RX_STATISTIC_NUM; i++) {
			*	u4Temp = ntohl(prEventAccessRxStat->au4Buffer[i]);
			*	kalMemCopy(prElement, &u4Temp, 4);

			*	if (i < (HQA_RX_STATISTIC_NUM - 1))
			*	prElement++;
			* }
			*/
		}

		DBGLOG(INIT, ERROR,
			"RX Statistics Test SeqNum = %d, TotalNum = %d, Buffer [0] = %d, Buffer [1] = %d\n",
			 (unsigned int)prEventAccessRxStat->u4SeqNum, (unsigned int)prEventAccessRxStat->u4TotalNum,
			 (unsigned int)prEventAccessRxStat->au4Buffer[0],
			 (unsigned int)prEventAccessRxStat->au4Buffer[1]);

		DBGLOG(INIT, ERROR, "Buffer [66] = %d, Buffer [67] = %d\n",
		       (unsigned int)prEventAccessRxStat->au4Buffer[66],
		       (unsigned int)prEventAccessRxStat->au4Buffer[67]);

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

	/* 4 <2> Update information of OID */
	if (!prCmdInfo->fgIsOid) {
		DBGLOG(NIC, INFO, "Not a OID, bypass\n");
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

	/* 4 <2> Update information of OID */
	if (!prCmdInfo->fgIsOid) {
		DBGLOG(NIC, INFO, "Not OID\n");
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

	DBGLOG(NIC, INFO, "event id : %x\n", prGetQd->u4EventId);
	for (i = 0; i < 14; i++)
		DBGLOG(NIC, INFO, "au4RawData[%d]: %x\n", i, prGetQd->au4RawData[i]);

}

VOID nicCmdEventGetCalcLq(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_GLUE_INFO_T prGlueInfo;
	P_EVENT_HQA_GET_MU_CALC_LQ prEventHqaGetMuCalcLq;
	UINT_32 i;
	INT_32 *qRep = NULL;

	P_PARAM_CUSTOM_GET_MU_CALC_LQ_STRUCT_T prGetMuCalcLq = NULL;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (!prCmdInfo->fgIsOid) {
		DBGLOG(NIC, INFO, "Not OID\n");
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

	DBGLOG(NIC, INFO, " event id : %x\n", prGetMuCalcLq->u4EventId);
	for (i = 0; i < NUM_OF_USER; i++) {
		qRep = &prGetMuCalcLq->rEntry.lq_report[i][0];
		DBGLOG(NIC, INFO, "lq_report[%d]:%x,%x,%x,%x,%x\n", i, qRep[0], qRep[1], qRep[2], qRep[3], qRep[4]);
	}

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

	/* 4 <2> Update information of OID */
	if (!prCmdInfo->fgIsOid) {
		DBGLOG(NIC, INFO, "Not OID\n");
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

		/* Memory copy length is depended on upper-layer */
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
#ifdef CFG_SUPPORT_LINK_QUALITY_MONITOR
	/* link quality monitor */
	struct WIFI_LINK_QUALITY_INFO *prLinkQualityInfo;
#endif

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

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
#ifdef CFG_SUPPORT_LINK_QUALITY_MONITOR
	prStatistics->rMdrdyCnt = prEventStatistics->rMdrdyCnt;
	prStatistics->rChnlIdleCnt = prEventStatistics->rChnlIdleCnt;

	/* link quality monitor */
	prLinkQualityInfo =
		&(prAdapter->rLinkQualityInfo);

	prLinkQualityInfo->u8TxRetryCount =
		prStatistics->rRetryCount.QuadPart;

	prLinkQualityInfo->u8TxRtsFailCount =
		prStatistics->rRTSFailureCount.QuadPart;
	prLinkQualityInfo->u8TxAckFailCount =
		prStatistics->rACKFailureCount.QuadPart;
	prLinkQualityInfo->u8TxFailCount = prLinkQualityInfo->u8TxRtsFailCount
									 + prLinkQualityInfo->u8TxAckFailCount;
	prLinkQualityInfo->u8TxTotalCount =
		prStatistics->rTransmittedFragmentCount.QuadPart;

	prLinkQualityInfo->u8RxTotalCount =
		prStatistics->rReceivedFragmentCount.QuadPart;
	/* FW report is diff, driver count total */
	prLinkQualityInfo->u8RxErrCount +=
		prStatistics->rFCSErrorCount.QuadPart;
	prLinkQualityInfo->u8MdrdyCount =
		prStatistics->rMdrdyCnt.QuadPart;
	prLinkQualityInfo->u8IdleSlotCount =
		prStatistics->rChnlIdleCnt.QuadPart;

	wlanFinishCollectingLinkQuality(prGlueInfo);
#endif
	if (prCmdInfo->fgIsOid)
		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);

}

VOID nicCmdEventEnterRfTest(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
#define WAIT_FW_READY_RETRY_CNT 200

	UINT_32 u4WHISR = 0, u4Value = 0;
	UINT_16 au2TxCount[16];
	UINT_16 u2RetryCnt = 0;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	kalTakeVcoreAction(VCORE_ADD_HIGHER_REQ);
	kalMayChangeVcore();
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

	kalTakeVcoreAction(VCORE_RESTORE_DEF);
	kalTakeVcoreAction(VCORE_DEC_HIGHER_REQ);
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
	 * DBGLOG(SCN, INFO, "--->nicCmdEventSetStopSchedScan\n" ));
	 */
	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	/*
	 * DBGLOG(SCN, INFO, "<--kalSchedScanStopped\n" );
	 */
	if (prCmdInfo->fgIsOid) {
		/* Update Set Information Length */
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery, prCmdInfo->u4InformationBufferLength, WLAN_STATUS_SUCCESS);
	}

	DBGLOG(SCN, INFO, "nicCmdEventSetStopSchedScan OID done, release lock and send event to uplayer\n");
	/* Due to dead lock issue, need to release the IO control before calling kernel APIs */
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
#if 1
	if (u4GetWf1 == 0) {
		if (u4IQ == 0)
			*prIQAry = &g_au4I0Data[0][u4IQDataOffset];
		else
			*prIQAry = &g_au4Q0Data[0][u4IQDataOffset];
		u4IQDataOffset += 200;
		if (u4IQDataOffset > u4IQDataIndex) {
			*prDataLen = 0;
			u4IQDataOffset = 0;
		} else
			*prDataLen = 800;
	} else {
		*prDataLen = 0;
		u4IQDataIndex = 0;
		u4IQDataOffset = 0;
	}
	DBGLOG(INIT, INFO, "QA_AGENT GetIQData prDataLen = %d\n", *prDataLen);
	return 0;
#else
	UINT_8 aucPath[50];	/* the path for iq data dump out */
	UINT_8 aucData[50];	/* iq data in string format */
	UINT_32 i = 0, j = 0, count = 0;
	INT_32 ret = -1;
	INT_32 rv;
	struct file *file = NULL;

	i = 0;

	/* sprintf(aucPath, "/pattern.txt");             // CSD's Pattern */
	sprintf(aucPath, "/data/dump_out_001_WF0.txt");
	DBGLOG(INIT, INFO, "iCap Read Dump File dump_out_001_WF0.txt\n");
	/*sprintf(aucPath, "/data/dump_out_00007_WF%d.txt", u4GetWf1);*/

	file = kalFileOpen(aucPath, O_RDONLY, 0);

	if ((file != NULL) && !IS_ERR(file)) {
		*prIQAry = &g_au4IQData[g_u2DumpIndex][0];
		DBGLOG(INIT, INFO, "open /data/dump_out_001_WF0.txt success\n");
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

			rv = kstrtoint(aucData, 0, &g_au4IQData[g_u2DumpIndex][i]);
		}
		*prDataLen = i * sizeof(UINT_32);
		kalFileClose(file);
		ret = 0;
	} else {
		DBGLOG(INIT, INFO, "open /data/dump_out_001_WF0.txt fail\n");
	}
	DBGLOG(INIT, INFO, "QA_AGENT GetIQData i = %d, prDataLen = %d\n\n", i, *prDataLen);
	return ret;
#endif
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
	PUINT_8 pucDataWF0;	/* the data write into file */
	PUINT_8 pucDataWF1;	/* the data write into file */
	PUINT_8 pucDataRAWWF0;	/* the data write into file */
	PUINT_8 pucDataRAWWF1;	/* the data write into file */
	UINT_32 u4SrcOffset;	/* record the buffer offset */
	UINT_32 u4FmtLen = 0;	/* bus format length */
	UINT_32 u4CpyLen = 0;
	UINT_32 u4RemainByte;
	BOOLEAN fgAppend;
	/*INT_32 u4Iqc160WF0Q0, u4Iqc160WF1I1;*/

	static UINT_8 ucDstOffset;	/* for alignment. bcs we send 2KB data per packet,*/
								/*the data will not align in 12 bytes case. */
	static UINT_32 u4CurTimeTick;

	static ICAP_BUS_FMT icapBusData;
	UINT_32 *ptr;

	pucDataWF0 = kmalloc(150, GFP_KERNEL);
	pucDataWF1 = kmalloc(150, GFP_KERNEL);
	pucDataRAWWF0 = kmalloc(150, GFP_KERNEL);
	pucDataRAWWF1 = kmalloc(150, GFP_KERNEL);

	if ((!pucDataWF0) || (!pucDataWF1) ||
	    (!pucDataRAWWF0) || (!pucDataRAWWF1)) {
		DBGLOG(INIT, ERROR, "kmalloc failed.\n");
		kfree(pucDataWF0);
		kfree(pucDataWF1);
		kfree(pucDataRAWWF0);
		kfree(pucDataRAWWF1);
		return -1;
	}

	fgAppend = TRUE;

	DBGLOG(RFTEST, INFO, "TsfRawData2IqFmt : prEventDumpMem->u4RemainLength = %u\n",
				prEventDumpMem->u4RemainLength);

	if (prEventDumpMem->ucFragNum == 1) {

		u4CurTimeTick = kalGetTimeTick();
		/* Store memory dump into sdcard,
		 * path /sdcard/dump_<current  system tick>_<memory address>_<memory length>.hex
		 */
#if defined(LINUX)

		/*if blbist mkdir undre /data/blbist, the dump files wouls put on it */
		sprintf(aucPathWF0, "/dump_out_%05hd_WF0.txt", g_u2DumpIndex);
		sprintf(aucPathWF1, "/dump_out_%05hd_WF1.txt", g_u2DumpIndex);
		DBGLOG(RFTEST, INFO, "kalCheckPath(aucPathWF0) == %d\n", kalCheckPath(aucPathWF0));
		if (kalCheckPath(aucPathWF0) == -1) {
			kalMemSet(aucPathWF0, 0x00, sizeof(aucPathWF0));
			sprintf(aucPathWF0, "/data/dump_out_%05hd_WF0.txt", g_u2DumpIndex);
		} else
			kalTrunkPath(aucPathWF0);
		DBGLOG(RFTEST, INFO, "kalCheckPath(aucPathWF1) == %d\n", kalCheckPath(aucPathWF1));
		if (kalCheckPath(aucPathWF1) == -1) {
			kalMemSet(aucPathWF1, 0x00, sizeof(aucPathWF1));
			sprintf(aucPathWF1, "/data/dump_out_%05hd_WF1.txt", g_u2DumpIndex);
		} else
			kalTrunkPath(aucPathWF1);

		sprintf(aucPathRAWWF0, "/dump_RAW_%05hd_WF0.txt", g_u2DumpIndex);
		sprintf(aucPathRAWWF1, "/dump_RAW_%05hd_WF1.txt", g_u2DumpIndex);
		DBGLOG(RFTEST, INFO, "kalCheckPath(aucPathRAWWF0) == %d\n", kalCheckPath(aucPathRAWWF0));
		if (kalCheckPath(aucPathRAWWF0) == -1) {
			kalMemSet(aucPathRAWWF0, 0x00, sizeof(aucPathRAWWF0));
			sprintf(aucPathRAWWF0, "/data/dump_RAW_%05hd_WF0.txt", g_u2DumpIndex);
		} else
			kalTrunkPath(aucPathRAWWF0);
		DBGLOG(RFTEST, INFO, "kalCheckPath(aucPathRAWWF1) == %d\n", kalCheckPath(aucPathRAWWF1));
		if (kalCheckPath(aucPathRAWWF1) == -1) {
			kalMemSet(aucPathRAWWF1, 0x00, sizeof(aucPathRAWWF1));
			sprintf(aucPathRAWWF1, "/data/dump_RAW_%05hd_WF1.txt", g_u2DumpIndex);
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
	DBGLOG(RFTEST, INFO, ": ==> (prEventDumpMem->eIcapContent = %x)\n", prEventDumpMem->eIcapContent);
	/*prEventDumpMem->eIcapContent = ICAP_CONTENT_ADC;*/

	for (u4SrcOffset = 0, u4RemainByte = prEventDumpMem->u4Length; u4RemainByte > 0;) {
		u4FmtLen = 4;
		if (u4RemainByte & 0x80000000) {
			u4RemainByte = 0;
			u4CpyLen = 0;
		} else {
			u4CpyLen = (u4RemainByte >= u4FmtLen) ? u4FmtLen : u4RemainByte;
		}

		DBGLOG(RFTEST, TRACE, "TsfRawData2IqFmt : u4CpyLen = %u\n", u4CpyLen);

		memcpy(&icapBusData + ucDstOffset, &prEventDumpMem->aucBuffer[0] + u4SrcOffset, u4CpyLen);
		if (prEventDumpMem->eIcapContent == ICAP_CONTENT_FIIQ ||
		    prEventDumpMem->eIcapContent == ICAP_CONTENT_FDIQ) {
			/* sprintf(pucDataWF0, "%8d,%8d\n",
			 *icapBusData.rIqcBusData.u4Iqc0I, icapBusData.rIqcBusData.u4Iqc0Q);
			 */
			if (icapBusData.rIqcBusData.u4Iqc0I & (1<<10))
				g_au4I0Data[0][u4IQDataIndex] = (UINT_32)icapBusData.rIqcBusData.u4Iqc0I | 0xFFFFF800;
			else
				g_au4I0Data[0][u4IQDataIndex] = icapBusData.rIqcBusData.u4Iqc0I;

			if (icapBusData.rIqcBusData.u4Iqc0Q & (1<<10))
				g_au4Q0Data[0][u4IQDataIndex] = (UINT_32)icapBusData.rIqcBusData.u4Iqc0Q | 0xFFFFF800;
			else
				g_au4Q0Data[0][u4IQDataIndex] = icapBusData.rIqcBusData.u4Iqc0Q;
		} else if (prEventDumpMem->eIcapContent == ICAP_CONTENT_SPECTRUM) {
			sprintf(pucDataWF0, "%8d,%8d\n", icapBusData.rSpectrumBusData.u4DcocI,
			icapBusData.rSpectrumBusData.u4DcocQ);
		} else if (prEventDumpMem->eIcapContent == ICAP_CONTENT_ADC) {
			if (icapBusData.rPackedAdcBusData.u4AdcI0T0 & (1<<10))
				g_au4I0Data[0][u4IQDataIndex] =
						(UINT_32)icapBusData.rPackedAdcBusData.u4AdcI0T0 | 0xFFFFFC00;
			else
				g_au4I0Data[0][u4IQDataIndex] = icapBusData.rPackedAdcBusData.u4AdcI0T0;

			if (icapBusData.rPackedAdcBusData.u4AdcQ0T0 & (1<<10))
				g_au4Q0Data[0][u4IQDataIndex] =
						(UINT_32)icapBusData.rPackedAdcBusData.u4AdcQ0T0 | 0xFFFFFC00;
			else
				g_au4Q0Data[0][u4IQDataIndex] = icapBusData.rPackedAdcBusData.u4AdcQ0T0;
		} else if (prEventDumpMem->eIcapContent == ICAP_CONTENT_TOAE) {
			/* actually, this is DCOC. we take TOAE as DCOC */
			sprintf(pucDataWF0, "%8d,%8d\n", icapBusData.rAdcBusData.u4Dcoc0I,
				icapBusData.rAdcBusData.u4Dcoc0Q);
			sprintf(pucDataWF1, "%8d,%8d\n", icapBusData.rAdcBusData.u4Dcoc1I,
				icapBusData.rAdcBusData.u4Dcoc1Q);
		}
#if 0
		if (u4CpyLen == u4FmtLen) {/* the data format is complete */
			DBGLOG(RFTEST, INFO, "pucDataWF0 : %s\n", pucDataWF0);
			/* DBGLOG(RFTEST, INFO, "pucDataWF1 : %s\n", pucDataWF1); */

			/* kalWriteToFile(aucPathWF0, fgAppend, pucDataWF0, strlen(pucDataWF0)); */
			/* kalWriteToFile(aucPathWF1, fgAppend, pucDataWF1, strlen(pucDataWF1)); */
		}
#endif
		ptr = (PUINT_32)(&prEventDumpMem->aucBuffer[0] + u4SrcOffset);
		sprintf(pucDataRAWWF0, "%08x%08x%08x\n", *(ptr + 2), *(ptr + 1), *ptr);
		/* kalWriteToFile(aucPathRAWWF0, fgAppend, pucDataRAWWF0, strlen(pucDataRAWWF0)); */
		/* kalWriteToFile(aucPathRAWWF1, fgAppend, pucDataRAWWF1, strlen(pucDataRAWWF1)); */

		u4IQDataIndex += 1;
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

	DBGLOG(RFTEST, INFO, "nicEventQueryMemDump\n");

	ASSERT(prAdapter);
	ASSERT(pucEventBuf);

	sprintf(aucPath, "/data/blbist/dump_%05d.hex", g_u2DumpIndex);

	prEventDumpMem = (P_EVENT_DUMP_MEM_T) (pucEventBuf);

	if (kalCheckPath(aucPath) == -1) {
		DBGLOG(RFTEST, INFO, "kalCheckPath(/data/blbist/dump_) == -1\n");
		kalMemSet(aucPath, 0x00, 256);
		sprintf(aucPath, "/data/dump_%05d.hex", g_u2DumpIndex);
	}

	DBGLOG(RFTEST, INFO, "prEventDumpMem->ucFragNum = %d\n", prEventDumpMem->ucFragNum);
	if (prEventDumpMem->ucFragNum == 1) {
		/* Store memory dump into sdcard,
		 * path /sdcard/dump_<current  system tick>_<memory address>_<memory length>.hex
		 */
		u4CurTimeTick = kalGetTimeTick();
#if defined(LINUX)
		DBGLOG(RFTEST, INFO,
					"defined(LINUX): if blbist mkdir undre /data/blbist, the dump files wouls put on it\n");

		/*if blbist mkdir undre /data/blbist, the dump files wouls put on it */
		sprintf(aucPath, "/data/blbist/dump_%05d.hex", g_u2DumpIndex);
		if (kalCheckPath(aucPath) == -1) {
			kalMemSet(aucPath, 0x00, 256);
			sprintf(aucPath, "/data/dump_%05d.hex", g_u2DumpIndex);
		}
#else
		DBGLOG(RFTEST, INFO, "defined(!LINUX): kal_sprintf_ddk(aucPath, sizeof(aucPath)\n");
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
	static UINT_8 aucPath[256];
	/*static UINT_8 aucPath_done[300];*/
	static UINT_32 u4CurTimeTick;
	P_GLUE_INFO_T prGlueInfo;
	P_EVENT_DUMP_MEM_T prEventDumpMem;
	UINT_32 u4QueryInfoLen;

	DBGLOG(RFTEST, INFO, "nicCmdEventQueryMemDump\n");

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (1) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEventDumpMem = (P_EVENT_DUMP_MEM_T) (pucEventBuf);

		u4QueryInfoLen = sizeof(P_PARAM_CUSTOM_MEM_DUMP_STRUCT_T);

		DBGLOG(RFTEST, INFO, "prEventDumpMem->ucFragNum = %d\n", prEventDumpMem->ucFragNum);
		if (prEventDumpMem->ucFragNum == 1) {
			/* Store memory dump into sdcard,
			 * path /sdcard/dump_<current  system tick>_<memory address>_<memory length>.hex
			 */
			u4CurTimeTick = kalGetTimeTick();
#if defined(LINUX)
			/* PeiHsuan add for avoiding out of memory 20160801 */

			if (g_u2DumpIndex >= 20)
				g_u2DumpIndex = 0;

			DBGLOG(RFTEST, INFO,
						"defined(LINUX) : if blbist mkdir undre /data/blbist, the dump files wouls put on it\n");
			/*if blbist mkdir undre /data/blbist, the dump files wouls put on it */
			sprintf(aucPath, "/data/blbist/dump_%05hd.hex", g_u2DumpIndex);
			if (kalCheckPath(aucPath) == -1) {
				kalMemSet(aucPath, 0x00, 256);
				sprintf(aucPath, "/data/dump_%05hd.hex", g_u2DumpIndex);
			} else
				kalTrunkPath(aucPath);
			DBGLOG(INIT, INFO, "iCap Create New Dump File dump_%05hd.hex\n", g_u2DumpIndex);
#else
			DBGLOG(RFTEST, INFO, "kal_sprintf_ddk(aucPath, sizeof(aucPath),\n");
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

				/*
				 * the oid would be complete only in oid-trigger  mode,
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
	UINT_32 u4LinkScore;
	UINT_32 u4TotalError;
	UINT_32 u4TxExceedThresholdCount;
	UINT_32 u4TxTotalCount;
	UINT_32 u4QueryInfoLen;
	P_EVENT_STA_STATISTICS_T prEvent;
	P_GLUE_INFO_T prGlueInfo;
	P_PARAM_GET_STA_STATISTICS prStaStatistics;
	ENUM_WMM_ACI_T eAci;
	P_STA_RECORD_T prStaRec;
#ifdef CFG_SUPPORT_LINK_QUALITY_MONITOR
	/* link quality monitor */
	struct WIFI_LINK_QUALITY_INFO *prLinkQualityInfo;
#endif

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

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

		prStaStatistics->u4TransmitCount = prEvent->u4TransmitCount;
		prStaStatistics->u4TransmitFailCount = prEvent->u4TransmitFailCount;

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
#if CFG_SUPPORT_WFD		/* dump statistics for WFD */
		if (prAdapter->rWifiVar.rWfdConfigureSettings.ucWfdEnable == 1) {
			/* Calcute Link Score */
			u4TxExceedThresholdCount = prStaStatistics->u4TxExceedThresholdCount;
			u4TxTotalCount = prStaStatistics->u4TxTotalCount;
			u4TotalError = prStaStatistics->u4TxFailCount + prStaStatistics->u4TxLifeTimeoutCount;

			/* u4LinkScore 10~100 , ExceedThreshold ratio 0~90 only */
			/* u4LinkScore 0~9    , Drop packet ratio 0~9 and all packets exceed threshold */
			if (u4TxTotalCount) {
				if (u4TxExceedThresholdCount <= u4TxTotalCount)
					u4LinkScore = (90 -
						      ((u4TxExceedThresholdCount * 90) / u4TxTotalCount));
				else
					u4LinkScore = 0;
			} else
				u4LinkScore = 90;

			u4LinkScore += 10;
			if (u4LinkScore == 10) {
				if (u4TotalError <= u4TxTotalCount)
					u4LinkScore = (10 - ((u4TotalError * 10) / u4TxTotalCount));
				else
					u4LinkScore = 0;
			}

			if (u4LinkScore > 100)
				u4LinkScore = 100;

			DBGLOG(P2P, INFO, "link_score=%u, rssi=%u, rate=%u, threshold_cnt=%u, fail_cnt=%u\n",
			       u4LinkScore, prStaStatistics->ucRcpi, prStaStatistics->u2LinkSpeed,
			       prStaStatistics->u4TxExceedThresholdCount, prStaStatistics->u4TxFailCount);
			DBGLOG(P2P, INFO, "timeout_cnt=%u, apt=%u, aat=%u, total_cnt=%u\n",
			       prStaStatistics->u4TxLifeTimeoutCount, prStaStatistics->u4TxAverageProcessTime,
			       prStaStatistics->u4TxAverageAirTime, prStaStatistics->u4TxTotalCount);
		}
#endif
#ifdef CFG_SUPPORT_LINK_QUALITY_MONITOR
		/* link quality monitor */
		prLinkQualityInfo = &(prAdapter->rLinkQualityInfo);
		prLinkQualityInfo->u4CurTxRate = prEvent->u2LinkSpeed * 5;
#endif
	}

	if (prCmdInfo->fgIsOid)
		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);

}

/*the same as gen4m*/
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
	P_EVENT_LTE_SAFE_CHN_T prEvent;
	P_PARAM_GET_CHN_INFO prLteSafeChnInfo;
	UINT_8 ucIdx = 0;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo;

	if ((prAdapter == NULL)
		|| (prCmdInfo == NULL)
		|| (pucEventBuf == NULL)
		|| (prCmdInfo->pvInformationBuffer == NULL)) {
		ASSERT(FALSE);
		return;
	}

		prEvent = (P_EVENT_LTE_SAFE_CHN_T) pucEventBuf;	/* FW responsed data */

		prLteSafeChnInfo = (P_PARAM_GET_CHN_INFO) prCmdInfo->pvInformationBuffer;
		if (prLteSafeChnInfo->ucRoleIndex >= BSS_P2P_NUM) {
			ASSERT(FALSE);
			return;
		}

		prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prLteSafeChnInfo->ucRoleIndex);

		/* Statistics from FW is valid */
		if (prEvent->u4Flags & BIT(0)) {
			P_CMD_LTE_SAFE_CHN_INFO_T prLteSafeChnList;

			prLteSafeChnList = &prLteSafeChnInfo->rLteSafeChnList;
			for (ucIdx = 0; ucIdx < 5; ucIdx++) {
				prLteSafeChnList->au4SafeChannelBitmask[ucIdx]
					= prEvent->rLteSafeChn.
						au4SafeChannelBitmask[ucIdx];

				DBGLOG(NIC, INFO,
					"[ACS]LTE safe channels[%d]=0x%08x\n",
					ucIdx,
					prLteSafeChnList->au4SafeChannelBitmask[ucIdx]);
			}

		} else {
			DBGLOG(NIC, ERROR, "FW's event is NOT valid.\n");
		}

		p2pFunProcessAcsReport(prAdapter,
						prLteSafeChnInfo->ucRoleIndex,
						prLteSafeChnInfo,
						&(prP2pRoleFsmInfo->rAcsReqInfo));
}


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

VOID nicEventUpdateFwInfo(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	UINT_8 i;
	P_EVENT_UPDATE_FW_INFO_T prEventUpdateFwInfo;

	prEventUpdateFwInfo = (P_EVENT_UPDATE_FW_INFO_T)(prEvent->aucBuffer);
	for (i = 0; i < (HW_BSSID_NUM + 1); i++) {
		if (prAdapter->aprBssInfo[i]) {
			prAdapter->aprBssInfo[i]->u4CoexPhyRateLimit = prEventUpdateFwInfo->au4PhyRateLimit[i];
			DBGLOG(NIC, TRACE, "Coex:BSS[%d]R:%d\n", i, prAdapter->aprBssInfo[i]->u4CoexPhyRateLimit);
		}
	}
}

#if CFG_SUPPORT_REPLAY_DETECTION
VOID nicCmdEventSetAddKey(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_WIFI_CMD_T prWifiCmd = NULL;
	P_CMD_802_11_KEY prCmdKey = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	struct GL_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	if (prCmdInfo->fgIsOid) {
		/* Update Set Information Length */
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery, prCmdInfo->u4InformationBufferLength, WLAN_STATUS_SUCCESS);
	}

	prGlueInfo = prAdapter->prGlueInfo;
	prDetRplyInfo = &prGlueInfo->prDetRplyInfo;
	if (pucEventBuf) {
		prWifiCmd = (P_WIFI_CMD_T) (pucEventBuf);
		prCmdKey = (P_CMD_802_11_KEY) (prWifiCmd->aucBuffer);
		if (!prCmdKey->ucKeyType && prCmdKey->ucKeyId >= 0 && prCmdKey->ucKeyId < 4) {
			/* Only save data broadcast key info. ucKeyType == 1 means unicast key
			** ucKeyId == 4 or ucKeyId == 5 means it is a PMF key
			*/
			prDetRplyInfo->ucCurKeyId = prCmdKey->ucKeyId;
			prDetRplyInfo->ucKeyType = prCmdKey->ucKeyType;
			prDetRplyInfo->arReplayPNInfo[prCmdKey->ucKeyId].fgRekey = TRUE;
			kalMemCopy(prDetRplyInfo->arReplayPNInfo[prCmdKey->ucKeyId].auPN,
				   prCmdKey->aucKeyRsc, 16);
			DBGLOG_MEM8(NIC, TRACE,
				    prDetRplyInfo->arReplayPNInfo[prCmdKey->ucKeyId].auPN,
				    8);
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
#endif

#if CFG_SUPPORT_REPORT_MISC
VOID nicCmdEventReportMisc(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	ASSERT(prAdapter);

	if (pucEventBuf) {
		UINT_64 now = 0;
		struct EVENT_REPORT_MISC *prEvent = (struct EVENT_REPORT_MISC *)pucEventBuf;
		struct EVENT_REPORT_MISC *prReportMisc = &prAdapter->rReportMiscSet.reportMisc;
		char *periodStr = NULL;

		switch (prAdapter->rReportMiscSet.eQueryNum) {
		case REPORT_AUTHASSOC_START:
		case REPORT_4WAYHS_START:
		case REPORT_DHCP_START:
			prReportMisc->ucFwVerMajor = prEvent->ucFwVerMajor;
			prReportMisc->ucFwVerMinor = prEvent->ucFwVerMinor;
			prReportMisc->u2FwVerBeta = prEvent->u2FwVerBeta;
			prReportMisc->u4MdrdyCnt = prEvent->u4RxMpduCnt;
			prReportMisc->u4ChannelIdleCnt = prEvent->u4ChannelIdleCnt;
			prAdapter->rReportMiscSet.u8Ts = sched_clock();
			break;
		case REPORT_AUTHASSOC_END:
			periodStr = "auth to assoc";
			goto REPORT_MISC;
		case REPORT_4WAYHS_END:
			periodStr = "4-way flow";
			if (prEvent->cRssi)
				prAdapter->rReportMiscSet.i4Rssi = prEvent->cRssi;
			goto REPORT_MISC;
		case REPORT_DHCP_END:
			periodStr = "dhcp flow";
			if (prEvent->cRssi)
				prAdapter->rReportMiscSet.i4Rssi = prEvent->cRssi;
			/* don't break here */
REPORT_MISC:
			now = sched_clock();
			DBGLOG(NIC, TRACE, "Ver=0x%x.%x.%04x,Time=%llu,Period=%s,MDRDY=%u,SLOTID=%u,MPDU=%u,RSSI=%d\n",
				prReportMisc->ucFwVerMajor,
				prReportMisc->ucFwVerMinor,
				prReportMisc->u2FwVerBeta,
				now - prAdapter->rReportMiscSet.u8Ts,
				periodStr,
				prEvent->u4MdrdyCnt - prReportMisc->u4MdrdyCnt,
				prEvent->u4ChannelIdleCnt - prReportMisc->u4ChannelIdleCnt,
				prEvent->u4RxMpduCnt - prReportMisc->u4RxMpduCnt,
				prAdapter->rReportMiscSet.i4Rssi);
			prAdapter->rReportMiscSet.eQueryNum = 0;
			break;
		default:
			DBGLOG(NIC, WARN, "Report Misc Error, prAdapter->rReportMiscSet.eQueryNum: %d\n",
			       prAdapter->rReportMiscSet.eQueryNum);
			break;

		}
	}
	if (prCmdInfo->fgIsOid)
		kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->u4InformationBufferLength, 0, WLAN_STATUS_SUCCESS);
}
#endif
