#ifdef CONFIG_MTK_MT8193_HDMI_SUPPORT

#include "mt8193hdmictrl.h"
#include "mt8193hdcp.h"
#include "mt8193_ctrl.h"
#include "mt8193ddc.h"

/* no encrypt key */
const u8 HDCP_NOENCRYPT_KEY[HDCP_KEY_RESERVE] = {
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

/* encrypt key */
const u8 HDCP_ENCRYPT_KEY[HDCP_KEY_RESERVE] = {
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

static u8 bHdcpKeyExternalBuff[HDCP_KEY_RESERVE] = {
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


extern HDMI_CTRL_STATE_T e_hdmi_ctrl_state;
extern HDCP_CTRL_STATE_T e_hdcp_ctrl_state;
#ifdef CONFIG_MTK_MT8193_HDCP_SUPPORT
static u8 _bHdcpOff;
#else
static u8 _bHdcpOff = 1;
#endif
u8 _bflagvideomute = 0;
u8 _bflagaudiomute = 0;

static u32 i4HdmiShareInfo[MAX_HDMI_SHAREINFO];
static u8 HDMI_AKSV[HDCP_AKSV_COUNT];
static u8 bKsv_buff[KSV_BUFF_SIZE];
static u8 bHdcpKeyBuff[HDCP_KEY_RESERVE];
static u8 _fgRepeater = FALSE;
static u8 _bReCompRiCount;
static u8 _bReAUTHCount;
static u8 _bReCheckReadyBit;
static u8 bSHABuff[20];
static HDMI_HDCP_KEY_T bhdcpkey = EXTERNAL_KEY;

extern size_t mt8193_TmrValue[MAX_HDMI_TMR_NUMBER];
extern size_t mt8193_hdmiCmd;

void vShowHdcpRawData(void)
{
	u16 bTemp, i, j, k;

	MT8193_HDCP_FUNC();

	pr_debug("==============================hdcpkey==============================\n");
	pr_debug("   | 00  01  02  03  04  05  06  07  08  09  0a  0b  0c  0d  0e  0f\n");
	pr_debug("===================================================================\n");
	for (bTemp = 0; bTemp < 3; bTemp++) {
		j = bTemp * 128;
		for (i = 0; i < 8; i++) {
			if (((i * 16) + j) < 0x10)
				pr_debug("0%x:  ", (i * 16) + j);
			else
				pr_debug("%x:  ", (i * 16) + j);

			for (k = 0; k < 16; k++) {
				if (k == 15) {
					if ((j + (i * 16 + k)) < 287)	/* for Buffer overflow error */
					{
						if (bHdcpKeyExternalBuff[j + (i * 16 + k)] > 0x0f)
							pr_debug("%2x\n",
								 bHdcpKeyExternalBuff[j +
										      (i * 16 +
										       k)]);
						else
							pr_debug("0%x\n",
								 bHdcpKeyExternalBuff[j +
										      (i * 16 +
										       k)]);
					}
				} else {
					if ((j + (i * 16 + k)) < 287)	/* for Buffer overflow error */
					{
						if (bHdcpKeyExternalBuff[j + (i * 16 + k)] > 0x0f)
							pr_debug("%2x  ",
								 bHdcpKeyExternalBuff[j +
										      (i * 16 +
										       k)]);
						else
							pr_debug("0%x  ",
								 bHdcpKeyExternalBuff[j +
										      (i * 16 +
										       k)]);
					} else {
						pr_debug("\n");
						pr_debug
						    ("===================================================================\n");
						return;
					}
				}
			}
		}
	}
}

void mt8193_hdcpkey(u8 *pbhdcpkey)
{
	u16 i;

	MT8193_HDCP_FUNC();

	for (i = 0; i < 287; i++) {
		bHdcpKeyExternalBuff[i] = *pbhdcpkey++;
	}

	vMoveHDCPInternalKey(EXTERNAL_KEY);
}

void vMoveHDCPInternalKey(HDMI_HDCP_KEY_T key)
{
	u8 *pbDramAddr;
	u16 i;

	MT8193_HDCP_FUNC();

	bhdcpkey = key;

	pbDramAddr = bHdcpKeyBuff;
	for (i = 0; i < 287; i++) {
		if (key == INTERNAL_ENCRYPT_KEY) {
			pbDramAddr[i] = HDCP_ENCRYPT_KEY[i];
		} else if (key == INTERNAL_NOENCRYPT_KEY) {
			pbDramAddr[i] = HDCP_NOENCRYPT_KEY[i];
		} else if (key == EXTERNAL_KEY) {
			pbDramAddr[i] = bHdcpKeyExternalBuff[i];
		}
	}
}

void vInitHdcpKeyGetMethod(u8 bMethod)
{
	MT8193_HDCP_FUNC();
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

u8 fgHostKey(void)
{
	u8 bTemp;

	MT8193_HDCP_FUNC();
	bTemp = bReadByteHdmiGRL(GRL_HDCP_CTL);
	vWriteByteHdmiGRL(GRL_HDCP_CTL, bTemp | HDCP_CTL_HOST_KEY);
	return TRUE;
}

u8 bReadHdmiIntMask(void)
{
	u8 bMask;
	MT8193_HDCP_FUNC();
	bMask = bReadByteHdmiGRL(GRL_INT_MASK);
	return bMask;

}

void vHalHDCPReset(void)
{
	u8 bTemp;
	MT8193_HDCP_FUNC();

	if (fgHostKey()) {
		bTemp = HDCP_CTL_CP_RSTB | HDCP_CTL_HOST_KEY;
	} else {
		bTemp = HDCP_CTL_CP_RSTB;
	}

	vWriteByteHdmiGRL(GRL_HDCP_CTL, bTemp);

	for (bTemp = 0; bTemp < 5; bTemp++) {
		udelay(255);
	}

	bTemp = bReadByteHdmiGRL(GRL_HDCP_CTL);
	bTemp &= (~HDCP_CTL_CP_RSTB);

	vWriteByteHdmiGRL(GRL_HDCP_CTL, bTemp);

	vSetCTL0BeZero(FALSE);
}

void vSetHDCPState(HDCP_CTRL_STATE_T e_state)
{
	MT8193_HDCP_FUNC();

	e_hdcp_ctrl_state = e_state;
}

void vHDCPReset(void)
{
	u8 bMask;
	MT8193_HDCP_FUNC();
	bMask = bReadHdmiIntMask();
	vWriteHdmiIntMask(0xff);	/* disable INT HDCP */

	vHalHDCPReset();
	vSetHDCPState(HDCP_RECEIVER_NOT_READY);
	vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
}

u8 fgIsHDCPCtrlTimeOut(void)
{
	MT8193_HDCP_FUNC();
	if (mt8193_TmrValue[HDMI_HDCP_PROTOCAL_CMD] <= 0)
		return TRUE;
	else
		return FALSE;
}

void vSendHdmiCmd(u8 u8cmd)
{
	MT8193_DRV_FUNC();
	mt8193_hdmiCmd = u8cmd;
}

void vClearHdmiCmd(void)
{
	MT8193_DRV_FUNC();
	mt8193_hdmiCmd = 0xff;
}

void vSetHDCPTimeOut(u32 i4_count)
{
	MT8193_HDCP_FUNC();
	mt8193_TmrValue[HDMI_HDCP_PROTOCAL_CMD] = i4_count;
}

u32 i4SharedInfo(u32 u4Index)
{
	MT8193_HDCP_FUNC();
	return i4HdmiShareInfo[u4Index];
}

void vSetSharedInfo(u32 u4Index, u32 i4Value)
{
	MT8193_DRV_FUNC();
	i4HdmiShareInfo[u4Index] = i4Value;
}

void vMiAnUpdateOrFix(u8 bUpdate)
{
	u8 bTemp;
	MT8193_HDCP_FUNC();
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

void vReadAksvFromReg(BYTE *PrBuff)
{
	u8 bTemp, i;
	MT8193_HDCP_FUNC();
	for (i = 0; i < 5; i++)	/* AKSV count 5 bytes */
	{
		bTemp = bReadByteHdmiGRL(GRL_RD_AKSV0 + i * 4);
		*(PrBuff + i) = bTemp;
	}
}

void vWriteAksvKeyMask(u8 *PrData)
{
	u8 bData;
	/* - write wIdx into 92. */
	MT8193_HDCP_FUNC();
	bData = (*(PrData + 2) & 0x0f) | ((*(PrData + 3) & 0x0f) << 4);

	vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, (bData << 16), SYS_KEYMASK2);
	bData = (*(PrData + 0) & 0x0f) | ((*(PrData + 1) & 0x0f) << 4);

	vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, (bData << 8), SYS_KEYMASK1);
}

void vEnableAuthHardware(void)
{
	u8 bTemp;
	MT8193_HDCP_FUNC();
	bTemp = bReadByteHdmiGRL(GRL_HDCP_CTL);
	bTemp |= HDCP_CTL_AUTHEN_EN;

	vWriteByteHdmiGRL(GRL_HDCP_CTL, bTemp);

}

u8 fgIsRepeater(void)
{
	MT8193_HDCP_FUNC();
	return (_fgRepeater == TRUE);
}

void vRepeaterOnOff(u8 fgIsRep)
{
	u8 bTemp;
	MT8193_HDCP_FUNC();
	bTemp = bReadByteHdmiGRL(GRL_HDCP_CTL);

	if (fgIsRep == TRUE) {
		bTemp |= HDCP_CTRL_RX_RPTR;
	} else {
		bTemp &= ~HDCP_CTRL_RX_RPTR;
	}

	vWriteByteHdmiGRL(GRL_HDCP_CTL, bTemp);

}

void vStopAn(void)
{
	u8 bTemp;
	MT8193_HDCP_FUNC();
	bTemp = bReadByteHdmiGRL(GRL_HDCP_CTL);
	bTemp |= HDCP_CTL_AN_STOP;
	vWriteByteHdmiGRL(GRL_HDCP_CTL, bTemp);

}

void bReadDataHdmiGRL(u8 bAddr, u8 bCount, u8 *bVal)
{
	u8 i;
	MT8193_HDCP_FUNC();
	for (i = 0; i < bCount; i++)
		*(bVal + i) = bReadByteHdmiGRL(bAddr + i * 4);
}

void vWriteDataHdmiGRL(u8 bAddr, u8 bCount, u8 *bVal)
{
	u8 i;
	MT8193_HDCP_FUNC();
	for (i = 0; i < bCount; i++)
		vWriteByteHdmiGRL(bAddr + i * 4, *(bVal + i));
}

void vSendAn(void)
{
	u8 bHDCPBuf[HDCP_AN_COUNT];
	MT8193_HDCP_FUNC();
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
	u8 bHDCPBuf[HDCP_AKSV_COUNT];
	MT8193_HDCP_FUNC();
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

}

void vHalSendAKey(u8 bData)
{
	MT8193_HDCP_FUNC();
	vWriteByteHdmiGRL(GRL_KEY_PORT, bData);
}

void vSendAKey(u8 *prAKey)
{
	u8 bData;
	u16 ui2Index;
	MT8193_HDCP_FUNC();
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

void bClearGRLInt(BYTE bInt)
{
	MT8193_DRV_FUNC();
	vWriteByteHdmiGRL(GRL_INT, bInt);
}

u8 bReadGRLInt(void)
{
	u8 bStatus;
	MT8193_DRV_FUNC();

	bStatus = bReadByteHdmiGRL(GRL_INT);

	return bStatus;
}

u8 bCheckHDCPStatus(u8 bMode)
{
	u8 bStatus = 0;
	MT8193_HDCP_FUNC();
	bStatus = bReadByteHdmiGRL(GRL_HDCP_STA);

	bStatus &= bMode;
	if (bStatus) {
		vWriteByteHdmiGRL(GRL_HDCP_STA, bMode);
		return TRUE;
	} else {
		return FALSE;
	}
}

u8 fgCompareRi(void)
{
	u8 bTemp;
	u8 bHDCPBuf[4];
	MT8193_HDCP_FUNC();
	/* Read R0/ Ri from Transmitter */
	/* fgI2CDataRead(HDMI_DEV_GRL, GRL_RI_0, HDCP_RI_COUNT, bHDCPBuf+HDCP_RI_COUNT); */
	bReadDataHdmiGRL(GRL_RI_0, HDCP_RI_COUNT, &bHDCPBuf[HDCP_RI_COUNT]);

	/* Read R0'/ Ri' from Receiver */
	fgDDCDataRead(RX_ID, RX_REG_RI, HDCP_RI_COUNT, bHDCPBuf);

	MT8193_HDCP_LOG("bHDCPBuf[0]=0x%x,bHDCPBuf[1]=0x%x,bHDCPBuf[2]=0x%x,bHDCPBuf[3]=0x%x\n",
			bHDCPBuf[0], bHDCPBuf[1], bHDCPBuf[2], bHDCPBuf[3]);
	/* compare R0 and R0' */
	for (bTemp = 0; bTemp < HDCP_RI_COUNT; bTemp++) {
		if (bHDCPBuf[bTemp] == bHDCPBuf[bTemp + HDCP_RI_COUNT]) {
			continue;
		} else {
			break;
		}
	}

	if (bTemp == HDCP_RI_COUNT) {

		return TRUE;
	} else {

		return FALSE;
	}

}

void vEnableEncrpt(void)
{
	u8 bTemp;
	MT8193_HDCP_FUNC();
	bTemp = bReadByteHdmiGRL(GRL_HDCP_CTL);
	bTemp |= HDCP_CTL_ENC_EN;
	vWriteByteHdmiGRL(GRL_HDCP_CTL, bTemp);

}

void vHalWriteKsvListPort(u8 *prKsvData, u8 bDevice_Count, u8 *prBstatus)
{
	u8 bIndex;
	MT8193_HDCP_FUNC();
	if ((bDevice_Count * 5) < KSV_BUFF_SIZE) {
		for (bIndex = 0; bIndex < (bDevice_Count * 5); bIndex++) {
			vWriteByteHdmiGRL(GRL_KSVLIST, *(prKsvData + bIndex));
		}

		for (bIndex = 0; bIndex < 2; bIndex++) {
			vWriteByteHdmiGRL(GRL_KSVLIST, *(prBstatus + bIndex));
		}
	}

}

void vHalWriteHashPort(u8 *prHashVBuff)
{
	u8 bIndex;
	MT8193_HDCP_FUNC();
	for (bIndex = 0; bIndex < 20; bIndex++) {
		vWriteByteHdmiGRL(GRL_REPEATER_HASH + bIndex * 4, *(prHashVBuff + bIndex));
	}

}

void vEnableHashHardwrae(void)
{
	u8 bData;
	MT8193_HDCP_FUNC();
	bData = bReadByteHdmiGRL(GRL_HDCP_CTL);
	bData |= HDCP_CTL_SHA_EN;
	vWriteByteHdmiGRL(GRL_HDCP_CTL, bData);
}

void vReadKSVFIFO(void)
{
	u8 bTemp, bIndex, bDevice_Count;	/* , bBlock; */
	u8 bStatus[2], bBstatus1;
	MT8193_HDCP_FUNC();
	fgDDCDataRead(RX_ID, RX_REG_BSTATUS1 + 1, 1, &bBstatus1);
	fgDDCDataRead(RX_ID, RX_REG_BSTATUS1, 1, &bDevice_Count);

	bDevice_Count &= DEVICE_COUNT_MASK;

	if ((bDevice_Count & MAX_DEVS_EXCEEDED) || (bBstatus1 & MAX_CASCADE_EXCEEDED)) {
		vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
		vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		return;
	}

	if (bDevice_Count > 32) {
		for (bTemp = 0; bTemp < 2; bTemp++)	/* retry 1 times */
		{
			fgDDCDataRead(RX_ID, RX_REG_BSTATUS1, 1, &bDevice_Count);
			bDevice_Count &= DEVICE_COUNT_MASK;
			if (bDevice_Count <= 32)
				break;
		}
		if (bTemp == 2) {
			bDevice_Count = 32;
		}
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

	if ((bDevice_Count * 5) < KSV_BUFF_SIZE)
		vHalWriteKsvListPort(bKsv_buff, bDevice_Count, bStatus);
	vHalWriteHashPort(bSHABuff);
	vEnableHashHardwrae();
	vSetHDCPState(HDCP_COMPARE_V);
	/* set time-out value as 0.5 sec */
	vSetHDCPTimeOut(HDCP_WAIT_V_RDY_TIMEOUE);

}

u8 bReadHDCPStatus(void)
{
	u8 bTemp;
	MT8193_HDCP_FUNC();
	bTemp = bReadByteHdmiGRL(GRL_HDCP_STA);

	return bTemp;
}

void vHDCPInitAuth(void)
{
	MT8193_HDCP_FUNC();
	vSetHDCPTimeOut(HDCP_WAIT_RES_CHG_OK_TIMEOUE);	/* 100 ms */
	vSetHDCPState(HDCP_WAIT_RES_CHG_OK);
}

void vDisableHDCP(u8 fgDisableHdcp)
{
	MT8193_HDCP_FUNC();

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
		vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
		vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);

		_bHdcpOff = 0;
	}

}


void mt8193_mutehdmi(u8 u1flagvideomute, u8 u1flagaudiomute)
{
	MT8193_HDCP_LOG("u1flagvideomute = %d, u1flagaudiomute = %d\n", u1flagvideomute,
			u1flagaudiomute);
	_bflagvideomute = u1flagvideomute;
	_bflagaudiomute = u1flagaudiomute;

	if (_bHdcpOff == 1) {
		if (u1flagvideomute == TRUE) {
			vBlackHDMIOnly();
		} else {
			vUnBlackHDMIOnly();
		}

		if (u1flagaudiomute == TRUE) {
			MuteHDMIAudio();
		} else {
			UnMuteHDMIAudio();
		}
	}
}

void HdcpService(HDCP_CTRL_STATE_T e_hdcp_state)
{
	u8 bIndx, bTemp;
	u8 bMask;

	MT8193_HDCP_FUNC();

	if (_bHdcpOff == 1) {
		vSetHDCPState(HDCP_RECEIVER_NOT_READY);
		vHDMIAVUnMute();
		vWriteHdmiIntMask(0xff);
	}

	switch (e_hdcp_state) {
	case HDCP_RECEIVER_NOT_READY:
		MT8193_HDCP_LOG("HDCP_RECEIVER_NOT_READY\n");
		break;

	case HDCP_READ_EDID:
		break;

	case HDCP_WAIT_RES_CHG_OK:
		MT8193_HDCP_LOG("HDCP_WAIT_RES_CHG_OK\n");
		_bReAUTHCount = 0;
		if (fgIsHDCPCtrlTimeOut()) {
			if (_bHdcpOff == 1)	/* disable HDCP */
			{
				vSetHDCPState(HDCP_RECEIVER_NOT_READY);
				vHDMIAVUnMute();
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			} else {
				vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			}
		}

		break;


	case HDCP_INIT_AUTHENTICATION:
		MT8193_HDCP_LOG("HDCP_INIT_AUTHENTICATION\n");

		_bReAUTHCount++;

		vHDMIAVMute();
		vSetSharedInfo(SI_HDMI_HDCP_RESULT, 0);

		if (!fgDDCDataRead(RX_ID, RX_REG_BCAPS, 1, &bTemp)) {
			vSetHDCPTimeOut(HDCP_WAIT_300MS_TIMEOUT);
			break;
		}

		vMiAnUpdateOrFix(TRUE);

		if (fgHostKey()) {
			for (bIndx = 0; bIndx < HDCP_AKSV_COUNT; bIndx++) {
				HDMI_AKSV[bIndx] = bHdcpKeyBuff[1 + bIndx];
			}

			if ((_bReAUTHCount > 2)
			    || ((HDMI_AKSV[0] == 0) && (HDMI_AKSV[1] == 0) && (HDMI_AKSV[2] == 0)
				&& (HDMI_AKSV[3] == 0))) {
				vSetHDCPState(HDCP_RECEIVER_NOT_READY);
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
				_bReAUTHCount = 0;
				return;
			}
		} else {
			vReadAksvFromReg(&HDMI_AKSV[0]);
		}

		if ((bhdcpkey == INTERNAL_ENCRYPT_KEY) || (bhdcpkey == EXTERNAL_KEY))
			vWriteAksvKeyMask(&HDMI_AKSV[0]);

		vEnableAuthHardware();
		fgDDCDataRead(RX_ID, RX_REG_BCAPS, 1, &bTemp);
		vSetSharedInfo(SI_REPEATER_DEVICE_COUNT, 0);
		if (bTemp & RX_BIT_ADDR_RPTR) {
			_fgRepeater = TRUE;
		} else {
			_fgRepeater = FALSE;
		}

		if (fgIsRepeater()) {
			vRepeaterOnOff(TRUE);
		} else {
			vRepeaterOnOff(FALSE);
		}

		vSendAn();

		vExchangeKSVs();

		if (fgHostKey()) {
			vSendAKey(&bHdcpKeyBuff[6]);	/* around 190msec */
			vSetHDCPTimeOut(HDCP_WAIT_R0_TIMEOUT);
		} else {
			vSetHDCPTimeOut(HDCP_WAIT_R0_TIMEOUT);	/* 100 ms */
		}

		/* change state as waiting R0 */
		vSetHDCPState(HDCP_WAIT_R0);
		break;


	case HDCP_WAIT_R0:
		MT8193_HDCP_LOG("HDCP_WAIT_R0\n");
		bTemp = bCheckHDCPStatus(HDCP_STA_RI_RDY);
		if (bTemp == TRUE) {
			vSetHDCPState(HDCP_COMPARE_R0);
		} else {
			vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			break;
		}

	case HDCP_COMPARE_R0:
		MT8193_HDCP_LOG("HDCP_COMPARE_R0\n");
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
			_bReCompRiCount = 0;
		}
		break;

	case HDCP_RE_COMPARE_R0:
		MT8193_HDCP_LOG("HDCP_RE_COMPARE_R0\n");
		_bReCompRiCount++;
		if (fgIsHDCPCtrlTimeOut() && _bReCompRiCount > 3) {
			vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			_bReCompRiCount = 0;
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
		MT8193_HDCP_LOG("HDCP_CHECK_REPEATER\n");
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
		MT8193_HDCP_LOG("HDCP_WAIT_KSV_LIST\n");
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
		MT8193_HDCP_LOG("HDCP_READ_KSV_LIST\n");
		vReadKSVFIFO();
		break;

	case HDCP_COMPARE_V:
		MT8193_HDCP_LOG("HDCP_COMPARE_V\n");
		bTemp = bReadHDCPStatus();
		if ((bTemp & HDCP_STA_V_MATCH) || (bTemp & HDCP_STA_V_RDY)) {
			if ((bTemp & HDCP_STA_V_MATCH))	/* for Simplay #7-20-5 */
			{
				vSetHDCPState(HDCP_WAIT_RI);
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
				vSetSharedInfo(SI_HDMI_HDCP_RESULT, (i4SharedInfo(SI_HDMI_HDCP_RESULT) | 0x02));	/* step 2 OK. */
			} else {
				vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			}
		}
		break;

	case HDCP_WAIT_RI:
		MT8193_HDCP_LOG("HDCP_WAIT_RI\n");
		vHDMIAVUnMute();

		bMask = bReadHdmiIntMask();
		vWriteHdmiIntMask(0xfd);
		break;

	case HDCP_CHECK_LINK_INTEGRITY:
		MT8193_HDCP_LOG("HDCP_CHECK_LINK_INTEGRITY\n");
		if (fgCompareRi() == TRUE) {
			vSetSharedInfo(SI_HDMI_HDCP_RESULT, (i4SharedInfo(SI_HDMI_HDCP_RESULT) | 0x04));	/* step 3 OK. */
			if (fgIsRepeater()) {
				if (i4SharedInfo(SI_HDMI_HDCP_RESULT) == 0x07)	/* step 1, 2, 3. */
				{
					vSetSharedInfo(SI_HDMI_HDCP_RESULT, (i4SharedInfo(SI_HDMI_HDCP_RESULT) | 0x08));	/* all ok. */
				}
			} else	/* not repeater, don't need step 2. */
			{
				if (i4SharedInfo(SI_HDMI_HDCP_RESULT) == 0x05)	/* step 1, 3. */
				{
					vSetSharedInfo(SI_HDMI_HDCP_RESULT, (i4SharedInfo(SI_HDMI_HDCP_RESULT) | 0x08));	/* all ok. */
				}
			}
		} else {
			bMask = bReadHdmiIntMask();
			vWriteHdmiIntMask(0xff);	/* disable INT HDCP */
			_bReCompRiCount = 0;
			vSetHDCPState(HDCP_RE_COMPARE_RI);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		}
		break;

	case HDCP_RE_COMPARE_RI:
		MT8193_HDCP_LOG("HDCP_RE_COMPARE_RI\n");
		_bReCompRiCount++;
		if (_bReCompRiCount > 5) {
			vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			_bReCompRiCount = 0;
		} else {
			if (fgCompareRi() == TRUE) {
				_bReCompRiCount = 0;
				vSetHDCPState(HDCP_CHECK_LINK_INTEGRITY);
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
				vSetSharedInfo(SI_HDMI_HDCP_RESULT, (i4SharedInfo(SI_HDMI_HDCP_RESULT) | 0x04));	/* step 3 OK. */
				if (fgIsRepeater()) {
					if (i4SharedInfo(SI_HDMI_HDCP_RESULT) == 0x07)	/* step 1, 2, 3. */
					{
						vSetSharedInfo(SI_HDMI_HDCP_RESULT, (i4SharedInfo(SI_HDMI_HDCP_RESULT) | 0x08));	/* all ok. */
					}
				} else {
					if (i4SharedInfo(SI_HDMI_HDCP_RESULT) == 0x05)	/* step 1, 3. */
					{
						vSetSharedInfo(SI_HDMI_HDCP_RESULT, (i4SharedInfo(SI_HDMI_HDCP_RESULT) | 0x08));	/* all ok. */
					}
				}

				bMask = bReadHdmiIntMask();
				vWriteHdmiIntMask(0xfd);
			} else {
				vSetHDCPState(HDCP_RE_COMPARE_RI);
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			}
		}
		break;

	case HDCP_RE_DO_AUTHENTICATION:
		MT8193_HDCP_LOG("HDCP_RE_DO_AUTHENTICATION\n");
		vHDMIAVMute();
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
		MT8193_HDCP_LOG("HDCP_WAIT_RESET_OK\n");
		if (fgIsHDCPCtrlTimeOut()) {
			vSetHDCPState(HDCP_INIT_AUTHENTICATION);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		}
		break;

	default:
		break;
	}
}
#endif
