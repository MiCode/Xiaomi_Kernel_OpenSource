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
	Module Name:
	gl_ate_agent.c
*/
/*******************************************************************************
 *						C O M P I L E R	 F L A G S
 ********************************************************************************
 */

/*******************************************************************************
 *						E X T E R N A L	R E F E R E N C E S
 ********************************************************************************
 */

#include "precomp.h"
#if CFG_SUPPORT_QA_TOOL
#include "gl_wext.h"
#include "gl_cfg80211.h"
#include "gl_ate_agent.h"
#include "gl_hook_api.h"
#include "gl_qa_agent.h"
#if KERNEL_VERSION(3, 8, 0) <= CFG80211_VERSION_CODE
#include <uapi/linux/nl80211.h>
#endif

/*******************************************************************************
*						C O N S T A N T S
********************************************************************************
*/

#if CFG_SUPPORT_TX_BF
PFMU_PROFILE_TAG1 g_rPfmuTag1;
PFMU_PROFILE_TAG2 g_rPfmuTag2;
PFMU_DATA g_rPfmuData;
#endif

typedef struct _ATE_PRIV_CMD {
	UINT_8 *name;
	int (*set_proc)(struct net_device *prNetDev, UINT_8 *prInBuf);
} ATE_PRIV_CMD, *P_ATE_PRIV_CMD;

ATE_PRIV_CMD rAtePrivCmdTable[] = {
	{"ResetCounter", Set_ResetStatCounter_Proc},
	{"ATE", SetATE},
#if 0
	{"ADCDump", SetADCDump},
	{"ATEBSSID", SetATEBssid},
#endif
	{"ATEDA", SetATEDa},
	{"ATESA", SetATESa},
	{"ATECHANNEL", SetATEChannel},
	{"ATETXPOW0", SetATETxPower0},
	{"ATETXGI", SetATETxGi},
	{"ATETXBW", SetATETxBw},
	{"ATETXLEN", SetATETxLength},
	{"ATETXCNT", SetATETxCount},
	{"ATETXMCS", SetATETxMcs},
	{"ATETXMODE", SetATETxMode},
	{"ATEIPG", SetATEIpg},
#if CFG_SUPPORT_TX_BF
	{"TxBfProfileTagHelp", Set_TxBfProfileTag_Help},
	{"TxBfProfileTagInValid", Set_TxBfProfileTag_InValid},
	{"TxBfProfileTagPfmuIdx", Set_TxBfProfileTag_PfmuIdx},
	{"TxBfProfileTagBfType", Set_TxBfProfileTag_BfType},
	{"TxBfProfileTagBw", Set_TxBfProfileTag_DBW},
	{"TxBfProfileTagSuMu", Set_TxBfProfileTag_SuMu},
	{"TxBfProfileTagMemAlloc", Set_TxBfProfileTag_Mem},
	{"TxBfProfileTagMatrix", Set_TxBfProfileTag_Matrix},
	{"TxBfProfileTagSnr", Set_TxBfProfileTag_SNR},
	{"TxBfProfileTagSmtAnt", Set_TxBfProfileTag_SmartAnt},
	{"TxBfProfileTagSeIdx", Set_TxBfProfileTag_SeIdx},
	{"TxBfProfileTagRmsdThrd", Set_TxBfProfileTag_RmsdThrd},
	{"TxBfProfileTagMcsThrd", Set_TxBfProfileTag_McsThrd},
	{"TxBfProfileTagTimeOut", Set_TxBfProfileTag_TimeOut},
	{"TxBfProfileTagDesiredBw", Set_TxBfProfileTag_DesiredBW},
	{"TxBfProfileTagDesiredNc", Set_TxBfProfileTag_DesiredNc},
	{"TxBfProfileTagDesiredNr", Set_TxBfProfileTag_DesiredNr},
	{"TxBfProfileTagRead", Set_TxBfProfileTagRead},
	{"TxBfProfileTagWrite", Set_TxBfProfileTagWrite},
	{"TxBfProfileDataRead", Set_TxBfProfileDataRead},
	{"TxBfProfileDataWrite", Set_TxBfProfileDataWrite},
	{"TxBfProfilePnRead", Set_TxBfProfilePnRead},
	{"TxBfProfilePnWrite", Set_TxBfProfilePnWrite},
	{"TxBfSounding", Set_Trigger_Sounding_Proc},
	{"TxBfSoundingStop", Set_Stop_Sounding_Proc},
	{"TxBfTxApply", Set_TxBfTxApply},
	{"TxBfManualAssoc", Set_TxBfManualAssoc},
	{"TxBfPfmuMemAlloc", Set_TxBfPfmuMemAlloc},
	{"TxBfPfmuMemRelease", Set_TxBfPfmuMemRelease},
	{"StaRecCmmUpdate", Set_StaRecCmmUpdate},
	{"StaRecBfUpdate", Set_StaRecBfUpdate},
	{"DevInfoUpdate", Set_DevInfoUpdate},
	{"BssInfoUpdate", Set_BssInfoUpdate},
#if CFG_SUPPORT_MU_MIMO
	{"MUGetInitMCS", Set_MUGetInitMCS},
	{"MUCalInitMCS", Set_MUCalInitMCS},
	{"MUCalLQ", Set_MUCalLQ},
	{"MUGetLQ", Set_MUGetLQ},
	{"MUSetSNROffset", Set_MUSetSNROffset},
	{"MUSetZeroNss", Set_MUSetZeroNss},
	{"MUSetSpeedUpLQ", Set_MUSetSpeedUpLQ},
	{"MUSetMUTable", Set_MUSetMUTable},
	{"MUSetGroup", Set_MUSetGroup},
	{"MUGetQD", Set_MUGetQD},
	{"MUSetEnable", Set_MUSetEnable},
	{"MUSetGID_UP", Set_MUSetGID_UP},
	{"MUTriggerTx", Set_MUTriggerTx},
#endif
#endif

	{"WriteEfuse", WriteEfuse},
	{"TxPower", SetTxTargetPower},
#if (CFG_SUPPORT_DFS_MASTER == 1)
	{"RDDReport", SetRddReport},
	{"ByPassCac", SetByPassCac},
	{"RadarDetectMode", SetRadarDetectMode},
#endif

	{NULL,}
};

/*******************************************************************************
*						F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Reset RX Statistic Counters.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
*/
/*----------------------------------------------------------------------------*/
int Set_ResetStatCounter_Proc(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	INT_32 i4Status;

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv Set_ResetStatCounter_Proc\n");

	i4Status = MT_ATEResetTXRXCounter(prNetDev);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Set Start Test Mode / Stop Test Mode / Start TX / Stop TX / Start RX / Stop RX.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int SetATE(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	INT_32 i4Status;

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv SetATE\n");

	if (!strcmp(prInBuf, "ATESTART")) {
		DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv SetATE - ATESTART\n");
		i4Status = MT_ATEStart(prNetDev, prInBuf);
	} else if (!strcmp(prInBuf, "ICAPSTART")) {
		DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv SetATE - ICAPSTART\n");
		i4Status = MT_ICAPStart(prNetDev, prInBuf);
	} else if (!strcmp(prInBuf, "ATESTOP")) {
		DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv SetATE - ATESTOP\n");
		i4Status = MT_ATEStop(prNetDev, prInBuf);
	} else if (!strcmp(prInBuf, "TXFRAME")) {
		DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv SetATE - TXFRAME\n");
		i4Status = MT_ATEStartTX(prNetDev, prInBuf);
	} else if (!strcmp(prInBuf, "TXSTOP")) {
		DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv SetATE - TXSTOP\n");
		i4Status = MT_ATEStopTX(prNetDev, prInBuf);
	} else if (!strcmp(prInBuf, "RXFRAME")) {
		DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv SetATE - RXFRAME\n");
		i4Status = MT_ATEStartRX(prNetDev, prInBuf);
	} else if (!strcmp(prInBuf, "RXSTOP")) {
		DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv SetATE - RXSTOP\n");
		i4Status = MT_ATEStopRX(prNetDev, prInBuf);
	} else {
		return -EINVAL;
	}

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Set TX Destination Address.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int SetATEDa(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	INT_32 i4Status = 0;
	UINT_32 addr[MAC_ADDR_LEN];
	UINT_8 addr2[MAC_ADDR_LEN];
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 : ATE_AGENT iwpriv SetDa\n");
	/* xx:xx:xx:xx:xx:xx */
	rv = sscanf(prInBuf, "%x:%x:%x:%x:%x:%x", &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]);
	if (rv == 6) {
		DBGLOG(RFTEST, ERROR, "MT6632 : ATE_AGENT iwpriv SetATEDa Sa:%02x:%02x:%02x:%02x:%02x:%02x\n",
		       addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

		addr2[0] = (UINT_8) addr[0];
		addr2[1] = (UINT_8) addr[1];
		addr2[2] = (UINT_8) addr[2];
		addr2[3] = (UINT_8) addr[3];
		addr2[4] = (UINT_8) addr[4];
		addr2[5] = (UINT_8) addr[5];

		i4Status = MT_ATESetMACAddress(prNetDev, RF_AT_FUNCID_SET_MAC_ADDRESS, addr2);
	} else {
		return -EINVAL;
	}
	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Set TX Source Address.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int SetATESa(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	INT_32 i4Status = 0;
	UINT_32 addr[MAC_ADDR_LEN];
	UINT_8 addr2[MAC_ADDR_LEN];
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 : ATE_AGENT iwpriv SetSa\n");
	/* xx:xx:xx:xx:xx:xx */
	rv = sscanf(prInBuf, "%x:%x:%x:%x:%x:%x", &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]);
	if (rv == 6) {
		DBGLOG(RFTEST, ERROR, "MT6632 : ATE_AGENT iwpriv SetATESa Sa:%02x:%02x:%02x:%02x:%02x:%02x\n",
		       addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

		addr2[0] = (UINT_8) addr[0];
		addr2[1] = (UINT_8) addr[1];
		addr2[2] = (UINT_8) addr[2];
		addr2[3] = (UINT_8) addr[3];
		addr2[4] = (UINT_8) addr[4];
		addr2[5] = (UINT_8) addr[5];

		i4Status = MT_ATESetMACAddress(prNetDev, RF_AT_FUNCID_SET_TA, addr2);
	} else {
		return -EINVAL;
	}
	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Set Channel Frequency.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int SetATEChannel(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 i4SetFreq = 0;
	INT_32 i4Status, i4SetChan = 0;
	INT_32 rv;

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv SetChannel\n");

	rv = kstrtoint(prInBuf, 0, &i4SetChan);
	if (rv == 0) {
		i4SetFreq = nicChannelNum2Freq(i4SetChan);
		i4Status = MT_ATESetChannel(prNetDev, 0, i4SetFreq);
	} else
		return -EINVAL;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Set TX WF0 Power.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int SetATETxPower0(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 i4SetTxPower0 = 0;
	INT_32 i4Status;
	INT_32 rv;

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv SetTxPower0\n");

	rv = kstrtoint(prInBuf, 0, &i4SetTxPower0);
	if (rv == 0)
		i4Status = MT_ATESetTxPower0(prNetDev, i4SetTxPower0);
	else
		return -EINVAL;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Set TX Guard Interval.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int SetATETxGi(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 i4SetTxGi = 0;
	INT_32 i4Status;
	INT_32 rv;

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv SetTxGi\n");

	rv = kstrtoint(prInBuf, 0, &i4SetTxGi);
	if (rv == 0)
		i4Status = MT_ATESetTxGi(prNetDev, i4SetTxGi);
	else
		return -EINVAL;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Set TX System Bandwidth.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int SetATETxBw(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 i4SetSystemBW = 0;
	INT_32 i4Status;
	INT_32 rv;

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv SetSystemBW\n");

	rv = kstrtoint(prInBuf, 0, &i4SetSystemBW);
	if (rv == 0)
		i4Status = MT_ATESetSystemBW(prNetDev, i4SetSystemBW);
	else
		return -EINVAL;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Set TX Mode (Preamble).
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int SetATETxMode(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 i4SetTxMode = 0;
	INT_32 i4Status;
	INT_32 rv;

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv SetTxMode\n");

	rv = kstrtoint(prInBuf, 0, &i4SetTxMode);
	if (rv == 0)
		i4Status = MT_ATESetPreamble(prNetDev, i4SetTxMode);
	else
		return -EINVAL;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Set TX Length.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int SetATETxLength(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 i4SetTxLength = 0;
	INT_32 i4Status;
	INT_32 rv;

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv SetTxLength\n");

	rv = kstrtoint(prInBuf, 0, &i4SetTxLength);
	if (rv == 0)
		i4Status = MT_ATESetTxLength(prNetDev, i4SetTxLength);
	else
		return -EINVAL;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Set TX Count.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int SetATETxCount(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 i4SetTxCount = 0;
	INT_32 i4Status;
	INT_32 rv;
	UCHAR addr[MAC_ADDR_LEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv SetTxCount\n");

	rv = kstrtoint(prInBuf, 0, &i4SetTxCount);
	if (rv == 0)
		i4Status = MT_ATESetTxCount(prNetDev, i4SetTxCount);
	else
		return -EINVAL;

	i4Status = MT_ATESetMACAddress(prNetDev, RF_AT_FUNCID_SET_MAC_ADDRESS, addr);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Set TX Rate.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int SetATETxMcs(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 i4SetTxMcs = 0;
	INT_32 i4Status;
	INT_32 rv;

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv SetTxMcs\n");

	rv = kstrtoint(prInBuf, 0, &i4SetTxMcs);
	if (rv == 0)
		i4Status = MT_ATESetRate(prNetDev, i4SetTxMcs);
	else
		return -EINVAL;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Set Inter-Packet Guard Interval.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int SetATEIpg(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 i4SetTxIPG = 0;
	INT_32 i4Status;
	INT_32 rv;

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv SetIpg\n");

	rv = kstrtoint(prInBuf, 0, &i4SetTxIPG);
	if (rv == 0)
		i4Status = MT_ATESetTxIPG(prNetDev, i4SetTxIPG);
	else
		return -EINVAL;

	return i4Status;
}

#if CFG_SUPPORT_TX_BF
int Set_TxBfProfileTag_Help(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	DBGLOG(RFTEST, ERROR,
	       "========================================================================================================================\n"
	       "TxBfProfile Tag1 setting example :\n"
	       "iwpriv ra0 set TxBfProfileTagPfmuIdx  =xx\n"
	       "iwpriv ra0 set TxBfProfileTagBfType   =xx (0: iBF; 1: eBF)\n"
	       "iwpriv ra0 set TxBfProfileTagBw       =xx (0/1/2/3 : BW20/40/80/160NC)\n"
	       "iwpriv ra0 set TxBfProfileTagSuMu     =xx (0:SU, 1:MU)\n"
	       "iwpriv ra0 set TxBfProfileTagInvalid  =xx (0: valid, 1: invalid)\n"
	       "iwpriv ra0 set TxBfProfileTagMemAlloc =xx:xx:xx:xx:xx:xx:xx:xx (mem_row, mem_col), ..\n"
	       "iwpriv ra0 set TxBfProfileTagMatrix   =nrow:nol:ng:LM\n"
	       "iwpriv ra0 set TxBfProfileTagSnr      =SNR_STS0:SNR_STS1:SNR_STS2:SNR_STS3\n"
	       "\n\n"
	       "TxBfProfile Tag2 setting example :\n"
	       "iwpriv ra0 set TxBfProfileTagSmtAnt   =xx (11:0)\n"
	       "iwpriv ra0 set TxBfProfileTagSeIdx    =xx\n"
	       "iwpriv ra0 set TxBfProfileTagRmsdThrd =xx\n"
	       "iwpriv ra0 set TxBfProfileTagMcsThrd  =xx:xx:xx:xx:xx:xx (MCS TH L1SS:S1SS:L2SS:....)\n"
	       "iwpriv ra0 set TxBfProfileTagTimeOut  =xx\n"
	       "iwpriv ra0 set TxBfProfileTagDesiredBw=xx (0/1/2/3 : BW20/40/80/160NC)\n"
	       "iwpriv ra0 set TxBfProfileTagDesiredNc=xx\n"
	       "iwpriv ra0 set TxBfProfileTagDesiredNr=xx\n"
	       "\n\n"
	       "Read TxBf profile Tag :\n"
	       "iwpriv ra0 set TxBfProfileTagRead     =xx (PFMU ID)\n"
	       "\n"
	       "Write TxBf profile Tag :\n"
	       "iwpriv ra0 set TxBfProfileTagWrite    =xx (PFMU ID)\n"
	       "When you use one of relative CMD to update one of tag parameters, you should call TxBfProfileTagWrite to update Tag\n"
	       "\n\n"
	       "Read TxBf profile Data	:\n"
	       "iwpriv ra0 set TxBfProfileDataRead    =xx (PFMU ID)\n"
	       "\n"
	       "Write TxBf profile Data :\n"
	       "iwpriv ra0 set TxBfProfileDataWrite   =BW :subcarrier:phi11:psi2l:Phi21:Psi31:Phi31:Psi41:Phi22:Psi32:Phi32:Psi42:Phi33:Psi43\n"
	       "iwpriv ra0 set TxBfProfileDataWriteAll=Profile ID : BW (BW       : 0x00 (20M) , 0x01 (40M), 0x02 (80M), 0x3 (160M)\n"
	       "When you use CMD TxBfProfileDataWrite to update profile data per subcarrier, you should call TxBfProfileDataWriteAll to update all of\n"
	       "subcarrier's profile data.\n\n"
	       "Read TxBf profile PN	:\n"
	       "iwpriv ra0 set TxBfProfilePnRead      =xx (PFMU ID)\n"
	       "\n"
	       "Write TxBf profile PN :\n"
	       "iwpriv ra0 set TxBfProfilePnWrite     =Profile ID:BW:1STS_Tx0:1STS_Tx1:1STS_Tx2:1STS_Tx3:2STS_Tx0:2STS_Tx1:2STS_Tx2:2STS_Tx3:3STS_Tx1:3STS_Tx2:3STS_Tx3\n"
	       "========================================================================================================================\n");
	return 0;
}

int Set_TxBfProfileTag_InValid(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 ucInValid;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_InValid\n");

	rv = kstrtoint(prInBuf, 0, &ucInValid);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_InValid prInBuf = %s, ucInValid = %d\n", prInBuf,
		       ucInValid);
		i4Status = TxBfProfileTag_InValid(prNetDev, &g_rPfmuTag1, ucInValid);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileTag_PfmuIdx(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 ucProfileIdx;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_PfmuIdx\n");

	rv = kstrtoint(prInBuf, 0, &ucProfileIdx);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_PfmuIdx prInBuf = %s, ucProfileIdx = %d\n", prInBuf,
		       ucProfileIdx);
		i4Status = TxBfProfileTag_PfmuIdx(prNetDev, &g_rPfmuTag1, ucProfileIdx);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileTag_BfType(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 ucBFType;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_BfType\n");

	rv = kstrtoint(prInBuf, 0, &ucBFType);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_BfType prInBuf = %s, ucBFType = %d\n",
			prInBuf, ucBFType);
		i4Status = TxBfProfileTag_TxBfType(prNetDev, &g_rPfmuTag1, ucBFType);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileTag_DBW(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 ucBW;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_DBW\n");

	rv = kstrtoint(prInBuf, 0, &ucBW);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_DBW prInBuf = %s, ucBW = %d\n", prInBuf, ucBW);
		i4Status = TxBfProfileTag_DBW(prNetDev, &g_rPfmuTag1, ucBW);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileTag_SuMu(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 ucSuMu;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_SuMu\n");

	rv = kstrtoint(prInBuf, 0, &ucSuMu);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_SuMu prInBuf = %s, ucSuMu = %d\n", prInBuf, ucSuMu);
		i4Status = TxBfProfileTag_SuMu(prNetDev, &g_rPfmuTag1, ucSuMu);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileTag_Mem(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 aucInput[8];
	INT_32 i4Status = 0;
	UINT_8 aucMemAddrColIdx[4], aucMemAddrRowIdx[4];
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_Mem\n");

	rv = sscanf(prInBuf, "%d:%d:%d:%d:%d:%d:%d:%d",
		    &aucInput[0], &aucInput[1], &aucInput[2], &aucInput[3], &aucInput[4], &aucInput[5], &aucInput[6],
		    &aucInput[7]);
	/* mem col0:row0:col1:row1:col2:row2:col3:row3 */
	if (rv == 8) {
		DBGLOG(RFTEST, ERROR,
			"MT6632 : ATE_AGENT iwpriv Set_TxBfProfileTag_Mem aucInput:%d:%d:%d:%d:%d:%d:%d:%d\n",
		    aucInput[0], aucInput[1], aucInput[2], aucInput[3], aucInput[4], aucInput[5], aucInput[6],
		    aucInput[7]);

		aucMemAddrColIdx[0] = (UINT_8) aucInput[0];
		aucMemAddrRowIdx[0] = (UINT_8) aucInput[1];
		aucMemAddrColIdx[1] = (UINT_8) aucInput[2];
		aucMemAddrRowIdx[1] = (UINT_8) aucInput[3];
		aucMemAddrColIdx[2] = (UINT_8) aucInput[4];
		aucMemAddrRowIdx[2] = (UINT_8) aucInput[5];
		aucMemAddrColIdx[3] = (UINT_8) aucInput[6];
		aucMemAddrRowIdx[3] = (UINT_8) aucInput[7];

		i4Status = TxBfProfileTag_Mem(prNetDev, &g_rPfmuTag1, aucMemAddrColIdx, aucMemAddrRowIdx);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileTag_Matrix(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 aucInput[6];
	UINT_8 ucNrow, ucNcol, ucNgroup, ucLM, ucCodeBook, ucHtcExist;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_Matrix\n");

	rv = sscanf(prInBuf, "%d:%d:%d:%d:%d:%d",
		    &aucInput[0], &aucInput[1], &aucInput[2], &aucInput[3], &aucInput[4], &aucInput[5]);
	/* nrow:nol:ng:LM:CodeBook:HtcExist */
	if (rv == 6) {
		DBGLOG(RFTEST, ERROR,
			"MT6632 : ATE_AGENT iwpriv Set_TxBfProfileTag_Matrix aucInput:%d:%d:%d:%d:%d:%d\n",
		       aucInput[0], aucInput[1], aucInput[2], aucInput[3], aucInput[4], aucInput[5]);
		ucNrow = (UINT_8) aucInput[0];
		ucNcol = (UINT_8) aucInput[1];
		ucNgroup = (UINT_8) aucInput[2];
		ucLM = (UINT_8) aucInput[3];
		ucCodeBook = (UINT_8) aucInput[4];
		ucHtcExist = (UINT_8) aucInput[5];

		i4Status = TxBfProfileTag_Matrix(prNetDev,
						 &g_rPfmuTag1, ucNrow, ucNcol, ucNgroup, ucLM, ucCodeBook, ucHtcExist);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileTag_SNR(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 aucInput[4];
	UINT_8 ucSNR_STS0, ucSNR_STS1, ucSNR_STS2, ucSNR_STS3;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_SNR\n");

	rv = sscanf(prInBuf, "%d:%d:%d:%d", &aucInput[0], &aucInput[1], &aucInput[2], &aucInput[3]);
	if (rv == 4) {
		DBGLOG(RFTEST, ERROR, "MT6632 : ATE_AGENT iwpriv Set_TxBfProfileTag_SNR aucInput:%d:%d:%d:%d\n",
		       aucInput[0], aucInput[1], aucInput[2], aucInput[3]);

		ucSNR_STS0 = (UINT_8) aucInput[0];
		ucSNR_STS1 = (UINT_8) aucInput[1];
		ucSNR_STS2 = (UINT_8) aucInput[2];
		ucSNR_STS3 = (UINT_8) aucInput[3];

		i4Status = TxBfProfileTag_SNR(prNetDev, &g_rPfmuTag1, ucSNR_STS0, ucSNR_STS1, ucSNR_STS2, ucSNR_STS3);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileTag_SmartAnt(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	INT_32 i4Status = 0;
	UINT_32 ucSmartAnt;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_SmartAnt\n");

	rv = kstrtoint(prInBuf, 0, &ucSmartAnt);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_SmartAnt prInBuf = %s, ucSmartAnt = %d\n", prInBuf,
		       ucSmartAnt);
		i4Status = TxBfProfileTag_SmtAnt(prNetDev, &g_rPfmuTag2, ucSmartAnt);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileTag_SeIdx(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	INT_32 i4Status = 0;
	UINT_32 ucSeIdx;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_SeIdx\n");

	rv = kstrtoint(prInBuf, 0, &ucSeIdx);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 TxBfProfileTag_SeIdx prInBuf = %s, ucSeIdx = %d\n", prInBuf, ucSeIdx);
		i4Status = TxBfProfileTag_SeIdx(prNetDev, &g_rPfmuTag2, ucSeIdx);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileTag_RmsdThrd(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	INT_32 i4Status = 0;
	UINT_32 ucRmsdThrd;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_RmsdThrd\n");

	rv = kstrtoint(prInBuf, 0, &ucRmsdThrd);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_RmsdThrd prInBuf = %s, ucRmsdThrd = %d\n", prInBuf,
		       ucRmsdThrd);
		i4Status = TxBfProfileTag_RmsdThd(prNetDev, &g_rPfmuTag2, ucRmsdThrd);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileTag_McsThrd(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 aucInput[6];
	UINT_8 ucMcsLss[3], ucMcsSss[3];
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_McsThrd\n");

	rv = sscanf(prInBuf, "%d:%d:%d:%d:%d:%d",
		    &aucInput[0], &aucInput[1], &aucInput[2], &aucInput[3], &aucInput[4], &aucInput[5]);
	if (rv == 6) {
		DBGLOG(RFTEST, ERROR,
			"MT6632 : ATE_AGENT iwpriv Set_TxBfProfileTag_McsThrd aucInput:%d:%d:%d:%d:%d:%d\n",
		       aucInput[0], aucInput[1], aucInput[2], aucInput[3], aucInput[4], aucInput[5]);

		ucMcsLss[0] = (UINT_8) aucInput[0];
		ucMcsSss[0] = (UINT_8) aucInput[1];
		ucMcsLss[1] = (UINT_8) aucInput[2];
		ucMcsSss[1] = (UINT_8) aucInput[3];
		ucMcsLss[2] = (UINT_8) aucInput[4];
		ucMcsSss[2] = (UINT_8) aucInput[5];

		i4Status = TxBfProfileTag_McsThd(prNetDev, &g_rPfmuTag2, ucMcsLss, ucMcsSss);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileTag_TimeOut(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 ucTimeOut;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_TimeOut\n");

	rv = kstrtouint(prInBuf, 0, &ucTimeOut);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_TimeOut prInBuf = %s, ucTimeOut = %d\n", prInBuf,
		       ucTimeOut);
		i4Status = TxBfProfileTag_TimeOut(prNetDev, &g_rPfmuTag2, ucTimeOut);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileTag_DesiredBW(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 ucDesiredBW;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_DesiredBW\n");

	rv = kstrtoint(prInBuf, 0, &ucDesiredBW);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_DesiredBW prInBuf = %s, ucDesiredBW = %d\n", prInBuf,
		       ucDesiredBW);
		i4Status = TxBfProfileTag_DesiredBW(prNetDev, &g_rPfmuTag2, ucDesiredBW);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileTag_DesiredNc(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 ucDesiredNc;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_DesiredNc\n");

	rv = kstrtoint(prInBuf, 0, &ucDesiredNc);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_DesiredNc prInBuf = %s, ucDesiredNc = %d\n", prInBuf,
		       ucDesiredNc);
		i4Status = TxBfProfileTag_DesiredNc(prNetDev, &g_rPfmuTag2, ucDesiredNc);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileTag_DesiredNr(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 ucDesiredNr;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_DesiredNr\n");

	rv = kstrtoint(prInBuf, 0, &ucDesiredNr);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTag_DesiredNr prInBuf = %s, ucDesiredNr = %d\n", prInBuf,
		       ucDesiredNr);
		i4Status = TxBfProfileTag_DesiredNr(prNetDev, &g_rPfmuTag2, ucDesiredNr);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileTagWrite(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 profileIdx;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTagWrite\n");

	rv = kstrtoint(prInBuf, 0, &profileIdx);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTagWrite prInBuf = %s, profileIdx = %d\n", prInBuf,
		       profileIdx);
		i4Status = TxBfProfileTagWrite(prNetDev, &g_rPfmuTag1, &g_rPfmuTag2, profileIdx);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileTagRead(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 profileIdx, fgBFer;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileTagRead\n");

	rv = sscanf(prInBuf, "%d:%d", &profileIdx, &fgBFer);
	if (rv == 2) {
		DBGLOG(RFTEST, ERROR,
			"MT6632 Set_TxBfProfileTagRead prInBuf = %s, profileIdx = %d, fgBFer = %d\n",
			prInBuf, profileIdx, fgBFer);
		i4Status = TxBfProfileTagRead(prNetDev, profileIdx, fgBFer);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileDataRead(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 profileIdx, fgBFer, subcarrierIdxMsb, subcarrierIdxLsb;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfileDataRead\n");

	rv = sscanf(prInBuf, "%d:%d:%x:%x", &profileIdx, &fgBFer, &subcarrierIdxMsb, &subcarrierIdxLsb);
	if (rv == 4) {
		DBGLOG(RFTEST, ERROR,
		       "MT6632 Set_TxBfProfileDataRead prInBuf = %s, profileIdx = %d, fgBFer = %d, subcarrierIdxMsb:%x, subcarrierIdxLsb:%x\n",
		       prInBuf, profileIdx, fgBFer, subcarrierIdxMsb, subcarrierIdxLsb);
		i4Status = TxBfProfileDataRead(prNetDev, profileIdx, fgBFer, subcarrierIdxMsb, subcarrierIdxLsb);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfileDataWrite(struct net_device *prNetDev, UINT_8 *prInBuf)
{

	UINT_32 u4ProfileIdx;
	UINT_32 u4SubcarrierIdx;
	UINT_32 au4Phi[6];
	UINT_32 au4Psi[6];
	UINT_32 au4DSnr[4];
	UINT_16 au2Phi[6];
	UINT_8 aucPsi[6];
	UINT_8 aucDSnr[4];
	UINT_32 i;
	INT_32 rv;

	INT_32 i4Status = 0;

	DBGLOG(RFTEST, ERROR, "MT6632 TxBfProfileDataWrite\n");

	rv = sscanf(prInBuf, "%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x",
			   &u4ProfileIdx, &u4SubcarrierIdx, &au4Phi[0], &au4Psi[0], &au4Phi[1], &au4Psi[1],
			   &au4Phi[2], &au4Psi[2], &au4Phi[3], &au4Psi[3], &au4Phi[4], &au4Psi[4],
			   &au4Phi[5], &au4Psi[5],
			   &au4DSnr[0], &au4DSnr[1], &au4DSnr[2], &au4DSnr[3]);

	if (rv == 18) {
		DBGLOG(RFTEST, ERROR,
			"MT6632 TxBfProfileDataWrite prInBuf = %s, u4ProfileIdx = %x, u4SubcarrierIdx = %x, au4Phi[0]:%x, au4Phi[1]:%x, au4Phi[2]:%x, au4Phi[3]:%x, au4Phi[4]:%x, au4Phi[5]:%x, au4Psi[0]:%x, au4Psi[1]:%x, au4Psi[2]:%x, au4Psi[3]:%x, au4Psi[4]:%x, au4Psi[5]:%x,au4DSnr[0]:%x, au4DSnr[1]:%x, au4DSnr[2]:%x, au4DSnr[3]:%x\n",
		       prInBuf, u4ProfileIdx, u4SubcarrierIdx,
		       au4Phi[0], au4Phi[1], au4Phi[2], au4Phi[3], au4Phi[4], au4Phi[5],
		       au4Psi[0], au4Psi[1], au4Psi[2], au4Psi[3], au4Psi[4], au4Psi[5],
		       au4DSnr[0], au4DSnr[1], au4DSnr[2], au4DSnr[3]);
		for (i = 0; i < 6; i++) {
			au2Phi[i] = au4Phi[i];
			aucPsi[i] = au4Psi[i];
		}
		for (i = 0; i < 4; i++)
			aucDSnr[i] = au4DSnr[i];

		i4Status = TxBfProfileDataWrite(prNetDev, u4ProfileIdx, u4SubcarrierIdx, au2Phi, aucPsi, aucDSnr);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfilePnRead(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 profileIdx;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfilePnRead\n");

	rv = kstrtoint(prInBuf, 0, &profileIdx);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_TxBfProfilePnRead prInBuf = %s, profileIdx = %d\n",
			prInBuf, profileIdx);
		i4Status = TxBfProfilePnRead(prNetDev, profileIdx);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfProfilePnWrite(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 ucProfileIdx;
	UINT_16 u2bw;
	UINT_16 au2XSTS[12];
	INT_32 rv;

	INT_32 i4Status = 0;

	DBGLOG(RFTEST, ERROR, "MT6632 TxBfProfilePnWrite\n");

	rv = sscanf(prInBuf,
		   "%d:%hd:%hd:%hd:%hd:%hd:%hd:%hd:%hd:%hd:%hd:%hd:%hd:%hd",
		   &ucProfileIdx, &u2bw, &au2XSTS[0], &au2XSTS[1], &au2XSTS[2], &au2XSTS[3],
		   &au2XSTS[4], &au2XSTS[5], &au2XSTS[6], &au2XSTS[7], &au2XSTS[8], &au2XSTS[9], &au2XSTS[10],
		   &au2XSTS[11]);
	if (rv == 14) {
		DBGLOG(RFTEST, ERROR,
			"MT6632 TxBfProfilePnWrite prInBuf = %s, ucProfileIdx = %d, u2bw = %dau2XSTS[0]:%d, au2XSTS[1]:%d, au2XSTS[2]:%d, au2XSTS[3]:%d, au2XSTS[4]:%d, au2XSTS[5]:%d, au2XSTS[6]:%d, au2XSTS[7]:%d, au2XSTS[8]:%d, au2XSTS[9]:%d, au2XSTS[10]:%d, au2XSTS[11]:%d\n",
		       prInBuf, ucProfileIdx, u2bw, au2XSTS[0], au2XSTS[1],
		       au2XSTS[2], au2XSTS[3], au2XSTS[4], au2XSTS[5],
		       au2XSTS[6], au2XSTS[7], au2XSTS[8], au2XSTS[9], au2XSTS[10], au2XSTS[11]);
		i4Status = TxBfProfilePnWrite(prNetDev, ucProfileIdx, u2bw, au2XSTS);
	} else
		return -EINVAL;

	return i4Status;
}

/* Su_Mu:NumSta:SndInterval:WLan0:WLan1:WLan2:WLan3 */
int Set_Trigger_Sounding_Proc(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 ucSuMu, ucNumSta, ucSndInterval, ucWLan0, ucWLan1, ucWLan2, ucWLan3;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_Trigger_Sounding_Proc\n");

	rv = sscanf
	    (prInBuf, "%x:%x:%x:%x:%x:%x:%x", &ucSuMu, &ucNumSta, &ucSndInterval, &ucWLan0, &ucWLan1, &ucWLan2,
	     &ucWLan3);
	if (rv == 7) {
		DBGLOG(RFTEST, ERROR,
		       "MT6632 Set_Trigger_Sounding_Proc prInBuf = %s, ucSuMu = %d, ucNumSta = %d, ucSndInterval = %d, ucWLan0 = %d, ucWLan1 = %d, ucWLan2:%d, ucWLan3:%d\n",
		       prInBuf, ucSuMu, ucNumSta, ucSndInterval, ucWLan0,
		       ucWLan1, ucWLan2, ucWLan3);
		i4Status = TxBfSounding(prNetDev, ucSuMu, ucNumSta, ucSndInterval, ucWLan0, ucWLan1, ucWLan2, ucWLan3);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_Stop_Sounding_Proc(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	INT_32 i4Status = 0;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_Stop_Sounding_Proc\n");

	i4Status = TxBfSoundingStop(prNetDev);

	return i4Status;
}

int Set_TxBfTxApply(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 u4WlanId, u4ETxBf, u4ITxBf, u4MuTxBf;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 TxBfTxApply\n");

	rv = sscanf(prInBuf, "%d:%d:%d:%d", &u4WlanId, &u4ETxBf, &u4ITxBf, &u4MuTxBf);
	if (rv == 4) {
		DBGLOG(RFTEST, ERROR,
		       "MT6632 TxBfTxApply prInBuf = %s, u4WlanId = %d, u4ETxBf = %d, u4ITxBf = %d, u4MuTxBf = %d\n",
		       prInBuf, u4WlanId, u4ETxBf, u4ITxBf, u4MuTxBf);
		i4Status = TxBfTxApply(prNetDev, u4WlanId, u4ETxBf, u4ITxBf, u4MuTxBf);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfManualAssoc(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	INT_32 au4Mac[MAC_ADDR_LEN];
	INT_32 u4Type, u4Wtbl, u4Ownmac, u4PhyMode, u4Bw, u4Nss, u4PfmuId, u4Mode, u4Marate, u4SpeIdx, ucaid, u4Rv;
	INT_8 aucMac[MAC_ADDR_LEN];
	INT_32 i4Status = 0;
	INT_32 i = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 TxBfManualAssoc\n");

	rv = sscanf(prInBuf, "%x:%x:%x:%x:%x:%x:%x:%d:%x:%x:%x:%x:%d:%x:%x:%x:%d:%x",
		   &au4Mac[0], &au4Mac[1], &au4Mac[2], &au4Mac[3], &au4Mac[4], &au4Mac[5],
		   &u4Type, &u4Wtbl, &u4Ownmac, &u4PhyMode, &u4Bw, &u4Nss, &u4PfmuId, &u4Mode, &u4Marate, &u4SpeIdx,
		   &ucaid, &u4Rv);
	if (rv == 18) {
		DBGLOG(RFTEST, ERROR,
			"MT6632 TxBfManualAssoc au4Mac[0] = %x, au4Mac[1] = %x, au4Mac[2] = %xau4Mac[3] = %x, au4Mac[4] = %x, au4Mac[5] = %x, u4Type = %x, u4Wtbl = %d, u4Ownmac = %x, u4PhyMode = %x u4Bw = %x, u4Nss = %x, u4PfmuId = %d, u4Mode = %x, u4Marate = %x, u4SpeIdx = %d, ucaid = %d, u4Rv = %x",
		       au4Mac[0], au4Mac[1], au4Mac[2], au4Mac[3], au4Mac[4], au4Mac[5], u4Type, u4Wtbl, u4Ownmac,
		       u4PhyMode, u4Bw, u4Nss, u4PfmuId, u4Mode, u4Marate, u4SpeIdx, ucaid, u4Rv);
		for (i = 0; i < MAC_ADDR_LEN; i++)
			aucMac[i] = au4Mac[i];

		i4Status =
		    TxBfManualAssoc(prNetDev, aucMac, u4Type, u4Wtbl, u4Ownmac, u4Mode, u4Bw, u4Nss, u4PfmuId, u4Marate,
				    u4SpeIdx, ucaid, u4Rv);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfPfmuMemAlloc(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 ucSuMuMode, ucWlanIdx;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 TxBfPfmuMemAlloc\n");

	rv = sscanf(prInBuf, "%d:%d", &ucSuMuMode, &ucWlanIdx);
	if (rv == 2) {
		DBGLOG(RFTEST, ERROR, "MT6632 TxBfPfmuMemAlloc ucSuMuMode = %d, ucWlanIdx = %d", ucSuMuMode, ucWlanIdx);
		i4Status = TxBfPfmuMemAlloc(prNetDev, ucSuMuMode, ucWlanIdx);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_TxBfPfmuMemRelease(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 ucWlanId;
	INT_32 i4Status = 0;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 TxBfPfmuMemRelease\n");

	rv = kstrtoint(prInBuf, 0, &ucWlanId);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 TxBfPfmuMemRelease ucWlanId = %d", ucWlanId);
		i4Status = TxBfPfmuMemRelease(prNetDev, ucWlanId);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_DevInfoUpdate(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 u4OwnMacIdx, fgBand;
	UINT_32 OwnMacAddr[MAC_ADDR_LEN];
	UINT_8 aucMacAddr[MAC_ADDR_LEN];
	INT_32 i4Status = 0;
	UINT_32 i;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 DevInfoUpdate\n");

	rv = sscanf
	    (prInBuf, "%d:%x:%x:%x:%x:%x:%x:%d", &u4OwnMacIdx, &OwnMacAddr[0], &OwnMacAddr[1], &OwnMacAddr[2],
	     &OwnMacAddr[3], &OwnMacAddr[4], &OwnMacAddr[5], &fgBand);
	if (rv == 8) {
		DBGLOG(RFTEST, ERROR,
			"MT6632 DevInfoUpdate prInBuf = %s, u4OwnMacIdx = %x, fgBand = %x,OwnMacAddr[0]:%x, OwnMacAddr[1]:%x, OwnMacAddr[2]:%x, OwnMacAddr[3]:%x, OwnMacAddr[4]:%x, OwnMacAddr[5]:%x,",
		    prInBuf, u4OwnMacIdx, fgBand, OwnMacAddr[0], OwnMacAddr[1], OwnMacAddr[2], OwnMacAddr[3],
		    OwnMacAddr[4], OwnMacAddr[5]);
		for (i = 0; i < MAC_ADDR_LEN; i++)
			aucMacAddr[i] = OwnMacAddr[i];

		i4Status = DevInfoUpdate(prNetDev, u4OwnMacIdx, fgBand, aucMacAddr);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_BssInfoUpdate(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 u4OwnMacIdx, u4BssIdx;
	UINT_32 au4BssId[MAC_ADDR_LEN];
	UINT_8 aucBssId[MAC_ADDR_LEN];
	INT_32 i4Status = 0;
	UINT_32 i;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 BssInfoUpdate\n");

	rv = sscanf
	    (prInBuf, "%d:%d:%x:%x:%x:%x:%x:%x", &u4OwnMacIdx, &u4BssIdx, &au4BssId[0], &au4BssId[1], &au4BssId[2],
	     &au4BssId[3], &au4BssId[4], &au4BssId[5]);
	if (rv == 8) {
		DBGLOG(RFTEST, ERROR,
			"MT6632 BssInfoUpdate prInBuf = %s, u4OwnMacIdx = %x, u4BssIdx = %x,au4BssId[0]:%x, au4BssId[1]:%x, au4BssId[2]:%x, au4BssId[3]:%x, au4BssId[4]:%x, au4BssId[5]:%x,",
		    prInBuf, u4OwnMacIdx, u4BssIdx, au4BssId[0], au4BssId[1], au4BssId[2], au4BssId[3], au4BssId[4],
		    au4BssId[5]);
		for (i = 0; i < MAC_ADDR_LEN; i++)
			aucBssId[i] = au4BssId[i];

		i4Status = BssInfoUpdate(prNetDev, u4OwnMacIdx, u4BssIdx, aucBssId);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_StaRecCmmUpdate(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 u4WlanId, u4BssId, u4Aid;
	UINT_32 au4MacAddr[MAC_ADDR_LEN];
	UINT_8 aucMacAddr[MAC_ADDR_LEN];
	INT_32 i4Status = 0;
	UINT_32 i;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_StaRecCmmUpdate\n");

	rv = sscanf
	    (prInBuf, "%x:%x:%x:%x:%x:%x:%x:%x:%x", &u4WlanId, &u4BssId, &u4Aid, &au4MacAddr[0], &au4MacAddr[1],
	     &au4MacAddr[2], &au4MacAddr[3], &au4MacAddr[4], &au4MacAddr[5]);
	if (rv == 9) {
		DBGLOG(RFTEST, ERROR,
			"MT6632 Set_StaRecCmmUpdate prInBuf = %s, u4WlanId = %x, u4BssId = %x, u4Aid = %x,aucMacAddr[0]:%x, aucMacAddr[1]:%x, aucMacAddr[2]:%x, aucMacAddr[3]:%x, aucMacAddr[4]:%x, aucMacAddr[5]:%x,",
			prInBuf, u4WlanId, u4BssId, u4Aid, au4MacAddr[0], au4MacAddr[1], au4MacAddr[2], au4MacAddr[3],
			au4MacAddr[4], au4MacAddr[5]);
		for (i = 0; i < MAC_ADDR_LEN; i++)
			aucMacAddr[i] = au4MacAddr[i];

		i4Status = StaRecCmmUpdate(prNetDev, u4WlanId, u4BssId, u4Aid, aucMacAddr);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_StaRecBfUpdate(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	STA_REC_BF_UPD_ARGUMENT rStaRecBfUpdArg;
	UINT_8 aucMemRow[4], aucMemCol[4];
	INT_32 i4Status = 0;
	UINT_32 i;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_StaRecBfUpdate\n");

	rv = sscanf(prInBuf, "%x:%x:%x:%x:%x:%d:%d:%d:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x",
		   &rStaRecBfUpdArg.u4WlanId, &rStaRecBfUpdArg.u4BssId, &rStaRecBfUpdArg.u4PfmuId,
		   &rStaRecBfUpdArg.u4SuMu, &rStaRecBfUpdArg.u4eTxBfCap, &rStaRecBfUpdArg.u4NdpaRate,
		   &rStaRecBfUpdArg.u4NdpRate, &rStaRecBfUpdArg.u4ReptPollRate, &rStaRecBfUpdArg.u4TxMode,
		   &rStaRecBfUpdArg.u4Nc, &rStaRecBfUpdArg.u4Nr, &rStaRecBfUpdArg.u4Bw, &rStaRecBfUpdArg.u4SpeIdx,
		   &rStaRecBfUpdArg.u4TotalMemReq, &rStaRecBfUpdArg.u4MemReq20M, &rStaRecBfUpdArg.au4MemRow[0],
		   &rStaRecBfUpdArg.au4MemCol[0], &rStaRecBfUpdArg.au4MemRow[1], &rStaRecBfUpdArg.au4MemCol[1],
		   &rStaRecBfUpdArg.au4MemRow[2], &rStaRecBfUpdArg.au4MemCol[2], &rStaRecBfUpdArg.au4MemRow[3],
		   &rStaRecBfUpdArg.au4MemCol[3]);
	if (rv == 23) {
		/*
		*DBGLOG(RFTEST, ERROR,
		*"MT6632 Set_StaRecBfUpdate prInBuf = %s, u4WlanId = %x, u4BssId = %x, u4Aid = %x,
		*   aucMacAddr[0]:%x, aucMacAddr[1]:%x, aucMacAddr[2]:%x, aucMacAddr[3]:%x, aucMacAddr[4]:%x,
		*   aucMacAddr[5]:%x",
		*   prInBuf, u4OwnMacIdx, u4BssIdx, u4Aid,
		*   aucMacAddr[0], aucMacAddr[1], aucMacAddr[2], aucMacAddr[3], aucMacAddr[4], aucMacAddr[5]);
		*/
		for (i = 0; i < 4; i++) {
			aucMemRow[i] = rStaRecBfUpdArg.au4MemRow[i];
			aucMemCol[i] = rStaRecBfUpdArg.au4MemCol[i];
		}
		i4Status = StaRecBfUpdate(prNetDev, rStaRecBfUpdArg, aucMemRow, aucMemCol);
	} else
		return -EINVAL;

	return i4Status;
}

#if CFG_SUPPORT_MU_MIMO
int Set_MUGetInitMCS(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T rMuMimoActionInfo;
	INT_32 i4Status = 0;
	UINT_32 u4BufLen = 0;

	UINT_32 u4groupIdx;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_MUGetInitMCS\n");

	kalMemZero(&rMuMimoActionInfo, sizeof(rMuMimoActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rv = kstrtouint(prInBuf, 0, &u4groupIdx);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Test\n");
		DBGLOG(RFTEST, ERROR, "MT6632 Set_MUGetInitMCS prInBuf = %s, u4groupIdx = %x", prInBuf, u4groupIdx);

		rMuMimoActionInfo.ucMuMimoCategory = MU_GET_CALC_INIT_MCS;
		rMuMimoActionInfo.unMuMimoParam.rMuGetCalcInitMcs.ucgroupIdx = u4groupIdx;

		i4Status = kalIoctl(prGlueInfo,
				    wlanoidMuMimoAction,
				    &rMuMimoActionInfo, sizeof(rMuMimoActionInfo), TRUE, TRUE, TRUE, &u4BufLen);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_MUCalInitMCS(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T rMuMimoActionInfo;
	INT_32 i4Status = 0;
	UINT_32 u4BufLen = 0;

	UINT_32 u4NumOfUser, u4Bandwidth, u4NssOfUser0, u4NssOfUser1, u4PfMuIdOfUser0, u4PfMuIdOfUser1, u4NumOfTxer,
	    u4SpeIndex, u4GroupIndex;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_MUCalInitMCS\n");

	kalMemZero(&rMuMimoActionInfo, sizeof(rMuMimoActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rv = sscanf
	    (prInBuf, "%x:%x:%x:%x:%x:%x:%x:%x:%x", &u4NumOfUser, &u4Bandwidth, &u4NssOfUser0, &u4NssOfUser1,
	     &u4PfMuIdOfUser0, &u4PfMuIdOfUser1, &u4NumOfTxer, &u4SpeIndex, &u4GroupIndex);
	if (rv == 9) {
		DBGLOG(RFTEST, ERROR,
		       "MT6632 Set_MUCalInitMCS prInBuf = %s, u4NumOfUser = %x, u4Bandwidth = %x, u4NssOfUser0 = %x, u4NssOfUser1 = %x, u4PfMuIdOfUser0 = %x, u4PfMuIdOfUser1 = %x, u4NumOfTxer = %x, u4SpeIndex = %x, u4GroupIndex = %x",
		       prInBuf, u4NumOfUser, u4Bandwidth, u4NssOfUser0, u4NssOfUser1, u4PfMuIdOfUser0, u4PfMuIdOfUser1,
		       u4NumOfTxer, u4SpeIndex, u4GroupIndex);

		rMuMimoActionInfo.ucMuMimoCategory = MU_SET_CALC_INIT_MCS;
		rMuMimoActionInfo.unMuMimoParam.rMuSetInitMcs.ucNumOfUser = u4NumOfUser;
		rMuMimoActionInfo.unMuMimoParam.rMuSetInitMcs.ucBandwidth = u4Bandwidth;
		rMuMimoActionInfo.unMuMimoParam.rMuSetInitMcs.ucNssOfUser0 = u4NssOfUser0;
		rMuMimoActionInfo.unMuMimoParam.rMuSetInitMcs.ucNssOfUser1 = u4NssOfUser1;
		rMuMimoActionInfo.unMuMimoParam.rMuSetInitMcs.ucPfMuIdOfUser0 = u4PfMuIdOfUser0;
		rMuMimoActionInfo.unMuMimoParam.rMuSetInitMcs.ucPfMuIdOfUser1 = u4PfMuIdOfUser1;
		rMuMimoActionInfo.unMuMimoParam.rMuSetInitMcs.ucNumOfTxer = u4NumOfTxer;
		rMuMimoActionInfo.unMuMimoParam.rMuSetInitMcs.ucSpeIndex = u4SpeIndex;
		rMuMimoActionInfo.unMuMimoParam.rMuSetInitMcs.u4GroupIndex = u4GroupIndex;

		i4Status = kalIoctl(prGlueInfo,
				    wlanoidMuMimoAction,
				    &rMuMimoActionInfo, sizeof(rMuMimoActionInfo), FALSE, FALSE, TRUE, &u4BufLen);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_MUCalLQ(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T rMuMimoActionInfo;
	INT_32 i4Status = 0;
	UINT_32 u4BufLen = 0;

	UINT_32 u4NumOfUser, u4Bandwidth, u4NssOfUser0, u4NssOfUser1, u4PfMuIdOfUser0, u4PfMuIdOfUser1,
	    u4NumOfTxer, u4SpeIndex, u4GroupIndex;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_MUCalLQ\n");

	kalMemZero(&rMuMimoActionInfo, sizeof(rMuMimoActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rv = sscanf
	    (prInBuf, "%x:%x:%x:%x:%x:%x:%x:%x:%x",
	     &u4NumOfUser, &u4Bandwidth, &u4NssOfUser0, &u4NssOfUser1,
	     &u4PfMuIdOfUser0, &u4PfMuIdOfUser1, &u4NumOfTxer, &u4SpeIndex, &u4GroupIndex);
	if (rv == 9) {
		DBGLOG(RFTEST, ERROR,
		       "MT6632 Set_MUCalLQ prInBuf = %s, u4NumOfUser = %x, u4Bandwidth = %x, u4NssOfUser0 = %x, u4NssOfUser1 = %x, u4PfMuIdOfUser0 = %x, u4PfMuIdOfUser1 = %x, u4NumOfTxer = %x, u4SpeIndex = %x, u4GroupIndex = %x",
		       prInBuf, u4NumOfUser, u4Bandwidth, u4NssOfUser0, u4NssOfUser1, u4PfMuIdOfUser0, u4PfMuIdOfUser1,
		       u4NumOfTxer, u4SpeIndex, u4GroupIndex);

		rMuMimoActionInfo.ucMuMimoCategory = MU_HQA_SET_CALC_LQ;
		/* rMuMimoActionInfo.unMuMimoParam.rMuSetCalcLq.ucType                           = u4Type; */
		rMuMimoActionInfo.unMuMimoParam.rMuSetCalcLq.ucNumOfUser = u4NumOfUser;
		rMuMimoActionInfo.unMuMimoParam.rMuSetCalcLq.ucBandwidth = u4Bandwidth;
		rMuMimoActionInfo.unMuMimoParam.rMuSetCalcLq.ucNssOfUser0 = u4NssOfUser0;
		rMuMimoActionInfo.unMuMimoParam.rMuSetCalcLq.ucNssOfUser1 = u4NssOfUser1;
		rMuMimoActionInfo.unMuMimoParam.rMuSetCalcLq.ucPfMuIdOfUser0 = u4PfMuIdOfUser0;
		rMuMimoActionInfo.unMuMimoParam.rMuSetCalcLq.ucPfMuIdOfUser1 = u4PfMuIdOfUser1;
		rMuMimoActionInfo.unMuMimoParam.rMuSetCalcLq.ucNumOfTxer = u4NumOfTxer;
		rMuMimoActionInfo.unMuMimoParam.rMuSetCalcLq.ucSpeIndex = u4SpeIndex;
		rMuMimoActionInfo.unMuMimoParam.rMuSetCalcLq.u4GroupIndex = u4GroupIndex;

		i4Status = kalIoctl(prGlueInfo,
				    wlanoidMuMimoAction,
				    &rMuMimoActionInfo, sizeof(rMuMimoActionInfo), FALSE, FALSE, TRUE, &u4BufLen);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_MUGetLQ(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T rMuMimoActionInfo;
	INT_32 i4Status = 0;
	/* UINT_32      u4Type; */
	UINT_32 u4BufLen = 0;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_MUGetLQ\n");

	kalMemZero(&rMuMimoActionInfo, sizeof(rMuMimoActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	/* if (sscanf(prInBuf, "%x", &u4Type) == 1) */
	/* { */
	/* DBGLOG(RFTEST, ERROR, "MT6632 Set_MUGetLQ prInBuf = %s, u4Type = %x", prInBuf, u4Type); */

	rMuMimoActionInfo.ucMuMimoCategory = MU_HQA_GET_CALC_LQ;
	/* rMuMimoActionInfo.unMuMimoParam.rMuGetLq.ucType                              = u4Type; */

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidMuMimoAction,
			    &rMuMimoActionInfo, sizeof(rMuMimoActionInfo), TRUE, TRUE, TRUE, &u4BufLen);
	/* } */
	/* else */
	/* { */
	/* return -EINVAL; */
	/* } */

	return i4Status;
}

int Set_MUSetSNROffset(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T rMuMimoActionInfo;
	INT_32 i4Status = 0;
	UINT_32 u4BufLen = 0;

	UINT_32 u4Val;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_MUSetSNROffset\n");

	kalMemZero(&rMuMimoActionInfo, sizeof(rMuMimoActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rv = kstrtoint(prInBuf, 0, &u4Val);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_MUSetSNROffset prInBuf = %s, u4Val = %x", prInBuf, u4Val);

		rMuMimoActionInfo.ucMuMimoCategory = MU_HQA_SET_SNR_OFFSET;
		rMuMimoActionInfo.unMuMimoParam.rMuSetSnrOffset.ucVal = u4Val;

		i4Status = kalIoctl(prGlueInfo,
				    wlanoidMuMimoAction,
				    &rMuMimoActionInfo, sizeof(rMuMimoActionInfo), FALSE, FALSE, TRUE, &u4BufLen);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_MUSetZeroNss(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T rMuMimoActionInfo;
	INT_32 i4Status = 0;
	UINT_32 u4BufLen = 0;

	UINT_32 u4Val;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_MUSetZeroNss\n");

	kalMemZero(&rMuMimoActionInfo, sizeof(rMuMimoActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rv = kstrtouint(prInBuf, 0, &u4Val);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_MUSetZeroNss prInBuf = %s, u4Val = %x", prInBuf, u4Val);

		rMuMimoActionInfo.ucMuMimoCategory = MU_HQA_SET_ZERO_NSS;
		rMuMimoActionInfo.unMuMimoParam.rMuSetZeroNss.ucVal = u4Val;

		i4Status = kalIoctl(prGlueInfo,
				    wlanoidMuMimoAction,
				    &rMuMimoActionInfo, sizeof(rMuMimoActionInfo), FALSE, FALSE, TRUE, &u4BufLen);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_MUSetSpeedUpLQ(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T rMuMimoActionInfo;
	INT_32 i4Status = 0;
	UINT_32 u4BufLen = 0;

	UINT_32 u4Val;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_MUSetSpeedUpLQ\n");

	kalMemZero(&rMuMimoActionInfo, sizeof(rMuMimoActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rv = kstrtouint(prInBuf, 0, &u4Val);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_MUSetSpeedUpLQ prInBuf = %s, u4Val = %x", prInBuf, u4Val);

		rMuMimoActionInfo.ucMuMimoCategory = MU_HQA_SET_SPEED_UP_LQ;
		rMuMimoActionInfo.unMuMimoParam.rMuSpeedUpLq.u4Val = u4Val;

		i4Status = kalIoctl(prGlueInfo,
				    wlanoidMuMimoAction,
				    &rMuMimoActionInfo, sizeof(rMuMimoActionInfo), FALSE, FALSE, TRUE, &u4BufLen);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_MUSetMUTable(struct net_device *prNetDev, UINT_8 *prTable)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T rMuMimoActionInfo;
	INT_32 i4Status = 0;
	UINT_32 u4BufLen = 0;
	/*UINT_32 i;
	 *UINT_32 u4Type, u4Length;
	 */

	DBGLOG(RFTEST, ERROR, "MT6632 Set_MUSetMUTable\n");

	kalMemZero(&rMuMimoActionInfo, sizeof(rMuMimoActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	/* if (sscanf(prInBuf, "%x:%x", &u4Type, &u4Length) == 2) */
	/* { */
	/* DBGLOG(RFTEST, ERROR, "MT6632 Set_MUSetMUTable prInBuf = %s, */
	/* u4Type = %x, u4Length = %x", prInBuf, u4Type, u4Length); */

	rMuMimoActionInfo.ucMuMimoCategory = MU_HQA_SET_MU_TABLE;
	/* rMuMimoActionInfo.unMuMimoParam.rMuSetMuTable.u2Type = u4Type; */
	/* rMuMimoActionInfo.unMuMimoParam.rMuSetMuTable.u4Length = u4Length; */

	/* for ( i = 0 ; i < NUM_MUT_NR_NUM * NUM_MUT_FEC * NUM_MUT_MCS * NUM_MUT_INDEX ; i++) */
	/* { */
	memcpy(rMuMimoActionInfo.unMuMimoParam.rMuSetMuTable.aucMetricTable, prTable,
	       NUM_MUT_NR_NUM * NUM_MUT_FEC * NUM_MUT_MCS * NUM_MUT_INDEX);
	/* } */

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidMuMimoAction,
			    &rMuMimoActionInfo, sizeof(rMuMimoActionInfo), FALSE, FALSE, TRUE, &u4BufLen);
	/* } */
	/* else */
	/* { */
	/* return -EINVAL; */
	/* } */

	return i4Status;
}

int Set_MUSetGroup(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T rMuMimoActionInfo;
	INT_32 i4Status = 0;
	UINT_32 u4BufLen = 0;
	UINT_32 i = 0;

	UINT_32 aucUser0MacAddr[PARAM_MAC_ADDR_LEN], aucUser1MacAddr[PARAM_MAC_ADDR_LEN];

	DBGLOG(RFTEST, ERROR, "MT6632 Set_MUSetGroup\n");

	kalMemZero(&rMuMimoActionInfo, sizeof(rMuMimoActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (sscanf(prInBuf, "%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x",
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4GroupIndex,
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4NumOfUser,
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User0Ldpc,
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User1Ldpc,
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4ShortGI,
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4Bw,
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User0Nss,
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User1Nss,
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4GroupId,
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User0UP,
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User1UP,
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User0MuPfId,
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User1MuPfId,
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User0InitMCS,
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User1InitMCS, &aucUser0MacAddr[0],
		   &aucUser0MacAddr[1], &aucUser0MacAddr[2], &aucUser0MacAddr[3], &aucUser0MacAddr[4],
		   &aucUser0MacAddr[5], &aucUser1MacAddr[0], &aucUser1MacAddr[1], &aucUser1MacAddr[2],
		   &aucUser1MacAddr[3], &aucUser1MacAddr[4], &aucUser1MacAddr[5]) == 27) {
		DBGLOG(RFTEST, ERROR,
			   "MT6632 Set_MUSetGroup prInBuf = %s,u4GroupIndex = %d, u4NumOfUser = %d, u4User0Ldpc = %d, u4User1Ldpc = %d, u4ShortGI = %d, u4Bw = %d, u4User0Nss = %d, u4User1Nss = %d, u4GroupId = %d, u4User0UP = %d, u4User1UP = %d,  u4User0MuPfId = %d, u4User1MuPfId = %d, u4User0InitMCS = %d,	u4User1InitMCS = %d,aucUser0MacAddr[0] = %x, aucUser0MacAddr[1] = %x, aucUser0MacAddr[2] = %x, aucUser0MacAddr[3] = %x, aucUser0MacAddr[4] = %x, aucUser0MacAddr[5] = %x,aucUser1MacAddr[0] = %x, aucUser1MacAddr[1] = %x, aucUser1MacAddr[2] = %x, aucUser1MacAddr[3] = %x, aucUser1MacAddr[4] = %x, aucUser1MacAddr[5] = %x,",
			   prInBuf,
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4GroupIndex,
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4NumOfUser,
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User0Ldpc,
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User1Ldpc,
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4ShortGI,
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4Bw,
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User0Nss,
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User1Nss,
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4GroupId,
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User0UP,
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User1UP,
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User0MuPfId,
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User1MuPfId,
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User0InitMCS,
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.u4User1InitMCS, aucUser0MacAddr[0],
		       aucUser0MacAddr[1], aucUser0MacAddr[2], aucUser0MacAddr[3], aucUser0MacAddr[4],
		       aucUser0MacAddr[5], aucUser1MacAddr[0], aucUser1MacAddr[1], aucUser1MacAddr[2],
		       aucUser1MacAddr[3], aucUser1MacAddr[4], aucUser1MacAddr[5]
		    );

		rMuMimoActionInfo.ucMuMimoCategory = MU_HQA_SET_GROUP;
		for (i = 0; i < PARAM_MAC_ADDR_LEN; i++) {
			rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.aucUser0MacAddr[i] = aucUser0MacAddr[i];
			rMuMimoActionInfo.unMuMimoParam.rMuSetGroup.aucUser1MacAddr[i] = aucUser1MacAddr[i];
		}

		i4Status = kalIoctl(prGlueInfo,
				    wlanoidMuMimoAction,
				    &rMuMimoActionInfo, sizeof(rMuMimoActionInfo), FALSE, FALSE, TRUE, &u4BufLen);
	} else {
		return -EINVAL;
	}

	return i4Status;
}

int Set_MUGetQD(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T rMuMimoActionInfo;
	INT_32 i4Status = 0;
	UINT_32 u4BufLen = 0;

	UINT_32 u4SubcarrierIndex, u4Length;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_MUGetQD\n");

	kalMemZero(&rMuMimoActionInfo, sizeof(rMuMimoActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (sscanf(prInBuf, "%x:%x", &u4SubcarrierIndex, &u4Length) == 2) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_MUGetQD prInBuf = %s, u4SubcarrierIndex = %x, u4Length = %x", prInBuf,
		       u4SubcarrierIndex, u4Length);

		rMuMimoActionInfo.ucMuMimoCategory = MU_HQA_GET_QD;
		rMuMimoActionInfo.unMuMimoParam.rMuGetQd.ucSubcarrierIndex = u4SubcarrierIndex;
		/* rMuMimoActionInfo.unMuMimoParam.rMuGetQd.u4Length = u4Length; */
		/* rMuMimoActionInfo.unMuMimoParam.rMuGetQd.ucgroupIdx = ucgroupIdx; */

		i4Status = kalIoctl(prGlueInfo,
				    wlanoidMuMimoAction,
				    &rMuMimoActionInfo, sizeof(rMuMimoActionInfo), TRUE, TRUE, TRUE, &u4BufLen);
	} else {
		return -EINVAL;
	}

	return i4Status;
}

int Set_MUSetEnable(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T rMuMimoActionInfo;
	INT_32 i4Status = 0;
	UINT_32 u4BufLen = 0;

	UINT_32 u4Val;
	INT_32 rv;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_MUSetEnable\n");

	kalMemZero(&rMuMimoActionInfo, sizeof(rMuMimoActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rv = kstrtouint(prInBuf, 0, &u4Val);
	if (rv == 0) {
		DBGLOG(RFTEST, ERROR, "MT6632 Set_MUSetEnable prInBuf = %s, u4Val = %x", prInBuf, u4Val);

		rMuMimoActionInfo.ucMuMimoCategory = MU_HQA_SET_ENABLE;
		rMuMimoActionInfo.unMuMimoParam.rMuSetEnable.ucVal = u4Val;

		i4Status = kalIoctl(prGlueInfo,
				    wlanoidMuMimoAction,
				    &rMuMimoActionInfo, sizeof(rMuMimoActionInfo), FALSE, FALSE, TRUE, &u4BufLen);
	} else
		return -EINVAL;

	return i4Status;
}

int Set_MUSetGID_UP(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T rMuMimoActionInfo;
	INT_32 i4Status = 0;
	UINT_32 u4BufLen = 0;

	DBGLOG(RFTEST, ERROR, "MT6632 Set_MUSetGID_UP\n");

	kalMemZero(&rMuMimoActionInfo, sizeof(rMuMimoActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (sscanf(prInBuf, "%x:%x:%x:%x:%x:%x",
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGidUp.au4Gid[0],
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGidUp.au4Gid[1],
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGidUp.au4Up[0],
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGidUp.au4Up[1],
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGidUp.au4Up[2],
		   &rMuMimoActionInfo.unMuMimoParam.rMuSetGidUp.au4Up[3]) == 6) {
		DBGLOG(RFTEST, ERROR,
		       "MT6632 Set_MUSetGID_UP prInBuf = %s, au4Gid[0] = %x, au4Gid[1] = %x, au4Up[0] = %x, au4Up[1] = %x, au4Up[2] = %x, au4Up[3] = %x",
		       prInBuf, rMuMimoActionInfo.unMuMimoParam.rMuSetGidUp.au4Gid[0],
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGidUp.au4Gid[1],
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGidUp.au4Up[0],
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGidUp.au4Up[1],
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGidUp.au4Up[2],
		       rMuMimoActionInfo.unMuMimoParam.rMuSetGidUp.au4Up[3]);

		rMuMimoActionInfo.ucMuMimoCategory = MU_HQA_SET_STA_PARAM;

		i4Status = kalIoctl(prGlueInfo,
				    wlanoidMuMimoAction,
				    &rMuMimoActionInfo, sizeof(rMuMimoActionInfo), FALSE, FALSE, TRUE, &u4BufLen);
	} else {
		return -EINVAL;
	}

	return i4Status;
}

int Set_MUTriggerTx(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T rMuMimoActionInfo;
	INT_32 i4Status = 0;
	UINT_32 u4BufLen = 0;
	UINT_32 i, j;

	UINT_32 u4IsRandomPattern, u4MsduPayloadLength0, u4MsduPayloadLength1, u4MuPacketCount, u4NumOfSTAs;
	UINT_32 au4MacAddrs[2][6];

	DBGLOG(RFTEST, ERROR, "MT6632 Set_MUTriggerTx\n");

	kalMemZero(&rMuMimoActionInfo, sizeof(rMuMimoActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (sscanf(prInBuf, "%d:%x:%x:%x:%d:%d:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x",
		   &u4IsRandomPattern, &u4MsduPayloadLength0, &u4MsduPayloadLength1, &u4MuPacketCount, &u4NumOfSTAs,
		   &au4MacAddrs[0][0], &au4MacAddrs[0][1], &au4MacAddrs[0][2], &au4MacAddrs[0][3], &au4MacAddrs[0][4],
		   &au4MacAddrs[0][5], &au4MacAddrs[1][0], &au4MacAddrs[1][1], &au4MacAddrs[1][2], &au4MacAddrs[1][3],
		   &au4MacAddrs[1][4], &au4MacAddrs[1][5]) == 17) {
		DBGLOG(RFTEST, ERROR,
			"MT6632 Set_MUTriggerTx prInBuf = %s, u4IsRandomPattern = %x, u4MsduPayloadLength0 = %x, u4MsduPayloadLength1 = %x, u4MuPacketCount = %x, u4NumOfSTAs = %x, au4MacAddrs[0][0] = %x, au4MacAddrs[0][1] = %x, au4MacAddrs[0][2] = %x, au4MacAddrs[0][3] = %x, au4MacAddrs[0][4] = %x, au4MacAddrs[0][5] = %x,au4MacAddrs[1][0] = %x, au4MacAddrs[1][1] = %x, au4MacAddrs[1][2] = %x, au4MacAddrs[1][3] = %x, au4MacAddrs[1][4] = %x, au4MacAddrs[1][5] = %x",
			prInBuf, u4IsRandomPattern, u4MsduPayloadLength0, u4MsduPayloadLength1, u4MuPacketCount,
		    u4NumOfSTAs, au4MacAddrs[0][0], au4MacAddrs[0][1], au4MacAddrs[0][2], au4MacAddrs[0][3],
		    au4MacAddrs[0][4], au4MacAddrs[0][5], au4MacAddrs[1][0], au4MacAddrs[1][1], au4MacAddrs[1][2],
		    au4MacAddrs[1][3], au4MacAddrs[1][4], au4MacAddrs[1][5]);

		rMuMimoActionInfo.ucMuMimoCategory = MU_SET_TRIGGER_MU_TX;
		rMuMimoActionInfo.unMuMimoParam.rMuTriggerMuTx.fgIsRandomPattern = u4IsRandomPattern;
		rMuMimoActionInfo.unMuMimoParam.rMuTriggerMuTx.u4MsduPayloadLength0 = u4MsduPayloadLength0;
		rMuMimoActionInfo.unMuMimoParam.rMuTriggerMuTx.u4MsduPayloadLength1 = u4MsduPayloadLength1;
		rMuMimoActionInfo.unMuMimoParam.rMuTriggerMuTx.u4MuPacketCount = u4MuPacketCount;
		rMuMimoActionInfo.unMuMimoParam.rMuTriggerMuTx.u4NumOfSTAs = u4NumOfSTAs;

		for (i = 0 ; i < 2 ; i++) {
			for (j = 0 ; j < PARAM_MAC_ADDR_LEN ; j++)
				rMuMimoActionInfo.unMuMimoParam.rMuTriggerMuTx.aucMacAddrs[i][j] = au4MacAddrs[i][j];
		}

		i4Status = kalIoctl(prGlueInfo,
				    wlanoidMuMimoAction,
				    &rMuMimoActionInfo, sizeof(rMuMimoActionInfo), FALSE, FALSE, TRUE, &u4BufLen);
	} else {
		return -EINVAL;
	}

	return i4Status;
}
#endif
#endif


/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Write Efuse.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int WriteEfuse(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	INT_32 i4Status;
	INT_32 rv;
	UINT_32 addr[2];
	UINT_16 addr2[2];

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv WriteEfuse, buf: %s\n", prInBuf);

	rv = sscanf(prInBuf, "%x:%x", &addr[0], &addr[1]);

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv WriteEfuse, prInBuf: %s\n", prInBuf);
	DBGLOG(INIT, ERROR, "MT6632 : ATE_AGENT iwpriv WriteEfuse :%02x:%02x\n", addr[0], addr[1]);

	addr2[0] = (UINT_16) addr[0];
	addr2[1] = (UINT_16) addr[1];

	if (rv == 2)
		i4Status = MT_ATEWriteEfuse(prNetDev, addr2[0], addr2[1]);
	else
		return -EINVAL;

	return i4Status;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Set Tx Power.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int SetTxTargetPower(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	INT_32 i4Status;
	INT_32 rv;
	int addr;
	UINT_8 addr2;

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv Set Tx Target Power, buf: %s\n", prInBuf);

	/* rv = sscanf(prInBuf, "%u", &addr);*/
	rv = kstrtoint(prInBuf, 0, &addr);

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv Set Tx Target Power, prInBuf: %s\n", prInBuf);
	DBGLOG(INIT, ERROR, "MT6632 : ATE_AGENT iwpriv Set Tx Target Power :%02x\n", addr);

	addr2 = (UINT_8) addr;

	if (rv == 0)
		i4Status = MT_ATESetTxTargetPower(prNetDev, addr2);
	else
		return -EINVAL;

	return i4Status;
}

#if (CFG_SUPPORT_DFS_MASTER == 1)
/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Set RDD Report
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int SetRddReport(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	INT_32 i4Status;
	INT_32 rv;
	int dbdcIdx;
	UINT_8 ucDbdcIdx;

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv Set RDD Report, buf: %s\n", prInBuf);

	/* rv = sscanf(prInBuf, "%u", &addr);*/
	rv = kstrtoint(prInBuf, 0, &dbdcIdx);

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv Set RDD Report, prInBuf: %s\n", prInBuf);
	DBGLOG(INIT, ERROR, "MT6632 : ATE_AGENT iwpriv Set RDD Report : Band %d\n", dbdcIdx);

	if (p2pFuncGetDfsState() == DFS_STATE_INACTIVE || p2pFuncGetDfsState() == DFS_STATE_DETECTED) {
		DBGLOG(REQ, ERROR, "RDD Report is not supported in this DFS state (inactive or deteted)\n");
		return WLAN_STATUS_NOT_SUPPORTED;
	}

	if (dbdcIdx != 0 && dbdcIdx != 1) {
		DBGLOG(REQ, ERROR, "RDD index is not \"0\" or \"1\", Invalid data\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	ucDbdcIdx = (UINT_8) dbdcIdx;

	if (rv == 0)
		i4Status = MT_ATESetRddReport(prNetDev, ucDbdcIdx);
	else
		return -EINVAL;

	return i4Status;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Set By Pass CAC.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int SetByPassCac(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	INT_32 i4Status;
	INT_32 rv;
	INT_32 i4ByPassCacTime;
	UINT_32 u4ByPassCacTime;

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv Set By Pass Cac, buf: %s\n", prInBuf);

	rv = kstrtoint(prInBuf, 0, &i4ByPassCacTime);

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv Set By Pass Cac, prInBuf: %s\n", prInBuf);
	DBGLOG(INIT, ERROR, "MT6632 : ATE_AGENT iwpriv Set By Pass Cac : %dsec\n", i4ByPassCacTime);

	if (i4ByPassCacTime < 0) {
		DBGLOG(REQ, ERROR, "Cac time < 0, Invalid data\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	u4ByPassCacTime = (UINT_32) i4ByPassCacTime;

	p2pFuncEnableManualCac();

	if (rv == 0)
		i4Status = p2pFuncSetDriverCacTime(u4ByPassCacTime);
	else
		return -EINVAL;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to Set Radar Detect Mode.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int SetRadarDetectMode(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	INT_32 i4Status;
	INT_32 rv;
	int radarDetectMode;
	UINT_8 ucRadarDetectMode;

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv Set Radar Detect Mode, buf: %s\n", prInBuf);

	rv = kstrtoint(prInBuf, 0, &radarDetectMode);

	DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv Set Radar Detect Mode, prInBuf: %s\n", prInBuf);
	DBGLOG(INIT, ERROR, "MT6632 : ATE_AGENT iwpriv Set Radar Detect Mode : %d\n", radarDetectMode);

	if (p2pFuncGetDfsState() == DFS_STATE_INACTIVE || p2pFuncGetDfsState() == DFS_STATE_DETECTED) {
		DBGLOG(REQ, ERROR, "RDD Report is not supported in this DFS state (inactive or deteted)\n");
		return WLAN_STATUS_NOT_SUPPORTED;
	}

	if (radarDetectMode != 0 && radarDetectMode != 1) {
		DBGLOG(REQ, ERROR, "Radar Detect Mode is not \"0\" or \"1\", Invalid data\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	ucRadarDetectMode = (UINT_8) radarDetectMode;

	p2pFuncSetRadarDetectMode(ucRadarDetectMode);

	if (rv == 0)
		i4Status = MT_ATESetRadarDetectMode(prNetDev, ucRadarDetectMode);
	else
		return -EINVAL;

	return i4Status;
}

#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief  This routine is called to search the corresponding ATE agent function.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf		A pointer to the command string buffer
* \param[in] u4InBufLen	The length of the buffer
* \param[out] None
*
* \retval 0				On success.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
int AteCmdSetHandle(struct net_device *prNetDev, UINT_8 *prInBuf, UINT_32 u4InBufLen)
{
	UINT_8 *this_char, *value;
	P_ATE_PRIV_CMD prAtePrivCmd;
	INT_32 i4Status = 0;

	while ((this_char = strsep((char **)&prInBuf, ",")) != NULL) {
		if (!*this_char)
			continue;
		DBGLOG(RFTEST, ERROR, "MT6632 : ATE_AGENT iwpriv this_char = %s\n", this_char);
		DBGLOG(RFTEST, INFO, "MT6632 : ATE_AGENT iwpriv this_char = %s\n", this_char);

		value = strchr(this_char, '=');
		if (value != NULL)
			*value++ = 0;

		DBGLOG(REQ, INFO, "MT6632 : ATE_AGENT iwpriv cmd = %s, value = %s\n", this_char, value);

		for (prAtePrivCmd = rAtePrivCmdTable; prAtePrivCmd->name; prAtePrivCmd++) {
			if (!strcmp(this_char, prAtePrivCmd->name)) {
				/*FALSE:Set private failed then return Invalid argument */
				if (prAtePrivCmd->set_proc(prNetDev, value) != 0)
					i4Status = -EINVAL;
				break;	/*Exit for loop. */
			}
		}

		if (prAtePrivCmd->name == NULL) {	/*Not found argument */
			i4Status = -EINVAL;
			break;
		}
	}
	return i4Status;
}
#endif /*CFG_SUPPORT_QA_TOOL */
