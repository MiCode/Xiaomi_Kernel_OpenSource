/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT

#include "hdmictrl.h"
#include "hdmihdcp.h"
#include "hdmi_ctrl.h"
#include "hdmiddc.h"
#include "hdcpbin.h"
#include <linux/types.h>

#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
#include "hdmi_ca.h"
#endif
struct HDMI_HDCP_BKSV_INFO hdmi_hdcp_info;

#define REPEAT_CHECK_AUTHHDCP_VALUE 25
#define MARK_LAST_AUTH_FAIL 0xf0
#define MARK_FIRST_AUTH_FAIL 0xf1
/* no encrypt key */
const unsigned char HDCP_NOENCRYPT_KEY[HDCP_KEY_RESERVE] = {
	0
};

/* encrypt key */
const unsigned char HDCP_ENCRYPT_KEY[HDCP_KEY_RESERVE] = {
	0
};

static unsigned char bHdcpKeyExternalBuff[HDCP_KEY_RESERVE] = {
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	    0xaa,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	    0xaa,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	    0xaa,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	    0xaa,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	    0xaa,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	    0xaa,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	    0xaa,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	    0xaa,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	    0xaa,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	    0xaa,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	    0xaa,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	    0xaa,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	    0xaa,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	    0xaa,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	    0xaa,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	    0xaa,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	    0xaa,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa
};

#ifdef CONFIG_MTK_HDMI_HDCP_SUPPORT
unsigned char _bHdcpOff;
#else
unsigned char _bHdcpOff = 1;
#endif
static unsigned int i4HdmiShareInfo[MAX_HDMI_SHAREINFO];
static unsigned char HDMI_AKSV[HDCP_AKSV_COUNT] = { 0 };
static unsigned char bKsv_buff[KSV_BUFF_SIZE] = { 0 };
static unsigned char bHdcpKeyBuff[HDCP_KEY_RESERVE];
static unsigned char _fgRepeater = FALSE;
static unsigned char _bReCompRiCount;
static unsigned char _bReCheckReadyBit;
static unsigned char bSHABuff[20];
static enum HDMI_HDCP_KEY_T bhdcpkey = EXTERNAL_KEY;
unsigned char _bflagvideomute = FALSE;
unsigned char _bflagaudiomute = FALSE;
unsigned char _bsvpvideomute = FALSE;
unsigned char _bsvpaudiomute = FALSE;
static unsigned char _bReAuthCnt;
static unsigned char _bReRepeaterPollCnt;
static unsigned char _bReRepeaterDoneCnt;

#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
static unsigned char u1CaHdcpAKsv[HDCP_AKSV_COUNT];
#endif
void vShowHdcpRawData(void)
{
/*
	unsigned short bTemp, i, j, k;

	HDMI_HDCP_FUNC();

	pr_err("==============================hdcpkey==============================\n");
	pr_err("   | 00  01  02  03  04  05  06  07  08  09  0a  0b  0c  0d  0e  0f\n");
	pr_err("===================================================================\n");
	for (bTemp = 0; bTemp < 3; bTemp++) {
		j = bTemp * 128;
		for (i = 0; i < 8; i++) {
			if (((i * 16) + j) < 0x10)
				pr_err("0%x:  ", (i * 16) + j);
			else
				pr_err("%x:  ", (i * 16) + j);

			for (k = 0; k < 16; k++) {
				if (k == 15) {
					if ((j + (i * 16 + k)) < 287) {
						if (bHdcpKeyExternalBuff[j + (i * 16 + k)] > 0x0f)
							pr_err("%2x\n", bHdcpKeyExternalBuff[j + (i * 16 + k)]);
						else
							pr_err("0%x\n", bHdcpKeyExternalBuff[j +	(i * 16 + k)]);
					}
				} else {
					if ((j + (i * 16 + k)) < 287) {
						if (bHdcpKeyExternalBuff[j + (i * 16 + k)] > 0x0f)
							pr_err("%2x  ", bHdcpKeyExternalBuff[j + (i * 16 + k)]);
						else
							pr_err("0%x  ",	bHdcpKeyExternalBuff[j + (i * 16 + k)]);
					} else {
						pr_err("\n");
						pr_err("==================================================\n");
						return;
					}
				}
			}
		}
	}
*/
}

void hdmi_hdcpkey(unsigned char *pbhdcpkey)
{
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
#ifdef CONFIG_MTK_DRM_KEY_MNG_SUPPORT
	HDMI_HDCP_FUNC();
	fgCaHDMIInstallHdcpKey(pbhdcpkey, 384);
	fgCaHDMIGetAKsv(u1CaHdcpAKsv);
#else
	pr_err("[HDMI]can not get hdcp key by this case\n");
#endif
#else
	unsigned short i;

	HDMI_HDCP_FUNC();

	for (i = 0; i < 287; i++)
		bHdcpKeyExternalBuff[i] = *pbhdcpkey++;

	vMoveHDCPInternalKey(EXTERNAL_KEY);
#endif
}

void vMoveHDCPInternalKey(enum HDMI_HDCP_KEY_T key)
{
	unsigned char *pbDramAddr;
	unsigned short i;

	HDMI_HDCP_FUNC();

	bhdcpkey = key;

	pbDramAddr = bHdcpKeyBuff;
	for (i = 0; i < 287; i++) {
		if (key == INTERNAL_ENCRYPT_KEY)
			pbDramAddr[i] = HDCP_ENCRYPT_KEY[i];
		else if (key == INTERNAL_NOENCRYPT_KEY)
			pbDramAddr[i] = HDCP_NOENCRYPT_KEY[i];
		else if (key == EXTERNAL_KEY)
			pbDramAddr[i] = bHdcpKeyExternalBuff[i];
	}
}

void vInitHdcpKeyGetMethod(unsigned char bMethod)
{
	HDMI_HDCP_FUNC();
	if (bMethod == NON_HOST_ACCESS_FROM_EEPROM) {
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, (I2CM_ON | EXT_E2PROM_ON),
				 (I2CM_ON | EXT_E2PROM_ON));
	} else if (bMethod == NON_HOST_ACCESS_FROM_MCM) {
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, (I2CM_ON | MCM_E2PROM_ON),
				 (I2CM_ON | MCM_E2PROM_ON));
	} else if (bMethod == NON_HOST_ACCESS_FROM_GCPU) {
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, AES_EFUSE_ENABLE,
				 (AES_EFUSE_ENABLE | I2CM_ON | EXT_E2PROM_ON | MCM_E2PROM_ON));
	}
}

unsigned char fgHostKey(void)
{
	unsigned char bTemp;

	HDMI_HDCP_FUNC();
	bTemp = bReadByteHdmiGRL(GRL_HDCP_CTL);
	vWriteByteHdmiGRL(GRL_HDCP_CTL, bTemp | HDCP_CTL_HOST_KEY);
	return TRUE;
}

unsigned char bReadHdmiIntMask(void)
{
	unsigned char bMask;

	HDMI_HDCP_FUNC();
	bMask = bReadByteHdmiGRL(GRL_INT_MASK);
	return bMask;

}

void vHalHDCPReset(void)
{
	unsigned char bTemp;

	HDMI_HDCP_FUNC();

	if (fgHostKey())
		bTemp = HDCP_CTL_CP_RSTB | HDCP_CTL_HOST_KEY;
	else
		bTemp = HDCP_CTL_CP_RSTB;

	vWriteByteHdmiGRL(GRL_HDCP_CTL, bTemp);


	for (bTemp = 0; bTemp < 5; bTemp++)
		udelay(255);

	bTemp = bReadByteHdmiGRL(GRL_HDCP_CTL);
	bTemp &= (~HDCP_CTL_CP_RSTB);

	vWriteByteHdmiGRL(GRL_HDCP_CTL, bTemp);


	vSetCTL0BeZero(FALSE);
}

void vSetHDCPState(enum HDCP_CTRL_STATE_T e_state)
{
	HDMI_HDCP_FUNC();

	if (bCheckPordHotPlug(PORD_MODE | HOTPLUG_MODE) == TRUE)
		e_hdcp_ctrl_state = e_state;
	else
		e_hdcp_ctrl_state = HDCP_RECEIVER_NOT_READY;

}

void vHDCPReset(void)
{
	HDMI_HDCP_FUNC();

	if (hdcp2_version_flag == FALSE)
		vHalHDCPReset();
	else
		vHDCP2Reset();

	vSetHDCPState(HDCP_RECEIVER_NOT_READY);
	vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
}

unsigned char fgIsHDCPCtrlTimeOut(void)
{
	HDMI_HDCP_FUNC();
	if (hdmi_TmrValue[HDMI_HDCP_PROTOCAL_CMD] <= 0)
		return TRUE;
	else
		return FALSE;
}

void vSendHdmiCmd(unsigned char u1icmd)
{
	HDMI_DRV_FUNC();
	hdmi_hdmiCmd = u1icmd;
}

void vClearHdmiCmd(void)
{
	HDMI_DRV_FUNC();
	hdmi_hdmiCmd = 0xff;
}

void vSetHDCPTimeOut(unsigned int i4_count)
{
	HDMI_HDCP_FUNC();
	hdmi_TmrValue[HDMI_HDCP_PROTOCAL_CMD] = i4_count;
}

unsigned int i4SharedInfo(unsigned int u4Index)
{
	HDMI_HDCP_FUNC();
	return i4HdmiShareInfo[u4Index];
}

void vSetSharedInfo(unsigned int u4Index, unsigned int i4Value)
{
	HDMI_DRV_FUNC();
	i4HdmiShareInfo[u4Index] = i4Value;
}

void vMiAnUpdateOrFix(unsigned char bUpdate)
{
	unsigned char bTemp;

	HDMI_HDCP_FUNC();
	if (bUpdate == TRUE) {
		bTemp = bReadByteHdmiGRL(GRL_CFG1);
		bTemp |= CFG1_HDCP_DEBUG;
		vWriteByteHdmiGRL(GRL_CFG1, bTemp);
	} else {
		bTemp = bReadByteHdmiGRL(GRL_CFG1);
		bTemp &= ~CFG1_HDCP_DEBUG;
		vWriteByteHdmiGRL(GRL_CFG1, bTemp);
	}

}

void vReadAksvFromReg(__u8 *PrBuff)
{
	unsigned char bTemp, i;

	HDMI_HDCP_FUNC();
	for (i = 0; i < 5; i++) {
		/* AKSV count 5 bytes */
		bTemp = bReadByteHdmiGRL(GRL_RD_AKSV0 + i * 4);
		*(PrBuff + i) = bTemp;
	}
}

void vWriteAksvKeyMask(unsigned char *PrData)
{
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
	HDMI_HDCP_FUNC();
	vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, 0, SYS_KEYMASK2);
	vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, 0, SYS_KEYMASK1);
#else
	unsigned char bData;
	/* - write wIdx into 92. */
	HDMI_HDCP_FUNC();


	bData = (*(PrData + 2) & 0x0f) | ((*(PrData + 3) & 0x0f) << 4);

	vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, (bData << 16), SYS_KEYMASK2);
	bData = (*(PrData + 0) & 0x0f) | ((*(PrData + 1) & 0x0f) << 4);

	vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, (bData << 8), SYS_KEYMASK1);
#endif
}


void vEnableAuthHardware(void)
{
	unsigned char bTemp;

	HDMI_HDCP_FUNC();
	bTemp = bReadByteHdmiGRL(GRL_HDCP_CTL);
	bTemp |= HDCP_CTL_AUTHEN_EN;

	vWriteByteHdmiGRL(GRL_HDCP_CTL, bTemp);

}

unsigned char fgIsRepeater(void)
{
	HDMI_HDCP_FUNC();
	return (_fgRepeater == TRUE);
}

void vRepeaterOnOff(unsigned char fgIsRep)
{
	unsigned char bTemp;

	HDMI_HDCP_FUNC();
	bTemp = bReadByteHdmiGRL(GRL_HDCP_CTL);

	if (fgIsRep == TRUE)
		bTemp |= HDCP_CTRL_RX_RPTR;
	else
		bTemp &= ~HDCP_CTRL_RX_RPTR;

	vWriteByteHdmiGRL(GRL_HDCP_CTL, bTemp);

}

void vStopAn(void)
{
	unsigned char bTemp;

	HDMI_HDCP_FUNC();
	bTemp = bReadByteHdmiGRL(GRL_HDCP_CTL);
	bTemp |= HDCP_CTL_AN_STOP;
	vWriteByteHdmiGRL(GRL_HDCP_CTL, bTemp);

}

void bReadDataHdmiGRL(unsigned char bAddr, unsigned char bCount, unsigned char *bVal)
{
	unsigned char i;

	HDMI_HDCP_FUNC();
	for (i = 0; i < bCount; i++)
		*(bVal + i) = bReadByteHdmiGRL(bAddr + i * 4);
}

void vWriteDataHdmiGRL(unsigned char bAddr, unsigned char bCount, unsigned char *bVal)
{
	unsigned char i;

	HDMI_HDCP_FUNC();
	for (i = 0; i < bCount; i++)
		vWriteByteHdmiGRL(bAddr + i * 4, *(bVal + i));
}

void vSendAn(void)
{
	unsigned char bHDCPBuf[HDCP_AN_COUNT];

	HDMI_HDCP_FUNC();
	/* Step 1: issue command to general a new An value */
	/* (1) read the value first */
	/* (2) set An control as stop to general a An first */
	vStopAn();

	/* Step 2: Read An from Transmitter */
	bReadDataHdmiGRL(GRL_WR_AN0, HDCP_AN_COUNT, bHDCPBuf);

	/* Step 3: Send An to Receiver */
	fgDDCDataWrite(RX_ID, RX_REG_HDCP_AN, HDCP_AN_COUNT, bHDCPBuf);

}

void vExchangeKSVs(void)
{
	unsigned char bHDCPBuf[HDCP_AKSV_COUNT];

	HDMI_HDCP_FUNC();
	/* Step 1: read Aksv from transmitter, and send to receiver */
	if (fgHostKey()) {
		fgDDCDataWrite(RX_ID, RX_REG_HDCP_AKSV, HDCP_AKSV_COUNT, HDMI_AKSV);
	} else {
		/* fgI2CDataRead(HDMI_DEV_GRL, GRL_RD_AKSV0, HDCP_AKSV_COUNT, bHDCPBuf); */
		bReadDataHdmiGRL(GRL_RD_AKSV0, HDCP_AKSV_COUNT, bHDCPBuf);
		fgDDCDataWrite(RX_ID, RX_REG_HDCP_AKSV, HDCP_AKSV_COUNT, bHDCPBuf);
	}
	/* Step 4: read Bksv from receiver, and send to transmitter */
	fgDDCDataRead(RX_ID, RX_REG_HDCP_BKSV, HDCP_BKSV_COUNT, bHDCPBuf);
	/* fgI2CDataWrite(HDMI_DEV_GRL, GRL_WR_BKSV0, HDCP_BKSV_COUNT, bHDCPBuf); */
	vWriteDataHdmiGRL(GRL_WR_BKSV0, HDCP_BKSV_COUNT, bHDCPBuf);

	hdmi_hdcp_info.bksv_list[0] = bHDCPBuf[0];
	hdmi_hdcp_info.bksv_list[1] = bHDCPBuf[1];
	hdmi_hdcp_info.bksv_list[2] = bHDCPBuf[2];
	hdmi_hdcp_info.bksv_list[3] = bHDCPBuf[3];
	hdmi_hdcp_info.bksv_list[4] = bHDCPBuf[4];
}

void vHalSendAKey(unsigned char bData)
{
	HDMI_HDCP_FUNC();
	vWriteByteHdmiGRL(GRL_KEY_PORT, bData);
}

void vSendAKey(unsigned char *prAKey)
{
	unsigned char bData;
	unsigned short ui2Index;

	HDMI_HDCP_FUNC();
	for (ui2Index = 0; ui2Index < 280; ui2Index++) {
		/* get key from flash */
		if ((ui2Index == 5) && (bCheckPordHotPlug(PORD_MODE | HOTPLUG_MODE) == FALSE)) {
			vSetHDCPState(HDCP_RECEIVER_NOT_READY);
			return;
		}
		bData = *(prAKey + ui2Index);
		vHalSendAKey(bData);
	}
}

void bClearGRLInt(__u8 bInt)
{
	HDMI_DRV_FUNC();
	vWriteByteHdmiGRL(GRL_INT, bInt);
}

unsigned char bReadGRLInt(void)
{
	unsigned char bStatus;

	HDMI_DRV_FUNC();

	bStatus = bReadByteHdmiGRL(GRL_INT);

	return bStatus;
}

unsigned char bCheckHDCPStatus(unsigned char bMode)
{
	unsigned char bStatus = 0;

	bStatus = bReadByteHdmiGRL(GRL_HDCP_STA);

	bStatus &= bMode;
	if (bStatus) {
		vWriteByteHdmiGRL(GRL_HDCP_STA, bMode);
		return TRUE;
	} else
		return FALSE;
}

unsigned char fgCompareRi(void)
{
	unsigned char bTemp;
	unsigned char bHDCPBuf[4];

	HDMI_HDCP_FUNC();
	/* Read R0/ Ri from Transmitter */
	/* fgI2CDataRead(HDMI_DEV_GRL, GRL_RI_0, HDCP_RI_COUNT, bHDCPBuf+HDCP_RI_COUNT); */
	bReadDataHdmiGRL(GRL_RI_0, HDCP_RI_COUNT, &bHDCPBuf[HDCP_RI_COUNT]);

	/* Read R0'/ Ri' from Receiver */
	fgDDCDataRead(RX_ID, RX_REG_RI, HDCP_RI_COUNT, bHDCPBuf);

	HDMI_HDCP_LOG("bHDCPBuf[0]=0x%x,bHDCPBuf[1]=0x%x,bHDCPBuf[2]=0x%x,bHDCPBuf[3]=0x%x\n",
		      bHDCPBuf[0], bHDCPBuf[1], bHDCPBuf[2], bHDCPBuf[3]);
	/* compare R0 and R0' */
	for (bTemp = 0; bTemp < HDCP_RI_COUNT; bTemp++) {
		if (bHDCPBuf[bTemp] == bHDCPBuf[bTemp + HDCP_RI_COUNT])
			continue;
		else
			break;
	}

	if (bTemp == HDCP_RI_COUNT)
		return TRUE;
	 else
		return FALSE;
}

void vEnableEncrpt(void)
{
	unsigned char bTemp;

	HDMI_HDCP_FUNC();
	bTemp = bReadByteHdmiGRL(GRL_HDCP_CTL);
	bTemp |= HDCP_CTL_ENC_EN;
	vWriteByteHdmiGRL(GRL_HDCP_CTL, bTemp);
}

void vHalWriteKsvListPort(unsigned char *prKsvData, unsigned char bDevice_Count,
			  unsigned char *prBstatus)
{
	unsigned char bIndex;

	HDMI_HDCP_FUNC();
	if ((bDevice_Count * 5) < KSV_BUFF_SIZE) {
		for (bIndex = 0; bIndex < (bDevice_Count * 5); bIndex++)
			vWriteByteHdmiGRL(GRL_KSVLIST, *(prKsvData + bIndex));

		for (bIndex = 0; bIndex < 2; bIndex++)
			vWriteByteHdmiGRL(GRL_KSVLIST, *(prBstatus + bIndex));
	}

}

void vHalWriteHashPort(unsigned char *prHashVBuff)
{
	unsigned char bIndex;

	HDMI_HDCP_FUNC();
	for (bIndex = 0; bIndex < 20; bIndex++)
		vWriteByteHdmiGRL(GRL_REPEATER_HASH + bIndex * 4, *(prHashVBuff + bIndex));
}

void vEnableHashHardwrae(void)
{
	unsigned char bData;

	HDMI_HDCP_FUNC();
	bData = bReadByteHdmiGRL(GRL_HDCP_CTL);
	bData |= HDCP_CTL_SHA_EN;
	vWriteByteHdmiGRL(GRL_HDCP_CTL, bData);
}

void vReadKSVFIFO(void)
{
	unsigned char bTemp, bIndex, bDevice_Count;	/* , bBlock; */
	unsigned char bStatus[2], bBstatus1;

	HDMI_HDCP_FUNC();
	fgDDCDataRead(RX_ID, RX_REG_BSTATUS1 + 1, 1, &bBstatus1);
	fgDDCDataRead(RX_ID, RX_REG_BSTATUS1, 1, &bDevice_Count);

	bDevice_Count &= DEVICE_COUNT_MASK;

	if ((bDevice_Count & MAX_DEVS_EXCEEDED) || (bBstatus1 & MAX_CASCADE_EXCEEDED)) {
		vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
		vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		return;
	}

	if (bDevice_Count > 32) {
		for (bTemp = 0; bTemp < 2; bTemp++) {
			/* retry 1 times */
			fgDDCDataRead(RX_ID, RX_REG_BSTATUS1, 1, &bDevice_Count);
			bDevice_Count &= DEVICE_COUNT_MASK;
			if (bDevice_Count <= 32)
				break;
		}
		if (bTemp == 2)
			bDevice_Count = 32;
	}

	vSetSharedInfo(SI_REPEATER_DEVICE_COUNT, bDevice_Count);

	if (bDevice_Count == 0) {
		for (bIndex = 0; bIndex < 5; bIndex++)
			bKsv_buff[bIndex] = 0;

		for (bIndex = 0; bIndex < 2; bIndex++)
			bStatus[bIndex] = 0;

		for (bIndex = 0; bIndex < 20; bIndex++)
			bSHABuff[bIndex] = 0;
	} else {
		fgDDCDataRead(RX_ID, RX_REG_KSV_FIFO, bDevice_Count * 5, bKsv_buff);
	}

	fgDDCDataRead(RX_ID, RX_REG_BSTATUS1, 2, bStatus);
	fgDDCDataRead(RX_ID, RX_REG_REPEATER_V, 20, bSHABuff);

	hdmi_hdcp_info.bstatus = (bStatus[1] << 8) | bStatus[0];
	for (bIndex = 5; bIndex < 160; bIndex++)
		hdmi_hdcp_info.bksv_list[bIndex] = bKsv_buff[bIndex-5];

	if ((bDevice_Count * 5) < KSV_BUFF_SIZE)
		vHalWriteKsvListPort(bKsv_buff, bDevice_Count, bStatus);
	vHalWriteHashPort(bSHABuff);
	vEnableHashHardwrae();
	vSetHDCPState(HDCP_COMPARE_V);
	/* set time-out value as 0.5 sec */
	vSetHDCPTimeOut(HDCP_WAIT_V_RDY_TIMEOUE);

}

unsigned char bReadHDCPStatus(void)
{
	unsigned char bTemp;

	HDMI_HDCP_FUNC();
	bTemp = bReadByteHdmiGRL(GRL_HDCP_STA);

	return bTemp;
}

void vHDCPInitAuth(void)
{
	HDMI_HDCP_FUNC();
	vSetHDCPTimeOut(HDCP_WAIT_RES_CHG_OK_TIMEOUE);	/* 100 ms */
	vSetHDCPState(HDCP_WAIT_RES_CHG_OK);
}

void vDisableHDCP(unsigned char fgDisableHdcp)
{
	HDMI_HDCP_FUNC();

	if (fgDisableHdcp) {
		vHDCPReset();

		if (fgDisableHdcp == 1)
			vMoveHDCPInternalKey(EXTERNAL_KEY);
		else if (fgDisableHdcp == 2)
			vMoveHDCPInternalKey(INTERNAL_NOENCRYPT_KEY);
		else if (fgDisableHdcp == 3)
			vMoveHDCPInternalKey(INTERNAL_ENCRYPT_KEY);

		_bHdcpOff = 1;
	} else {
		if (hdcp2_version_flag == FALSE) {
			vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		} else {
			vHDCP2InitAuth();
		}
		_bHdcpOff = 1;
	}

}

void VHdmiMuteVideoAudio(unsigned char u1flagvideomute, unsigned char u1flagaudiomute)
{
	if (u1flagvideomute == TRUE)
		vBlackHDMIOnly();
	else
		vUnBlackHDMIOnly();

	if (u1flagaudiomute == TRUE)
		MuteHDMIAudio();
	else
		UnMuteHDMIAudio();

}

void vDrm_mutehdmi(unsigned char u1flagvideomute, unsigned char u1flagaudiomute)
{
	HDMI_HDCP_LOG("u1flagvideomute = %d, u1flagaudiomute = %d\n", u1flagvideomute,
		      u1flagaudiomute);
	_bflagvideomute = u1flagvideomute;
	_bflagaudiomute = u1flagaudiomute;

	if ((_bHdcpOff == 1) && (_bsvpvideomute == FALSE))
		VHdmiMuteVideoAudio(u1flagvideomute, u1flagaudiomute);
}

void vSvp_mutehdmi(unsigned char u1svpvideomute, unsigned char u1svpaudiomute)
{
	HDMI_HDCP_LOG("u1svpvideomute = %d, u1svpaudiomute = %d\n", u1svpvideomute, u1svpaudiomute);
	_bsvpvideomute = u1svpvideomute;
	_bsvpaudiomute = u1svpaudiomute;

	VHdmiMuteVideoAudio(u1svpvideomute, u1svpaudiomute);
}

/*hdcp2.2 start here*/
void vHdcpDdcHwPoll(unsigned char _bhw)
{
	HDMI_HDCP_FUNC();

	if (_bhw == TRUE)
		vWriteHdmiGRLMsk(HDCP2X_POL_CTRL, 0, HDCP2X_DIS_POLL_EN);
	else
		vWriteHdmiGRLMsk(HDCP2X_POL_CTRL, HDCP2X_DIS_POLL_EN, HDCP2X_DIS_POLL_EN);
}

void vHDCP2Reset(void)
{
	HDMI_HDCP_FUNC();

	vWriteHdmiGRLMsk(TOP_CLK_RST_CFG, SOFT_HDCP_RST, SOFT_HDCP_RST);
	vWriteHdmiGRLMsk(TOP_CLK_RST_CFG, SOFT_HDCP_CORE_RST, SOFT_HDCP_CORE_RST);
	udelay(10);
	vWriteHdmiGRLMsk(TOP_CLK_RST_CFG, SOFT_HDCP_NOR, SOFT_HDCP_RST);
	vWriteHdmiGRLMsk(TOP_CLK_RST_CFG, SOFT_HDCP_CORE_NOR, SOFT_HDCP_CORE_RST);

	vHdcpDdcHwPoll(FALSE);
	mdelay(1);
	vWriteHdmiGRLMsk(HDCP2X_CTRL_0, 0, HDCP2X_EN | HDCP2X_ENCRYPT_EN);
}

void vHDCP2InitAuth(void)
{
	HDMI_HDCP_FUNC();
	vSetHDCPTimeOut(HDCP2_WAIT_RES_CHG_OK_TIMEOUE);	/* 100 ms */
	vSetHDCPState(HDCP2_WAIT_RES_CHG_OK);
	vHdcpDdcHwPoll(FALSE);
	memset(hdmi_hdcp_info.bksv_list, 0, sizeof(hdmi_hdcp_info.bksv_list));
	hdmi_hdcp_info.bstatus = 0;
	if (i4SharedInfo(SI_HDMI_RECEIVER_STATUS) != HDMI_PLUG_IN_AND_SINK_POWER_ON) {
		vSetHDCPState(HDCP_RECEIVER_NOT_READY);
		vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
	}
}

void vHDMI2ClearINT(void)
{
	HDMI_HDCP_FUNC();

	vWriteByteHdmiGRL(TOP_INT_CLR00, 0xffffffff);
	vWriteByteHdmiGRL(TOP_INT_CLR01, 0xffffffff);
	udelay(100);
	vWriteByteHdmiGRL(TOP_INT_CLR00, 0x0);
	vWriteByteHdmiGRL(TOP_INT_CLR01, 0x0);
}

void vDisableHDCP2(unsigned char fgDisableHdcp)
{
	HDMI_HDCP_FUNC();

	vHDCP2Reset();
	if (fgDisableHdcp) {
		vSetHDCPState(HDCP_RECEIVER_NOT_READY);
		vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		_bHdcpOff = 1;
	} else {
		vHDCP2InitAuth();
		_bHdcpOff = 0;
	}

}
void vCleanAuthFailInt(void)
{
	vWriteByteHdmiGRL(TOP_INT_CLR00, 0x00020000);
	udelay(100);
	vWriteByteHdmiGRL(TOP_INT_CLR00, 0x00000000);
	HDMI_HDCP_LOG("0x14025c8c = 0x%08x\n", bReadByteHdmiGRL(HDCP2X_STATUS_0));
}


void HdcpService(void)
{
	unsigned char bIndx, bTemp, bRxStatus[2];
	unsigned char bMask, bRptID[155];
	unsigned int readvalue, i, devicecnt, depth, data;

	HDMI_HDCP_FUNC();

	if (_bHdcpOff == 1) {
		HDMI_HDCP_LOG("_bHdcpOff==1\n");
		vSetHDCPState(HDCP_RECEIVER_NOT_READY);
		vCaHDCPOffState(TRUE, false);
		vHDMIAVUnMute();
		/* vWriteHdmiIntMask(0xff); */
	}

	switch (e_hdcp_ctrl_state) {
	case HDCP_RECEIVER_NOT_READY:
		HDMI_HDCP_LOG("HDCP_RECEIVER_NOT_READY\n");
		break;

	case HDCP_READ_EDID:
		break;

	case HDCP_WAIT_RES_CHG_OK:
		HDMI_HDCP_LOG("HDCP_WAIT_RES_CHG_OK\n");
		if (fgIsHDCPCtrlTimeOut()) {
			if (_bHdcpOff == 1) {
				/* disable HDCP */
				vSetHDCPState(HDCP_RECEIVER_NOT_READY);
				/*vHDMIAVUnMute();*/
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			} else {
				vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			}
		}
		break;


	case HDCP_INIT_AUTHENTICATION:
		HDMI_HDCP_LOG("HDCP_INIT_AUTHENTICATION\n");
		/*vHDMIAVMute();*/

		memset(hdmi_hdcp_info.bksv_list, 0, sizeof(hdmi_hdcp_info.bksv_list));
		hdmi_hdcp_info.bstatus = 0;

		vSetSharedInfo(SI_HDMI_HDCP_RESULT, 0);

		if (!fgDDCDataRead(RX_ID, RX_REG_BCAPS, 1, &bTemp)) {
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
			vCaHDCPFailState(TRUE, HDCP_INIT_AUTHENTICATION);
#endif
			vSetHDCPTimeOut(HDCP_WAIT_300MS_TIMEOUT);
			break;
		}

		vMiAnUpdateOrFix(TRUE);

		if (fgHostKey()) {
			for (bIndx = 0; bIndx < HDCP_AKSV_COUNT; bIndx++) {
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
				HDMI_AKSV[bIndx] = u1CaHdcpAKsv[bIndx];
#else
				HDMI_AKSV[bIndx] = bHdcpKeyBuff[1 + bIndx];
#endif
			}

			if ((HDMI_AKSV[0] == 0) && (HDMI_AKSV[1] == 0) && (HDMI_AKSV[2] == 0)
			    && (HDMI_AKSV[3] == 0)) {
				vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
				vCaHDCPFailState(TRUE, HDCP_RECEIVER_NOT_READY);
#endif
				break;
			}
		} else {
			vReadAksvFromReg(&HDMI_AKSV[0]);
		}

		if ((bhdcpkey == INTERNAL_ENCRYPT_KEY) || (bhdcpkey == EXTERNAL_KEY))
			vWriteAksvKeyMask(&HDMI_AKSV[0]);

		vEnableAuthHardware();
		fgDDCDataRead(RX_ID, RX_REG_BCAPS, 1, &bTemp);
		vSetSharedInfo(SI_REPEATER_DEVICE_COUNT, 0);
		if (bTemp & RX_BIT_ADDR_RPTR)
			_fgRepeater = TRUE;
		else
			_fgRepeater = FALSE;

		if (fgIsRepeater())
			vRepeaterOnOff(TRUE);
		 else
			vRepeaterOnOff(FALSE);

		vSendAn();

		vExchangeKSVs();

		if (fgHostKey()) {
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
			fgCaHDMILoadHDCPKey();
#else
			vSendAKey(&bHdcpKeyBuff[6]);	/* around 190msec */
#endif
			vSetHDCPTimeOut(HDCP_WAIT_R0_TIMEOUT);
		} else {
			vSetHDCPTimeOut(HDCP_WAIT_R0_TIMEOUT);	/* 100 ms */
		}

		/* change state as waiting R0 */
		vSetHDCPState(HDCP_WAIT_R0);
		break;


	case HDCP_WAIT_R0:
		HDMI_HDCP_LOG("HDCP_WAIT_R0\n");
		bTemp = bCheckHDCPStatus(HDCP_STA_RI_RDY);
		if (bTemp == TRUE) {
			vSetHDCPState(HDCP_COMPARE_R0);
		} else {
			vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
			vCaHDCPFailState(TRUE, HDCP_WAIT_R0);
#endif
			break;
		}

	case HDCP_COMPARE_R0:
		HDMI_HDCP_LOG("HDCP_COMPARE_R0\n");
		if (fgCompareRi() == TRUE) {
			vMiAnUpdateOrFix(FALSE);

			vEnableEncrpt();	/* Enabe encrption */
			vSetCTL0BeZero(TRUE);

			/* change state as check repeater */
			vSetHDCPState(HDCP_CHECK_REPEATER);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			vSetSharedInfo(SI_HDMI_HDCP_RESULT, 0x01);	/* step 1 OK. */
		} else {
			vSetHDCPState(HDCP_RE_COMPARE_R0);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
			vCaHDCPFailState(TRUE, HDCP_COMPARE_R0);
#endif
			_bReCompRiCount = 0;
		}
		break;

	case HDCP_RE_COMPARE_R0:
		HDMI_HDCP_LOG("HDCP_RE_COMPARE_R0\n");
		_bReCompRiCount++;
		if (fgIsHDCPCtrlTimeOut() && _bReCompRiCount > 3) {
			vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			_bReCompRiCount = 0;
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
			vCaHDCPFailState(TRUE, HDCP_RE_COMPARE_R0);
#endif
		} else {
			if (fgCompareRi() == TRUE) {
				vMiAnUpdateOrFix(FALSE);
				vEnableEncrpt();	/* Enabe encrption */
				vSetCTL0BeZero(TRUE);

				/* change state as check repeater */
				vSetHDCPState(HDCP_CHECK_REPEATER);
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
				vSetSharedInfo(SI_HDMI_HDCP_RESULT, 0x01);	/* step 1 OK. */
			} else {
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			}

		}
		break;

	case HDCP_CHECK_REPEATER:
		HDMI_HDCP_LOG("HDCP_CHECK_REPEATER\n");
		/* if the device is a Repeater, */
		if (fgIsRepeater()) {
			_bReCheckReadyBit = 0;
			vSetHDCPState(HDCP_WAIT_KSV_LIST);
			vSetHDCPTimeOut(HDCP_WAIT_KSV_LIST_TIMEOUT);
		} else {
			vSetHDCPState(HDCP_WAIT_RI);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		}

		break;

	case HDCP_WAIT_KSV_LIST:
		HDMI_HDCP_LOG("HDCP_WAIT_KSV_LIST\n");
		fgDDCDataRead(RX_ID, RX_REG_BCAPS, 1, &bTemp);
		if ((bTemp & RX_BIT_ADDR_READY)) {
			_bReCheckReadyBit = 0;
			vSetHDCPState(HDCP_READ_KSV_LIST);
		} else if (_bReCheckReadyBit > HDCP_CHECK_KSV_LIST_RDY_RETRY_COUNT) {
			vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			_bReCheckReadyBit = 0;
			break;
		} else {
			_bReCheckReadyBit++;
			vSetHDCPState(HDCP_WAIT_KSV_LIST);
			vSetHDCPTimeOut(HDCP_WAIT_KSV_LIST_RETRY_TIMEOUT);
			break;
		}

	case HDCP_READ_KSV_LIST:
		HDMI_HDCP_LOG("HDCP_READ_KSV_LIST\n");
		vReadKSVFIFO();
		break;

	case HDCP_COMPARE_V:
		HDMI_HDCP_LOG("HDCP_COMPARE_V\n");
		bTemp = bReadHDCPStatus();
		if ((bTemp & HDCP_STA_V_MATCH) || (bTemp & HDCP_STA_V_RDY)) {
			if ((bTemp & HDCP_STA_V_MATCH))	{
				/* for Simplay #7-20-5 */
				vSetHDCPState(HDCP_WAIT_RI);
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
				/* step 2 OK. */
				vSetSharedInfo(SI_HDMI_HDCP_RESULT, (i4SharedInfo(SI_HDMI_HDCP_RESULT) | 0x02));
			} else {
				vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			}
		}
		break;

	case HDCP_WAIT_RI:
		HDMI_HDCP_LOG("HDCP_WAIT_RI\n");
		/*vHDMIAVUnMute();*/
		pr_err("[hdcp]hdcp1.x pass\n");
		bMask = bReadHdmiIntMask();
		/* vWriteHdmiIntMask(0xfd); */
		break;

	case HDCP_CHECK_LINK_INTEGRITY:
		HDMI_HDCP_LOG("HDCP_CHECK_LINK_INTEGRITY\n");
		if (fgCompareRi() == TRUE) {
			/* step 3 OK. */
			vSetSharedInfo(SI_HDMI_HDCP_RESULT, (i4SharedInfo(SI_HDMI_HDCP_RESULT) | 0x04));
			if (fgIsRepeater()) {
				if (i4SharedInfo(SI_HDMI_HDCP_RESULT) == 0x07)	/* step 1, 2, 3. */
					vSetSharedInfo(SI_HDMI_HDCP_RESULT, (i4SharedInfo(SI_HDMI_HDCP_RESULT) | 0x08));
			} else	{
			    /* not repeater, don't need step 2. */
				if (i4SharedInfo(SI_HDMI_HDCP_RESULT) == 0x05)
					vSetSharedInfo(SI_HDMI_HDCP_RESULT, (i4SharedInfo(SI_HDMI_HDCP_RESULT) | 0x08));
			}
		} else {
			bMask = bReadHdmiIntMask();
			/* vWriteHdmiIntMask(0xff);//disable INT HDCP */
			_bReCompRiCount = 0;
			vSetHDCPState(HDCP_RE_COMPARE_RI);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
			vCaHDCPFailState(TRUE, HDCP_CHECK_LINK_INTEGRITY);
#endif
		}
		break;

	case HDCP_RE_COMPARE_RI:
		HDMI_HDCP_LOG("HDCP_RE_COMPARE_RI\n");
		_bReCompRiCount++;
		if (_bReCompRiCount > 5) {
			vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			_bReCompRiCount = 0;
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
			vCaHDCPFailState(TRUE, HDCP_RE_COMPARE_RI);
#endif
		} else {
			if (fgCompareRi() == TRUE) {
				_bReCompRiCount = 0;
				vSetHDCPState(HDCP_CHECK_LINK_INTEGRITY);
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
				vSetSharedInfo(SI_HDMI_HDCP_RESULT, (i4SharedInfo(SI_HDMI_HDCP_RESULT) | 0x04));
				if (fgIsRepeater()) {
					if (i4SharedInfo(SI_HDMI_HDCP_RESULT) == 0x07)	/* step 1, 2, 3. */
						vSetSharedInfo(SI_HDMI_HDCP_RESULT, (0x07 | 0x08));
				} else {
					if (i4SharedInfo(SI_HDMI_HDCP_RESULT) == 0x05)	/* step 1, 3. */
						vSetSharedInfo(SI_HDMI_HDCP_RESULT,	(0x05 | 0x08));
				}

				bMask = bReadHdmiIntMask();
				/* vWriteHdmiIntMask(0xfd); */
			} else {
				vSetHDCPState(HDCP_RE_COMPARE_RI);
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			}
		}
		break;

	case HDCP_RE_DO_AUTHENTICATION:
		HDMI_HDCP_LOG("HDCP_RE_DO_AUTHENTICATION\n");
		/*vHDMIAVMute();*/
		vHDCPReset();
		if (i4SharedInfo(SI_HDMI_RECEIVER_STATUS) != HDMI_PLUG_IN_AND_SINK_POWER_ON) {
			vSetHDCPState(HDCP_RECEIVER_NOT_READY);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		} else {
			vSetHDCPState(HDCP_WAIT_RESET_OK);
			vSetHDCPTimeOut(HDCP_WAIT_RE_DO_AUTHENTICATION);
		}
		break;

	case HDCP_WAIT_RESET_OK:
		HDMI_HDCP_LOG("HDCP_WAIT_RESET_OK\n");
		if (fgIsHDCPCtrlTimeOut()) {
			vSetHDCPState(HDCP_INIT_AUTHENTICATION);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		}
		break;

		/*hdcp2 code start here */
	case HDCP2_WAIT_RES_CHG_OK:
		HDMI_HDCP_LOG("hdcp2 command flow: HDCP2_WAIT_RES_CHG_OK\n");
		if (fgIsHDCPCtrlTimeOut()) {
			if (_bHdcpOff == 1) {
				vSetHDCPState(HDCP_RECEIVER_NOT_READY);
				/*vHDMI2AVUnMute();*/
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			} else {
				/*vHDMI2AVMute();*/
				_bReRepeaterPollCnt = 0;
				_bReAuthCnt = 0;
				vSetHDCPState(HDCP2_SWTICH_OK);
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
				vWriteHdmiGRLMsk(HDCP2X_CTRL_0, 0, HDCP2X_EN | HDCP2X_ENCRYPT_EN);
				vWriteHdmiGRLMsk(HPD_DDC_CTRL, DDC2_CLOK << DDC_DELAY_CNT_SHIFT,
						 DDC_DELAY_CNT);

				vWriteByteHdmiGRL(DDC_CTRL,
						  (SEQ_READ_NO_ACK << DDC_CMD_SHIFT) +
						  (1 << DDC_DIN_CNT_SHIFT) +
						  (RX_REG_HDCP2VERSION << DDC_OFFSET_SHIFT) +
						  (RX_ID << 1));
				mdelay(2);
			}
		}
		break;

	case HDCP2_SWTICH_OK:
		HDMI_HDCP_LOG("hdcp2 command flow: HDCP2_SWTICH_OK\n");

		vWriteByteHdmiGRL(SI2C_CTRL, (SI2C_ADDR_READ << SI2C_ADDR_SHIFT) + SI2C_RD);
		vWriteByteHdmiGRL(SI2C_CTRL,
				  (SI2C_ADDR_READ << SI2C_ADDR_SHIFT) + SI2C_CONFIRM_READ);

		readvalue = (bReadByteHdmiGRL(HPD_DDC_STATUS) & DDC_DATA_OUT) >> DDC_DATA_OUT_SHIFT;
		if (readvalue & 0x4) {
			HDMI_HDCP_LOG("hdcp2 switch pass\n");
			vHDMI2ClearINT();
			vSetHDCPState(HDCP2_LOAD_BIN);
			vSetHDCPTimeOut(HDCP2_WAIT_LOADBIN_TIMEOUE);
		} else {
			HDMI_HDCP_LOG("hdcp2 switch fail\n");
			vSetHDCPState(HDCP2_WAIT_RES_CHG_OK);
			vSetHDCPTimeOut(HDCP2_WAIT_SWITCH_TIMEOUE);
			vHdcpDdcHwPoll(FALSE);
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
			vCaHDCPFailState(TRUE, HDCP_RE_COMPARE_RI);
#endif
		}
		break;

	case HDCP2_LOAD_BIN:
		HDMI_HDCP_LOG("hdcp2 command flow: HDCP2_LOAD_BIN\n");
		vWriteHdmiGRLMsk(TOP_CLK_RST_CFG, SOFT_HDCP_CORE_RST, SOFT_HDCP_CORE_RST);
		/* vWriteHdmiGRLMsk(TOP_CLK_RST_CFG, SOFT_HDCP_RST, SOFT_HDCP_RST); */
		/* vWriteHdmiGRLMsk(TOP_CLK_RST_CFG, 0, SOFT_HDCP_RST); */
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
		fgCaHDMILoadROM();
#else
		for (readvalue = 0; readvalue < 0x8000; readvalue++) {
			udelay(1);
			vWriteByteHdmiGRL(PROM_CTRL,
					  (readvalue << PROM_ADDR_SHIFT) +
					  (hdcp_prom[readvalue] << PROM_WDATA_SHIFT) + PROM_CS +
					  PROM_WR);
		}

		vWriteByteHdmiGRL(PROM_CTRL, 0);
		HDMI_HDCP_LOG("write hdcp2 prom, readvalue=0x%x, hdcp_pram[readvalue]=0x%x\n",
			      readvalue, hdcp_prom[readvalue - 1]);

		for (readvalue = 0; readvalue < 0x4000; readvalue++) {
			udelay(1);
			vWriteByteHdmiGRL(PRAM_CTRL,
					  (readvalue << PRAM_ADDR_SHIFT) +
					  (hdcp_pram[readvalue] << PRAM_WDATA_SHIFT) +
					  PRAM_CTRL_SEL + PRAM_CS + PRAM_WR);
		}
		udelay(5);
		vWriteByteHdmiGRL(PRAM_CTRL, 0);
		HDMI_HDCP_LOG("write hdcp2 pram, readvalue=0x%x, hdcp_pram[readvalue]=0x%x\n",
			      readvalue, hdcp_pram[readvalue - 1]);
#endif
		vWriteHdmiGRLMsk(HDCP2X_CTRL_0, HDCP2X_CUPD_START, HDCP2X_CUPD_START);
		vWriteHdmiGRLMsk(TOP_CLK_RST_CFG, 0, SOFT_HDCP_CORE_RST);
		vWriteHdmiGRLMsk(TOP_CLK_RST_CFG, HDCP_TCLK_EN, HDCP_TCLK_EN);

		vSetHDCPState(HDCP2_INITAIL_OK);
		/* vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD); */
		vSetHDCPTimeOut(HDCP2_WAIT_AUTHEN_TIMEOUE);

		break;

	case HDCP2_INITAIL_OK:
		HDMI_HDCP_LOG("hdcp2 command flow: HDCP2_INITAIL_OK\n");
		readvalue = bReadByteHdmiGRL(TOP_INT_STA00);
		if (readvalue & HDCP2X_CCHK_DONE_INT_STA) {
			HDMI_HDCP_LOG("hdcp2.2 ram/rom check is done\n");
			vSetHDCPState(HDCP2_AUTHENTICATION);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		} else {
			HDMI_HDCP_LOG("hdcp2.2 ram/rom check is fail\n");
			vHDCP2InitAuth();
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
			vCaHDCPFailState(TRUE, HDCP2_INITAIL_OK);
#endif
			return;
		}
		break;

	case HDCP2_AUTHENTICATION:
		HDMI_HDCP_LOG("hdcp2 command flow: HDCP2_AUTHENTICATION\n");
		vHdcpDdcHwPoll(TRUE);
		vWriteHdmiGRLMsk(HDCP2X_TEST_TP0, 0x75 << HDCP2X_TP1_SHIFT, HDCP2X_TP1);
		/* vWriteHdmiGRLMsk(HDCP2X_GP_IN, 0x22<<HDCP2X_GP_IN1_SHIFT, HDCP2X_GP_IN1); */
		vWriteHdmiGRLMsk(HDCP2X_CTRL_0, HDCP2X_EN | HDCP2X_HDMIMODE,
				 HDCP2X_EN | HDCP2X_HDMIMODE);
		vWriteHdmiGRLMsk(HDCP2X_POL_CTRL, 0x3402, HDCP2X_POL_VAL1 | HDCP2X_POL_VAL0);
		/* vWriteHdmiGRLMsk(HPD_DDC_CTRL, 0x62, DDC_DELAY_CNT); */
		vWriteByteHdmiGRL(HDCP2X_TEST_TP0, 0x2a018c02);
		vWriteByteHdmiGRL(HDCP2X_TEST_TP1, 0x09026411);
		vWriteByteHdmiGRL(HDCP2X_TEST_TP2, 0xa7111109);
		vWriteByteHdmiGRL(HDCP2X_TEST_TP3, 0x00fa0d7d);
		vWriteHdmiGRLMsk(HDCP2X_GP_IN, 0x0 << HDCP2X_GP_IN2_SHIFT, HDCP2X_GP_IN2);

		vWriteHdmiGRLMsk(HDCP2X_CTRL_2, HDCP2X_RPT_SMNG_WR_START, HDCP2X_RPT_SMNG_WR_START);

		vWriteHdmiGRLMsk(HDCP2X_CTRL_2, 1 << HDCP2X_RPT_SMNG_K_SHIFT, HDCP2X_RPT_SMNG_K);

		vWriteHdmiGRLMsk(HDCP2X_CTRL_2, HDCP2X_RPT_SMNG_WR, HDCP2X_RPT_SMNG_WR);
		udelay(1);
		vWriteHdmiGRLMsk(HDCP2X_CTRL_2, 0, HDCP2X_RPT_SMNG_WR);

		/* enforce type 1 only in 4K mode */
		HDMI_PLUG_LOG("_stAvdAVInfo.e_resolution=0x%x\n", _stAvdAVInfo.e_resolution);
		if ((_stAvdAVInfo.e_resolution > 0x12) && (_stAvdAVInfo.e_resolution < 0x19)) {
			HDMI_PLUG_LOG("hdcp2 command flow: HDCP2_AUTHENTICATION enforcing type 1 for 4K\n");
			vWriteHdmiGRLMsk(HDCP2X_CTRL_2, 1 << HDCP2X_RPT_SMNG_IN_SHIFT, HDCP2X_RPT_SMNG_IN);
		} else {
			HDMI_PLUG_LOG("hdcp2 command flow: HDCP2_AUTHENTICATION type 1 not needed\n");
			vWriteHdmiGRLMsk(HDCP2X_CTRL_2, 0, HDCP2X_RPT_SMNG_IN);
		}

		vWriteHdmiGRLMsk(HDCP2X_CTRL_2, HDCP2X_RPT_SMNG_WR, HDCP2X_RPT_SMNG_WR);
		udelay(1);
		vWriteHdmiGRLMsk(HDCP2X_CTRL_2, 0, HDCP2X_RPT_SMNG_WR);

		/* udelay(100); */
		vWriteHdmiGRLMsk(HDCP2X_CTRL_0, HDCP2X_REAUTH_SW, HDCP2X_REAUTH_SW);
		udelay(100);
		vWriteHdmiGRLMsk(HDCP2X_CTRL_0, 0, HDCP2X_REAUTH_SW);

		vSetHDCPState(HDCP2_REPEATER_CHECK);
		vSetHDCPTimeOut(HDCP2_WAIT_REPEATER_CHECK_TIMEOUE);

		break;

	case HDCP2_AUTHEN_CHECK:
		HDMI_HDCP_LOG("hdcp2 command flow: HDCP2_AUTHEN_CHECK\n");
		readvalue = bReadByteHdmiGRL(TOP_INT_STA00);
		if (readvalue & HDCP2X_AUTH_DONE_INT_STA) {
			vSetHDCPState(HDCP2_ENCRYPTION);
			vSetHDCPTimeOut(HDCP2_WAIT_AITHEN_DEALY_TIMEOUE);
		data = bReadByteHdmiGRL(HDCP2X_RCVR_ID);
		hdmi_hdcp_info.bksv_list[0] = (data&0xFF);
		hdmi_hdcp_info.bksv_list[1] = ((data>>8)&0xFF);
		hdmi_hdcp_info.bksv_list[2] = ((data>>16)&0xFF);
		hdmi_hdcp_info.bksv_list[3] = ((data>>24)&0xFF);
		data = bReadByteHdmiGRL(HDCP2X_RPT_SEQ);
		hdmi_hdcp_info.bksv_list[4] = ((data>>24)&0xFF);
			_bReAuthCnt = 0;
		} else if ((readvalue & HDCP2X_AUTH_FAIL_INT_STA) || (_bReAuthCnt > 10)) {
			HDMI_HDCP_LOG("hdcp2.2 authentication is fail\n");
			vHDCP2InitAuth();
			_bReAuthCnt = 0;
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
			vCaHDCPFailState(TRUE, HDCP2_AUTHEN_CHECK);
#endif
			if (readvalue & HDCP2X_AUTH_FAIL_INT_STA) {
				vCleanAuthFailInt();
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
				vCaHDCPFailState(TRUE, MARK_LAST_AUTH_FAIL);
#endif
			}
		} else {
			if ((readvalue & HDCP2X_AUTH_FAIL_INT_STA) && (_bReAuthCnt == 0)) {
				vCleanAuthFailInt();
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
				vCaHDCPFailState(TRUE, MARK_FIRST_AUTH_FAIL);
#endif
			}
			_bReAuthCnt++;
			HDMI_HDCP_LOG("hdcp2.2 authentication wait=%d\n", _bReAuthCnt);
			vSetHDCPState(HDCP2_AUTHEN_CHECK);
			vSetHDCPTimeOut(HDCP2_WAIT_AUTHEN_TIMEOUE);
		}

		break;

	case HDCP2_ENCRYPTION:
		HDMI_HDCP_LOG("hdcp2 command flow: HDCP2_ENCRYPTION\n");
		if (i4SharedInfo(SI_HDMI_RECEIVER_STATUS) != HDMI_PLUG_IN_AND_SINK_POWER_ON) {
			vSetHDCPState(HDCP_RECEIVER_NOT_READY);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		} else {
			HDMI_HDCP_LOG("hdcp2.2 HDCP2_ENCRYPTION\n");
			vWriteHdmiGRLMsk(HDCP2X_CTRL_0, HDCP2X_ENCRYPT_EN, HDCP2X_ENCRYPT_EN);
			/* vHdcpDdcHwPoll(FALSE); */
			/* vSetHDCPState(HDCP2_POLLING_RXSTATUS); */
			/* vSetHDCPTimeOut(HDCP2_WAIT_POLLING_TIMEOUE); */
			/*vHDMI2AVUnMute();*/
			_bReAuthCnt = 0;
			pr_err("[hdcp]hdcp2.2 pass\n");
		}
		break;

	case HDCP2_POLLING_RXSTATUS:
		HDMI_HDCP_LOG("hdcp2 command flow: HDCP2_POLLING_RXSTATUS\n");
		vHdcpDdcHwPoll(FALSE);
		if (i4SharedInfo(SI_HDMI_RECEIVER_STATUS) != HDMI_PLUG_IN_AND_SINK_POWER_ON) {
			vSetHDCPState(HDCP_RECEIVER_NOT_READY);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		} else {
			vWriteByteHdmiGRL(DDC_CTRL,
					  (SEQ_READ_NO_ACK << DDC_CMD_SHIFT) +
					  (2 << DDC_DIN_CNT_SHIFT) +
					  (RX_REG_RXSTATUS << DDC_OFFSET_SHIFT) + (RX_ID << 1));
			mdelay(1);
			readvalue = 0;

			for (bIndx = 0; bIndx < 2; bIndx++) {
				vWriteByteHdmiGRL(SI2C_CTRL,
						  (SI2C_ADDR_READ << SI2C_ADDR_SHIFT) + SI2C_RD);
				vWriteByteHdmiGRL(SI2C_CTRL,
						  (SI2C_ADDR_READ << SI2C_ADDR_SHIFT) +
						  SI2C_CONFIRM_READ);

				bRxStatus[bIndx] =
				    (bReadByteHdmiGRL(HPD_DDC_STATUS) & DDC_DATA_OUT) >>
				    DDC_DATA_OUT_SHIFT;
			}

			readvalue = (bRxStatus[1] << 8) + bRxStatus[0];
			HDMI_HDCP_LOG("hdcp2.2 rxstatus req readvalue=0x%x\n", readvalue);

			if (readvalue & RX_RXSTATUS_REAUTH_REQ) {
				vHDCP2InitAuth();
				HDMI_HDCP_LOG("hdcp2.2 rxstatus req send failure\n");
			} else {
				vSetHDCPState(HDCP2_POLLING_RXSTATUS);
				vSetHDCPTimeOut(HDCP2_WAIT_POLLING_TIMEOUE);
			}
		}
		break;

	case HDCP2_REPEATER_CHECK:
		HDMI_HDCP_LOG("hdcp2 command flow: HDCP2_REPEATER_CHECK\n");
		readvalue = bReadByteHdmiGRL(HDCP2X_STATUS_0);
		if (readvalue & HDCP2X_RPT_REPEATER) {
			HDMI_HDCP_LOG("downstream device is repeater\n");
			vSetHDCPState(HDCP2_REPEATER_CHECK_OK);
			vSetHDCPTimeOut(HDCP2_WAIT_REPEATER_POLL_TIMEOUE);
		} else {
			HDMI_HDCP_LOG("downstream device is receiver\n");
			vSetHDCPState(HDCP2_AUTHEN_CHECK);
			vSetHDCPTimeOut(HDCP2_WAIT_AUTHEN_TIMEOUE);
		}
		break;

	case HDCP2_REPEATER_CHECK_OK:
		HDMI_HDCP_LOG("hdcp2 command flow: HDCP2_REPEATER_CHECK_OK\n");
		readvalue = bReadByteHdmiGRL(TOP_INT_STA00);
		if (readvalue & HDCP2X_RPT_RCVID_CHANGED_INT_STA) {
			_bReRepeaterPollCnt = 0;
			vSetHDCPState(HDCP2_RESET_RECEIVER);
			vSetHDCPTimeOut(HDCP2_WAIT_RESET_RECEIVER_TIMEOUE);

		} else if (_bReRepeaterPollCnt < 10) {
			_bReRepeaterPollCnt++;
			HDMI_HDCP_LOG("_bReRepeaterPollCnt=%d\n", _bReRepeaterPollCnt);
			vSetHDCPState(HDCP2_REPEATER_CHECK_OK);
			vSetHDCPTimeOut(HDCP2_WAIT_REPEATER_POLL_TIMEOUE);
		} else {
			vHDCP2InitAuth();
			HDMI_HDCP_LOG("hdcp2.2 assume repeater failure\n");
			_bReRepeaterPollCnt = 0;
		}
		break;

	case HDCP2_RESET_RECEIVER:
		HDMI_HDCP_LOG("hdcp2 command flow: HDCP2_RESET_RECEIVER\n");
		vWriteHdmiGRLMsk(HDCP2X_CTRL_2, HDCP2X_RPT_RCVID_RD_START,
				 HDCP2X_RPT_RCVID_RD_START);
		udelay(1);
		vWriteHdmiGRLMsk(HDCP2X_CTRL_2, 0, HDCP2X_RPT_RCVID_RD_START);
		depth = bReadByteHdmiGRL(HDCP2X_STATUS_1) & HDCP2X_RPT_DEPTH;
		devicecnt =
		    (bReadByteHdmiGRL(HDCP2X_STATUS_1) & HDCP2X_RPT_DEVCNT) >>
		    HDCP2X_RPT_DEVCNT_SHIFT;
		bRptID[0] =
		    (bReadByteHdmiGRL(HDCP2X_STATUS_1) & HDCP2X_RPT_RCVID_OUT) >>
		    HDCP2X_RPT_RCVID_OUT_SHIFT;
		for (i = 1; i < 5 * devicecnt; i++) {
			vWriteHdmiGRLMsk(HDCP2X_CTRL_2, HDCP2X_RPT_RCVID_RD, HDCP2X_RPT_RCVID_RD);
			udelay(1);
			vWriteHdmiGRLMsk(HDCP2X_CTRL_2, 0, HDCP2X_RPT_RCVID_RD);
			if (i < 155) {
				bRptID[i] =
				    (bReadByteHdmiGRL(HDCP2X_STATUS_1) & HDCP2X_RPT_RCVID_OUT) >>
				    HDCP2X_RPT_RCVID_OUT_SHIFT;
			} else
				HDMI_HDCP_LOG("device count exceed\n");
		}

		for (i = 0; i < 5 * devicecnt; i++) {
			if ((i % 5) == 0)
				HDMI_HDCP_LOG("ID[%d]:", i / 5);

			HDMI_HDCP_LOG("0x%x,", bRptID[i]);

			if ((i % 5) == 4)
				HDMI_HDCP_LOG("\n");
		}

		hdmi_hdcp_info.bstatus = devicecnt&0x7F;
		hdmi_hdcp_info.bstatus |= ((depth&0x07)<<8);
		hdmi_hdcp_info.bstatus |= (1<<12);
		data = bReadByteHdmiGRL(HDCP2X_STATUS_0);
		if (data&HDCP2X_RPT_MX_DEVS_EXC)
			hdmi_hdcp_info.bstatus |= (1<<7);

		if (data&HDCP2X_RPT_MAX_CASC_EXC)
			hdmi_hdcp_info.bstatus |= (1<<11);

		for (i = 5; i < 160; i++)
			hdmi_hdcp_info.bksv_list[i] = bRptID[i-5];

		vWriteHdmiGRLMsk(HDCP2X_CTRL_2, HDCP2X_RPT_SMNG_WR_START, HDCP2X_RPT_SMNG_WR_START);
		vWriteHdmiGRLMsk(HDCP2X_CTRL_2, 1 << HDCP2X_RPT_SMNG_K_SHIFT, HDCP2X_RPT_SMNG_K);
		vWriteHdmiGRLMsk(HDCP2X_CTRL_2, HDCP2X_RPT_SMNG_WR, HDCP2X_RPT_SMNG_WR);
		udelay(1);
		vWriteHdmiGRLMsk(HDCP2X_CTRL_2, 0, HDCP2X_RPT_SMNG_WR);
		vWriteHdmiGRLMsk(HDCP2X_CTRL_2, 1 << HDCP2X_RPT_SMNG_IN_SHIFT, HDCP2X_RPT_SMNG_IN);
		vWriteHdmiGRLMsk(HDCP2X_CTRL_2, HDCP2X_RPT_SMNG_WR, HDCP2X_RPT_SMNG_WR);
		udelay(1);
		vWriteHdmiGRLMsk(HDCP2X_CTRL_2, 0, HDCP2X_RPT_SMNG_WR);

		vSetHDCPState(HDCP2_REPEAT_MSG_DONE);
		vSetHDCPTimeOut(HDCP2_WAIT_REPEATER_DONE_TIMEOUE);
		break;

	case HDCP2_REPEAT_MSG_DONE:
		HDMI_HDCP_LOG("hdcp2 command flow: HDCP2_REPEAT_MSG_DONE\n");
		readvalue = bReadByteHdmiGRL(TOP_INT_STA00);
		if (readvalue & HDCP2X_RPT_SMNG_XFER_DONE_INT_STA) {
			_bReRepeaterDoneCnt = 0;
			vWriteHdmiGRLMsk(HDCP2X_CTRL_2, 0, HDCP2X_RPT_SMNG_WR_START);
			vSetHDCPState(HDCP2_AUTHEN_CHECK);
			vSetHDCPTimeOut(HDCP2_WAIT_AUTHEN_TIMEOUE);
		} else if (_bReRepeaterDoneCnt < 10) {
			_bReRepeaterDoneCnt++;
			HDMI_HDCP_LOG("_bReRepeaterDoneCnt=%d\n", _bReRepeaterDoneCnt);
			vSetHDCPState(HDCP2_REPEAT_MSG_DONE);
			vSetHDCPTimeOut(HDCP2_WAIT_REPEATER_DONE_TIMEOUE);
		} else {
			vHDCP2InitAuth();
			HDMI_HDCP_LOG("hdcp2.2 repeater smsg done failure\n");
			_bReRepeaterDoneCnt = 0;
		}
		break;

	default:
		break;
	}
}

unsigned char hdmi_check_hdcp_key(void)
{
	if ((HDMI_AKSV[0] == 0) && (HDMI_AKSV[1] == 0) && (HDMI_AKSV[2] == 0) && (HDMI_AKSV[3] == 0)
	    && (HDMI_AKSV[4] == 0))
		return 0;
	return 1;
}

unsigned char hdmi_check_hdcp_state(void)
{
	if ((e_hdcp_ctrl_state == HDCP_WAIT_RI) || (e_hdcp_ctrl_state == HDCP_CHECK_LINK_INTEGRITY))
		return 1;
	return 0;
}

unsigned char hdmi_check_hdcp_enable(void)
{
	unsigned char bTemp;

	bTemp = bReadByteHdmiGRL(GRL_HDCP_CTL);
	if ((bTemp & HDCP_CTL_AUTHEN_EN) == 0)
		return 0;
	return 1;
}

#endif
