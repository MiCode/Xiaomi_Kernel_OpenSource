/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "mtk_dp_hdcp1x.h"
#include "mtk_dp_hal.h"
#include "mtk_dp_common.h"
#include "mtk_dp_reg.h"
#include "mtk_dp.h"
#include "ca/tlcDpHdcp.h"

#ifdef DPTX_HDCP_ENABLE

//Please replace AKSV and Key0~Key39 with your product key.
unsigned char DPTX_HDCP1X_KEY_REAL[289] = {
	//Need add AKSV(5Bytes) (The format can refer to P77 of HDCP SPEC1_1)
		//For Example
		//0x14, 0xf7, 0x61, 0x03, 0xb7,
	//dummy(3Bytes)
	0x00, 0x00, 0x00,
	//Key0 ~ Key39 (The format can refer to P77 of HDCP SPEC1_1)
		//For Example
		//0x69, 0x1e, 0x13, 0x8f, 0x58, 0xa4, 0x4d,	// Key 0
		//...
		//0x41, 0x20, 0x56, 0xb4, 0xbb, 0x73, 0x25,	// Key 39
	//Ln_seed(1Byte)
	0x00
};

static DWORD gdwPreTime;

void mhal_DPTx_HDCP1X_StartCipher(struct mtk_dp *mtk_dp, bool bEnable)
{
	if (bEnable) {
		msWriteByteMask(mtk_dp, REG_3480_DP_TRANS_P0 + 1, BIT4, BIT4);
		msWriteByteMask(mtk_dp, REG_3480_DP_TRANS_P0, BIT4, BIT4);
	} else {
		msWriteByteMask(mtk_dp, REG_3480_DP_TRANS_P0, 0, BIT4);
		msWriteByteMask(mtk_dp, REG_3480_DP_TRANS_P0 + 1, 0, BIT4);
	}
}

bool mhal_DPTx_HDCP1X_GetTxR0Available(struct mtk_dp *mtk_dp)
{
	bool R0Available;

	if (msReadByte(mtk_dp, REG_34A4_DP_TRANS_P0) & BIT12)
		R0Available = true;
	else
		R0Available = false;

	return R0Available;
}

void mhal_DPTx_HDCP1X_SetTxRepeater(struct mtk_dp *mtk_dp, bool bEnable)
{
	if (bEnable)
		msWriteByteMask(mtk_dp, REG_34A4_DP_TRANS_P0 + 1, BIT7, BIT7);
	else
		msWriteByteMask(mtk_dp, REG_34A4_DP_TRANS_P0 + 1, 0,  BIT7);

#if 0
	if (mtk_dp->info.hdcp1x_info.bRepeater) {
		unsigned char temp;

		temp = BIT0;//REAUTHENTICATION_ENABLE_IRQ_HPD
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_6803B, &temp, 1);
	}
#endif
}

void mdrv_DPTx_HDCP1X_SetStartAuth(struct mtk_dp *mtk_dp, bool bEnable)
{
	mtk_dp->info.hdcp1x_info.bEnable = bEnable;
	if (bEnable) {
		mtk_dp->info.bAuthStatus = AUTH_INIT;
		mtk_dp->info.hdcp1x_info.MainStates = HDCP1X_MainState_A0;
		mtk_dp->info.hdcp1x_info.SubStates = HDCP1X_SubFSM_IDLE;
	} else {
		mtk_dp->info.bAuthStatus = AUTH_ZERO;
		mtk_dp->info.hdcp1x_info.MainStates = HDCP1X_MainState_H2;
		mtk_dp->info.hdcp1x_info.SubStates = HDCP1X_SubFSM_IDLE;
		tee_hdcp_enableEncrypt(false, HDCP_NONE);
		mhal_DPTx_HDCP1X_StartCipher(mtk_dp, false);
		tee_hdcp1x_softRst();
	}

	mtk_dp->info.hdcp1x_info.uRetryCount = 0;
}

bool mdrv_DPTx_HDCP1x_Support(struct mtk_dp *mtk_dp)
{
	uint8_t bTempBuffer[2];

	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_68028, bTempBuffer, 0x1);

	mtk_dp->info.hdcp1x_info.bEnable = bTempBuffer[0x0] & BIT0;
	mtk_dp->info.hdcp1x_info.bRepeater = (bTempBuffer[0x0] & BIT1) >> 1;

	DPTXMSG("HDCP1.3 CAPABLE: %d, Reapeater: %d\n",
		mtk_dp->info.hdcp1x_info.bEnable,
		mtk_dp->info.hdcp1x_info.bRepeater);

	if (!mtk_dp->info.hdcp1x_info.bEnable)
		return false;

	if (tee_addDevice(HDCP_VERSION_1X) != RET_SUCCESS) {
		DPTXERR("HDCP TA has some error\n");
		mtk_dp->info.hdcp1x_info.bEnable = false;
	}

	return mtk_dp->info.hdcp1x_info.bEnable;
}

bool mdrv_DPTx_HDCP1X_irq(struct mtk_dp *mtk_dp)
{
	BYTE RxStatus = 0;
	BYTE ClearCpirq = BIT2;

	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_68029, &RxStatus,
			DP_HDCP1_BSTATUS_SIZE);
	DPTXDBG("CP_IRQ Bstatus:0x%x\n", RxStatus);
	if (RxStatus & BIT1) {
		DPTXMSG("R0'_AVAILABLE Ready!\n");
		mtk_dp->info.hdcp1x_info.bR0Read = true;
	}

	if (RxStatus & BIT0) {
		DPTXMSG("KSV_READY Ready!\n");
		mtk_dp->info.hdcp1x_info.bKSV_READY = true;
	}

	if (RxStatus & BIT2 || RxStatus & BIT3) {
		DPTXMSG("Re-Auth HDCP1X!\n");
		mdrv_DPTx_HDCP1X_SetStartAuth(mtk_dp, true);
		mdrv_DPTx_reAuthentication(mtk_dp);
	}

	drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00201, &ClearCpirq, 0x1);

	return true;
}

bool mdrv_DPTx_HDCP1X_Init(struct mtk_dp *mtk_dp)
{
	unsigned char i;

	mtk_dp->info.hdcp1x_info.bKSV_READY = false;
	mtk_dp->info.hdcp1x_info.bR0Read = false;
	mtk_dp->info.hdcp1x_info.ubBstatus = 0x00;
	for (i = 0; i < 5; i++) {
		mtk_dp->info.hdcp1x_info.ubBksv[i] = 0x00;
		mtk_dp->info.hdcp1x_info.ubAksv[i] = 0x00;
	}

	for (i = 0; i < 5; i++)
		mtk_dp->info.hdcp1x_info.ubV[i] = 0x00;

	mtk_dp->info.hdcp1x_info.ubBinfo[0] = 0x00;
	mtk_dp->info.hdcp1x_info.ubBinfo[1] = 0x00;
	mtk_dp->info.hdcp1x_info.bMAX_CASCADE = false;
	mtk_dp->info.hdcp1x_info.bMAX_DEVS = false;
	mtk_dp->info.hdcp1x_info.ubDEVICE_COUNT = 0x00;

	tee_hdcp_enableEncrypt(false, HDCP_NONE);
	mhal_DPTx_HDCP1X_StartCipher(mtk_dp, false);
	tee_hdcp1x_softRst();

	return true;
}

bool mdrv_DPTx_HDCP1X_ReadSinkBksv(struct mtk_dp *mtk_dp)
{
	unsigned char pReadBuffer[5], i;

	if (mtk_dp->info.hdcp1x_info.bEnable) {
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_68000, pReadBuffer, 5);

		for (i = 0; i < 5; i++) {
			mtk_dp->info.hdcp1x_info.ubBksv[i]
				= pReadBuffer[i];
			DPTXDBG("Bksv = 0x%x\n", pReadBuffer[i]);
		}
	}

	return true;
}

bool mdrv_DPTx_HDCP1X_CheckSinkKSVReady(struct mtk_dp *mtk_dp)
{
	unsigned char pReadBuffer;

	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_68029, &pReadBuffer, 1);

	mtk_dp->info.hdcp1x_info.bKSV_READY
		= (pReadBuffer & BIT0)  ? true : false;

	return mtk_dp->info.hdcp1x_info.bKSV_READY;
}

bool mdrv_DPTx_HDCP1X_CheckSinkCap(struct mtk_dp *mtk_dp)
{
	unsigned char  pReadBuffer[0x2];

	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_68028, pReadBuffer, 1);

	mtk_dp->info.hdcp1x_info.bRepeater
		= (pReadBuffer[0] & BIT1) ? true : false;

	return true;
}


bool mdrv_DPTx_HDCP1X_ReadSinkBinfo(struct mtk_dp *mtk_dp)
{
	unsigned char pReadBuffer[2];

	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_6802A, pReadBuffer, 2);

	mtk_dp->info.hdcp1x_info.ubBinfo[0] = pReadBuffer[0];
	mtk_dp->info.hdcp1x_info.ubBinfo[1] = pReadBuffer[1];
	mtk_dp->info.hdcp1x_info.bMAX_CASCADE
		= (pReadBuffer[1] & BIT3) ? true:false;
	mtk_dp->info.hdcp1x_info.bMAX_DEVS
		= (pReadBuffer[0] & BIT7) ? true:false;
	mtk_dp->info.hdcp1x_info.ubDEVICE_COUNT = pReadBuffer[0] & 0x7F;

	DPTXMSG("HDCP Binfo MAX_CASCADE_EXCEEDED = %d\n",
		mtk_dp->info.hdcp1x_info.bMAX_CASCADE);
	DPTXMSG("HDCP Binfo DEPTH = %d\n", pReadBuffer[1] & 0x07);
	DPTXMSG("HDCP Binfo MAX_DEVS_EXCEEDED = %d\n",
		mtk_dp->info.hdcp1x_info.bMAX_DEVS);
	DPTXMSG("HDCP Binfo DEVICE_COUNT = %d\n",
		mtk_dp->info.hdcp1x_info.ubDEVICE_COUNT);
	return true;
}

bool mdrv_DPTx_HDCP1X_ReadSinkKSV(struct mtk_dp *mtk_dp,
	unsigned char ucDevCount)
{
	unsigned char i;
	unsigned char times = ucDevCount / 3;
	unsigned char remain = ucDevCount % 3;

	if (times > 0) {
		for (i = 0; i < times; i++)
			drm_dp_dpcd_read(&mtk_dp->aux, DPCD_6802C,
				mtk_dp->info.hdcp1x_info.ubKSVFIFO + i*15,
				15);
	}

	if (remain > 0)
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_6802C,
			mtk_dp->info.hdcp1x_info.ubKSVFIFO + times*15,
			remain * 5);

	return true;
}

bool mdrv_DPTx_HDCP1X_ReadSinkSHA_V(struct mtk_dp *mtk_dp)
{
	unsigned char pReadBuffer[4], i, j;

	for (i = 0; i < 5; i++) {
		drm_dp_dpcd_read(&mtk_dp->aux,
			DPCD_68014 + (i * 4), pReadBuffer, 4);
		for (j = 0; j < 4; j++) {
			mtk_dp->info.hdcp1x_info.ubV[(i * 4) + j]
				= pReadBuffer[3 - j];
			DPTXDBG("HDCP Read V = %x\n",
				mtk_dp->info.hdcp1x_info.ubV[(i * 4) + j]);
		}
	}

	return true;
}

bool mdrv_DPTx_HDCP1X_AuthWithRepeater(struct mtk_dp *mtk_dp)
{
	bool ret = false;
	uint8_t *buffer = NULL;
	uint32_t len = 0;

	if (mtk_dp->info.hdcp1x_info.ubDEVICE_COUNT > HDCP1X_REP_MAXDEVS) {
		DPTXERR("HDCP Repeater: %d DEVs!\n",
			mtk_dp->info.hdcp1x_info.ubDEVICE_COUNT);
		return false;
	}

	mdrv_DPTx_HDCP1X_ReadSinkKSV(mtk_dp,
		mtk_dp->info.hdcp1x_info.ubDEVICE_COUNT);
	mdrv_DPTx_HDCP1X_ReadSinkSHA_V(mtk_dp);

	len = mtk_dp->info.hdcp1x_info.ubDEVICE_COUNT * 5 + 2;
	buffer = kmalloc(len, GFP_KERNEL);
	if (!buffer) {
		DPTXERR("Out of Memory\n");
		return false;
	}

	memcpy(buffer, mtk_dp->info.hdcp1x_info.ubKSVFIFO, len - 2);
	memcpy(buffer + (len - 2), mtk_dp->info.hdcp1x_info.ubBinfo, 2);
	if (tee_hdcp1x_ComputeCompareV(buffer,
		len,
		mtk_dp->info.hdcp1x_info.ubV) == RET_COMPARE_PASS) {
		DPTXMSG("Check V' PASS\n");
		ret = true;
	} else
		DPTXMSG("Check V' Fail\n");

	kfree(buffer);
	return ret;
}

bool mdrv_DPTx_HDCP1X_VerifyBksv(struct mtk_dp *mtk_dp)
{
	int i, j, k = 0;
	unsigned char ksv;

	for (i = 0; i < 5; i++) {
		ksv = mtk_dp->info.hdcp1x_info.ubBksv[i];
		for (j = 0; j < 8; j++)
			k += (ksv >> j) & 0x01;
	}

	if (k != 20) {
		DPTXERR("Check BKSV 20'1' 20'0' Fail\n");
		return false;
	}

	return true;
}

bool mdrv_DPTx_HDCP1X_WriteAksv(struct mtk_dp *mtk_dp)
{
	unsigned char temp;
	int i, k, j;

	tee_getAksv(mtk_dp->info.hdcp1x_info.ubAksv);
	drm_dp_dpcd_write(&mtk_dp->aux, DPCD_68007,
		mtk_dp->info.hdcp1x_info.ubAksv, 5);

	for (i = 0, k = 0; i < 5; i++) {
		temp = mtk_dp->info.hdcp1x_info.ubAksv[i];

		for (j = 0; j < 8; j++)
			k += (temp >> j) & 0x01;

		DPTXDBG("Aksv 0x%x\n", temp);
	}

	if (k != 20) {
		DPTXERR("Check AKSV 20'1' 20'0' Fail\n");
		return false;
	}

	return true;
}

void mdrv_DPTx_HDCP1X_WriteAn(struct mtk_dp *mtk_dp)
{
	unsigned char ubAnValue[0x8] = {//on DP Spec p99
		0x03, 0x04, 0x07, 0x0C, 0x13, 0x1C, 0x27, 0x34};

	tee_hdcp1x_setTxAn(ubAnValue);
	drm_dp_dpcd_write(&mtk_dp->aux, DPCD_6800C, ubAnValue, 8);
	mdelay(5);
}

bool mdrv_DPTx_HDCP1X_CheckR0(struct mtk_dp *mtk_dp)
{
	unsigned char ubTempValue[2];
	unsigned char uuRetryCount = 0;
	bool bSinkR0Available = false;

	if (!mhal_DPTx_HDCP1X_GetTxR0Available(mtk_dp)) {
		DPTXERR("HDCP ERR: R0 No Available\n");
		return false;
	}

	if (!mtk_dp->info.hdcp1x_info.bR0Read) {
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_68029, ubTempValue, 1);
		bSinkR0Available
			= ((ubTempValue[0x0] & BIT1) == BIT1) ? true : false;

		if (!bSinkR0Available) {
			drm_dp_dpcd_read(&mtk_dp->aux,
				DPCD_68029, ubTempValue, 1);
			bSinkR0Available
				= ((ubTempValue[0x0] & BIT1) == BIT1)
					? true : false;

			if (!bSinkR0Available)
				return false;
		}
	}

	while (uuRetryCount < 3) {
		drm_dp_dpcd_read(&mtk_dp->aux,
			DPCD_68005, ubTempValue, 2);

		if (tee_compareR0(ubTempValue, 2) == RET_COMPARE_PASS) {
			DPTXMSG("R0 check PASS:Rx_R0=0x%x%x\n",
				ubTempValue[0x1], ubTempValue[0x0]);
			return true;
		}

		DPTXMSG("R0 check FAIL:Rx_R0=0x%x%x\n",
			ubTempValue[0x1], ubTempValue[0x0]);
		mdelay(5);

		uuRetryCount++;
	}

	return false;
}

void mdrv_DPTx_HDCP1X_StateRst(struct mtk_dp *mtk_dp)
{
	DPTXMSG("Before State Reset:(M : S)= (%d, %d)",
		mtk_dp->info.hdcp1x_info.MainStates,
		mtk_dp->info.hdcp1x_info.SubStates);
	mtk_dp->info.hdcp1x_info.MainStates = HDCP1X_MainState_A0;
	mtk_dp->info.hdcp1x_info.SubStates = HDCP1X_SubFSM_IDLE;
}

void mdrv_DPTx_HDCP1X_FSM(struct mtk_dp *mtk_dp)
{
	static int u8PreMain, u8PreSub;

	if ((u8PreMain != mtk_dp->info.hdcp1x_info.MainStates)
		|| (mtk_dp->info.hdcp1x_info.SubStates != u8PreSub)) {

		DPTXMSG("HDCP1.x State(M : S)= (%d, %d)",
			mtk_dp->info.hdcp1x_info.MainStates,
			mtk_dp->info.hdcp1x_info.SubStates);
		u8PreMain = mtk_dp->info.hdcp1x_info.MainStates;
		u8PreSub = mtk_dp->info.hdcp1x_info.SubStates;
	}

	switch (mtk_dp->info.hdcp1x_info.MainStates) {
	case HDCP1X_MainState_H2:
		switch (mtk_dp->info.hdcp1x_info.SubStates) {
		case HDCP1X_SubFSM_IDLE:
			break;
		case HDCP1X_SubFSM_AuthFail:
			tee_hdcp_enableEncrypt(false, HDCP_NONE);
			DPTXMSG("HDCP1.3 Authentication Fail\n");
			mtk_dp->info.bAuthStatus = AUTH_FAIL;
			mtk_dp->info.hdcp1x_info.MainStates
					= HDCP1X_MainState_H2;
			mtk_dp->info.hdcp1x_info.SubStates
					= HDCP1X_SubFSM_IDLE;
			break;

		default:
			DPTXERR("HDCP A0 Sub Invalid State\n");
			break;
		}

		break;
	case HDCP1X_MainState_A0:
		switch (mtk_dp->info.hdcp1x_info.SubStates) {
		case HDCP1X_SubFSM_IDLE:
			if (mtk_dp->info.hdcp1x_info.uRetryCount
				> HDCP1X_REAUNTH_COUNT) {
				DPTXMSG("Too much retry!\n");
				mtk_dp->info.hdcp1x_info.MainStates
					= HDCP1X_MainState_H2;
				mtk_dp->info.hdcp1x_info.SubStates
					= HDCP1X_SubFSM_AuthFail;
			} else {
				mdrv_DPTx_HDCP1X_Init(mtk_dp);
				mtk_dp->info.hdcp1x_info.MainStates
					= HDCP1X_MainState_A0;
				mtk_dp->info.hdcp1x_info.SubStates
					= HDCP1X_SubFSM_CHECKHDCPCAPABLE;
			}

			break;
		case HDCP1X_SubFSM_CHECKHDCPCAPABLE:
			if (mtk_dp->info.hdcp1x_info.bEnable) {
				tee_hdcp1x_setKey(DPTX_HDCP1X_KEY_REAL);
				mtk_dp->info.hdcp1x_info.uRetryCount++;
				mtk_dp->info.hdcp1x_info.MainStates
					= HDCP1X_MainState_A1;
				mtk_dp->info.hdcp1x_info.SubStates
					= HDCP1X_SubFSM_ExchangeKSV;
			} else
				mdrv_DPTx_HDCP1X_StateRst(mtk_dp);
			break;

		default:
			DPTXERR("HDCP A0 Sub Invalid State\n");
			break;
		}
		break;
	case HDCP1X_MainState_A1:
		switch (mtk_dp->info.hdcp1x_info.SubStates) {
		case HDCP1X_SubFSM_ExchangeKSV:
			mdrv_DPTx_HDCP1X_WriteAn(mtk_dp);
			if (mdrv_DPTx_HDCP1X_WriteAksv(mtk_dp)) {
				gdwPreTime = getSystemTime();
				mtk_dp->info.hdcp1x_info.MainStates
					= HDCP1X_MainState_A1;
				mtk_dp->info.hdcp1x_info.SubStates
					= HDCP1X_SubFSM_VerifyBksv;
			} else
				mdrv_DPTx_HDCP1X_StateRst(mtk_dp);
			break;

		case HDCP1X_SubFSM_VerifyBksv:
			mdrv_DPTx_HDCP1X_ReadSinkBksv(mtk_dp);
			mhal_DPTx_HDCP1X_SetTxRepeater(mtk_dp,
				mtk_dp->info.hdcp1x_info.bRepeater);

			if (getTimeDiff(gdwPreTime)
				< HDCP1X_BSTATUS_TIMEOUT_CNT) {
				gdwPreTime = getSystemTime();
				if (mdrv_DPTx_HDCP1X_VerifyBksv(mtk_dp)) {
					mtk_dp->info.hdcp1x_info.MainStates
						= HDCP1X_MainState_A2;
					mtk_dp->info.hdcp1x_info.SubStates
						= HDCP1X_SubFSM_Computation;
				} else {
					mdrv_DPTx_HDCP1X_StateRst(mtk_dp);
					DPTXMSG("Invalid BKSV!!\n");
				}
			} else
				mdrv_DPTx_HDCP1X_StateRst(mtk_dp);
			break;

		default:
			DPTXERR("HDCP A1 Sub Invalid State\n");
			break;
		}
		break;

	case HDCP1X_MainState_A2:
		switch (mtk_dp->info.hdcp1x_info.SubStates) {
		case HDCP1X_SubFSM_Computation:
			tee_calculateLm(mtk_dp->info.hdcp1x_info.ubBksv);
			mhal_DPTx_HDCP1X_StartCipher(mtk_dp, true);
			mtk_dp->info.hdcp1x_info.MainStates
				= HDCP1X_MainState_A3;
			mtk_dp->info.hdcp1x_info.SubStates
				= HDCP1X_SubFSM_CheckR0;
			gdwPreTime = getSystemTime();
			break;

		default:
			DPTXERR("HDCP A2 Sub Invalid State\n");
			break;
		}
		break;

	case HDCP1X_MainState_A3:
		switch (mtk_dp->info.hdcp1x_info.SubStates) {
		case HDCP1X_SubFSM_CheckR0:
			//wait 100ms(at least) before check R0
			if (getTimeDiff(gdwPreTime) < HDCP1X_R0_WDT
				&& !mtk_dp->info.hdcp1x_info.bR0Read) {
				mdelay(10);
				break;
			}

			if (mdrv_DPTx_HDCP1X_CheckR0(mtk_dp)) {
				tee_hdcp_enableEncrypt(true, HDCP_V1);
				mtk_dp->info.hdcp1x_info.MainStates
					= HDCP1X_MainState_A5;
				mtk_dp->info.hdcp1x_info.SubStates
					= HDCP1X_SubFSM_IDLE;
			} else
				mdrv_DPTx_HDCP1X_StateRst(mtk_dp);

			break;

		default:
			DPTXERR("HDCP A3 Sub Invalid State!\n");
			break;
		}
		break;

	case HDCP1X_MainState_A4:
		switch (mtk_dp->info.hdcp1x_info.SubStates) {
		case HDCP1X_SubFSM_IDLE:
			break;
		case HDCP1X_SubFSM_AuthDone:
			DPTXMSG("HDCP1X: Authentication done!\n");
			mtk_dp->info.hdcp1x_info.uRetryCount = 0;
			mtk_dp->info.bAuthStatus = AUTH_PASS;
			mtk_dp->info.hdcp1x_info.MainStates
						= HDCP1X_MainState_A4;
			mtk_dp->info.hdcp1x_info.SubStates
						= HDCP1X_SubFSM_IDLE;
			break;

		default:
			break;
		}
		break;

	case HDCP1X_MainState_A5:
		switch (mtk_dp->info.hdcp1x_info.SubStates) {
		case HDCP1X_SubFSM_IDLE:
			mdrv_DPTx_HDCP1X_CheckSinkCap(mtk_dp);
			if (mtk_dp->info.hdcp1x_info.bRepeater) {
				DPTXMSG("HDCP1X:Repeater!\n");
				gdwPreTime = getSystemTime();
				mtk_dp->info.hdcp1x_info.MainStates
					= HDCP1X_MainState_A6;
				mtk_dp->info.hdcp1x_info.SubStates
					= HDCP1X_SubFSM_PollingRdyBit;
			} else {
				DPTXMSG("HDCP1X:No Repeater!\n");
				mtk_dp->info.hdcp1x_info.MainStates
						= HDCP1X_MainState_A4;
				mtk_dp->info.hdcp1x_info.SubStates
						= HDCP1X_SubFSM_AuthDone;
			}
			break;

		default:
			DPTXERR("Invalid State!\n");
			break;
		}
		break;

	case HDCP1X_MainState_A6:
		switch (mtk_dp->info.hdcp1x_info.SubStates) {
		case HDCP1X_SubFSM_PollingRdyBit:
			if (getTimeDiff(gdwPreTime) > HDCP1X_REP_RDY_WDT) {
				mdrv_DPTx_HDCP1X_StateRst(mtk_dp);
				break;
			}

			if (!mtk_dp->info.hdcp1x_info.bKSV_READY
				&& getTimeDiff(gdwPreTime)
					> HDCP1X_REP_RDY_WDT / 2)
				mdrv_DPTx_HDCP1X_CheckSinkKSVReady(mtk_dp);

			if (mtk_dp->info.hdcp1x_info.bKSV_READY) {
				mdrv_DPTx_HDCP1X_ReadSinkBinfo(mtk_dp);
				mtk_dp->info.hdcp1x_info.MainStates
					= HDCP1X_MainState_A7;
				mtk_dp->info.hdcp1x_info.SubStates
					= HDCP1X_SubFSM_AuthWithRepeater;
				mtk_dp->info.hdcp1x_info.bKSV_READY = false;
			}
			break;
		default:
			DPTXERR("Invalid State!\n");
			break;
		}
		break;

	case HDCP1X_MainState_A7:
		switch (mtk_dp->info.hdcp1x_info.SubStates) {
		case HDCP1X_SubFSM_AuthWithRepeater:
			if (mtk_dp->info.hdcp1x_info.bMAX_CASCADE ||
				mtk_dp->info.hdcp1x_info.bMAX_DEVS){
				DPTXERR("MAX CASCADE or MAX DEVS!\n");
				mdrv_DPTx_HDCP1X_StateRst(mtk_dp);
				break;
			}

			if (mdrv_DPTx_HDCP1X_AuthWithRepeater(mtk_dp)) {
				mtk_dp->info.hdcp1x_info.MainStates
						= HDCP1X_MainState_A4;
				mtk_dp->info.hdcp1x_info.SubStates
						= HDCP1X_SubFSM_AuthDone;
			} else
				mdrv_DPTx_HDCP1X_StateRst(mtk_dp);

			break;

		default:
			DPTXERR("Invalid State!\n");
			break;
		}
		break;

	default:
		break;
	}
}

#endif

