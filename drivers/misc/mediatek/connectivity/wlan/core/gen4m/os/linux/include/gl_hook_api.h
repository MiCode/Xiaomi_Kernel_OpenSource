/*******************************************************************************
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
 ******************************************************************************/
/*! \file   gl_hook_api.h
 *    \brief  This file includes private ioctl support.
 */

#ifndef _GL_HOOK_API_H
#define _GL_HOOK_API_H
#if CFG_SUPPORT_QA_TOOL
/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

int32_t MT_ATEStart(struct net_device *prNetDev,
		    uint8_t *prInBuf);
int32_t MT_ICAPStart(struct net_device *prNetDev,
		     uint8_t *prInBuf);
int32_t MT_ICAPCommand(struct net_device *prNetDev,
		       uint8_t *prInBuf);
int32_t MT_ATEStop(struct net_device *prNetDev,
		   uint8_t *prInBuf);
int32_t MT_ATEStartTX(struct net_device *prNetDev,
		      uint8_t *prInBuf);
int32_t MT_ATEStopTX(struct net_device *prNetDev,
		     uint8_t *prInBuf);
int32_t MT_ATEStartRX(struct net_device *prNetDev,
		      uint8_t *prInBuf);
int32_t MT_ATEStopRX(struct net_device *prNetDev,
		     uint8_t *prInBuf);
int32_t MT_ATESetChannel(struct net_device *prNetDev,
			 uint32_t u4SXIdx, uint32_t u4SetFreq);
int32_t MT_ATESetPreamble(struct net_device *prNetDev,
			  uint32_t u4Mode);
int32_t MT_ATESetSystemBW(struct net_device *prNetDev,
			  uint32_t u4BW);
int32_t MT_ATESetTxLength(struct net_device *prNetDev,
			  uint32_t u4TxLength);
int32_t MT_ATESetTxCount(struct net_device *prNetDev,
			 uint32_t u4TxCount);
int32_t MT_ATESetTxIPG(struct net_device *prNetDev,
		       uint32_t u4TxIPG);
int32_t MT_ATESetTxPath(struct net_device *prNetDev,
			uint32_t u4Tx_path);
int32_t MT_ATESetRxPath(struct net_device *prNetDev,
			uint32_t u4Rx_path);
int32_t MT_ATESetTxPower0(struct net_device *prNetDev,
			  uint32_t u4TxPower0);
int32_t MT_ATESetPerPacketBW(struct net_device *prNetDev,
			     uint32_t u4BW);
int32_t MT_ATEPrimarySetting(struct net_device *prNetDev,
			     uint32_t u4PrimaryCh);
int32_t MT_ATESetTxGi(struct net_device *prNetDev,
		      uint32_t u4SetTxGi);
int32_t MT_ATESetTxPayLoad(struct net_device *prNetDev,
			   uint32_t u4Gen_payload_rule, uint8_t ucPayload);
int32_t MT_ATESetTxSTBC(struct net_device *prNetDev,
			uint32_t u4Stbc);
int32_t MT_ATESetTxPath(struct net_device *prNetDev,
			uint32_t u4Tx_path);
int32_t MT_ATESetTxVhtNss(struct net_device *prNetDev,
			  uint32_t u4VhtNss);
int32_t MT_ATESetRate(struct net_device *prNetDev,
		      uint32_t u4Rate);
int32_t MT_ATESetEncodeMode(struct net_device *prNetDev,
			    uint32_t u4Ldpc);
int32_t MT_ATESetiBFEnable(struct net_device *prNetDev,
			   uint32_t u4iBF);
int32_t MT_ATESeteBFEnable(struct net_device *prNetDev,
			   uint32_t u4eBF);
int32_t MT_ATESetMACAddress(struct net_device *prNetDev,
			    uint32_t u4Type, uint8_t ucAddr[]);
int32_t MT_ATELogOnOff(struct net_device *prNetDev,
		       uint32_t u4Type, uint32_t u4On_off, uint32_t u4Size);
int32_t MT_ATEGetDumpRXV(struct net_device *prNetDev,
			 uint8_t *pData, int32_t *pCount);
int32_t MT_ATEResetTXRXCounter(struct net_device *prNetDev);
int32_t MT_ATESetDBDCBandIndex(struct net_device *prNetDev,
			       uint32_t u4BandIdx);
int32_t MT_ATESetBand(struct net_device *prNetDev,
		      int32_t i4Band);
int32_t MT_ATESetTxToneType(struct net_device *prNetDev,
			    int32_t i4ToneType);
int32_t MT_ATESetTxToneBW(struct net_device *prNetDev,
			  int32_t i4ToneFreq);
int32_t MT_ATESetTxToneDCOffset(struct net_device *prNetDev,
				int32_t i4DcOffsetI, int32_t i4DcOffsetQ);
int32_t MT_ATESetDBDCTxTonePower(struct net_device *prNetDev,
		int32_t i4AntIndex, int32_t i4RF_Power, int32_t i4Digi_Power);
int32_t MT_ATEDBDCTxTone(struct net_device *prNetDev,
			 int32_t i4Control);
int32_t MT_ATESetMacHeader(struct net_device *prNetDev,
			   uint32_t u2FrameCtrl, uint32_t u2DurationID,
			   uint32_t u4SeqCtrl);
int32_t MT_ATE_IRRSetADC(struct net_device *prNetDev,
			 uint32_t u4WFIdx,
			 uint32_t u4ChFreq,
			 uint32_t u4BW, uint32_t u4Sx, uint32_t u4Band,
			 uint32_t u4RunType, uint32_t u4FType);
int32_t MT_ATE_IRRSetRxGain(struct net_device *prNetDev,
			    uint32_t u4PgaLpfg, uint32_t u4Lna, uint32_t u4Band,
			    uint32_t u4WF_inx, uint32_t u4Rfdgc);
int32_t MT_ATE_IRRSetTTG(struct net_device *prNetDev,
			 uint32_t u4TTGPwrIdx, uint32_t u4ChFreq,
			 uint32_t u4FIToneFreq, uint32_t u4Band);
int32_t MT_ATE_IRRSetTrunOnTTG(struct net_device *prNetDev, uint32_t u4TTGOnOff,
				uint32_t u4Band, uint32_t u4WF_inx);
int32_t MT_ATE_TMRSetting(struct net_device *prNetDev, uint32_t u4Setting,
		uint32_t u4Version, uint32_t u4MPThres, uint32_t u4MPIter);
int32_t MT_ATERDDStart(struct net_device *prNetDev,
		       uint8_t *prInBuf);
int32_t MT_ATERDDStop(struct net_device *prNetDev,
		      uint8_t *prInBuf);
int32_t MT_ATEMPSSetSeqData(struct net_device *prNetDev, uint32_t u4TestNum,
				uint32_t *pu4Phy, uint32_t u4Band);
int32_t MT_ATEMPSSetPayloadLength(struct net_device *prNetDev,
		uint32_t u4TestNum, uint32_t *pu4Length, uint32_t u4Band);
int32_t MT_ATEMPSSetPacketCount(struct net_device *prNetDev, uint32_t u4TestNum,
				uint32_t *pu4PktCnt, uint32_t u4Band);
int32_t MT_ATEMPSSetPowerGain(struct net_device *prNetDev, uint32_t u4TestNum,
			uint32_t *pu4PwrGain, uint32_t u4Band);
int32_t MT_ATEMPSSetNss(struct net_device *prNetDev,
			uint32_t u4TestNum, uint32_t *pu4Nss, uint32_t u4Band);
int32_t MT_ATEMPSSetPerpacketBW(struct net_device *prNetDev, uint32_t u4TestNum,
			uint32_t *pu4PerPktBW, uint32_t u4Band);


int32_t MT_ATEWriteEfuse(struct net_device *prNetDev,
			 uint16_t u2Offset, uint16_t u2Content);
int32_t MT_ATESetTxTargetPower(struct net_device *prNetDev,
			       uint8_t ucTxTargetPower);

#if CFG_SUPPORT_ANT_SWAP
int32_t MT_ATESetAntSwap(struct net_device *prNetDev, uint32_t u4Ant);
#endif

#if (CFG_SUPPORT_DFS_MASTER == 1)
int32_t MT_ATESetRddReport(struct net_device *prNetDev,
			   uint8_t ucDbdcIdx);
int32_t MT_ATESetRadarDetectMode(struct net_device
				 *prNetDev, uint8_t ucRadarDetectMode);
#endif


#if CFG_SUPPORT_TX_BF
int32_t TxBfProfileTag_InValid(struct net_device *prNetDev,
			       union PFMU_PROFILE_TAG1 *prPfmuTag1,
			       uint8_t ucInValid);
int32_t TxBfProfileTag_PfmuIdx(struct net_device *prNetDev,
			       union PFMU_PROFILE_TAG1 *prPfmuTag1,
			       uint8_t ucProfileIdx);
int32_t TxBfProfileTag_TxBfType(struct net_device *prNetDev,
				union PFMU_PROFILE_TAG1 *prPfmuTag1,
				uint8_t ucBFType);
int32_t TxBfProfileTag_DBW(struct net_device *prNetDev,
			   union PFMU_PROFILE_TAG1 *prPfmuTag1, uint8_t ucBW);
int32_t TxBfProfileTag_SuMu(struct net_device *prNetDev,
			union PFMU_PROFILE_TAG1 *prPfmuTag1, uint8_t ucSuMu);
int32_t TxBfProfileTag_Mem(struct net_device *prNetDev,
			union PFMU_PROFILE_TAG1 *prPfmuTag1,
			uint8_t *aucMemAddrColIdx, uint8_t *aucMemAddrRowIdx);
int32_t TxBfProfileTag_Matrix(struct net_device *prNetDev,
			      union PFMU_PROFILE_TAG1 *prPfmuTag1,
			      uint8_t ucNrow,
			      uint8_t ucNcol, uint8_t ucNgroup, uint8_t ucLM,
			      uint8_t ucCodeBook, uint8_t ucHtcExist);
int32_t TxBfProfileTag_SNR(struct net_device *prNetDev,
			   union PFMU_PROFILE_TAG1 *prPfmuTag1,
			   uint8_t ucSNR_STS0, uint8_t ucSNR_STS1,
			   uint8_t ucSNR_STS2,
			   uint8_t ucSNR_STS3);
int32_t TxBfProfileTag_SmtAnt(struct net_device *prNetDev,
			      union PFMU_PROFILE_TAG2 *prPfmuTag2,
			      uint8_t ucSmartAnt);
int32_t TxBfProfileTag_SeIdx(struct net_device *prNetDev,
			     union PFMU_PROFILE_TAG2 *prPfmuTag2,
			     uint8_t ucSeIdx);
int32_t TxBfProfileTag_RmsdThd(struct net_device *prNetDev,
			       union PFMU_PROFILE_TAG2 *prPfmuTag2,
			       uint8_t ucRmsdThrd);
int32_t TxBfProfileTag_McsThd(struct net_device *prNetDev,
			      union PFMU_PROFILE_TAG2 *prPfmuTag2,
			      uint8_t *pMCSThLSS,
			      uint8_t *pMCSThSSS);
int32_t TxBfProfileTag_TimeOut(struct net_device *prNetDev,
			       union PFMU_PROFILE_TAG2 *prPfmuTag2,
			       uint8_t ucTimeOut);
int32_t TxBfProfileTag_DesiredBW(struct net_device
				 *prNetDev, union PFMU_PROFILE_TAG2 *prPfmuTag2,
				 uint8_t ucDesiredBW);
int32_t TxBfProfileTag_DesiredNc(struct net_device
				 *prNetDev, union PFMU_PROFILE_TAG2 *prPfmuTag2,
				 uint8_t ucDesiredNc);
int32_t TxBfProfileTag_DesiredNr(struct net_device
				 *prNetDev, union PFMU_PROFILE_TAG2 *prPfmuTag2,
				 uint8_t ucDesiredNr);
int32_t TxBfProfileTagWrite(struct net_device *prNetDev,
			    union PFMU_PROFILE_TAG1 *prPfmuTag1,
			    union PFMU_PROFILE_TAG2 *prPfmuTag2,
			    uint8_t profileIdx);
int32_t TxBfProfileTagRead(struct net_device *prNetDev,
			   uint8_t PfmuIdx, uint8_t fgBFer);
int32_t TxBfProfileDataRead(struct net_device *prNetDev,
			    uint8_t profileIdx, uint8_t fgBFer,
			    uint8_t subcarrierIdxMsb, uint8_t subcarrierIdxLsb);
int32_t TxBfProfileDataWrite(struct net_device *prNetDev,
			     uint8_t profileIdx,
			     uint16_t subcarrierIdx,
			     uint16_t au2Phi[6],
			     uint8_t aucPsi[6], uint8_t aucDSnr[4]
			    );
int32_t TxBfProfilePnRead(struct net_device *prNetDev,
			  uint8_t profileIdx);
int32_t TxBfProfilePnWrite(struct net_device *prNetDev, uint8_t ucProfileIdx,
			   uint16_t u2bw, uint16_t au2XSTS[12]);

int32_t TxBfSounding(struct net_device *prNetDev,
		     uint8_t ucSuMu,	/* 0/1/2/3 */
		     uint8_t ucNumSta,	/* 00~04 */
		     uint8_t ucSndInterval,	/* 00~FF */
		     uint8_t ucWLan0,	/* 00~7F */
		     uint8_t ucWLan1,	/* 00~7F */
		     uint8_t ucWLan2,	/* 00~7F */

		     uint8_t ucWLan3	/* 00~7F */
		    );
int32_t TxBfSoundingStop(struct net_device *prNetDev);
int32_t TxBfTxApply(struct net_device *prNetDev,
		    uint8_t ucWlanId, uint8_t fgETxBf, uint8_t fgITxBf,
		    uint8_t fgMuTxBf);

int32_t TxBfManualAssoc(struct net_device *prNetDev,
			uint8_t aucMac[MAC_ADDR_LEN],
			uint8_t ucType,
			uint8_t ucWtbl,
			uint8_t ucOwnmac,
			uint8_t ucPhyMode,
			uint8_t ucBw,
			uint8_t ucNss, uint8_t ucPfmuId, uint8_t ucMarate,
			uint8_t ucSpeIdx, uint8_t ucRca2, uint8_t ucRv);

int32_t TxBfPfmuMemAlloc(struct net_device *prNetDev,
			 uint8_t ucSuMuMode, uint8_t ucWlanIdx);

int32_t TxBfPfmuMemRelease(struct net_device *prNetDev,
			   uint8_t ucWlanId);

int32_t DevInfoUpdate(struct net_device *prNetDev,
		      uint8_t ucOwnMacIdx, uint8_t fgBand,
		      uint8_t aucMacAddr[MAC_ADDR_LEN]);

int32_t BssInfoUpdate(struct net_device *prNetDev,
		      uint8_t u4OwnMacIdx, uint8_t u4BssIdx,
		      uint8_t u4BssId[MAC_ADDR_LEN]);

int32_t StaRecCmmUpdate(struct net_device *prNetDev,
			uint8_t ucWlanId, uint8_t ucBssId, uint8_t u4Aid,
			uint8_t aucMacAddr[MAC_ADDR_LEN]
		       );

int32_t StaRecBfUpdate(struct net_device *prNetDev,
		       struct STA_REC_BF_UPD_ARGUMENT rStaRecBfUpdArg,
		       uint8_t aucMemRow[4], uint8_t aucMemCol[4]
		      );

#if CFG_SUPPORT_TX_BF_FPGA
int32_t TxBfPseudoTagUpdate(struct net_device *prNetDev,
			    uint8_t ucLm, uint8_t ucNr,
			    uint8_t ucNc, uint8_t ucBw, uint8_t ucCodeBook,
			    uint8_t ucGroup);
#endif

#endif
#endif /*CFG_SUPPORT_QA_TOOL */
#if (CONFIG_WLAN_SERVICE == 1)
uint32_t ServiceWlanOid(void *prNetDev,
	 uint32_t oidType,
	 void *param,
	 uint32_t paramLen,
	 uint32_t *u4BufLen,
	 void *rsp_data);
#endif /*#if (CONFIG_WLAN_SERVICE == 1)*/

#endif /* _GL_HOOK_API_H */
