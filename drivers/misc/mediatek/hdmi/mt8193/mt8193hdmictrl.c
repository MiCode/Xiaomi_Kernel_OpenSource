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

#ifdef HDMI_MT8193_SUPPORT
#include "mt8193_ctrl.h"
#include "mt8193hdmictrl.h"
#include "mt8193_iic.h"
#include "mt8193hdcp.h"
#include "mt8193edid.h"
#include "mt8193cec.h"
HDMI_AV_INFO_T _stAvdAVInfo = { 0 };

static unsigned char _bAudInfoFm[5];
static unsigned char _bAviInfoFm[5];
static unsigned char _bSpdInf[25] = { 0 };

static unsigned char _bAcpType;
static unsigned char _bAcpData[16] = { 0 };
static unsigned char _bIsrc1Data[16] = { 0 };

static unsigned int _u4NValue;

unsigned char is_user_mute_hdmi_audio = 0;

static const char *szHdmiResStr[HDMI_VIDEO_RESOLUTION_NUM] = {
	"RES_480P",
	"RES_576P",
	"RES_720P60HZ",
	"RES_720P50HZ",
	"RES_1080I60HZ",
	"RES_1080I50HZ",
	"RES_1080P30HZ",
	"RES_1080P25HZ",
	"RES_1080P24HZ",
	"RES_1080P23_976HZ",
	"RES_1080P29_97HZ",
	"RES_1080P60HZ",
	"RES_1080P50HZ",
};

static const char *cHdmiAudFsStr[7] = {
	"HDMI_FS_32K",
	"HDMI_FS_44K",
	"HDMI_FS_48K",
	"HDMI_FS_88K",
	"HDMI_FS_96K",
	"HDMI_FS_176K",
	"HDMI_FS_192K"
};


static const char *cAudCodingTypeStr[16] = {
	"Refer to Stream Header",
	"PCM",
	"AC3",
	"MPEG1",
	"MP3",
	"MPEG2",
	"AAC",
	"DTS",
	"ATRAC",
	"ONE Bit Audio",
	"Dolby Digital+",
	"DTS-HD",
	"MAT(MLP)",
	"DST",
	"WMA Pro",
	"Reserved",
};

static const char *cAudChCountStr[8] = {
	"Refer to Stream Header",
	"2ch",
	"3ch",
	"4ch",
	"5ch",
	"6ch",
	"7ch",
	"8ch",

};

static const char *cAudFsStr[8] = {
	"Refer to Stream Header",
	"32 khz",
	"44.1 khz",
	"48 khz",
	"88.2 khz",
	"96 khz",
	"176.4 khz",
	"192 khz"
};


static const char *cAudChMapStr[32] = {
	"FR,FL",
	"LFE,FR,FL",
	"FC,FR,FL",
	"FC,LFE,FR,FL",
	"RC,FR,FL",
	"RC,LFE,FR,FL",
	"RC,FC,FR,FL",
	"RC,FC,LFE,FR,FL",
	"RR,RL,FR,FL",
	"RR,RL,LFE,FR,FL",
	"RR,RL,FC,FR,FL",
	"RR,RL,FC,LFE,FR,FL",
	"RC,RR,RL,FR,FL",
	"RC,RR,RL,LFE,FR,FL",
	"RC,RR,RL,FC,FR,FL",
	"RC,RR,RL,FC,LFE,FR,FL",
	"RRC,RLC,RR,RL,FR,FL",
	"RRC,RLC,RR,RL,LFE,FR,FL",
	"RRC,RLC,RR,RL,FC,FR,FL",
	"RRC,RLC,RR,RL,FC,LFE,FR,FL",
	"FRC,FLC,FR,FL",
	"FRC,FLC,LFE,FR,FL",
	"FRC,FLC,FC,FR,FL",
	"FRC,FLC,FC,LFE,FR,FL",
	"FRC,FLC,RC,FR,FL",
	"FRC,FLC,RC,LFE,FR,FL",
	"FRC,FLC,RC,FC,FR,FL",
	"FRC,FLC,RC,FC,LFE,FR,FL",
	"FRC,FLC,RR,RL,FR,FL",
	"FRC,FLC,RR,RL,LFE,FR,FL",
	"FRC,FLC,RR,RL,FC,FR,FL",
	"FRC,FLC,RR,RL,FC,LFE,FR,FL",
};

static const char *cAudDMINHStr[2] = {
	"Permiited down mixed stereo or no information",
	"Prohibited down mixed stereo"
};

static const char *cAudSampleSizeStr[4] = {
	"Refer to Stream Header",
	"16 bit",
	"20 bit",
	"24 bit"
};

static const char *cAviRgbYcbcrStr[4] = {
	"RGB",
	"YCbCr 4:2:2",
	"YCbCr 4:4:4",
	"Future"
};

static const char *cAviActivePresentStr[2] = {
	"No data",
	"Actuve Format(R0..R3) Valid",

};

static const char *cAviBarStr[4] = {
	"Bar data not valid",
	"Vert. Bar info valid",
	"Horiz. Bar info valid",
	"Vert. and Horiz Bar info valid",
};

static const char *cAviScanStr[4] = {
	"No data",
	"Overscanned display",
	"underscanned display",
	"Future",
};

static const char *cAviColorimetryStr[4] = {
	"no data",
	"ITU601",
	"ITU709",
	"Extended Colorimetry infor valid",
};

static const char *cAviAspectStr[4] = {
	"No data",
	"4:3",
	"16:9",
	"Future",
};


static const char *cAviActiveStr[16] = {
	"reserved",
	"reserved",
	"box 16:9(top)",
	"box 14:9(top)",
	"box > 16:9(center)",
	"reserved",
	"reserved",
	"reserved",
	"Same as picture aspect ratio",
	"4:3(Center)",
	"16:9(Center)",
	"14:9(Center)",
	"reserved",
	"4:3(with shoot & protect 14:9 center)",
	"16:9(with shoot & protect 14:9 center)",
	"16:3(with shoot & protect 4:3 center)"
};

static const char *cAviItContentStr[2] = {
	"no data",
	"IT Content"
};

static const char *cAviExtColorimetryStr[2] = {
	"xvYCC601",
	"xvYCC709",
};

static const char *cAviRGBRangeStr[4] = {
	"depends on video format",
	"Limit range",
	"FULL range",
	"Reserved",
};

static const char *cAviScaleStr[4] = {
	"Unknown non-uniform scaling",
	"Picture has been scaled horizontally",
	"Picture has been scaled vertically",
	"Picture has been scaled horizontally and vertically",
};



static const char *cSPDDeviceStr[16] = {
	"unknown",
	"Digital STB",
	"DVD Player",
	"D-VHS",
	"HDD Videorecorder",
	"DVC",
	"DSC",
	"Video CD",
	"Game",
	"PC General",
	"Blu-Ray Disc",
	"Super Audio CD",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
};

/* //////////////////////////////////////////// */
unsigned int mt8193_hdmidrv_read(unsigned short u2Reg)
{
	unsigned int u4Data;

	mt8193_i2c_read(HDMIDRV_BASE + u2Reg, &u4Data);
	/* MT8193_DRV_LOG("[R]addr = 0x%04x, data = 0x%08x\n", u2Reg, u4Data); */
	return u4Data;
}

void mt8193_hdmidrv_write(unsigned short u2Reg, unsigned int u4Data)
{
	/* MT8193_DRV_LOG("[W]addr = 0x%04x, data = 0x%08x\n", u2Reg, u4Data); */
	mt8193_i2c_write(HDMIDRV_BASE + u2Reg, u4Data);
}

/* /////////////////////////////////////////////// */
unsigned int mt8193_hdmisys_read(unsigned short u2Reg)
{
	unsigned int u4Data;

	mt8193_i2c_read(HDMISYS_BASE + u2Reg, &u4Data);
	/* MT8193_PLL_LOG("[R]addr = 0x%04x, data = 0x%08x\n", u2Reg, u4Data); */
	return u4Data;
}

void mt8193_hdmisys_write(unsigned short u2Reg, unsigned int u4Data)
{
	/* MT8193_PLL_LOG("[W]addr= 0x%04x, data = 0x%08x\n", u2Reg, u4Data); */
	mt8193_i2c_write(HDMISYS_BASE + u2Reg, u4Data);
}

/* /////////////////////////////////////////////////// */
unsigned int mt8193_hdmidgi_read(unsigned short u2Reg)
{
	unsigned int u4Data;

	mt8193_i2c_read(HDMIDGI_BASE + u2Reg, &u4Data);
	/* MT8193_DGI_LOG("[R]addr= 0x%04x, data = 0x%08x\n", u2Reg, u4Data); */
	return u4Data;
}

void mt8193_hdmidgi_write(unsigned short u2Reg, unsigned int u4Data)
{
	/* MT8193_DGI_LOG("[W]addr= 0x%04x, data = 0x%08x\n", u2Reg, u4Data); */
	mt8193_i2c_write(HDMIDGI_BASE + u2Reg, u4Data);
}

/* ///////////////////////////////////////////////////// */
unsigned int mt8193_pad_read(unsigned short u2Reg)
{
	unsigned int u4Data;

	mt8193_i2c_read(HDMIPAD_BASE + u2Reg, &u4Data);
	return u4Data;
}

void mt8193_pad_write(unsigned short u2Reg, unsigned int u4Data)
{
	mt8193_i2c_write(HDMIPAD_BASE + u2Reg, u4Data);
}

/* ///////////////////////////////////////////////////// */

void vHotPlugPinInit(void)
{
	vWriteIoPadMsk(IO_PAD_PD, IO_PAD_HOT_PLUG_PD, IO_PAD_HOT_PLUG_PD);
}

unsigned char fgIsAcpEnable(void)
{
	if (bReadByteHdmiGRL(GRL_ACP_ISRC_CTRL) & ACP_EN)
		return TRUE;
	else
		return FALSE;
}

unsigned char fgIsVSEnable(void)
{
	if (bReadByteHdmiGRL(GRL_ACP_ISRC_CTRL) & VS_EN)
		return TRUE;
	else
		return FALSE;
}

unsigned char fgIsISRC1Enable(void)
{
	if (bReadByteHdmiGRL(GRL_ACP_ISRC_CTRL) & ISRC1_EN)
		return TRUE;
	else
		return FALSE;
}


unsigned char fgIsISRC2Enable(void)
{
	if (bReadByteHdmiGRL(GRL_ACP_ISRC_CTRL) & ISRC2_EN)
		return TRUE;
	else
		return FALSE;
}

unsigned char vIsDviMode(void)
{
	unsigned char bData;

	bData = bReadByteHdmiGRL(GRL_CFG1);
	if (bData & CFG1_DVI)
		return TRUE;
	else
		return FALSE;
}

unsigned char bCheckStatus(unsigned char bMode)
{
	unsigned char bStatus = 0;

	bStatus = bReadByteHdmiGRL(GRL_STATUS);

	if ((bStatus & bMode) == bMode)
		return TRUE;
	else
		return FALSE;

}

unsigned char bCheckPordHotPlug(unsigned char bMode)
{
	unsigned char bStatus = FALSE;

	if (bMode == (PORD_MODE | HOTPLUG_MODE))
		bStatus = bCheckStatus(STATUS_PORD | STATUS_HTPLG);
	else if (bMode == HOTPLUG_MODE)
		bStatus = bCheckStatus(STATUS_HTPLG);
	else if (bMode == PORD_MODE)
		bStatus = bCheckStatus(STATUS_PORD);


	return bStatus;

}

unsigned char hdmi_port_status(void)
{
	return bReadByteHdmiGRL(GRL_STATUS) & (PORD_MODE | HOTPLUG_MODE);
}

void MuteHDMIAudio(void)
{
	unsigned char bData;

	MT8193_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
	bData |= AUDIO_ZERO;
	vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
}

void vBlackHDMIOnly(void)
{
	MT8193_DRV_FUNC();
}

void vHDMIAVMute(void)
{
	MT8193_AUDIO_FUNC();

	vBlackHDMIOnly();
	MuteHDMIAudio();
}

void vTxSignalOnOff(unsigned char bOn)
{
	unsigned char bData1;

	MT8193_PLL_FUNC();

	bData1 = bReadByteHdmiGRL(GRL_INT) & INT_MDI;

	if (bOn) {
		HDMI_DEF_LOG("[hdmi]tmds on\n");
		vWriteHdmiSYSMsk(HDMI_SYS_AMPCTRL, 0, RG_SET_DTXST);
		/* the 5ms delay time after pll setting , resolve CTS 7-6 can't find trigger and result fail */
		mdelay(5);

		vWriteHdmiSYSMsk(HDMI_SYS_PLLCTRL6, 0, ABIST_MODE_SET_MSK);
		vWriteHdmiSYSMsk(HDMI_SYS_PLLCTRL6, 0, ABIST_MODE_EN | ABIST_LV_EN);

		udelay(20);
		vWriteHdmiSYSMsk(HDMI_SYS_PLLCTRL2, 0, POW_HDMITX | POW_PLL_L);
		udelay(20);
		vWriteHdmiSYSMsk(HDMI_SYS_PLLCTRL2, POW_HDMITX, POW_HDMITX | POW_PLL_L);
		udelay(20);
		vWriteHdmiSYSMsk(HDMI_SYS_PLLCTRL2, POW_HDMITX | POW_PLL_L, POW_HDMITX | POW_PLL_L);
	} else {
		HDMI_DEF_LOG("[hdmi]tmds off\n");
		vWriteHdmiSYSMsk(HDMI_SYS_AMPCTRL, 0, RG_SET_DTXST);
		vWriteHdmiSYSMsk(HDMI_SYS_PLLCTRL1, 0, RG_ENCKST);
		vWriteHdmiSYSMsk(HDMI_SYS_PLLCTRL6, ABIST_MODE_SET, ABIST_MODE_SET_MSK);
		vWriteHdmiSYSMsk(HDMI_SYS_PLLCTRL6, ABIST_MODE_EN | ABIST_LV_EN,
				 ABIST_MODE_EN | ABIST_LV_EN);
		vWriteHdmiSYSMsk(HDMI_SYS_PLLCTRL7, 0, TX_DRV_ENABLE_MSK);
	}

	if (bData1 != INT_MDI)
		bClearGRLInt(INT_MDI);
}

void vSetCTL0BeZero(unsigned char fgBeZero)
{
	unsigned char bTemp;

	MT8193_VIDEO_FUNC();

	if (fgBeZero == TRUE) {
		bTemp = bReadByteHdmiGRL(GRL_CFG1);
		bTemp |= (1 << 4);
		vWriteByteHdmiGRL(GRL_CFG1, bTemp);
	} else {
		bTemp = bReadByteHdmiGRL(GRL_CFG1);
		bTemp &= ~(1 << 4);
		vWriteByteHdmiGRL(GRL_CFG1, bTemp);
	}

}

void vWriteHdmiIntMask(unsigned char bMask)
{
	MT8193_DRV_FUNC();

	vWriteByteHdmiGRL(GRL_INT_MASK, bMask);	/* INT mask */

}

void vUnBlackHDMIOnly(void)
{
	HDMI_DEF_LOG("[hdmi]vid unmute\n");
}

void UnMuteHDMIAudio(void)
{
	unsigned char bData;

	HDMI_DEF_LOG("[hdmi]aud unmute\n");
	bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
	bData &= ~AUDIO_ZERO;
	vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
}

void vTmdsOnOffAndResetHdcp(unsigned char fgHdmiTmdsEnable)
{
	MT8193_DRV_FUNC();

	if (fgHdmiTmdsEnable == 1) {
		mdelay(10);
		vTxSignalOnOff(SV_ON);
	} else {
		vHDMIAVMute();
		mdelay(2);
		vTxSignalOnOff(SV_OFF);
		vHDCPReset();
		mdelay(10);
	}
}

void vVideoPLLInit(void)
{
	MT8193_DRV_FUNC();
	/* init analog part */
	vWriteHdmiSYS(HDMI_SYS_AMPCTRL, HDMI_ANL_INIT[0]);
	vWriteHdmiSYS(HDMI_SYS_AMPCTRL1, HDMI_ANL_INIT[1]);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL1, HDMI_ANL_INIT[2]);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL2, HDMI_ANL_INIT[3]);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL3, HDMI_ANL_INIT[4]);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL4, HDMI_ANL_INIT[5]);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL5, HDMI_ANL_INIT[6]);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL5, HDMI_ANL_INIT[7]);

}

void vVideoPLLInit_1080P_36BIT(void)
{
	MT8193_DRV_FUNC();
	/* init analog part */
	vWriteHdmiSYS(HDMI_SYS_AMPCTRL, HDMI_ANL_INIT_1080P_36BIT[0]);
	vWriteHdmiSYS(HDMI_SYS_AMPCTRL1, HDMI_ANL_INIT_1080P_36BIT[1]);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL1, HDMI_ANL_INIT_1080P_36BIT[2]);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL2, HDMI_ANL_INIT_1080P_36BIT[3]);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL3, HDMI_ANL_INIT_1080P_36BIT[4]);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL4, HDMI_ANL_INIT_1080P_36BIT[5]);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL5, HDMI_ANL_INIT_1080P_36BIT[6]);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL5, HDMI_ANL_INIT_1080P_36BIT[7]);
}

void vSetHDMITxPLL(unsigned char bResIndex, unsigned char bdeepmode)
{
	unsigned char u4Feq = 0;

	MT8193_PLL_FUNC();

	vWriteHdmiDGIMsk(dgi0_anaif_ctrl1, dgi1_pad_clk_en,
			 anaif_dig1_clk_sel | dgi1_pad_clk_en | clk_sel_tv_mode | data_in_tv_mode |
			 dgi1_clk_pad_sel_tv_mode | tv_mode_clk_en);

	if ((bResIndex == HDMI_VIDEO_720x480p_60Hz) || (bResIndex == HDMI_VIDEO_720x576p_50Hz))
		u4Feq = 0;	/* 27M */
	else if ((bResIndex == HDMI_VIDEO_1920x1080p_60Hz)
		 || (bResIndex == HDMI_VIDEO_1920x1080p_50Hz))
		u4Feq = 2;	/* 148M */
	else
		u4Feq = 1;	/* 74M */

	if (((bResIndex == HDMI_VIDEO_1920x1080p_60Hz) || (bResIndex == HDMI_VIDEO_1920x1080p_50Hz))
	    && (bdeepmode == HDMI_DEEP_COLOR_12_BIT))
		vVideoPLLInit_1080P_36BIT();
	else
		vVideoPLLInit();

	udelay(10);
	if (bdeepmode == HDMI_NO_DEEP_COLOR) {
		vWriteHdmiSYSMsk(HDMI_SYS_PLLCTRL2, (HDMI_PLL_SETTING[u4Feq][0]), 0xFF);
		vWriteHdmiSYS(HDMI_SYS_PLLCTRL3,
			      (HDMI_PLL_SETTING[u4Feq][1]) | (HDMI_PLL_SETTING[u4Feq][2] << 8) |
			      (HDMI_PLL_SETTING[u4Feq][3] << 16) | (HDMI_PLL_SETTING[u4Feq][4] <<
								    24));
	} else if (bdeepmode == HDMI_DEEP_COLOR_10_BIT) {
		vWriteHdmiSYSMsk(HDMI_SYS_PLLCTRL2, (HDMI_PLL_SETTING_X_1_25[u4Feq][0]), 0xFF);
		vWriteHdmiSYS(HDMI_SYS_PLLCTRL3,
			      (HDMI_PLL_SETTING_X_1_25[u4Feq][1]) |
			      (HDMI_PLL_SETTING_X_1_25[u4Feq][2] << 8) |
			      (HDMI_PLL_SETTING_X_1_25[u4Feq][3] << 16) |
			      (HDMI_PLL_SETTING_X_1_25[u4Feq][4] << 24));
	} else if (bdeepmode == HDMI_DEEP_COLOR_12_BIT) {
		vWriteHdmiSYSMsk(HDMI_SYS_PLLCTRL2, (HDMI_PLL_SETTING_X_1_5[u4Feq][0]), 0xFF);
		vWriteHdmiSYS(HDMI_SYS_PLLCTRL3,
			      (HDMI_PLL_SETTING_X_1_5[u4Feq][1]) | (HDMI_PLL_SETTING_X_1_5[u4Feq][2]
								    << 8) |
			      (HDMI_PLL_SETTING_X_1_5[u4Feq][3] << 16) |
			      (HDMI_PLL_SETTING_X_1_5[u4Feq][4] << 24));
	}
	vWriteHdmiSYSMsk(HDMI_SYS_PLLCTRL6, RG_CK148M_EN, RG_CK148M_EN);


}

void vEnableDeepColor(unsigned char ui1Mode)
{
	unsigned int u4Data;

	MT8193_DRV_FUNC();
	if (ui1Mode == HDMI_DEEP_COLOR_10_BIT)
		u4Data = COLOR_10BIT_MODE;
	else if (ui1Mode == HDMI_DEEP_COLOR_12_BIT)
		u4Data = COLOR_12BIT_MODE;
	else if (ui1Mode == HDMI_DEEP_COLOR_16_BIT)
		u4Data = COLOR_16BIT_MODE;
	else
		u4Data = COLOR_8BIT_MODE;


	if (u4Data == COLOR_8BIT_MODE) {
		vWriteHdmiSYSMsk(HDMI_SYS_CFG20, u4Data, DEEP_COLOR_MODE_MASK | DEEP_COLOR_EN);
	} else {
		vWriteHdmiSYSMsk(HDMI_SYS_CFG20, u4Data | DEEP_COLOR_EN,
				 DEEP_COLOR_MODE_MASK | DEEP_COLOR_EN);
	}

}

void vResetHDMIPLL(void)
{
	vWriteHdmiSYSMsk(HDMI_SYS_PLLCTRL2, 0, POW_PLL_L);
	udelay(2);
	vWriteHdmiSYSMsk(HDMI_SYS_PLLCTRL2, POW_PLL_L, POW_PLL_L);
	udelay(2);
}


void vSetHDMITxPLLTrigger(void)
{
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL7, 0);

	vWriteHdmiSYS(HDMI_SYS_AMPCTRL, 0x0000bbbb);
	vWriteHdmiSYS(HDMI_SYS_AMPCTRL1, 0x00000000);
	/* 480i pll setting */
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL1, 0x1F001F00);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL2, 0x00300094);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL3, 0xF4c81400);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL4, 0x46331717);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL5, 0x00004140);

	udelay(20);

	vWriteHdmiSYS(HDMI_SYS_PLLCTRL1, 0x1F001F00);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL2, 0x00200094);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL3, 0xF4c81400);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL4, 0x46331717);
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL5, 0x00004140);

	udelay(20);

	vWriteHdmiSYS(HDMI_SYS_PLLCTRL2, 0x00300094);	/* later, toggle 0x0f20[12] =1 */
	vWriteHdmiSYS(HDMI_SYS_PLLCTRL7, 0x31ff0000);
}

void vChangeVpll(unsigned char bRes, unsigned char bdeepmode)
{
	MT8193_PLL_FUNC();
	vSetHDMITxPLL(bRes, bdeepmode);	/* set PLL */
	vEnableDeepColor(bdeepmode);
}

void vResetHDMI(unsigned char bRst)
{
	MT8193_DRV_FUNC();
	if (bRst) {
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, HDMI_RST, HDMI_RST);
	} else {
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, 0, HDMI_RST);
		vWriteHdmiGRLMsk(GRL_CFG3, 0, CFG3_CONTROL_PACKET_DELAY);
		/* Designer suggest adjust Control packet deliver time */
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, ANLG_ON, ANLG_ON);
	}
}

void vHDMIAVUnMute(void)
{
	MT8193_AUDIO_FUNC();
	if (_bflagvideomute == FALSE)
		vUnBlackHDMIOnly();
	if ((_bflagaudiomute == FALSE) && (is_user_mute_hdmi_audio == 0))
		UnMuteHDMIAudio();
}

void hdmi_user_mute_audio(unsigned char mute)
{
	HDMI_DEF_LOG("is_user_mute_hdmi_audio=%d\n", is_user_mute_hdmi_audio);
	if (mute == 0) {
		is_user_mute_hdmi_audio = 0;
		UnMuteHDMIAudio();
	} else {
		is_user_mute_hdmi_audio = 1;
		MuteHDMIAudio();
	}
}

void vHDMIVideoOutput(unsigned char ui1Res, unsigned char ui1ColorSpace)
{
	MT8193_VIDEO_FUNC();

	vWriteHdmiDGIMsk(fifo_ctrl, sw_rst, sw_rst);
	vWriteHdmiDGIMsk(dgi1_clk_rst_ctrl,
			 dgi1_clk_out_enable | dgi1_clk_in_inv_enable | dgi1_clk_in_enable,
			 dgi1_clk_out_enable | dgi1_clk_in_inv_enable | dgi1_clk_in_enable);
	vWriteHdmiDGIMsk(data_out_ctrl, fall_use_fall, fall_use_fall | rise_use_fall);
	vWriteHdmiDGIMsk(fifo_ctrl, (0x40 << 0), rd_start);
	vWriteHdmiDGIMsk(fifo_ctrl, fifo_reset_on, fifo_reset_sel | fifo_reset_on);
	vWriteHdmiDGIMsk(dec_ctl, dgi1_on, dgi1_on);
	vWriteHdmiDGIMsk(ctrl_422_444, (CBCR_PRELOAD[ui1Res] << 8), rg_cbcr_preload);

	if (ui1ColorSpace == HDMI_YCBCR_444) {
		vWriteHdmiDGIMsk(ctrl_422_444, rpt_422_444, rpt_422_444 | bypass_422_444);
		vWriteHdmiDGIMsk(data_out_ctrl, (0x1 << 0) | (0x0 << 2) | (0x0 << 4),
				 y_out_delay | c1_out_delay | c2_out_delay);
		vWriteHdmiDGIMsk(dgi1_yuv2rgb_ctr, 0, rg_yuv709_rgb | rg_yuv2rgb_en);
	} else if (ui1ColorSpace == HDMI_YCBCR_422) {
		vWriteHdmiDGIMsk(ctrl_422_444, bypass_422_444, rpt_422_444 | bypass_422_444);
		vWriteHdmiDGIMsk(data_out_ctrl, (0x1 << 0) | (0x0 << 2) | (0x0 << 4),
				 y_out_delay | c1_out_delay | c2_out_delay);
		vWriteHdmiDGIMsk(dgi1_yuv2rgb_ctr, 0, rg_yuv709_rgb | rg_yuv2rgb_en);
	} else if (ui1ColorSpace == HDMI_RGB) {
		vWriteHdmiDGIMsk(ctrl_422_444, rpt_422_444, rpt_422_444 | bypass_422_444);
		vWriteHdmiDGIMsk(data_out_ctrl, (0x1 << 0) | (0x1 << 2) | (0x0 << 4),
				 y_out_delay | c1_out_delay | c2_out_delay);
		vWriteHdmiDGIMsk(dgi1_yuv2rgb_ctr, rg_yuv709_rgb | rg_yuv2rgb_en,
				 rg_yuv709_rgb | rg_yuv2rgb_en);
	} else {
		hdmi_print("color space type error\n");
	}
}

void vSetHDMIDataEnable(unsigned char bResIndex)
{
	MT8193_VIDEO_FUNC();
	if (bResIndex < HDMI_VIDEO_RESOLUTION_NUM) {
		vWriteHdmiDGIMsk(tg_ctrl02, HVSYNC_TOTAL_WIDTH_ACTIVE[bResIndex][0], 0xffffffff);
		vWriteHdmiDGIMsk(tg_ctrl03, HVSYNC_TOTAL_WIDTH_ACTIVE[bResIndex][1], 0xffffffff);
		vWriteHdmiDGIMsk(tg_ctrl04, HVSYNC_TOTAL_WIDTH_ACTIVE[bResIndex][5] << 5,
				 0xffffffff);
		vWriteHdmiDGIMsk(tg_ctrl05, HVSYNC_TOTAL_WIDTH_ACTIVE[bResIndex][4], 0xffffffff);
		vWriteHdmiDGIMsk(tg_ctrl06, HVSYNC_TOTAL_WIDTH_ACTIVE[bResIndex][3], 0xffffffff);
		vWriteHdmiDGIMsk(tg_ctrl07, HVSYNC_TOTAL_WIDTH_ACTIVE[bResIndex][2], 0xffffffff);
	}

}

void vSetHDMISyncDelay(unsigned char bResIndex)
{
	MT8193_VIDEO_FUNC();
	vWriteHdmiDGIMsk(tg_ctrl00, prgs_out, syn_del | prgs_out);
	vWriteHdmiDGIMsk(tg_ctrl01, HVSYNC_DELAY[bResIndex],
			 rg_vsync_delay | rg_hsync_delay | rg_vsync_forward);
}

void vEnableNotice(unsigned char bOn)
{
	unsigned char bData;

	MT8193_VIDEO_FUNC();
	if (bOn == TRUE) {
		bData = bReadByteHdmiGRL(GRL_CFG2);
		bData |= 0x40;	/* temp. solve 720p issue. to avoid audio packet jitter problem */
		vWriteByteHdmiGRL(GRL_CFG2, bData);
	} else {
		bData = bReadByteHdmiGRL(GRL_CFG2);
		bData &= ~0x40;
		vWriteByteHdmiGRL(GRL_CFG2, bData);
	}
}

void vEnableHdmiMode(unsigned char bOn)
{
	unsigned char bData;

	MT8193_DRV_FUNC();
	if (bOn == TRUE) {
		bData = bReadByteHdmiGRL(GRL_CFG1);
		bData &= ~CFG1_DVI;	/* enable HDMI mode */
		vWriteByteHdmiGRL(GRL_CFG1, bData);
		HDMI_DEF_LOG("[hdmi]to hdmi mode\n");
	} else {
		bData = bReadByteHdmiGRL(GRL_CFG1);
		bData |= CFG1_DVI;	/* disable HDMI mode */
		vWriteByteHdmiGRL(GRL_CFG1, bData);
		HDMI_DEF_LOG("[hdmi]to dvi mode\n");
	}

}

void vEnableNCTSAutoWrite(void)
{
	unsigned char bData;

	MT8193_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_DIVN);
	bData |= NCTS_WRI_ANYTIME;	/* enabel N-CTS can be written in any time */
	vWriteByteHdmiGRL(GRL_DIVN, bData);

}

void vHDMISettingColorSpace(unsigned char ui1colorspace)
{
	if (ui1colorspace == HDMI_YCBCR_444)
		vWriteByteHdmiGRL(GRL_DIV_RESET, UNSWAP_YC);
	else if (ui1colorspace == HDMI_YCBCR_422)
		vWriteByteHdmiGRL(GRL_DIV_RESET, SWAP_YC);
	else
		vWriteByteHdmiGRL(GRL_DIV_RESET, UNSWAP_YC);

}

void vHDMIResetGenReg(unsigned char ui1resindex, unsigned char ui1colorspace)
{
	MT8193_DRV_FUNC();
	vResetHDMI(1);
	vHDMIVideoOutput(ui1resindex, ui1colorspace);
	vResetHDMI(0);
	vEnableNotice(TRUE);
	vWriteHdmiIntMask(0xff);
	vSetHDMIDataEnable(ui1resindex);
	vSetHDMISyncDelay(ui1resindex);

	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == TRUE)
		vEnableHdmiMode(TRUE);
	else
		vEnableHdmiMode(FALSE);

	vEnableNCTSAutoWrite();
	vHDMISettingColorSpace(ui1colorspace);
}

void vAudioPacketOff(unsigned char bOn)
{
	unsigned char bData;

	MT8193_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_SHIFT_R2);
	if (bOn)
		bData |= 0x40;
	else
		bData &= ~0x40;

	vWriteByteHdmiGRL(GRL_SHIFT_R2, bData);
}

void vSetChannelSwap(unsigned char u1SwapBit)
{
	MT8193_AUDIO_FUNC();
	vWriteHdmiGRLMsk(GRL_CH_SWAP, u1SwapBit, 0xff);
}

void vEnableIecTxRaw(void)
{
	unsigned char bData;

	MT8193_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_MIX_CTRL);
	bData |= MIX_CTRL_FLAT;
	vWriteByteHdmiGRL(GRL_MIX_CTRL, bData);
}

void vSetHdmiI2SDataFmt(unsigned char bFmt)
{
	unsigned char bData;

	MT8193_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_CFG0);
	bData &= ~0x33;
	switch (bFmt) {
	case RJT_24BIT:
		bData |= (CFG0_I2S_MODE_RTJ | CFG0_I2S_MODE_24Bit);
		break;

	case RJT_16BIT:
		bData |= (CFG0_I2S_MODE_RTJ | CFG0_I2S_MODE_16Bit);
		break;

	case LJT_24BIT:
		bData |= (CFG0_I2S_MODE_LTJ | CFG0_I2S_MODE_24Bit);
		break;

	case LJT_16BIT:
		bData |= (CFG0_I2S_MODE_LTJ | CFG0_I2S_MODE_16Bit);
		break;

	case I2S_24BIT:
		bData |= (CFG0_I2S_MODE_I2S | CFG0_I2S_MODE_24Bit);
		break;

	case I2S_16BIT:
		bData |= (CFG0_I2S_MODE_I2S | CFG0_I2S_MODE_16Bit);
		break;

	}


	vWriteByteHdmiGRL(GRL_CFG0, bData);
}

void vAOUT_BNUM_SEL(unsigned char bBitNum)
{
	MT8193_AUDIO_FUNC();
	vWriteByteHdmiGRL(GRL_AOUT_BNUM_SEL, bBitNum);

}

void vSetHdmiHighBitrate(unsigned char fgHighBitRate)
{
	unsigned char bData;

	MT8193_AUDIO_FUNC();
	if (fgHighBitRate == TRUE) {
		bData = bReadByteHdmiGRL(GRL_AOUT_BNUM_SEL);
		bData |= HIGH_BIT_RATE_PACKET_ALIGN;
		vWriteByteHdmiGRL(GRL_AOUT_BNUM_SEL, bData);
		udelay(100);	/* 1ms */
		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData |= HIGH_BIT_RATE;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	} else {
		bData = bReadByteHdmiGRL(GRL_AOUT_BNUM_SEL);
		bData &= ~HIGH_BIT_RATE_PACKET_ALIGN;
		vWriteByteHdmiGRL(GRL_AOUT_BNUM_SEL, bData);

		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData &= ~HIGH_BIT_RATE;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	}


}

void vDSTNormalDouble(unsigned char fgEnable)
{
	unsigned char bData;

	MT8193_AUDIO_FUNC();
	if (fgEnable) {
		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData |= DST_NORMAL_DOUBLE;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	} else {
		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData &= ~DST_NORMAL_DOUBLE;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	}

}

void vEnableDSTConfig(unsigned char fgEnable)
{
	unsigned char bData;

	MT8193_AUDIO_FUNC();
	if (fgEnable) {
		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData |= SACD_DST;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	} else {
		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData &= ~SACD_DST;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	}

}

void vDisableDsdConfig(void)
{
	unsigned char bData;

	MT8193_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
	bData &= ~SACD_SEL;
	vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);

}

void vSetHdmiI2SChNum(unsigned char bChNum, unsigned char bChMapping)
{
	unsigned char bData, bData1, bData2, bData3;

	MT8193_AUDIO_FUNC();
	if (bChNum == 2) {	/* I2S 2ch */
		bData = 0x04;	/* 2ch data */
		bData1 = 0x50;	/* data0 */


	} else if ((bChNum == 3) || (bChNum == 4)) {	/* I2S 2ch */
		if ((bChNum == 4) && (bChMapping == 0x08))
			bData = 0x14;	/* 4ch data */
		else
			bData = 0x0c;	/* 4ch data */

		bData1 = 0x50;	/* data0 */


	} else if ((bChNum == 6) || (bChNum == 5)) {	/* I2S 5.1ch */
		if ((bChNum == 6) && (bChMapping == 0x0E)) {
			bData = 0x3C;	/* 6.0 ch data */
			bData1 = 0x50;	/* data0 */
		} else {
			bData = 0x1C;	/* 5.1ch data, 5/0ch */
			bData1 = 0x50;	/* data0 */
		}


	} else if (bChNum == 8) {	/* I2S 5.1ch */
		bData = 0x3C;	/* 7.1ch data */
		bData1 = 0x50;	/* data0 */
	} else if (bChNum == 7) {	/* I2S 6.1ch */
		bData = 0x3C;	/* 6.1ch data */
		bData1 = 0x50;	/* data0 */
	} else {
		bData = 0x04;	/* 2ch data */
		bData1 = 0x50;	/* data0 */
	}

	bData2 = 0xc6;
	bData3 = 0xfa;

	vWriteByteHdmiGRL(GRL_CH_SW0, bData1);
	vWriteByteHdmiGRL(GRL_CH_SW1, bData2);
	vWriteByteHdmiGRL(GRL_CH_SW2, bData3);
	vWriteByteHdmiGRL(GRL_I2S_UV, bData);

	/* vDisableDsdConfig(); */

}

void vSetHdmiIecI2s(unsigned char bIn)
{
	unsigned char bData;

	MT8193_AUDIO_FUNC();
	if (bIn == SV_SPDIF) {

		bData = bReadByteHdmiGRL(GRL_CFG1);

		if ((bData & CFG1_SPDIF) == 0) {
			bData |= CFG1_SPDIF;
			vWriteByteHdmiGRL(GRL_CFG1, bData);
		}
	} else {
		bData = bReadByteHdmiGRL(GRL_CFG1);
		if (bData & CFG1_SPDIF) {
			bData &= ~CFG1_SPDIF;
			vWriteByteHdmiGRL(GRL_CFG1, bData);
		}
		bData = bReadByteHdmiGRL(GRL_CFG1);
	}
}

void vSetHDMIAudioIn(void)
{
	unsigned char bData2;

	MT8193_AUDIO_FUNC();

	bData2 = vCheckPcmBitSize(0);

	vSetChannelSwap(LFE_CC_SWAP);
	vEnableIecTxRaw();

	vSetHdmiI2SDataFmt(I2S_16BIT);

	if (bData2 == PCM_24BIT)
		vAOUT_BNUM_SEL(AOUT_24BIT);
	else
		vAOUT_BNUM_SEL(AOUT_16BIT);

	vSetHdmiHighBitrate(FALSE);
	vDSTNormalDouble(FALSE);
	vEnableDSTConfig(FALSE);

	vDisableDsdConfig();
	vSetHdmiI2SChNum(2, 0);
	vSetHdmiIecI2s(SV_I2S);

}

void vHwNCTSOnOff(unsigned char bHwNctsOn)
{
	unsigned char bData;

	MT8193_AUDIO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;
	bData = bReadByteHdmiGRL(GRL_CTS_CTRL);

	if (bHwNctsOn == TRUE)
		bData &= ~CTS_CTRL_SOFT;
	else
		bData |= CTS_CTRL_SOFT;

	vWriteByteHdmiGRL(GRL_CTS_CTRL, bData);

}

void vHalHDMI_NCTS(unsigned char bAudioFreq, unsigned char bPix, unsigned char bDeepMode)
{
	unsigned char bTemp, bData, bData1[NCTS_BYTES];
	unsigned int u4Temp, u4NTemp = 0;

	MT8193_AUDIO_FUNC();
	MT8193_AUDIO_LOG("bAudioFreq=%d,  bPix=%d, bDeepMode=%d\n", bAudioFreq, bPix, bDeepMode);

	bData = 0;
	vWriteByteHdmiGRL(GRL_NCTS, bData);	/* YT suggest 3 dummy N-CTS */
	vWriteByteHdmiGRL(GRL_NCTS, bData);
	vWriteByteHdmiGRL(GRL_NCTS, bData);

	for (bTemp = 0; bTemp < NCTS_BYTES; bTemp++)
		bData1[bTemp] = 0;


	if (bDeepMode == HDMI_NO_DEEP_COLOR) {
		for (bTemp = 0; bTemp < NCTS_BYTES; bTemp++) {

			if ((bAudioFreq < 7) && (bPix < 9))

				bData1[bTemp] = HDMI_NCTS[bAudioFreq][bPix][bTemp];
		}

		u4NTemp = (bData1[4] << 16) | (bData1[5] << 8) | (bData1[6]);	/* N */
		u4Temp = (bData1[0] << 24) | (bData1[1] << 16) | (bData1[2] << 8) | (bData1[3]);	/* CTS */

	} else {
		for (bTemp = 0; bTemp < NCTS_BYTES; bTemp++) {
			if ((bAudioFreq < 7) && (bPix < 9))

				bData1[bTemp] = HDMI_NCTS[bAudioFreq][bPix][bTemp];
		}

		u4NTemp = (bData1[4] << 16) | (bData1[5] << 8) | (bData1[6]);	/* N */
		u4Temp = (bData1[0] << 24) | (bData1[1] << 16) | (bData1[2] << 8) | (bData1[3]);

		if (bDeepMode == HDMI_DEEP_COLOR_10_BIT)
			u4Temp = (u4Temp >> 2) * 5;	/* (*5/4) */
		else if (bDeepMode == HDMI_DEEP_COLOR_12_BIT)
			u4Temp = (u4Temp >> 1) * 3;	/* (*3/2) */
		else if (bDeepMode == HDMI_DEEP_COLOR_16_BIT)
			u4Temp = (u4Temp << 1);	/* (*2) */


		bData1[0] = (u4Temp >> 24) & 0xff;
		bData1[1] = (u4Temp >> 16) & 0xff;
		bData1[2] = (u4Temp >> 8) & 0xff;
		bData1[3] = (u4Temp) & 0xff;

	}
	for (bTemp = 0; bTemp < NCTS_BYTES; bTemp++) {
		bData = bData1[bTemp];
		vWriteByteHdmiGRL(GRL_NCTS, bData);
	}

	_u4NValue = u4NTemp;
}

void vHDMI_NCTS(unsigned char bHDMIFsFreq, unsigned char bResolution, unsigned char bdeepmode)
{
	unsigned char bPix;

	MT8193_AUDIO_FUNC();

	vWriteHdmiGRLMsk(DUMMY_304, AUDIO_I2S_NCTS_SEL_64, AUDIO_I2S_NCTS_SEL);

	switch (bResolution) {
	case HDMI_VIDEO_720x480p_60Hz:
	case HDMI_VIDEO_720x576p_50Hz:
	default:
		bPix = 0;
		break;

	case HDMI_VIDEO_1280x720p_60Hz:	/* 74.175M pixel clock */
	case HDMI_VIDEO_1920x1080i_60Hz:
	case HDMI_VIDEO_1920x1080p_23Hz:
		bPix = 2;
		break;

	case HDMI_VIDEO_1280x720p_50Hz:	/* 74.25M pixel clock */
	case HDMI_VIDEO_1920x1080i_50Hz:
	case HDMI_VIDEO_1920x1080p_24Hz:
		bPix = 3;
		break;
	}

	vHalHDMI_NCTS(bHDMIFsFreq, bPix, bdeepmode);

}

void vSetHdmiClkRefIec2(unsigned char fgSyncIec2Clock)
{
	MT8193_AUDIO_FUNC();
}

void vSetHDMISRCOff(void)
{
	unsigned char bData;

	MT8193_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_MIX_CTRL);
	bData &= ~MIX_CTRL_SRC_EN;
	vWriteByteHdmiGRL(GRL_MIX_CTRL, bData);
	bData = 0x00;
	vWriteByteHdmiGRL(GRL_SHIFT_L1, bData);
}

void vHalSetHDMIFS(unsigned char bFs)
{
	unsigned char bData;

	MT8193_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_CFG5);
	bData &= CFG5_CD_RATIO_MASK;
	bData |= bFs;
	vWriteByteHdmiGRL(GRL_CFG5, bData);

}

void vHdmiAclkInv(unsigned char bInv)
{
	unsigned char bData;

	MT8193_AUDIO_FUNC();
	if (bInv == TRUE) {
		bData = bReadByteHdmiGRL(GRL_CFG2);
		bData |= 0x80;
		vWriteByteHdmiGRL(GRL_CFG2, bData);
	} else {
		bData = bReadByteHdmiGRL(GRL_CFG2);
		bData &= ~0x80;
		vWriteByteHdmiGRL(GRL_CFG2, bData);
	}
}

void vSetHDMIFS(unsigned char bFs, unsigned char fgAclInv)
{
	MT8193_AUDIO_FUNC();
	vHalSetHDMIFS(bFs);

	if (fgAclInv)
		vHdmiAclkInv(TRUE);	/* //fix 192kHz, SPDIF downsample noise issue, ACL iNV */
	else
		vHdmiAclkInv(FALSE);	/* fix 192kHz, SPDIF downsample noise issue */


	_stAvdAVInfo.u1HdmiI2sMclk = bFs;
}

void vReEnableSRC(void)
{
	unsigned char bData;

	MT8193_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_MIX_CTRL);
	if (bData & MIX_CTRL_SRC_EN) {
		bData &= ~MIX_CTRL_SRC_EN;
		vWriteByteHdmiGRL(GRL_MIX_CTRL, bData);
		udelay(255);
		bData |= MIX_CTRL_SRC_EN;
		vWriteByteHdmiGRL(GRL_MIX_CTRL, bData);
	}

}

void vHDMIAudioSRC(unsigned char ui1hdmifs, unsigned char ui1resindex, unsigned char bdeepmode)
{

	MT8193_AUDIO_FUNC();

	vHwNCTSOnOff(FALSE);

	vSetHdmiClkRefIec2(FALSE);

	switch (ui1hdmifs) {
	case HDMI_FS_44K:
		vSetHDMISRCOff();
		vSetHDMIFS(CFG5_FS128, FALSE);
		break;

	case HDMI_FS_48K:
		vSetHDMISRCOff();
		vSetHDMIFS(CFG5_FS128, FALSE);
		break;

	default:
		break;
	}

	vHDMI_NCTS(ui1hdmifs, ui1resindex, bdeepmode);
	vReEnableSRC();

}

void vHwSet_Hdmi_I2S_C_Status(unsigned char *prLChData, unsigned char *prRChData)
{
	unsigned char bData;

	MT8193_AUDIO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;
	bData = prLChData[0];

	vWriteByteHdmiGRL(GRL_I2S_C_STA0, bData);
	vWriteByteHdmiGRL(GRL_L_STATUS_0, bData);

	bData = prRChData[0];

	vWriteByteHdmiGRL(GRL_R_STATUS_0, bData);

	bData = prLChData[1];
	vWriteByteHdmiGRL(GRL_I2S_C_STA1, bData);
	vWriteByteHdmiGRL(GRL_L_STATUS_1, bData);
	bData = prRChData[1];
	vWriteByteHdmiGRL(GRL_R_STATUS_1, bData);

	bData = prLChData[2];
	vWriteByteHdmiGRL(GRL_I2S_C_STA2, bData);
	vWriteByteHdmiGRL(GRL_L_STATUS_2, bData);
	bData = prRChData[2];
	vWriteByteHdmiGRL(GRL_R_STATUS_2, bData);

	bData = prLChData[3];
	vWriteByteHdmiGRL(GRL_I2S_C_STA3, bData);
	vWriteByteHdmiGRL(GRL_L_STATUS_3, bData);
	bData = prRChData[3];
	vWriteByteHdmiGRL(GRL_R_STATUS_3, bData);

	bData = prLChData[4];
	vWriteByteHdmiGRL(GRL_I2S_C_STA4, bData);
	vWriteByteHdmiGRL(GRL_L_STATUS_4, bData);
	bData = prRChData[4];
	vWriteByteHdmiGRL(GRL_R_STATUS_4, bData);

	for (bData = 0; bData < 19; bData++) {
		vWriteByteHdmiGRL(GRL_L_STATUS_5 + bData * 4, 0);
		vWriteByteHdmiGRL(GRL_R_STATUS_5 + bData * 4, 0);

	}
}

void vHDMI_I2S_C_Status(void)
{
	unsigned char bData = 0;
	unsigned char bhdmi_RCh_status[5];
	unsigned char bhdmi_LCh_status[5];

	MT8193_AUDIO_FUNC();

	bhdmi_LCh_status[0] = _stAvdAVInfo.bhdmiLChstatus[0];
	bhdmi_LCh_status[1] = _stAvdAVInfo.bhdmiLChstatus[1];
	bhdmi_LCh_status[2] = _stAvdAVInfo.bhdmiLChstatus[2];
	bhdmi_RCh_status[0] = _stAvdAVInfo.bhdmiRChstatus[0];
	bhdmi_RCh_status[1] = _stAvdAVInfo.bhdmiRChstatus[1];
	bhdmi_RCh_status[2] = _stAvdAVInfo.bhdmiRChstatus[2];


	bhdmi_LCh_status[0] &= ~0x02;
	bhdmi_RCh_status[0] &= ~0x02;

	bData = _stAvdAVInfo.bhdmiLChstatus[3] & 0xf0;

	switch (_stAvdAVInfo.e_hdmi_fs) {
	case HDMI_FS_32K:
		bData |= 0x03;
		break;
	case HDMI_FS_44K:
		break;

	case HDMI_FS_48K:
	default:
		bData |= 0x02;
		break;
	}


	bhdmi_LCh_status[3] = bData;
	bhdmi_RCh_status[3] = bData;

	bData = _stAvdAVInfo.bhdmiLChstatus[4];

	bData |= ((~(bhdmi_LCh_status[3] & 0x0f)) << 4);

	bhdmi_LCh_status[4] = bData;
	bhdmi_RCh_status[4] = bData;

	vHwSet_Hdmi_I2S_C_Status(&bhdmi_LCh_status[0], &bhdmi_RCh_status[0]);

}

void vHalSendAudioInfoFrame(unsigned char bData1, unsigned char bData2, unsigned char bData4, unsigned char bData5)
{
	unsigned char bAUDIO_CHSUM;
	unsigned char bData = 0;

	MT8193_AUDIO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;
	vWriteHdmiGRLMsk(GRL_CTRL, 0, CTRL_AUDIO_EN);
	vWriteByteHdmiGRL(GRL_INFOFRM_VER, AUDIO_VERS);
	vWriteByteHdmiGRL(GRL_INFOFRM_TYPE, AUDIO_TYPE);
	vWriteByteHdmiGRL(GRL_INFOFRM_LNG, AUDIO_LEN);

	bAUDIO_CHSUM = AUDIO_TYPE + AUDIO_VERS + AUDIO_LEN;

	bAUDIO_CHSUM += bData1;
	bAUDIO_CHSUM += bData2;
	bAUDIO_CHSUM += bData4;
	bAUDIO_CHSUM += bData5;

	bAUDIO_CHSUM = 0x100 - bAUDIO_CHSUM;
	vWriteByteHdmiGRL(GRL_IFM_PORT, bAUDIO_CHSUM);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData1);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData2);	/* bData2 */
	vWriteByteHdmiGRL(GRL_IFM_PORT, 0);	/* bData3 */
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData4);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData5);

	for (bData = 0; bData < 5; bData++)
		vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

	bData = bReadByteHdmiGRL(GRL_CTRL);
	bData |= CTRL_AUDIO_EN;
	vWriteByteHdmiGRL(GRL_CTRL, bData);

}

void vSendAudioInfoFrame(void)
{
	MT8193_AUDIO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;
	_bAudInfoFm[0] = 0x01;
	_bAudInfoFm[2] = 0x00;
	_bAudInfoFm[1] = 0;
	_bAudInfoFm[3] = 0x0;
	_bAudInfoFm[4] = 0x0;
	vHalSendAudioInfoFrame(_bAudInfoFm[0], _bAudInfoFm[1], _bAudInfoFm[2], _bAudInfoFm[3]);
}

void vChgHDMIAudioOutput(unsigned char ui1hdmifs, unsigned char ui1resindex, unsigned char bdeepmode)
{
	unsigned int ui4Index;

	MT8193_AUDIO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;
	MuteHDMIAudio();
	vAudioPacketOff(TRUE);
	vSetHDMIAudioIn();
	vHDMIAudioSRC(ui1hdmifs, ui1resindex, bdeepmode);

	vHDMI_I2S_C_Status();
	vSendAudioInfoFrame();

	for (ui4Index = 0; ui4Index < 5; ui4Index++)
		udelay(5);

	vHwNCTSOnOff(TRUE);

	vAudioPacketOff(FALSE);

	if (is_user_mute_hdmi_audio == 0)
		UnMuteHDMIAudio();

	HDMI_DEF_LOG("[hdmi]set aud,e_hdmi_fs:%d,u1HdmiI2sMclk:%d,ui1_aud_out_ch_number=%d\n",
		     _stAvdAVInfo.e_hdmi_fs, _stAvdAVInfo.u1HdmiI2sMclk,
		     _stAvdAVInfo.ui1_aud_out_ch_number);
}

void vDisableGamut(void)
{
	MT8193_AUDIO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;
	vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, GAMUT_EN);
}

void vHalSendAVIInfoFrame(unsigned char *pr_bData)
{
	unsigned char bAVI_CHSUM;
	unsigned char bData1 = 0, bData2 = 0, bData3 = 0, bData4 = 0, bData5 = 0;
	unsigned char bData;

	MT8193_VIDEO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;
	bData1 = *pr_bData;
	bData2 = *(pr_bData + 1);
	bData3 = *(pr_bData + 2);
	bData4 = *(pr_bData + 3);
	bData5 = *(pr_bData + 4);

	vWriteHdmiGRLMsk(GRL_CTRL, 0, CTRL_AVI_EN);
	vWriteByteHdmiGRL(GRL_INFOFRM_VER, AVI_VERS);
	vWriteByteHdmiGRL(GRL_INFOFRM_TYPE, AVI_TYPE);
	vWriteByteHdmiGRL(GRL_INFOFRM_LNG, AVI_LEN);

	bAVI_CHSUM = AVI_TYPE + AVI_VERS + AVI_LEN;

	bAVI_CHSUM += bData1;
	bAVI_CHSUM += bData2;
	bAVI_CHSUM += bData3;
	bAVI_CHSUM += bData4;
	bAVI_CHSUM += bData5;
	bAVI_CHSUM = 0x100 - bAVI_CHSUM;
	vWriteByteHdmiGRL(GRL_IFM_PORT, bAVI_CHSUM);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData1);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData2);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData3);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData4);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData5);

	for (bData2 = 0; bData2 < 8; bData2++)
		vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

	bData = bReadByteHdmiGRL(GRL_CTRL);
	bData |= CTRL_AVI_EN;
	vWriteByteHdmiGRL(GRL_CTRL, bData);
}

void vSendAVIInfoFrame(unsigned char ui1resindex, unsigned char ui1colorspace)
{
	MT8193_VIDEO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;
	if (ui1colorspace == HDMI_YCBCR_444)
		_bAviInfoFm[0] = 0x40;
	else if (ui1colorspace == HDMI_YCBCR_422)
		_bAviInfoFm[0] = 0x20;
	else
		_bAviInfoFm[0] = 0x00;


	_bAviInfoFm[0] |= 0x10;	/* A0=1, Active format (R0~R3) inf valid */

	_bAviInfoFm[1] = 0x0;	/* bData2 */


	if ((ui1resindex == HDMI_VIDEO_720x480p_60Hz) || (ui1resindex == HDMI_VIDEO_720x576p_50Hz))
		_bAviInfoFm[1] |= AV_INFO_SD_ITU601;
	else
		_bAviInfoFm[1] |= AV_INFO_HD_ITU709;


	_bAviInfoFm[1] |= 0x20;
	_bAviInfoFm[1] |= 0x08;
	_bAviInfoFm[2] = 0;	/* bData3 */
	_bAviInfoFm[2] |= 0x04;	/* limit Range */
	_bAviInfoFm[3] = HDMI_VIDEO_ID_CODE[ui1resindex];	/* bData4 */

	if ((_bAviInfoFm[1] & AV_INFO_16_9_OUTPUT)
	    && ((ui1resindex == HDMI_VIDEO_720x480p_60Hz)
		|| (ui1resindex == HDMI_VIDEO_720x576p_50Hz))) {
		_bAviInfoFm[3] = _bAviInfoFm[3] + 1;
	}

	_bAviInfoFm[4] = 0x00;

	vHalSendAVIInfoFrame(&_bAviInfoFm[0]);

}

void vHalSendSPDInfoFrame(unsigned char *pr_bData)
{
	unsigned char bSPD_CHSUM, bData;
	unsigned char i = 0;

	MT8193_VIDEO_FUNC();

	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;
	vWriteHdmiGRLMsk(GRL_CTRL, 0, CTRL_SPD_EN);
	vWriteByteHdmiGRL(GRL_INFOFRM_VER, SPD_VERS);
	vWriteByteHdmiGRL(GRL_INFOFRM_TYPE, SPD_TYPE);

	vWriteByteHdmiGRL(GRL_INFOFRM_LNG, SPD_LEN);
	bSPD_CHSUM = SPD_TYPE + SPD_VERS + SPD_LEN;

	for (i = 0; i < SPD_LEN; i++)
		bSPD_CHSUM += (*(pr_bData + i));


	bSPD_CHSUM = 0x100 - bSPD_CHSUM;
	vWriteByteHdmiGRL(GRL_IFM_PORT, bSPD_CHSUM);
	for (i = 0; i < SPD_LEN; i++)
		vWriteByteHdmiGRL(GRL_IFM_PORT, *(pr_bData + i));

	bData = bReadByteHdmiGRL(GRL_CTRL);
	bData |= CTRL_SPD_EN;
	vWriteByteHdmiGRL(GRL_CTRL, bData);

}

void vSend_AVUNMUTE(void)
{
	unsigned char bData;

	MT8193_VIDEO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;

	bData = bReadByteHdmiGRL(GRL_CFG4);
	bData |= CFG4_AV_UNMUTE_EN;	/* disable original mute */
	bData &= ~CFG4_AV_UNMUTE_SET;	/* disable */

	vWriteByteHdmiGRL(GRL_CFG4, bData);
	udelay(30);

	bData &= ~CFG4_AV_UNMUTE_EN;	/* disable original mute */
	bData |= CFG4_AV_UNMUTE_SET;	/* disable */

	vWriteByteHdmiGRL(GRL_CFG4, bData);

}

void vChgHDMIVideoResolution(unsigned char ui1resindex,
	unsigned char ui1colorspace, unsigned char ui1hdmifs, unsigned char bdeepmode)
{
	unsigned int u4Index;

	MT8193_VIDEO_FUNC();

	vHDMIAVMute();
	vTxSignalOnOff(SV_ON);
	vHDMIResetGenReg(ui1resindex, ui1colorspace);

	vWriteHdmiDGIMsk(dgi1_yuv2rgb_ctr, 0, fifo_write_en);
	mdelay(20);

	vChgHDMIAudioOutput(ui1hdmifs, ui1resindex, bdeepmode);
	for (u4Index = 0; u4Index < 5; u4Index++)
		udelay(10);


	vDisableGamut();
	vSendAVIInfoFrame(ui1resindex, ui1colorspace);
	vHalSendSPDInfoFrame(&_bSpdInf[0]);
	vSend_AVUNMUTE();
	HDMI_DEF_LOG("[hdmi]vid:%x,cs:%x,fs:%x,dp:%x\n", ui1resindex, ui1colorspace, ui1hdmifs,
		     bdeepmode);

	vWriteHdmiDGIMsk(fifo_ctrl, 0, fifo_reset_sel | fifo_reset_on | sw_rst);
	vWriteHdmiDGIMsk(dgi1_yuv2rgb_ctr, fifo_write_en, fifo_write_en);

	vWriteHdmiSYSMsk(HDMI_SYS_PLLCTRL7, TX_DRV_ENABLE, TX_DRV_ENABLE_MSK);
}

void vChgtoSoftNCTS(unsigned char ui1resindex, unsigned char ui1audiosoft,
	unsigned char ui1hdmifs, unsigned char bdeepmode)
{
	MT8193_AUDIO_FUNC();

	vHwNCTSOnOff(ui1audiosoft);	/* change to software NCTS; */
	vHDMI_NCTS(ui1hdmifs, ui1resindex, bdeepmode);

}

void vShowHpdRsenStatus(void)
{

	if (bCheckPordHotPlug(HOTPLUG_MODE) == TRUE)
		hdmi_print("[HDMI]HPD ON\n");
	else
		hdmi_print("[HDMI]HPD OFF\n");

	if (bCheckPordHotPlug(PORD_MODE) == TRUE)
		hdmi_print("[HDMI]RSEN ON\n");
	else
		hdmi_print("[HDMI]RSEN OFF\n");

	if (i4SharedInfo(SI_HDMI_RECEIVER_STATUS) == HDMI_PLUG_IN_ONLY)
		hdmi_print("[HDMI]SI_HDMI_RECEIVER_STATUS = HDMI_PLUG_IN_ONLY\n");
	else if (i4SharedInfo(SI_HDMI_RECEIVER_STATUS) == HDMI_PLUG_IN_AND_SINK_POWER_ON)
		hdmi_print("[HDMI]SI_HDMI_RECEIVER_STATUS = HDMI_PLUG_IN_AND_SINK_POWER_ON\n");
	else if (i4SharedInfo(SI_HDMI_RECEIVER_STATUS) == HDMI_PLUG_OUT)
		hdmi_print("[HDMI]SI_HDMI_RECEIVER_STATUS = HDMI_PLUG_OUT\n");
	else
		hdmi_print("[HDMI]SI_HDMI_RECEIVER_STATUS error\n");



}

void vShowOutputVideoResolution(void)
{
	hdmi_print("[HDMI]HDMI output resolution = %s\n", szHdmiResStr[_stAvdAVInfo.e_resolution]);	/*  */

}

void vShowDviOrHdmiMode(void)
{
	if (vIsDviMode())
		hdmi_print("[HDMI]DVI Mode\n");
	else
		hdmi_print("[HDMI]HDMI Mode\n");

}

void vShowDeepColor(void)
{

	if (_stAvdAVInfo.e_deep_color_bit == HDMI_NO_DEEP_COLOR)
		hdmi_print("[HDMI]HDMI output deepcolor = HDMI_NO_DEEP_COLOR\n");
	else if (_stAvdAVInfo.e_deep_color_bit == HDMI_DEEP_COLOR_10_BIT)
		hdmi_print("[HDMI]HDMI output deepcolor = HDMI_DEEP_COLOR_10_BIT\n");
	else if (_stAvdAVInfo.e_deep_color_bit == HDMI_DEEP_COLOR_12_BIT)
		hdmi_print("[HDMI]HDMI output deepcolor = HDMI_DEEP_COLOR_12_BIT\n");
	else if (_stAvdAVInfo.e_deep_color_bit == HDMI_DEEP_COLOR_16_BIT)
		hdmi_print("[HDMI]HDMI output deepcolor = HDMI_DEEP_COLOR_16_BIT\n");
	else
		hdmi_print("[HDMI]HDMI output deepcolor error\n");

}

void vShowColorSpace(void)
{
	if (_stAvdAVInfo.e_video_color_space == HDMI_RGB)
		hdmi_print("[HDMI]HDMI output colorspace = HDMI_RGB\n");
	else if (_stAvdAVInfo.e_video_color_space == HDMI_RGB_FULL)
		hdmi_print("[HDMI]HDMI output colorspace = HDMI_RGB_FULL\n");
	else if (_stAvdAVInfo.e_video_color_space == HDMI_YCBCR_444)
		hdmi_print("[HDMI]HDMI output colorspace = HDMI_YCBCR_444\n");
	else if (_stAvdAVInfo.e_video_color_space == HDMI_YCBCR_422)
		hdmi_print("[HDMI]HDMI output colorspace = HDMI_YCBCR_422\n");
	else if (_stAvdAVInfo.e_video_color_space == HDMI_XV_YCC)
		hdmi_print("[HDMI]HDMI output colorspace = HDMI_XV_YCC\n");
	else
		hdmi_print("[HDMI]HDMI output colorspace error\n");

}

void vShowInforFrame(void)
{
	hdmi_print
	    ("====================Audio inforFrame Start ====================================\n");
	hdmi_print("Data Byte (1~5) = 0x%x  0x%x  0x%x  0x%x  0x%x\n", _bAudInfoFm[0],
		   _bAudInfoFm[1], _bAudInfoFm[2], _bAudInfoFm[3], _bAudInfoFm[4]);
	hdmi_print("CC2~ CC0: 0x%x, %s\n", _bAudInfoFm[0] & 0x07,
		   cAudChCountStr[_bAudInfoFm[0] & 0x07]);
	hdmi_print("CT3~ CT0: 0x%x, %s\n", (_bAudInfoFm[0] >> 4) & 0x0f,
		   cAudCodingTypeStr[(_bAudInfoFm[0] >> 4) & 0x0f]);
	hdmi_print("SS1, SS0: 0x%x, %s\n", _bAudInfoFm[1] & 0x03,
		   cAudSampleSizeStr[_bAudInfoFm[1] & 0x03]);
	hdmi_print("SF2~ SF0: 0x%x, %s\n", (_bAudInfoFm[1] >> 2) & 0x07,
		   cAudFsStr[(_bAudInfoFm[1] >> 2) & 0x07]);
	hdmi_print("CA7~ CA0: 0x%x, %s\n", _bAudInfoFm[3] & 0xff,
		   cAudChMapStr[_bAudInfoFm[3] & 0xff]);
	hdmi_print("LSV3~LSV0: %d db\n", (_bAudInfoFm[4] >> 3) & 0x0f);
	hdmi_print("DM_INH: 0x%x ,%s\n", (_bAudInfoFm[4] >> 7) & 0x01,
		   cAudDMINHStr[(_bAudInfoFm[4] >> 7) & 0x01]);
	hdmi_print
	    ("====================Audio inforFrame End ======================================\n");

	hdmi_print
	    ("====================AVI inforFrame Start ====================================\n");
	hdmi_print("Data Byte (1~5) = 0x%x  0x%x  0x%x  0x%x  0x%x\n", _bAviInfoFm[0],
		   _bAviInfoFm[1], _bAviInfoFm[2], _bAviInfoFm[3], _bAviInfoFm[4]);
	hdmi_print("S1,S0: 0x%x, %s\n", _bAviInfoFm[0] & 0x03, cAviScanStr[_bAviInfoFm[0] & 0x03]);
	hdmi_print("B1,S0: 0x%x, %s\n", (_bAviInfoFm[0] >> 2) & 0x03,
		   cAviBarStr[(_bAviInfoFm[0] >> 2) & 0x03]);
	hdmi_print("A0: 0x%x, %s\n", (_bAviInfoFm[0] >> 4) & 0x01,
		   cAviActivePresentStr[(_bAviInfoFm[0] >> 4) & 0x01]);
	hdmi_print("Y1,Y0: 0x%x, %s\n", (_bAviInfoFm[0] >> 5) & 0x03,
		   cAviRgbYcbcrStr[(_bAviInfoFm[0] >> 5) & 0x03]);
	hdmi_print("R3~R0: 0x%x, %s\n", (_bAviInfoFm[1]) & 0x0f,
		   cAviActiveStr[(_bAviInfoFm[1]) & 0x0f]);
	hdmi_print("M1,M0: 0x%x, %s\n", (_bAviInfoFm[1] >> 4) & 0x03,
		   cAviAspectStr[(_bAviInfoFm[1] >> 4) & 0x03]);
	hdmi_print("C1,C0: 0x%x, %s\n", (_bAviInfoFm[1] >> 6) & 0x03,
		   cAviColorimetryStr[(_bAviInfoFm[1] >> 6) & 0x03]);
	hdmi_print("SC1,SC0: 0x%x, %s\n", (_bAviInfoFm[2]) & 0x03,
		   cAviScaleStr[(_bAviInfoFm[2]) & 0x03]);
	hdmi_print("Q1,Q0: 0x%x, %s\n", (_bAviInfoFm[2] >> 2) & 0x03,
		   cAviRGBRangeStr[(_bAviInfoFm[2] >> 2) & 0x03]);
	if (((_bAviInfoFm[2] >> 4) & 0x07) <= 1)
		hdmi_print("EC2~EC0: 0x%x, %s\n", (_bAviInfoFm[2] >> 4) & 0x07,
			   cAviExtColorimetryStr[(_bAviInfoFm[2] >> 4) & 0x07]);
	else
		hdmi_print("EC2~EC0: resevered\n");
	hdmi_print("ITC: 0x%x, %s\n", (_bAviInfoFm[2] >> 7) & 0x01,
		   cAviItContentStr[(_bAviInfoFm[2] >> 7) & 0x01]);
	hdmi_print
	    ("====================AVI inforFrame End ======================================\n");

	hdmi_print
	    ("====================SPD inforFrame Start ====================================\n");
	hdmi_print("Data Byte (1~8)  = 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
		   _bSpdInf[0], _bSpdInf[1], _bSpdInf[2], _bSpdInf[3], _bSpdInf[4], _bSpdInf[5],
		   _bSpdInf[6], _bSpdInf[7]);
	hdmi_print("Data Byte (9~16) = 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
		   _bSpdInf[8], _bSpdInf[9], _bSpdInf[10], _bSpdInf[11], _bSpdInf[12], _bSpdInf[13],
		   _bSpdInf[14], _bSpdInf[15]);
	hdmi_print("Data Byte (17~24)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
		   _bSpdInf[16], _bSpdInf[17], _bSpdInf[18], _bSpdInf[19], _bSpdInf[20],
		   _bSpdInf[21], _bSpdInf[22], _bSpdInf[23]);
	hdmi_print("Data Byte  25    = 0x%x\n", _bSpdInf[24]);
	hdmi_print("Source Device information is %s\n", cSPDDeviceStr[_bSpdInf[24]]);
	hdmi_print
	    ("====================SPD inforFrame End ======================================\n");

	if (fgIsAcpEnable()) {
		hdmi_print
		    ("====================ACP inforFrame Start ====================================\n");
		hdmi_print("Acp type =0x%x\n", _bAcpType);

		if (_bAcpType == 0) {
			hdmi_print("Generic Audio\n");
			hdmi_print
			    ("Data Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			     _bAcpData[0], _bAcpData[1], _bAcpData[2], _bAcpData[3], _bAcpData[4],
			     _bAcpData[5], _bAcpData[6], _bAcpData[7]);
			hdmi_print
			    ("Data Byte (9~16)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			     _bAcpData[8], _bAcpData[9], _bAcpData[10], _bAcpData[11],
			     _bAcpData[12], _bAcpData[13], _bAcpData[14], _bAcpData[15]);
		} else if (_bAcpType == 1) {
			hdmi_print("IEC 60958-Identified Audio\n");
			hdmi_print
			    ("Data Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			     _bAcpData[0], _bAcpData[1], _bAcpData[2], _bAcpData[3], _bAcpData[4],
			     _bAcpData[5], _bAcpData[6], _bAcpData[7]);
			hdmi_print
			    ("Data Byte (9~16)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			     _bAcpData[8], _bAcpData[9], _bAcpData[10], _bAcpData[11],
			     _bAcpData[12], _bAcpData[13], _bAcpData[14], _bAcpData[15]);
		} else if (_bAcpType == 2) {
			hdmi_print("DVD Audio\n");
			hdmi_print("DVD-AUdio_TYPE_Dependent Generation = 0x%x\n", _bAcpData[0]);
			hdmi_print("Copy Permission = 0x%x\n", (_bAcpData[1] >> 6) & 0x03);
			hdmi_print("Copy Number = 0x%x\n", (_bAcpData[1] >> 3) & 0x07);
			hdmi_print("Quality = 0x%x\n", (_bAcpData[1] >> 1) & 0x03);
			hdmi_print("Transaction = 0x%x\n", _bAcpData[1] & 0x01);

		} else if (_bAcpType == 3) {
			hdmi_print("SuperAudio CD\n");
			hdmi_print
			    ("CCI_1 Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			     _bAcpData[0], _bAcpData[1], _bAcpData[2], _bAcpData[3], _bAcpData[4],
			     _bAcpData[5], _bAcpData[6], _bAcpData[7]);
			hdmi_print
			    ("CCI_1 Byte (9~16)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			     _bAcpData[8], _bAcpData[9], _bAcpData[10], _bAcpData[11],
			     _bAcpData[12], _bAcpData[13], _bAcpData[14], _bAcpData[15]);

		}
		hdmi_print
		    ("====================ACP inforFrame End ======================================\n");
	}

	if (fgIsISRC1Enable()) {
		hdmi_print
		    ("====================ISRC1 inforFrame Start ====================================\n");
		hdmi_print("Data Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			   _bIsrc1Data[0], _bIsrc1Data[1], _bIsrc1Data[2], _bIsrc1Data[3],
			   _bIsrc1Data[4], _bIsrc1Data[5], _bIsrc1Data[6], _bIsrc1Data[7]);
		hdmi_print
		    ("====================ISRC1 inforFrame End ======================================\n");
	}

	if (fgIsISRC2Enable()) {
		hdmi_print
		    ("====================ISRC2 inforFrame Start ====================================\n");
		hdmi_print("Data Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			   _bIsrc1Data[8], _bIsrc1Data[9], _bIsrc1Data[10], _bIsrc1Data[11],
			   _bIsrc1Data[12], _bIsrc1Data[13], _bIsrc1Data[14], _bIsrc1Data[15]);
		hdmi_print
		    ("====================ISRC2 inforFrame End ======================================\n");
	}
}

unsigned int u4ReadNValue(void)
{
	return _u4NValue;
}

unsigned int u4ReadCtsValue(void)
{
	unsigned int u4Data;

	u4Data = bReadByteHdmiGRL(GRL_CTS0) & 0xff;
	u4Data |= ((bReadByteHdmiGRL(GRL_CTS1) & 0xff) << 8);
	u4Data |= ((bReadByteHdmiGRL(GRL_CTS2) & 0x0f) << 16);

	return u4Data;
}

void vShowHdmiAudioStatus(void)
{
	hdmi_print("[HDMI]HDMI output audio Channel Number =%d\n",
		   _stAvdAVInfo.ui1_aud_out_ch_number);
	hdmi_print("[HDMI]HDMI output Audio Fs = %s\n", cHdmiAudFsStr[_stAvdAVInfo.e_hdmi_fs]);
	hdmi_print("[HDMI]HDMI MCLK =%d\n", _stAvdAVInfo.u1HdmiI2sMclk);
	hdmi_print("[HDMI]HDMI output ACR N= %d, CTS = %d\n", u4ReadNValue(), u4ReadCtsValue());
}

void vCheckDGI1CRC(void)
{
	unsigned short u4Data = 0xffff, i;

	for (i = 0; i < 10; i++) {
		vWriteHdmiDGIMsk(dgi1_crc_mon_ctrl, c_crc_clr, c_crc_clr);
		vWriteHdmiDGIMsk(dgi1_crc_mon_ctrl, c_crc_start, c_crc_start);

		mdelay(30);

		if (i == 0)
			u4Data = dReadHdmiDGI(dgi1_crc_out) & 0xffff;
		else {
			if ((u4Data != (dReadHdmiDGI(dgi1_crc_out) & 0xffff)) || (u4Data == 0)) {
				hdmi_print("[HDMI]number = %d, u4Data = 0x%x\n", i, u4Data);
				hdmi_print("[HDMI]dgi crc error\n");
				return;
			}
		}
	}
	hdmi_print("[HDMI]dgi crc pass\n");
}

void vCheckHDMICRC(void)
{
	unsigned short u4Data = 0xffff, i;

	for (i = 0; i < 10; i++) {
		vWriteHdmiGRLMsk(CRC_CTRL, clr_crc_result, clr_crc_result | init_crc);
		vWriteHdmiGRLMsk(CRC_CTRL, init_crc, clr_crc_result | init_crc);

		mdelay(40);

		if (i == 0)
			u4Data =
			    (bReadByteHdmiGRL(CRC_RESULT_L) & 0xff) +
			    ((bReadByteHdmiGRL(CRC_RESULT_H) & 0xff) << 8);
		else {
			if ((u4Data !=
			     ((bReadByteHdmiGRL(CRC_RESULT_L) & 0xff) +
			      ((bReadByteHdmiGRL(CRC_RESULT_H) & 0xff) << 8))) || (u4Data == 0)) {
				hdmi_print("[HDMI]number = %d, u4Data = 0x%x\n", i, u4Data);
				hdmi_print("[HDMI]hdmi crc error\n");
				return;
			}
		}
	}
	hdmi_print("[HDMI]hdmi crc pass\n");
}

void vCheckHDMICLKPIN(void)
{
	unsigned int u4Data, i;

	for (i = 0; i < 5; i++) {
		vWriteHdmiSYSMsk(HDMI_SYS_FMETER, TRI_CAL | CALSEL, TRI_CAL | CALSEL);
		vWriteHdmiSYSMsk(HDMI_SYS_FMETER, CLK_EXC, CLK_EXC);

		while (!(dReadHdmiSYS(HDMI_SYS_FMETER) && CAL_OK))
			;

		u4Data = ((dReadHdmiSYS(HDMI_SYS_FMETER) & (0xffff0000)) >> 16) * 26000 / 1024;

		hdmi_print("[HDMI]hdmi pin clk = %d.%dM\n", (u4Data / 1000),
			   (u4Data - ((u4Data / 1000) * 1000)));
	}
}

void mt8193_hdmistatus(void)
{
	vShowHpdRsenStatus();
	vShowOutputVideoResolution();
	vShowDviOrHdmiMode();
	vShowDeepColor();
	vShowColorSpace();
	vShowInforFrame();
	vShowHdmiAudioStatus();
	vShowEdidRawData();
	vShowEdidInformation();
	vShowHdcpRawData();

	vCheckDGI1CRC();
	vCheckHDMICRC();
	vCheckHDMICLKPIN();

}

#endif
