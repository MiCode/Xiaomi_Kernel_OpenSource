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
/*! \file   gl_hook_api.h
 * \brief  This file includes private ioctl support.
 */

#ifndef _GL_HOOK_API_H
#define _GL_HOOK_API_H
#if CFG_SUPPORT_QA_TOOL
/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

INT_32 MT_ATEStart(struct net_device *prNetDev, UINT_8 *prInBuf);
INT_32 MT_ICAPStart(struct net_device *prNetDev, UINT_8 *prInBuf);
INT_32 MT_ATEStop(struct net_device *prNetDev, UINT_8 *prInBuf);
INT_32 MT_ATEStartTX(struct net_device *prNetDev, UINT_8 *prInBuf);
INT_32 MT_ATEStopTX(struct net_device *prNetDev, UINT_8 *prInBuf);
INT_32 MT_ATEStartRX(struct net_device *prNetDev, UINT_8 *prInBuf);
INT_32 MT_ATEStopRX(struct net_device *prNetDev, UINT_8 *prInBuf);
INT_32 MT_ATESetChannel(struct net_device *prNetDev, UINT_32 u4SXIdx, UINT_32 u4SetFreq);
INT_32 MT_ATESetPreamble(struct net_device *prNetDev, UINT_32 u4Mode);
INT_32 MT_ATESetSystemBW(struct net_device *prNetDev, UINT_32 u4BW);
INT_32 MT_ATESetTxLength(struct net_device *prNetDev, UINT_32 u4TxLength);
INT_32 MT_ATESetTxCount(struct net_device *prNetDev, UINT_32 u4TxCount);
INT_32 MT_ATESetTxIPG(struct net_device *prNetDev, UINT_32 u4TxIPG);
INT_32 MT_ATESetTxPower0(struct net_device *prNetDev, UINT_32 u4TxPower0);
INT_32 MT_ATESetPerPacketBW(struct net_device *prNetDev, UINT_32 u4BW);
INT_32 MT_ATEPrimarySetting(struct net_device *prNetDev, UINT_32 u4PrimaryCh);
INT_32 MT_ATESetTxGi(struct net_device *prNetDev, UINT_32 u4SetTxGi);
INT_32 MT_ATESetTxPayLoad(struct net_device *prNetDev, UINT_32 u4Gen_payload_rule, UINT_8 ucPayload);
INT_32 MT_ATESetTxSTBC(struct net_device *prNetDev, UINT_32 u4Stbc);
INT_32 MT_ATESetTxPath(struct net_device *prNetDev, UINT_32 u4Tx_path);
INT_32 MT_ATESetTxVhtNss(struct net_device *prNetDev, UINT_32 u4VhtNss);
INT_32 MT_ATESetRate(struct net_device *prNetDev, UINT_32 u4Rate);
INT_32 MT_ATESetEncodeMode(struct net_device *prNetDev, UINT_32 u4Ldpc);
INT_32 MT_ATESetiBFEnable(struct net_device *prNetDev, UINT_32 u4iBF);
INT_32 MT_ATESeteBFEnable(struct net_device *prNetDev, UINT_32 u4eBF);
INT_32 MT_ATESetMACAddress(struct net_device *prNetDev, UINT_32 u4Type, UCHAR ucAddr[]);
INT_32 MT_ATELogOnOff(struct net_device *prNetDev, UINT_32 u4Type, UINT_32 u4On_off, UINT_32 u4Size);
INT_32 MT_ATEResetTXRXCounter(struct net_device *prNetDev);
INT_32 MT_ATESetDBDCBandIndex(struct net_device *prNetDev, UINT_32 u4BandIdx);
INT_32 MT_ATESetBand(struct net_device *prNetDev, INT_32 i4Band);
INT_32 MT_ATESetTxToneType(struct net_device *prNetDev, INT_32 i4ToneType);
INT_32 MT_ATESetTxToneBW(struct net_device *prNetDev, INT_32 i4ToneFreq);
INT_32 MT_ATESetTxToneDCOffset(struct net_device *prNetDev, INT_32 i4DcOffsetI, INT_32 i4DcOffsetQ);
INT_32 MT_ATESetDBDCTxTonePower(struct net_device *prNetDev, INT_32 i4AntIndex, INT_32 i4RF_Power, INT_32 i4Digi_Power);
INT_32 MT_ATEDBDCTxTone(struct net_device *prNetDev, INT_32 i4Control);
INT_32 MT_ATESetMacHeader(struct net_device *prNetDev, UINT_32 u2FrameCtrl, UINT_32 u2DurationID, UINT_32 u4SeqCtrl);
INT_32 MT_ATE_IRRSetADC(struct net_device *prNetDev,
			UINT_32 u4WFIdx,
			UINT_32 u4ChFreq,
			UINT_32 u4BW, UINT_32 u4Sx, UINT_32 u4Band, UINT_32 u4RunType, UINT_32 u4FType);
INT_32 MT_ATE_IRRSetRxGain(struct net_device *prNetDev,
			   UINT_32 u4PgaLpfg, UINT_32 u4Lna, UINT_32 u4Band, UINT_32 u4WF_inx, UINT_32 u4Rfdgc);
INT_32 MT_ATE_IRRSetTTG(struct net_device *prNetDev,
			UINT_32 u4TTGPwrIdx, UINT_32 u4ChFreq, UINT_32 u4FIToneFreq, UINT_32 u4Band);
INT_32 MT_ATE_IRRSetTrunOnTTG(struct net_device *prNetDev, UINT_32 u4TTGOnOff, UINT_32 u4Band, UINT_32 u4WF_inx);
INT_32 MT_ATE_TMRSetting(struct net_device *prNetDev,
			 UINT_32 u4Setting, UINT_32 u4Version, UINT_32 u4MPThres, UINT_32 u4MPIter);
INT_32 MT_ATERDDStart(struct net_device *prNetDev, UINT_8 *prInBuf);
INT_32 MT_ATERDDStop(struct net_device *prNetDev, UINT_8 *prInBuf);
INT_32 MT_ATEMPSSetSeqData(struct net_device *prNetDev,
			UINT_32 u4TestNum, UINT_32 *pu4Phy, UINT_32 u4Band);
INT_32 MT_ATEMPSSetPayloadLength(struct net_device *prNetDev,
			UINT_32 u4TestNum, UINT_32 *pu4Length, UINT_32 u4Band);
INT_32 MT_ATEMPSSetPacketCount(struct net_device *prNetDev,
			UINT_32 u4TestNum, UINT_32 *pu4PktCnt, UINT_32 u4Band);
INT_32 MT_ATEMPSSetPowerGain(struct net_device *prNetDev,
			UINT_32 u4TestNum, UINT_32 *pu4PwrGain, UINT_32 u4Band);
INT_32 MT_ATEMPSSetNss(struct net_device *prNetDev,
			UINT_32 u4TestNum, UINT_32 *pu4Nss, UINT_32 u4Band);
INT_32 MT_ATEMPSSetPerpacketBW(struct net_device *prNetDev,
			UINT_32 u4TestNum, UINT_32 *pu4PerPktBW, UINT_32 u4Band);


INT_32 MT_ATEWriteEfuse(struct net_device *prNetDev, UINT_16 u2Offset, UINT_16 u2Content);


#if CFG_SUPPORT_TX_BF
INT_32 TxBfProfileTag_InValid(struct net_device *prNetDev, P_PFMU_PROFILE_TAG1 prPfmuTag1, UINT_8 ucInValid);
INT_32 TxBfProfileTag_PfmuIdx(struct net_device *prNetDev, P_PFMU_PROFILE_TAG1 prPfmuTag1, UINT_8 ucProfileIdx);
INT_32 TxBfProfileTag_TxBfType(struct net_device *prNetDev, P_PFMU_PROFILE_TAG1 prPfmuTag1, UINT_8 ucBFType);
INT_32 TxBfProfileTag_DBW(struct net_device *prNetDev, P_PFMU_PROFILE_TAG1 prPfmuTag1, UINT_8 ucBW);
INT_32 TxBfProfileTag_SuMu(struct net_device *prNetDev, P_PFMU_PROFILE_TAG1 prPfmuTag1, UINT_8 ucSuMu);
INT_32 TxBfProfileTag_Mem(struct net_device *prNetDev,
			  P_PFMU_PROFILE_TAG1 prPfmuTag1, PUINT_8 aucMemAddrColIdx, PUINT_8 aucMemAddrRowIdx);
INT_32 TxBfProfileTag_Matrix(struct net_device *prNetDev,
			     P_PFMU_PROFILE_TAG1 prPfmuTag1,
			     UINT_8 ucNrow,
			     UINT_8 ucNcol, UINT_8 ucNgroup, UINT_8 ucLM, UINT_8 ucCodeBook, UINT_8 ucHtcExist);
INT_32 TxBfProfileTag_SNR(struct net_device *prNetDev,
			  P_PFMU_PROFILE_TAG1 prPfmuTag1,
			  UINT_8 ucSNR_STS0, UINT_8 ucSNR_STS1, UINT_8 ucSNR_STS2, UINT_8 ucSNR_STS3);
INT_32 TxBfProfileTag_SmtAnt(struct net_device *prNetDev, P_PFMU_PROFILE_TAG2 prPfmuTag2, UINT_8 ucSmartAnt);
INT_32 TxBfProfileTag_SeIdx(struct net_device *prNetDev, P_PFMU_PROFILE_TAG2 prPfmuTag2, UINT_8 ucSeIdx);
INT_32 TxBfProfileTag_RmsdThd(struct net_device *prNetDev, P_PFMU_PROFILE_TAG2 prPfmuTag2, UINT_8 ucRmsdThrd);
INT_32 TxBfProfileTag_McsThd(struct net_device *prNetDev,
			     P_PFMU_PROFILE_TAG2 prPfmuTag2, PUINT_8 pMCSThLSS, PUINT_8 pMCSThSSS);
INT_32 TxBfProfileTag_TimeOut(struct net_device *prNetDev, P_PFMU_PROFILE_TAG2 prPfmuTag2, UINT_8 ucTimeOut);
INT_32 TxBfProfileTag_DesiredBW(struct net_device *prNetDev, P_PFMU_PROFILE_TAG2 prPfmuTag2, UINT_8 ucDesiredBW);
INT_32 TxBfProfileTag_DesiredNc(struct net_device *prNetDev, P_PFMU_PROFILE_TAG2 prPfmuTag2, UINT_8 ucDesiredNc);
INT_32 TxBfProfileTag_DesiredNr(struct net_device *prNetDev, P_PFMU_PROFILE_TAG2 prPfmuTag2, UINT_8 ucDesiredNr);
INT_32 TxBfProfileTagWrite(struct net_device *prNetDev,
			   P_PFMU_PROFILE_TAG1 prPfmuTag1, P_PFMU_PROFILE_TAG2 prPfmuTag2, UINT_8 profileIdx);
INT_32 TxBfProfileTagRead(struct net_device *prNetDev, UINT_8 PfmuIdx, UINT_8 fgBFer);
INT_32 TxBfProfileDataRead(struct net_device *prNetDev,
			   UINT_8 profileIdx, UINT_8 fgBFer, UINT_8 subcarrierIdxMsb, UINT_8 subcarrierIdxLsb);
INT_32 TxBfProfileDataWrite(struct net_device *prNetDev,
			    UINT_8 profileIdx,
			    UINT_16 subcarrierIdx, UINT_16 au2Phi[6], UINT_8 aucPsi[6], UINT_8 aucDSnr[4]
);
INT_32 TxBfProfilePnRead(struct net_device *prNetDev, UINT_8 profileIdx);
INT_32 TxBfProfilePnWrite(struct net_device *prNetDev, UINT_8 ucProfileIdx, UINT_16 u2bw, UINT_16 au2XSTS[12]);

INT_32 TxBfSounding(struct net_device *prNetDev, UINT_8 ucSuMu,	/* 0/1/2/3 */
		    UINT_8 ucNumSta,	/* 00~04 */
		    UINT_8 ucSndInterval,	/* 00~FF */
		    UINT_8 ucWLan0,	/* 00~7F */
		    UINT_8 ucWLan1,	/* 00~7F */
		    UINT_8 ucWLan2,	/* 00~7F */

		    UINT_8 ucWLan3	/* 00~7F */
);
INT_32 TxBfSoundingStop(struct net_device *prNetDev);
INT_32 TxBfTxApply(struct net_device *prNetDev, UINT_8 ucWlanId, UINT_8 fgETxBf, UINT_8 fgITxBf, UINT_8 fgMuTxBf);

INT_32 TxBfManualAssoc(struct net_device *prNetDev,
		       UINT_8 aucMac[MAC_ADDR_LEN],
		       UINT_8 ucType,
		       UINT_8 ucWtbl,
		       UINT_8 ucOwnmac,
		       UINT_8 ucPhyMode,
		       UINT_8 ucBw,
		       UINT_8 ucNss, UINT_8 ucPfmuId, UINT_8 ucMarate, UINT_8 ucSpeIdx, UINT_8 ucRca2, UINT_8 ucRv);

INT_32 TxBfPfmuMemAlloc(struct net_device *prNetDev, UINT_8 ucSuMuMode, UINT_8 ucWlanIdx);

INT_32 TxBfPfmuMemRelease(struct net_device *prNetDev, UINT_8 ucWlanId);

INT_32 DevInfoUpdate(struct net_device *prNetDev, UINT_8 ucOwnMacIdx, UINT_8 fgBand, UINT_8 aucMacAddr[MAC_ADDR_LEN]);

INT_32 BssInfoUpdate(struct net_device *prNetDev, UINT_8 u4OwnMacIdx, UINT_8 u4BssIdx, UINT_8 u4BssId[MAC_ADDR_LEN]);

INT_32 StaRecCmmUpdate(struct net_device *prNetDev,
		       UINT_8 ucWlanId, UINT_8 ucBssId, UINT_8 u4Aid, UINT_8 aucMacAddr[MAC_ADDR_LEN]
);

INT_32 StaRecBfUpdate(struct net_device *prNetDev,
		      STA_REC_BF_UPD_ARGUMENT rStaRecBfUpdArg, UINT_8 aucMemRow[4], UINT_8 aucMemCol[4]
);

#endif
#endif /*CFG_SUPPORT_QA_TOOL */
#endif /* _GL_HOOK_API_H */
