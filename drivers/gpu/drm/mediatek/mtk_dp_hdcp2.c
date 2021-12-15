/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include "mtk_dp_hdcp2.h"
#include "mtk_dp.h"
#include "mtk_dp_reg.h"
#include "mtk_dp_hal.h"
#include "ca/tlcDpHdcp.h"

#ifdef DPTX_HDCP_ENABLE

struct hdcp2_info_tx {
	UINT8 rtx[HDCP2_RTX_SIZE];
	UINT8 txCaps[HDCP2_TXCAPS_SIZE];
	UINT8 Ekpub_km[HDCP2_EKPUBKM_SIZE];
	UINT8 Eks[HDCP2_EDKEYKS_SIZE];
	UINT8 VPrime[HDCP2_VPRIME_SIZE];
	UINT8 rn[HDCP2_RN_SIZE];
	UINT8 riv[HDCP2_RIV_SIZE];
	UINT8 seq_num_M[HDCP2_SEQ_NUM_M_SIZE];
	UINT8 k[HDCP2_K_SIZE];
	UINT8 u8StreamIDType[HDCP2_STREAMID_TYPE_SIZE];
};

struct hdcp2_info_rx {
	UINT8 cert[HDCP2_CERTRX_SIZE];
	UINT8 rrx[HDCP2_RRX_SIZE];
	UINT8 rxCaps[HDCP2_RXCAPS_SIZE];
	UINT8 rxInfo[HDCP2_RXINFO_SIZE];
	UINT8 Ekh_km[HDCP2_EKHKM_SIZE];
	UINT8 VPrime[HDCP2_VPRIME_SIZE];
	UINT8 MPrime[HDCP2_REP_MPRIME_SIZE];
	UINT8 HPrime[HDCP2_HPRIME_SIZE];
	UINT8 LPrime[HDCP2_LPRIME_SIZE];
	UINT8 RecvIDList[HDCP2_MAX_DEVICE_COUNT * HDCP2_RECVID_SIZE];
	UINT8 u8SeqNumV[HDCP2_SEQ_NUM_V_SIZE];
};

struct hdcp2_info_tx rhdcp_tx;
struct hdcp2_info_rx rhdcp_rx;

UINT8 g_u8LC128_real[16] = {
	// need add LC128(16Bytes)
};

UINT8 t_kpubdcp_real[385] = {
	//add DCP LLC Public Key (HDCP22 SPEC p73)
};

UINT8 t_rtx[HDCP2_RTX_SIZE] = {
	0x18, 0xfa, 0xe4, 0x20, 0x6a, 0xfb, 0x51, 0x49
};

UINT8 t_txCaps[HDCP2_TXCAPS_SIZE] = {
	0x02, 0x00, 0x00
};

UINT8 t_rn[HDCP2_RN_SIZE] = {
	0x32, 0x75, 0x3e, 0xa8, 0x78, 0xa6, 0x38, 0x1c
};

UINT8 t_riv[HDCP2_RIV_SIZE] = {
	0x40, 0x2b, 0x6b, 0x43, 0xc5, 0xe8, 0x86, 0xd8
};

struct HDCP2_HANDLER {
	BYTE u8MainState;
	BYTE u8SubState;
	BYTE u8DownStreamDevCnt;
	BYTE u8HDCPRxVer;
	bool bSendAKEInit:1;
	bool bGetRecvIDList:1;
	bool bStoredKm:1;
	bool bSendLCInit:1;
	bool bSendACK:1;
	bool bSinkIsRepeater:1;
	bool bRecvMsg:1;
	bool bSendPair:1;
	DWORD u32SeqNumVCnt;
	uint32_t u8RetryCnt;
};

struct HDCP2_HANDLER g_stHdcpHandler;
struct HDCP2_PAIRING_INFO g_stStoredPairingInfo;
static DWORD g_u32PreTime;

void HDCPTx_cmmDumpHex(const char *pName, const unsigned char *data, int len)
{
	int pos = 0;

	if (data == NULL || len <= 0)
		return;

	DPTXDBG("----Start Dump:%s %d---\n", pName, len);

	while (pos < len) {
		DPTXDBG("[%d]|0x%x\n", pos, data[pos]);
		pos++;
	}

	DPTXDBG("--------END---------\n");
}


void HDCPTx_Hdcp2SetState(BYTE u8MainState, BYTE u8SubState)
{
	g_stHdcpHandler.u8MainState = u8MainState;
	g_stHdcpHandler.u8SubState = u8SubState;
}

void HDCPTx_Hdcp2xSetAuthPass(struct mtk_dp *mtk_dp, bool bEnable)
{
	if (bEnable) {
		msWriteByteMask(mtk_dp, REG_3400_DP_TRANS_P0 + 1, BIT3, BIT3);
		msWriteByteMask(mtk_dp, REG_34A4_DP_TRANS_P0, BIT4, BIT4);
	} else {
		msWriteByteMask(mtk_dp, REG_3400_DP_TRANS_P0 + 1, 0, BIT3);
		msWriteByteMask(mtk_dp, REG_34A4_DP_TRANS_P0, 0, BIT4);
	}
}

void HDCPTx_Hdcp2EnableAuth(struct mtk_dp *mtk_dp, bool bEnable)
{
	DPTXFUNC();
	HDCPTx_Hdcp2xSetAuthPass(mtk_dp, bEnable);
	if (bEnable) {
		uint32_t version = HDCP_V2_3;

		if (rhdcp_rx.rxInfo[1] & BIT0)
			version = HDCP_V1;
		else if (rhdcp_rx.rxInfo[1] & BIT1)
			version = HDCP_V2;

		tee_hdcp_enableEncrypt(bEnable, version);
	} else
		tee_hdcp_enableEncrypt(bEnable, HDCP_NONE);
}

int HDCPTx_Hdcp2Init(struct mtk_dp *mtk_dp)
{
	int enErrCode = HDCP_ERR_NONE;

	DPTXFUNC();

	memset(&rhdcp_tx, 0, sizeof(struct hdcp2_info_tx));
	memset(&rhdcp_rx, 0, sizeof(struct hdcp2_info_rx));
	memcpy(rhdcp_tx.rtx, t_rtx, HDCP2_RTX_SIZE);
	memcpy(rhdcp_tx.txCaps, t_txCaps, HDCP2_TXCAPS_SIZE);
	memcpy(rhdcp_tx.rn, t_rn, HDCP2_RN_SIZE);
	memcpy(rhdcp_tx.riv, t_riv, HDCP2_RIV_SIZE);

	memset(&g_stHdcpHandler, 0, sizeof(struct HDCP2_HANDLER));
	memset(&g_stStoredPairingInfo, 0, sizeof(struct HDCP2_PAIRING_INFO));

	g_u32PreTime = 0;
	HDCPTx_Hdcp2EnableAuth(mtk_dp, false);

	return enErrCode;
}

void mdrv_DPTx_HDCP2_RestVariable(struct mtk_dp *mtk_dp)
{
	mtk_dp->info.hdcp2_info.bReadcertrx = false;
	mtk_dp->info.hdcp2_info.bReadHprime = false;
	mtk_dp->info.hdcp2_info.bReadPairing = false;
	mtk_dp->info.hdcp2_info.bReadLprime = false;
	mtk_dp->info.hdcp2_info.bksExchangeDone = false;
	mtk_dp->info.hdcp2_info.bReadVprime = false;
}

void mhal_DPTx_HDCP2_FillStreamType(struct mtk_dp *mtk_dp, BYTE ucType)
{
	msWriteByte(mtk_dp, REG_34D0_DP_TRANS_P0, ucType);
}

bool HDCPTx_Hdcp2xIncSeqNumM(void)
{
	BYTE i = 0;
	DWORD u32TempValue = 0;

	for (i = 0; i < HDCP2_SEQ_NUM_M_SIZE; i++)
		u32TempValue |= rhdcp_tx.seq_num_M[i] << (i*8);

	if (u32TempValue == 0xFFFFFF)
		return false;

	u32TempValue++;

	for (i = 0; i < HDCP2_SEQ_NUM_M_SIZE; i++)
		rhdcp_tx.seq_num_M[i]
			= (u32TempValue & ((DWORD)0xFF << (i*8))) >> (i*8);
	return true;
}

bool HDCPTx_Hdcp2ProcessRepAuthStreamManage(struct mtk_dp *mtk_dp)
{
	bool ret = false;

	rhdcp_tx.k[0] = 0x00;
	rhdcp_tx.k[1] = 0x01;

	rhdcp_tx.u8StreamIDType[0] = 0x00; //Payload ID
	rhdcp_tx.u8StreamIDType[1] = mtk_dp->info.hdcp2_info.uStreamIDType;

	ret = HDCPTx_Hdcp2xIncSeqNumM();

	return ret;
}


bool HDCPTx_Hdcp2RecvRepAuthSendRecvIDList(struct mtk_dp *mtk_dp)
{
	bool ret = false;
	uint8_t *buffer = NULL;
	uint32_t len = 0, len_RecvIDList = 0;

	len_RecvIDList
		= mtk_dp->info.hdcp2_info.uDeviceCount * HDCP2_RECVID_SIZE;
	len = len_RecvIDList + HDCP2_RXINFO_SIZE + HDCP2_SEQ_NUM_V_SIZE;
	buffer = kmalloc(len, GFP_KERNEL);
	if (!buffer) {
		DPTXERR("Out of Memory\n");
		return ret;
	}

	memcpy(buffer, rhdcp_rx.RecvIDList, len_RecvIDList);
	memcpy(buffer + len_RecvIDList, rhdcp_rx.rxInfo, HDCP2_RXINFO_SIZE);
	memcpy(buffer + len_RecvIDList + HDCP2_RXINFO_SIZE,
		rhdcp_rx.u8SeqNumV, HDCP2_SEQ_NUM_V_SIZE);

	if (tee_hdcp2_ComputeCompareV(buffer, len,
		rhdcp_rx.VPrime, rhdcp_tx.VPrime) == RET_COMPARE_PASS) {
		ret = true;
		DPTXMSG("V' is PASS!!\n");
	} else
		DPTXMSG("V' is FAIL!!\n");

	kfree(buffer);
	return ret;
}

bool HDCPTx_Hdcp2RecvRepAuthStreamReady(struct mtk_dp *mtk_dp)
{
	bool ret = false;
	uint8_t *buffer = NULL;
	uint32_t len = 0;

	len = HDCP2_STREAMID_TYPE_SIZE + HDCP2_SEQ_NUM_M_SIZE;
	buffer = kmalloc(len, GFP_KERNEL);
	if (!buffer) {
		DPTXERR("Out of Memory\n");
		return ret;
	}

	memcpy(buffer, rhdcp_tx.u8StreamIDType, HDCP2_STREAMID_TYPE_SIZE);
	memcpy(buffer + HDCP2_STREAMID_TYPE_SIZE, rhdcp_tx.seq_num_M,
		HDCP2_SEQ_NUM_M_SIZE);

	if (tee_hdcp2_ComputeCompareM(buffer, len, rhdcp_rx.MPrime)
		== RET_COMPARE_PASS) {
		ret = true;
		DPTXMSG("M' is PASS!!\n");
	} else
		DPTXMSG("M' is FAIL!!\n");

	kfree(buffer);
	return ret;
}


bool HDCPTx_Hdcp2CheckSeqNumV(struct mtk_dp *mtk_dp)
{
	if (((rhdcp_rx.u8SeqNumV[0] == 0x00)
			&& (rhdcp_rx.u8SeqNumV[1] == 0x00)
			&& (rhdcp_rx.u8SeqNumV[2] == 0x00))
		&& g_stHdcpHandler.u32SeqNumVCnt > 0xFFFFFF) {
		DPTXMSG("SeqNumV Rollover!\n");
		return false;
	}

	if ((rhdcp_rx.u8SeqNumV[0]
		!= (BYTE)((g_stHdcpHandler.u32SeqNumVCnt & 0xFF0000) >> 16))
			|| (rhdcp_rx.u8SeqNumV[1]
		!= (BYTE)((g_stHdcpHandler.u32SeqNumVCnt & 0x00FF00) >> 8))
			|| (rhdcp_rx.u8SeqNumV[2]
		!= (BYTE)((g_stHdcpHandler.u32SeqNumVCnt & 0x0000FF)))) {
		DPTXMSG("Invalid Seq_num_V!\n");
		return false;
	}

	g_stHdcpHandler.u32SeqNumVCnt++;
	return true;
}

void HDCPTx_ERRHandle(int errMsg, int line)
{
	DPTXERR("MainState:%d; SubState:%d;\n", g_stHdcpHandler.u8MainState,
		g_stHdcpHandler.u8SubState);

	switch (errMsg) {
	case HDCP_ERR_UNKNOWN_STATE:
		DPTXERR("Unknown State, line:%d\n", line);
		HDCPTx_Hdcp2SetState(HDCP2_MS_H1P1, HDCP2_MSG_AUTH_FAIL);
		break;

	case HDCP_ERR_SEND_MSG_FAIL:
		DPTXERR("Send Msg Fail, line:%d\n", line);
		HDCPTx_Hdcp2SetState(HDCP2_MS_A0F0, HDCP2_MSG_ZERO);
		break;
	case HDCP_ERR_RESPONSE_TIMEROUT:
		DPTXERR("Response Timeout, line:%d!\n", line);
		HDCPTx_Hdcp2SetState(HDCP2_MS_A0F0, HDCP2_MSG_ZERO);
		break;

	case HDCP_ERR_PROCESS_FAIL:
		DPTXERR("Process Fail, line:%d!\n", line);
		HDCPTx_Hdcp2SetState(HDCP2_MS_A0F0, HDCP2_MSG_ZERO);
		break;

	default:
		DPTXERR("NO ERROR!");
		break;
	}
}

bool HDCPTx_ReadMsg(struct mtk_dp *mtk_dp, BYTE u8CmdID)
{
	bool bRet = false;
	uint8_t size = 0;

	switch (u8CmdID) {
	case HDCP2_MSG_AKE_SEND_CERT:
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_6900B, rhdcp_rx.cert,
			HDCP2_CERTRX_SIZE);
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_69215, rhdcp_rx.rrx,
			HDCP2_RRX_SIZE);
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_6921D, rhdcp_rx.rxCaps,
			HDCP2_RXCAPS_SIZE);

		mtk_dp->info.hdcp2_info.bReadcertrx = false;
		g_stHdcpHandler.bRecvMsg = true;
		bRet = true;
		DPTXMSG("HDCP2_MSG_AKE_SEND_CERT\n");
		break;

	case HDCP2_MSG_AKE_SEND_H_PRIME:
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_692C0, rhdcp_rx.HPrime,
			HDCP2_HPRIME_SIZE);
		mtk_dp->info.hdcp2_info.bReadHprime = false;
		g_stHdcpHandler.bRecvMsg = true;
		bRet = true;

		DPTXMSG("HDCP2_MSG_AKE_SEND_H_PRIME\n");
		break;

	case HDCP2_MSG_AKE_SEND_PAIRING_INFO:
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_692E0, rhdcp_rx.Ekh_km,
			HDCP2_EKHKM_SIZE);
		mtk_dp->info.hdcp2_info.bReadPairing = false;
		g_stHdcpHandler.bRecvMsg = true;
		bRet = true;
		DPTXMSG("HDCP2_MSG_AKE_SEND_PAIRING_INFO\n");
		break;

	case HDCP2_MSG_LC_SEND_L_PRIME:
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_692F8, rhdcp_rx.LPrime,
			HDCP2_LPRIME_SIZE);

		mtk_dp->info.hdcp2_info.bReadLprime = false;
		g_stHdcpHandler.bRecvMsg = true;
		bRet = true;
		DPTXMSG("HDCP2_MSG_LC_SEND_L_PRIME\n");
		break;

	case HDCP2_MSG_REPAUTH_SEND_RECVID_LIST:
		drm_dp_dpcd_read(&mtk_dp->aux,
			DPCD_69330,
			rhdcp_rx.rxInfo,
			HDCP2_RXINFO_SIZE);
		mtk_dp->info.hdcp2_info.uDeviceCount
			= ((rhdcp_rx.rxInfo[1] & MASKBIT(7:4)) >> 4)
				| ((rhdcp_rx.rxInfo[0] & BIT0) << 4);

		drm_dp_dpcd_read(&mtk_dp->aux,
			DPCD_69332,
			rhdcp_rx.u8SeqNumV,
			HDCP2_SEQ_NUM_V_SIZE);
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_69335,
			rhdcp_rx.VPrime,
			HDCP2_VPRIME_SIZE);
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_69345,
			rhdcp_rx.RecvIDList,
			mtk_dp->info.hdcp2_info.uDeviceCount
				* HDCP2_RECVID_SIZE);

		mtk_dp->info.hdcp2_info.bReadVprime = false;
		g_stHdcpHandler.bRecvMsg = true;
		bRet = true;
		DPTXMSG("HDCP2_MSG_REPAUTH_SEND_RECVID_LIST\n");
		break;

	case HDCP2_MSG_REPAUTH_STREAM_READY:
		size = drm_dp_dpcd_read(&mtk_dp->aux,
			DPCD_69473,
			rhdcp_rx.MPrime,
			HDCP2_REP_MPRIME_SIZE);

		if (size == HDCP2_REP_MPRIME_SIZE)
			g_stHdcpHandler.bRecvMsg = true;
		bRet = true;
		DPTXMSG("HDCP2_MSG_REPAUTH_STREAM_READY\n");
		break;

	default:
		DPTXMSG("Invalid DPTX_HDCP2_OffSETADDR_ReadMessage !\n");
		break;
	}

	return bRet;
}

bool HDCPTx_WriteMsg(struct mtk_dp *mtk_dp, BYTE u8CmdID)
{
	bool bRet = false;

	switch (u8CmdID) {
	case HDCP2_MSG_AKE_INIT:
		tee_hdcp2_softRst();
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_69000, rhdcp_tx.rtx,
			HDCP2_RTX_SIZE);
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_69008, rhdcp_tx.txCaps,
			HDCP2_TXCAPS_SIZE);

		bRet = true;
		DPTXMSG("HDCP2_MSG_AKE_Init !\n");
		break;

	case HDCP2_MSG_AKE_NO_STORED_KM:
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_69220, rhdcp_tx.Ekpub_km,
			HDCP2_EKPUBKM_SIZE);

		bRet = true;

		DPTXMSG("HDCP2_MSG_AKE_NO_STORED_KM !\n");
		break;

	case HDCP2_MSG_AKE_STORED_KM:
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_692A0,
			g_stStoredPairingInfo.u8EkhKM, HDCP2_EKHKM_SIZE);
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_692B0,
			g_stStoredPairingInfo.u8M, HDCP2_M_SIZE);

		bRet = true;

		DPTXMSG("DPTX_HDCP2_MSG_AKE_STORED_KM !\n");
		break;

	case HDCP2_MSG_LC_INIT:
		drm_dp_dpcd_write(&mtk_dp->aux,
			DPCD_692F0,
			rhdcp_tx.rn,
			HDCP2_RN_SIZE);

		mtk_dp->info.hdcp2_info.bReadLprime = true;
		bRet = true;

		DPTXMSG("HDCP2_MSG_LC_INIT !\n");
		break;

	case HDCP2_MSG_SKE_SEND_EKS:
		drm_dp_dpcd_write(&mtk_dp->aux,
			DPCD_69318,
			rhdcp_tx.Eks,
			HDCP2_EDKEYKS_SIZE);
		drm_dp_dpcd_write(&mtk_dp->aux,
			DPCD_69328,
			rhdcp_tx.riv,
			HDCP2_RIV_SIZE);

		mtk_dp->info.hdcp2_info.bksExchangeDone = true;

		bRet = true;
		DPTXMSG("HDCP2_MSG_SKE_SEND_EKS !\n");
		break;

	case HDCP2_MSG_REPAUTH_SEND_ACK:
		drm_dp_dpcd_write(&mtk_dp->aux,
			DPCD_693E0,
			rhdcp_tx.VPrime,
			HDCP2_VPRIME_SIZE);

		bRet = true;
		DPTXMSG("HDCP2_MSG_SEND_ACK !\n");
		break;

	case HDCP2_MSG_REPAUTH_STREAM_MANAGE:
		drm_dp_dpcd_write(&mtk_dp->aux,
			DPCD_693F0,
			rhdcp_tx.seq_num_M,
			HDCP2_SEQ_NUM_M_SIZE);
		drm_dp_dpcd_write(&mtk_dp->aux,
			DPCD_693F3,
			rhdcp_tx.k,
			HDCP2_K_SIZE);
		drm_dp_dpcd_write(&mtk_dp->aux,
			DPCD_693F5,
			rhdcp_tx.u8StreamIDType,
			HDCP2_STREAMID_TYPE_SIZE);

		mhal_DPTx_HDCP2_FillStreamType(mtk_dp,
			mtk_dp->info.hdcp2_info.uStreamIDType);

		bRet = true;
		DPTXMSG("HDCP2_MSG_STREAM_MANAGE !\n");
		break;

	default:
		DPTXMSG("Invalid HDCP2_OffSETADDR_WriteMessage !\n");
		break;
	}

	return bRet;

}

int HDCPTx_Hdcp2FSM(struct mtk_dp *mtk_dp)
{
	static WORD u16TimeoutValue;
	int enErrCode = HDCP_ERR_NONE;
	static BYTE u8PreMain;
	static BYTE u8PreSub;
	bool bStored = false;

	if ((u8PreMain != g_stHdcpHandler.u8MainState)
		|| (g_stHdcpHandler.u8SubState != u8PreSub)) {

		DPTXMSG("Port(M : S)= (%d, %d)", g_stHdcpHandler.u8MainState,
			g_stHdcpHandler.u8SubState);
		u8PreMain = g_stHdcpHandler.u8MainState;
		u8PreSub = g_stHdcpHandler.u8SubState;
	}

	switch (g_stHdcpHandler.u8MainState) {
	case HDCP2_MS_H1P1:
		switch (g_stHdcpHandler.u8SubState) {
		case HDCP2_MSG_ZERO:
			break;
		case HDCP2_MSG_AUTH_FAIL:
			DPTXERR("HDCP2.x Authentication Fail\n");
			HDCPTx_Hdcp2EnableAuth(mtk_dp, false);
			mtk_dp->info.bAuthStatus = AUTH_FAIL;
			break;
		default:
			enErrCode = HDCP_ERR_UNKNOWN_STATE;
			HDCPTx_ERRHandle(enErrCode, __LINE__);
			break;
		}
		break;
	case HDCP2_MS_A0F0:
		switch (g_stHdcpHandler.u8SubState) {
		case HDCP2_MSG_ZERO:
			if (mtk_dp->info.hdcp2_info.bEnable) {
				HDCPTx_Hdcp2Init(mtk_dp);
				HDCPTx_Hdcp2SetState(HDCP2_MS_A1F1,
					HDCP2_MSG_ZERO);
				DPTXMSG("Sink Support Hdcp2x!\n");
			} else {
				HDCPTx_Hdcp2SetState(HDCP2_MS_H1P1,
					HDCP2_MSG_AUTH_FAIL);
				DPTXMSG("Sink Doesn't Support Hdcp2x!\n");
			}
			break;

		default:
			enErrCode = HDCP_ERR_UNKNOWN_STATE;
			HDCPTx_ERRHandle(enErrCode, __LINE__);
			break;
		}
		break;

	case HDCP2_MS_A1F1:
		switch (g_stHdcpHandler.u8SubState) {
		case HDCP2_MSG_ZERO:
			if (mtk_dp->info.hdcp2_info.uRetryCount
				< HDCP2_TX_RETRY_CNT) {
				tee_hdcp2_setKey(t_kpubdcp_real,
					g_u8LC128_real);
				mtk_dp->info.hdcp2_info.uRetryCount++;
				HDCPTx_Hdcp2SetState(HDCP2_MS_A1F1,
					HDCP2_MSG_AKE_INIT);
			} else {
				HDCPTx_Hdcp2SetState(HDCP2_MS_H1P1,
					HDCP2_MSG_AUTH_FAIL);
				DPTXERR("Try Max Count\n");
			}
			break;

		case HDCP2_MSG_AKE_INIT:
			if (!HDCPTx_WriteMsg(mtk_dp, HDCP2_MSG_AKE_INIT)) {
				enErrCode = HDCP_ERR_SEND_MSG_FAIL;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			mdrv_DPTx_HDCP2_RestVariable(mtk_dp);
			mtk_dp->info.hdcp2_info.bReadcertrx = true;

			g_stHdcpHandler.bSendAKEInit = true;
			HDCPTx_Hdcp2SetState(HDCP2_MS_A1F1,
				HDCP2_MSG_AKE_SEND_CERT);
			g_u32PreTime = getSystemTime();
			break;

		case HDCP2_MSG_AKE_SEND_CERT:
			if (mtk_dp->info.hdcp2_info.bReadcertrx)
				HDCPTx_ReadMsg(mtk_dp, HDCP2_MSG_AKE_SEND_CERT);

			if (getTimeDiff(g_u32PreTime) > HDCP2_AKESENDCERT_WDT) {
				enErrCode = HDCP_ERR_RESPONSE_TIMEROUT;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			if (!g_stHdcpHandler.bRecvMsg)
				break;

			if (tee_akeCertificate(rhdcp_rx.cert,
				&bStored,
				g_stStoredPairingInfo.u8M,
				g_stStoredPairingInfo.u8EkhKM)
				!= RET_COMPARE_PASS) {
				enErrCode = HDCP_ERR_PROCESS_FAIL;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			g_stHdcpHandler.bStoredKm = bStored;
			g_stHdcpHandler.bRecvMsg = false;
			HDCPTx_Hdcp2SetState(HDCP2_MS_A1F1,
				g_stHdcpHandler.bStoredKm ?
					HDCP2_MSG_AKE_STORED_KM :
					HDCP2_MSG_AKE_NO_STORED_KM);
			break;

		case HDCP2_MSG_AKE_NO_STORED_KM:
			DPTXMSG("4. Get Km, derive Ekpub(km)\n");

			tee_encRsaesOaep(rhdcp_tx.Ekpub_km);
			//prepare Ekpub_km to send
			if (!HDCPTx_WriteMsg(mtk_dp,
				HDCP2_MSG_AKE_NO_STORED_KM)) {
				enErrCode = HDCP_ERR_SEND_MSG_FAIL;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			HDCPTx_Hdcp2SetState(HDCP2_MS_A1F1,
				HDCP2_MSG_AKE_SEND_H_PRIME);
			u16TimeoutValue = HDCP2_AKESENDHPRIME_NO_STORED_WDT;
			g_stHdcpHandler.bRecvMsg = false;
			g_u32PreTime = getSystemTime();
			break;
		case HDCP2_MSG_AKE_STORED_KM:
			//prepare Ekh_km & M to send
			if (!HDCPTx_WriteMsg(mtk_dp, HDCP2_MSG_AKE_STORED_KM)) {
				enErrCode = HDCP_ERR_SEND_MSG_FAIL;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			enErrCode = HDCP_ERR_NONE;
			HDCPTx_Hdcp2SetState(HDCP2_MS_A1F1,
				HDCP2_MSG_AKE_SEND_H_PRIME);
			u16TimeoutValue = HDCP2_AKESENDHPRIME_STORED_WDT;
			g_stHdcpHandler.bRecvMsg = false;
			g_u32PreTime = getSystemTime();
			break;

		case HDCP2_MSG_AKE_SEND_H_PRIME:
			if (mtk_dp->info.hdcp2_info.bReadHprime)
				HDCPTx_ReadMsg(mtk_dp,
					HDCP2_MSG_AKE_SEND_H_PRIME);

			if (getTimeDiff(g_u32PreTime) > u16TimeoutValue) {
				enErrCode = HDCP_ERR_RESPONSE_TIMEROUT;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			if (!g_stHdcpHandler.bRecvMsg)
				break;

			if (tee_akeHPrime(rhdcp_tx.rtx,
				rhdcp_rx.rrx,
				rhdcp_rx.rxCaps,
				rhdcp_tx.txCaps,
				rhdcp_rx.HPrime,
				HDCP2_HPRIME_SIZE) != RET_COMPARE_PASS) {
				if (g_stHdcpHandler.bStoredKm)
					tee_clearParing();
				enErrCode = HDCP_ERR_PROCESS_FAIL;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			if (g_stHdcpHandler.bStoredKm)
				HDCPTx_Hdcp2SetState(HDCP2_MS_A2F2,
					HDCP2_MSG_LC_INIT);
			else
				HDCPTx_Hdcp2SetState(HDCP2_MS_A1F1,
					HDCP2_MSG_AKE_SEND_PAIRING_INFO);

			g_u32PreTime = getSystemTime();
			g_stHdcpHandler.bRecvMsg = false;
			break;

		case HDCP2_MSG_AKE_SEND_PAIRING_INFO:
			if (mtk_dp->info.hdcp2_info.bReadPairing
				&& !g_stHdcpHandler.bStoredKm)
				HDCPTx_ReadMsg(mtk_dp,
					HDCP2_MSG_AKE_SEND_PAIRING_INFO);

			if (getTimeDiff(g_u32PreTime) >
				HDCP2_AKESENDPAIRINGINFO_WDT) {
				enErrCode = HDCP_ERR_RESPONSE_TIMEROUT;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			if (!g_stHdcpHandler.bRecvMsg)
				break;

			// ... store m,km,Ekh(km)
			tee_akeParing(rhdcp_rx.Ekh_km);

			g_stHdcpHandler.bSendPair = true;
			g_stHdcpHandler.bRecvMsg = false;
			HDCPTx_Hdcp2SetState(HDCP2_MS_A2F2, HDCP2_MSG_LC_INIT);
			g_u32PreTime = getSystemTime();
			break;

		default:
			enErrCode = HDCP_ERR_UNKNOWN_STATE;
			HDCPTx_ERRHandle(enErrCode, __LINE__);
			break;
		}
		break;

	case HDCP2_MS_A2F2:
		switch (g_stHdcpHandler.u8SubState) {
		case HDCP2_MSG_LC_INIT:
			//prepare Rn to send
			if (!HDCPTx_WriteMsg(mtk_dp, HDCP2_MSG_LC_INIT)) {
				enErrCode = HDCP_ERR_SEND_MSG_FAIL;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}
			g_stHdcpHandler.bSendLCInit = true;

			HDCPTx_Hdcp2SetState(HDCP2_MS_A2F2,
				HDCP2_MSG_LC_SEND_L_PRIME);
			g_u32PreTime = getSystemTime();

			break;

		case HDCP2_MSG_LC_SEND_L_PRIME:
			if (mtk_dp->info.hdcp2_info.bReadLprime)
				HDCPTx_ReadMsg(mtk_dp,
					HDCP2_MSG_LC_SEND_L_PRIME);

			if (getTimeDiff(g_u32PreTime)
					> HDCP2_LCSENDLPRIME_WDT) {
				if (g_stHdcpHandler.u8RetryCnt
					> HDCP2_TX_LC_RETRY_CNT) {
					enErrCode = HDCP_ERR_RESPONSE_TIMEROUT;
					HDCPTx_ERRHandle(enErrCode, __LINE__);
				} else {
					g_stHdcpHandler.u8RetryCnt++;
					HDCPTx_Hdcp2SetState(HDCP2_MS_A2F2,
						HDCP2_MSG_LC_INIT);
				}

				break;
			}

			if (!g_stHdcpHandler.bRecvMsg)
				break;

			if (tee_lcLPrime(rhdcp_tx.rn,
				rhdcp_rx.LPrime,
				HDCP2_LPRIME_SIZE) != RET_COMPARE_PASS) {
				enErrCode = HDCP_ERR_PROCESS_FAIL;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			DPTXMSG("L' is PASS!!\n");
			g_stHdcpHandler.bRecvMsg = false;
			HDCPTx_Hdcp2SetState(HDCP2_MS_A3F3, HDCP2_MSG_ZERO);
			g_u32PreTime = getSystemTime();
			break;

		default:
			enErrCode = HDCP_ERR_UNKNOWN_STATE;
			HDCPTx_ERRHandle(enErrCode, __LINE__);
			break;
		}
		break;

	case HDCP2_MS_A3F3:
		switch (g_stHdcpHandler.u8SubState) {
		case HDCP2_MSG_ZERO:
			tee_skeEncKs(rhdcp_tx.riv, rhdcp_tx.Eks);

			if (!HDCPTx_WriteMsg(mtk_dp, HDCP2_MSG_SKE_SEND_EKS)) {
				enErrCode = HDCP_ERR_SEND_MSG_FAIL;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			HDCPTx_Hdcp2SetState(HDCP2_MS_A3F3,
				HDCP2_MSG_SKE_SEND_EKS);
			g_u32PreTime = getSystemTime();
			break;

		case HDCP2_MSG_SKE_SEND_EKS:
			if (getTimeDiff(g_u32PreTime) >= HDCP2_ENC_EN_TIMER) {
				HDCPTx_Hdcp2SetState(HDCP2_MS_A4F4,
					HDCP2_MSG_ZERO);
				//HDCPTx_Hdcp2EnableAuth(mtk_dp, true);
			}
			break;

		default:
			enErrCode = HDCP_ERR_UNKNOWN_STATE;
			HDCPTx_ERRHandle(enErrCode, __LINE__);
			break;
		}
		break;

	case HDCP2_MS_A4F4:
		switch (g_stHdcpHandler.u8SubState) {
		case HDCP2_MSG_ZERO:
			if (!mtk_dp->info.hdcp2_info.bRepeater)
				HDCPTx_Hdcp2SetState(HDCP2_MS_A5F5,
					HDCP2_MSG_AUTH_DONE);
			else {
				HDCPTx_Hdcp2SetState(HDCP2_MS_A6F6,
					HDCP2_MSG_REPAUTH_SEND_RECVID_LIST);
				g_stHdcpHandler.bRecvMsg = false;
				g_u32PreTime = getSystemTime();
			}

			break;
		default:
			enErrCode = HDCP_ERR_UNKNOWN_STATE;
			HDCPTx_ERRHandle(enErrCode, __LINE__);
			break;
		}
		break;

	case HDCP2_MS_A5F5:
		switch (g_stHdcpHandler.u8SubState) {
		case HDCP2_MSG_ZERO:
			break;
		case HDCP2_MSG_AUTH_DONE:
			DPTXMSG("HDCP2.x Authentication done.\n");
			mtk_dp->info.bAuthStatus = AUTH_PASS;
			mtk_dp->info.hdcp2_info.uRetryCount = 0;
			HDCPTx_Hdcp2SetState(HDCP2_MS_A5F5, HDCP2_MSG_ZERO);
			HDCPTx_Hdcp2EnableAuth(mtk_dp, true);
			break;
		default:
			enErrCode = HDCP_ERR_UNKNOWN_STATE;
			HDCPTx_ERRHandle(enErrCode, __LINE__);
			break;
		}
		break;
	case HDCP2_MS_A6F6:
		switch (g_stHdcpHandler.u8SubState) {
		case HDCP2_MSG_REPAUTH_SEND_RECVID_LIST:
			if (mtk_dp->info.hdcp2_info.bReadVprime)
				HDCPTx_ReadMsg(mtk_dp,
					HDCP2_MSG_REPAUTH_SEND_RECVID_LIST);

			if (getTimeDiff(g_u32PreTime)
				> HDCP2_REPAUTHSENDRECVID_WDT) {
				enErrCode = HDCP_ERR_RESPONSE_TIMEROUT;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			if (!g_stHdcpHandler.bRecvMsg)
				break;

			g_u32PreTime = getSystemTime();
			g_stHdcpHandler.bRecvMsg = false;
			HDCPTx_Hdcp2SetState(HDCP2_MS_A7F7,
				HDCP2_MSG_REPAUTH_VERIFY_RECVID_LIST);

			break;

		default:
			enErrCode = HDCP_ERR_UNKNOWN_STATE;
			HDCPTx_ERRHandle(enErrCode, __LINE__);
			break;
		}
		break;

	case HDCP2_MS_A7F7:
		switch (g_stHdcpHandler.u8SubState) {
		case HDCP2_MSG_REPAUTH_VERIFY_RECVID_LIST:
			if ((rhdcp_rx.rxInfo[1] & (BIT2 | BIT3)) != 0) {
				DPTXERR("DEVS_EXCEEDED or CASCADE_EXCEDDED!\n");
				enErrCode = HDCP_ERR_PROCESS_FAIL;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			//check seqNumV here;
			if (!HDCPTx_Hdcp2CheckSeqNumV(mtk_dp)) {
				enErrCode = HDCP_ERR_PROCESS_FAIL;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			if (!HDCPTx_Hdcp2RecvRepAuthSendRecvIDList(mtk_dp)) {
				enErrCode = HDCP_ERR_PROCESS_FAIL;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			HDCPTx_Hdcp2SetState(HDCP2_MS_A8F8,
				HDCP2_MSG_REPAUTH_SEND_ACK);

			break;

		default:
			enErrCode = HDCP_ERR_UNKNOWN_STATE;
			HDCPTx_ERRHandle(enErrCode, __LINE__);
			break;
		}
		break;

	case HDCP2_MS_A8F8:
		switch (g_stHdcpHandler.u8SubState) {
		case HDCP2_MSG_REPAUTH_SEND_ACK:
			if (!HDCPTx_WriteMsg(mtk_dp,
				HDCP2_MSG_REPAUTH_SEND_ACK)) {
				enErrCode = HDCP_ERR_SEND_MSG_FAIL;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			if (getTimeDiff(g_u32PreTime) > HDCP2_REP_SEND_ACK) {
				enErrCode = HDCP_ERR_RESPONSE_TIMEROUT;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			HDCPTx_Hdcp2SetState(HDCP2_MS_A9F9,
				HDCP2_MSG_REPAUTH_STREAM_MANAGE);
			g_stHdcpHandler.u8RetryCnt = 0;
			break;
		default:
			enErrCode = HDCP_ERR_UNKNOWN_STATE;
			HDCPTx_ERRHandle(enErrCode, __LINE__);
			break;
		}
		break;

	case HDCP2_MS_A9F9:
		switch (g_stHdcpHandler.u8SubState) {
		case HDCP2_MSG_REPAUTH_STREAM_MANAGE:
			if (!HDCPTx_Hdcp2ProcessRepAuthStreamManage(mtk_dp)) {
				enErrCode = HDCP_ERR_PROCESS_FAIL;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			if (!HDCPTx_WriteMsg(mtk_dp,
				HDCP2_MSG_REPAUTH_STREAM_MANAGE)) {
				enErrCode = HDCP_ERR_SEND_MSG_FAIL;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			g_u32PreTime = getSystemTime();
			g_stHdcpHandler.bRecvMsg = false;
			HDCPTx_Hdcp2SetState(HDCP2_MS_A9F9,
					HDCP2_MSG_REPAUTH_STREAM_READY);
			break;
		case HDCP2_MSG_REPAUTH_STREAM_READY:
			if (getTimeDiff(g_u32PreTime)
				> HDCP2_REPAUTHSTREAMRDY_WDT/2)
				HDCPTx_ReadMsg(mtk_dp,
					HDCP2_MSG_REPAUTH_STREAM_READY);
			else
				break;

			if (getTimeDiff(g_u32PreTime)
					> HDCP2_REPAUTHSTREAMRDY_WDT) {
				enErrCode = HDCP_ERR_RESPONSE_TIMEROUT;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			} else if (!g_stHdcpHandler.bRecvMsg) {
				if (g_stHdcpHandler.u8RetryCnt
					>= HDCP2_STREAM_MANAGE_RETRY_CNT) {
					enErrCode = HDCP_ERR_RESPONSE_TIMEROUT;
					HDCPTx_ERRHandle(enErrCode, __LINE__);
					break;
				} else
					g_stHdcpHandler.u8RetryCnt++;

				HDCPTx_Hdcp2SetState(HDCP2_MS_A9F9,
					HDCP2_MSG_REPAUTH_STREAM_READY);
				break;
			}

			if (!HDCPTx_Hdcp2RecvRepAuthStreamReady(mtk_dp)) {
				enErrCode = HDCP_ERR_PROCESS_FAIL;
				HDCPTx_ERRHandle(enErrCode, __LINE__);
				break;
			}

			HDCPTx_Hdcp2SetState(HDCP2_MS_A5F5,
				HDCP2_MSG_AUTH_DONE);
			break;

		default:
			enErrCode = HDCP_ERR_UNKNOWN_STATE;
			HDCPTx_ERRHandle(enErrCode, __LINE__);
			break;
		}
		break;
	default:
		enErrCode = HDCP_ERR_UNKNOWN_STATE;
		HDCPTx_ERRHandle(enErrCode, __LINE__);
		break;
	}

	return enErrCode;
}

void mdrv_DPTx_HDCP2_SetStartAuth(struct mtk_dp *mtk_dp, bool bEnable)
{
	mtk_dp->info.hdcp2_info.bEnable = bEnable;
	if (bEnable) {
		mtk_dp->info.bAuthStatus = AUTH_INIT;
		HDCPTx_Hdcp2SetState(HDCP2_MS_A0F0, HDCP2_MSG_ZERO);
	} else {
		mtk_dp->info.bAuthStatus = AUTH_ZERO;
		HDCPTx_Hdcp2SetState(HDCP2_MS_H1P1, HDCP2_MSG_ZERO);
		HDCPTx_Hdcp2EnableAuth(mtk_dp, false);
	}

	mtk_dp->info.hdcp2_info.uRetryCount = 0;
}

bool mdrv_DPTx_HDCP2_Support(struct mtk_dp *mtk_dp)
{
	uint8_t bTempBuffer[3];

	if (mtk_dp->info.bForceHDCP1x) {
		DPTXMSG("Force HDCP1x, not support HDCP2x");
		return false;
	}

	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_6921D, bTempBuffer, 0x3);

	if ((bTempBuffer[2] & BIT1) && (bTempBuffer[0] == 0x02)) {
		mtk_dp->info.hdcp2_info.bEnable = true;
		mtk_dp->info.hdcp2_info.bRepeater = bTempBuffer[2] & BIT0;
	} else
		mtk_dp->info.hdcp2_info.bEnable = false;

	DPTXMSG("HDCP.2x CAPABLE: %d, Reapeater: %d\n",
		mtk_dp->info.hdcp2_info.bEnable,
		mtk_dp->info.hdcp2_info.bRepeater);

	if (!mtk_dp->info.hdcp2_info.bEnable)
		return false;

	if (tee_addDevice(HDCP_VERSION_2X) != RET_SUCCESS) {
		DPTXERR("HDCP TA has some error\n");
		mtk_dp->info.hdcp2_info.bEnable = false;
	}

	return mtk_dp->info.hdcp2_info.bEnable;
}

bool mdrv_DPTx_HDCP2_irq(struct mtk_dp *mtk_dp)
{
	BYTE RxStatus = 0;
	BYTE ClearCpirq = BIT2;

	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_69493, &RxStatus,
		HDCP2_RXSTATUS_SIZE);

	DPTXMSG("HDCP2x RxStatus = 0x%x\n", RxStatus);
	if (RxStatus & BIT0) {
		DPTXMSG("READY_BIT0 Ready!\n");
		mtk_dp->info.hdcp2_info.bReadVprime = true;
	}

	if (RxStatus & BIT1) {
		DPTXMSG("H'_AVAILABLE Ready!\n");
		mtk_dp->info.hdcp2_info.bReadHprime = true;
	}

	if (RxStatus & BIT2) {
		DPTXMSG("PAIRING_AVAILABLE Ready!\n");
		mtk_dp->info.hdcp2_info.bReadPairing = true;
	}

	if (RxStatus & BIT4 || RxStatus & BIT3) {
		DPTXMSG("Re-Auth HDCP2X!\n");
		mdrv_DPTx_HDCP2_SetStartAuth(mtk_dp, true);
		mdrv_DPTx_reAuthentication(mtk_dp);
	}

	drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00201, &ClearCpirq, 0x1);

	return true;
}

#endif

