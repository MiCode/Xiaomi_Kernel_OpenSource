/*
 * HDMI support
 *
 * Copyright (C) 2013 ITE Tech. Inc.
 * Author: Hermes Wu <hermes.wu@ite.com.tw>
 *
 * HDMI TX driver for IT66121
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _HDMITX_DRV_H_
#define _HDMITX_DRV_H_

#include "itx_typedef.h"

/* #define EXTERN_HDCPROM */
/* /////////////////////////////////////// */
/* DDC Address */
/* /////////////////////////////////////// */
#define DDC_HDCP_ADDRESS 0x74
#define DDC_EDID_ADDRESS 0xA0
#define DDC_FIFO_MAXREQ 0x20

/* I2C address */

#define _80MHz 80000000
#define HDMI_TX_I2C_SLAVE_ADDR 0x98
#define CEC_I2C_SLAVE_ADDR 0x9C
/* ///////////////////////////////////////////////////////////////////// */
/* Register offset */
/* ///////////////////////////////////////////////////////////////////// */

#define REG_TX_VENDOR_ID0 0x00
#define REG_TX_VENDOR_ID1 0x01
#define REG_TX_DEVICE_ID0 0x02
#define REG_TX_DEVICE_ID1 0x03
#define FALSE 0
#define TRUE 1

#define O_TX_DEVID 0
#define M_TX_DEVID 0xF
#define O_TX_REVID 4
#define M_TX_REVID 0xF

#define REG_TX_SW_RST 0x04
#define B_TX_ENTEST (1 << 7)
#define B_TX_REF_RST_HDMITX (1 << 5)
#define B_TX_AREF_RST (1 << 4)
#define B_HDMITX_VID_RST (1 << 3)
#define B_HDMITX_AUD_RST (1 << 2)
#define B_TX_HDMI_RST (1 << 1)
#define B_TX_HDCP_RST_HDMITX (1 << 0)

#define REG_TX_INT_CTRL 0x05
#define B_TX_INTPOL_ACTL 0
#define B_TX_INTPOL_ACTH (1 << 7)
#define B_TX_INT_PUSHPULL 0
#define B_TX_INT_OPENDRAIN (1 << 6)

#define REG_TX_INT_STAT1 0x06
#define B_TX_INT_AUD_OVERFLOW (1 << 7)
#define B_TX_INT_ROMACQ_NOACK (1 << 6)
#define B_TX_INT_RDDC_NOACK (1 << 5)
#define B_TX_INT_DDCFIFO_ERR (1 << 4)
#define B_TX_INT_ROMACQ_BUS_HANG (1 << 3)
#define B_TX_INT_DDC_BUS_HANG (1 << 2)
#define B_TX_INT_RX_SENSE (1 << 1)
#define B_TX_INT_HPD_PLUG (1 << 0)

#define REG_TX_INT_STAT2 0x07
#define B_TX_INT_HDCP_SYNC_DET_FAIL (1 << 7)
#define B_TX_INT_VID_UNSTABLE (1 << 6)
#define B_TX_INT_PKTACP (1 << 5)
#define B_TX_INT_PKTNULL (1 << 4)
#define B_TX_INT_PKTGENERAL (1 << 3)
#define B_TX_INT_KSVLIST_CHK (1 << 2)
#define B_TX_INT_AUTH_DONE (1 << 1)
#define B_TX_INT_AUTH_FAIL (1 << 0)

#define REG_TX_INT_STAT3 0x08
#define B_TX_INT_AUD_CTS (1 << 6)
#define B_TX_INT_VSYNC (1 << 5)
#define B_TX_INT_VIDSTABLE (1 << 4)
#define B_TX_INT_PKTMPG (1 << 3)
#define B_TX_INT_PKTSPD (1 << 2)
#define B_TX_INT_PKTAUD (1 << 1)
#define B_TX_INT_PKTAVI (1 << 0)

#define REG_TX_INT_MASK1 0x09
#define B_TX_AUDIO_OVFLW_MASK (1 << 7)
#define B_TX_DDC_NOACK_MASK (1 << 5)
#define B_TX_DDC_FIFO_ERR_MASK (1 << 4)
#define B_TX_DDC_BUS_HANG_MASK (1 << 2)
#define B_TX_RXSEN_MASK (1 << 1)
#define B_TX_HPD_MASK (1 << 0)

#define REG_TX_INT_MASK2 0x0A
#define B_TX_PKT_AVI_MASK (1 << 7)
#define B_TX_PKT_VID_UNSTABLE_MASK (1 << 6)
#define B_TX_PKT_ACP_MASK (1 << 5)
#define B_TX_PKT_NULL_MASK (1 << 4)
#define B_TX_PKT_GEN_MASK (1 << 3)
#define B_TX_KSVLISTCHK_MASK (1 << 2)
#define B_TX_AUTH_DONE_MASK (1 << 1)
#define B_TX_AUTH_FAIL_MASK (1 << 0)

#define REG_TX_INT_MASK3 0x0B
#define B_TX_HDCP_SYNC_DET_FAIL_MASK (1 << 6)
#define B_TX_AUDCTS_MASK (1 << 5)
#define B_TX_VSYNC_MASK (1 << 4)
#define B_TX_VIDSTABLE_MASK (1 << 3)
#define B_TX_PKT_MPG_MASK (1 << 2)
#define B_TX_PKT_SPD_MASK (1 << 1)
#define B_TX_PKT_AUD_MASK (1 << 0)

#define REG_TX_INT_CLR0 0x0C
#define B_TX_CLR_PKTACP (1 << 7)
#define B_TX_CLR_PKTNULL (1 << 6)
#define B_TX_CLR_PKTGENERAL (1 << 5)
#define B_TX_CLR_KSVLISTCHK (1 << 4)
#define B_TX_CLR_AUTH_DONE (1 << 3)
#define B_TX_CLR_AUTH_FAIL (1 << 2)
#define B_TX_CLR_RXSENSE (1 << 1)
#define B_TX_CLR_HPD (1 << 0)

#define REG_TX_INT_CLR1 0x0D
#define B_TX_CLR_VSYNC (1 << 7)
#define B_TX_CLR_VIDSTABLE (1 << 6)
#define B_TX_CLR_PKTMPG (1 << 5)
#define B_TX_CLR_PKTSPD (1 << 4)
#define B_TX_CLR_PKTAUD (1 << 3)
#define B_TX_CLR_PKTAVI (1 << 2)
#define B_TX_CLR_HDCP_SYNC_DET_FAIL (1 << 1)
#define B_TX_CLR_VID_UNSTABLE (1 << 0)

#define REG_TX_SYS_STATUS 0x0E
/* readonly */
#define B_TX_INT_ACTIVE (1 << 7)
#define B_TX_HPDETECT (1 << 6)
#define B_TX_RXSENDETECT (1 << 5)
#define B_TXVIDSTABLE (1 << 4)
/* read/write */
#define O_TX_CTSINTSTEP 2
#define M_TX_CTSINTSTEP (3 << 2)
#define B_TX_CLR_AUD_CTS (1 << 1)
#define B_TX_INTACTDONE (1 << 0)

#define REG_TX_BANK_CTRL 0x0F
#define B_TX_BANK0 0
#define B_TX_BANK1 1

/* DDC */

#define REG_TX_DDC_MASTER_CTRL 0x10
#define B_TX_MASTERROM (1 << 1)
#define B_TX_MASTERDDC (0 << 1)
#define B_TX_MASTERHOST (1 << 0)
#define B_TX_MASTERHDCP (0 << 0)

#define REG_TX_DDC_HEADER 0x11
#define REG_TX_DDC_REQOFF 0x12
#define REG_TX_DDC_REQCOUNT 0x13
#define REG_TX_DDC_EDIDSEG 0x14
#define REG_TX_DDC_CMD 0x15
#define CMD_DDC_SEQ_BURSTREAD 0
#define CMD_LINK_CHKREAD 2
#define CMD_EDID_READ 3
#define CMD_FIFO_CLR 9
#define CMD_GEN_SCLCLK 0xA
#define CMD_DDC_ABORT 0xF

#define REG_TX_DDC_STATUS 0x16
#define B_TX_DDC_DONE (1 << 7)
#define B_TX_DDC_ACT (1 << 6)
#define B_TX_DDC_NOACK (1 << 5)
#define B_TX_DDC_WAITBUS (1 << 4)
#define B_TX_DDC_ARBILOSE (1 << 3)
#define B_TX_DDC_ERROR (B_TX_DDC_NOACK | B_TX_DDC_WAITBUS | B_TX_DDC_ARBILOSE)
#define B_TX_DDC_FIFOFULL (1 << 2)
#define B_TX_DDC_FIFOEMPTY (1 << 1)

#define REG_TX_DDC_READFIFO 0x17
#define REG_TX_ROM_STARTADDR 0x18
#define REG_TX_HDCP_HEADER 0x19
#define REG_TX_ROM_HEADER 0x1A
#define REG_TX_BUSHOLD_T 0x1B
#define REG_TX_ROM_STAT 0x1C
#define B_TX_ROM_DONE (1 << 7)
#define B_TX_ROM_ACTIVE (1 << 6)
#define B_TX_ROM_NOACK (1 << 5)
#define B_TX_ROM_WAITBUS (1 << 4)
#define B_TX_ROM_ARBILOSE (1 << 3)
#define B_TX_ROM_BUSHANG (1 << 2)

/* HDCP */
#define REG_TX_AN_GENERATE 0x1F
#define B_TX_START_CIPHER_GEN 1
#define B_TX_STOP_CIPHER_GEN 0

#define REG_TX_CLK_CTRL0 0x58
#define O_TX_OSCLK_SEL 5
#define M_TX_OSCLK_SEL 3
#define B_TX_AUTO_OVER_SAMPLING_CLOCK (1 << 4)
#define O_TX_EXT_MCLK_SEL 2
#define M_TX_EXT_MCLK_SEL (3 << O_TX_EXT_MCLK_SEL)
#define B_TX_EXT_128FS (0 << O_TX_EXT_MCLK_SEL)
#define B_TX_EXT_256FS (1 << O_TX_EXT_MCLK_SEL)
#define B_TX_EXT_512FS (2 << O_TX_EXT_MCLK_SEL)
#define B_TX_EXT_1024FS (3 << O_TX_EXT_MCLK_SEL)

#define REG_TX_SHA_SEL 0x50
#define REG_TX_SHA_RD_BYTE1 0x51
#define REG_TX_SHA_RD_BYTE2 0x52
#define REG_TX_SHA_RD_BYTE3 0x53
#define REG_TX_SHA_RD_BYTE4 0x54
#define REG_TX_AKSV_RD_BYTE5 0x55

#define REG_TX_CLK_CTRL1 0x59
#define B_TX_EN_TXCLK_COUNT (1 << 5)
#define B_TX_VDO_LATCH_EDGE (1 << 3)

#define REG_TX_CLK_STATUS1 0x5E
#define REG_TX_CLK_STATUS2 0x5F
#define B_TX_IP_LOCK (1 << 7)
#define B_TX_XP_LOCK (1 << 6)
#define B_TX_OSF_LOCK (1 << 5)

#define REG_TX_AUD_COUNT 0x60
#define REG_TX_AFE_DRV_CTRL 0x61

#define B_TX_AFE_DRV_PWD (1 << 5)
#define B_TX_AFE_DRV_RST (1 << 4)

/* Input Data Format Register */
#define REG_TX_INPUT_MODE 0x70
#define O_TX_INCLKDLY 0
#define M_TX_INCLKDLY 3
#define B_TX_INDDR (1 << 2)
#define B_TX_SYNCEMB (1 << 3)
#define B_TX_2X656CLK (1 << 4)
#define B_TX_PCLKDIV2 (1 << 5)
#define M_TX_INCOLMOD (3 << 6)
#define B_TX_IN_RGB 0
#define B_TX_IN_YUV422 (1 << 6)
#define B_TX_IN_YUV444 (2 << 6)

#define REG_TX_TXFIFO_RST 0x71
#define B_TX_ENAVMUTERST 1
#define B_TXFFRST (1 << 1)

#define REG_TX_CSC_CTRL 0x72
#define B_HDMITX_CSC_BYPASS 0
#define B_HDMITX_CSC_RGB2YUV 2
#define B_HDMITX_CSC_YUV2RGB 3
#define M_TX_CSC_SEL 3
#define B_TX_EN_DITHER (1 << 7)
#define B_TX_EN_UDFILTER (1 << 6)
#define B_TX_DNFREE_GO (1 << 5)

#define SIZEOF_CSCMTX 21
#define SIZEOF_CSCGAIN 6
#define SIZEOF_CSCOFFSET 3

#define REG_TX_CSC_YOFF 0x73
#define REG_TX_CSC_COFF 0x74
#define REG_TX_CSC_RGBOFF 0x75

#define REG_TX_CSC_MTX11_L 0x76
#define REG_TX_CSC_MTX11_H 0x77
#define REG_TX_CSC_MTX12_L 0x78
#define REG_TX_CSC_MTX12_H 0x79
#define REG_TX_CSC_MTX13_L 0x7A
#define REG_TX_CSC_MTX13_H 0x7B
#define REG_TX_CSC_MTX21_L 0x7C
#define REG_TX_CSC_MTX21_H 0x7D
#define REG_TX_CSC_MTX22_L 0x7E
#define REG_TX_CSC_MTX22_H 0x7F
#define REG_TX_CSC_MTX23_L 0x80
#define REG_TX_CSC_MTX23_H 0x81
#define REG_TX_CSC_MTX31_L 0x82
#define REG_TX_CSC_MTX31_H 0x83
#define REG_TX_CSC_MTX32_L 0x84
#define REG_TX_CSC_MTX32_H 0x85
#define REG_TX_CSC_MTX33_L 0x86
#define REG_TX_CSC_MTX33_H 0x87

#define REG_TX_CSC_GAIN1V_L 0x88
#define REG_TX_CSC_GAIN1V_H 0x89
#define REG_TX_CSC_GAIN2V_L 0x8A
#define REG_TX_CSC_GAIN2V_H 0x8B
#define REG_TX_CSC_GAIN3V_L 0x8C
#define REG_TX_CSC_GAIN3V_H 0x8D

#define REG_TX_HVPol 0x90
#define REG_TX_HfPixel 0x91
#define REG_TX_HSSL 0x95
#define REG_TX_HSEL 0x96
#define REG_TX_HSH 0x97
#define REG_TX_VSS1 0xA0
#define REG_TX_VSE1 0xA1
#define REG_TX_VSS2 0xA2
#define REG_TX_VSE2 0xA3

/* HDMI General Control Registers */

#define REG_TX_HDMI_MODE 0xC0
#define B_TX_HDMI_MODE 1
#define B_TX_DVI_MODE 0
#define REG_TX_AV_MUTE 0xC1
#define REG_TX_GCP 0xC1
#define B_TX_CLR_AVMUTE 0
#define B_TX_SET_AVMUTE 1
#define B_TX_SETAVMUTE (1 << 0)
#define B_TX_BLUE_SCR_MUTE (1 << 1)
#define B_TX_NODEF_PHASE (1 << 2)
#define B_TX_PHASE_RESYNC (1 << 3)

#define O_TX_COLOR_DEPTH 4
#define M_TX_COLOR_DEPTH 7
#define B_TX_COLOR_DEPTH_MASK (M_TX_COLOR_DEPTH << O_TX_COLOR_DEPTH)
#define B_TX_CD_NODEF 0
#define B_TX_CD_24 (4 << 4)
#define B_TX_CD_30 (5 << 4)
#define B_TX_CD_36 (6 << 4)
#define B_TX_CD_48 (7 << 4)
#define REG_TX_PKT_GENERAL_CTRL 0xC6

#define REG_TX_OESS_CYCLE 0xC3

/* /////////////////////////////////////////////////////////////////// */
/* Macro */
/* /////////////////////////////////////////////////////////////////// */
#define Switch_HDMITX_Bank(x) HDMITX_SetI2C_Byte(0x0f, 1, (x)&1)
#define HDMITX_OrReg_Byte(reg, ormask)                                         \
	HDMITX_SetI2C_Byte(reg, (ormask), (ormask))
#define HDMITX_AndReg_Byte(reg, andmask)                                       \
	HDMITX_WriteI2C_Byte(reg, (HDMITX_ReadI2C_Byte(reg) & (andmask)))

/* /////////////////////////////////////////////////////////////////// */
/* data structure */
/* /////////////////////////////////////////////////////////////////// */
struct _HDMITXDEV {

	unsigned char I2C_DEV;
	unsigned char I2C_ADDR;

	/* /////////////////////////////////////////////// */
	/* Interrupt Type */
	/* /////////////////////////////////////////////// */
	unsigned char bIntType; /* = 0 ; */
	/* /////////////////////////////////////////////// */
	/* Video Property */
	/* /////////////////////////////////////////////// */
	unsigned char
		bInputVideoSignalType; /* for Sync Embedded,CCIR656,InputDDR */
	/* /////////////////////////////////////////////// */
	/* Audio Property */
	/* /////////////////////////////////////////////// */
	unsigned char bOutputAudioMode;  /* = 0 ; */
	unsigned char bAudioChannelSwap; /* = 0 ; */
	unsigned char bAudioChannelEnable;
	unsigned char bAudFs;
	unsigned long TMDSClock;
	unsigned long RCLK;
	unsigned char bAuthenticated : 1;
	unsigned char bHDMIMode : 1;
	unsigned char bIntPOL : 1; /* 0 = Low Active */
	unsigned char bHPD : 1;
	/* 2009/11/11 added by jj_tseng@ite.com.tw */
	unsigned char bSPDIF_OUT;
	unsigned char TxEMEMStatus : 1;
	/* ~jau-chih.tseng@ite.com.tw 2009/11/11 */
};
#define HDMITXDEV struct _HDMITXDEV

/* 2008/02/27 added by jj_tseng@chipadvanced.com */
enum _MODE_ID {
	UNKNOWN_MODE = 0,
	CEA_640x480p60,
	CEA_720x480p60,
	CEA_1280x720p60,
	CEA_1920x1080i60,
	CEA_720x480i60,
	CEA_720x240p60,
	CEA_1440x480i60,
	CEA_1440x240p60,
	CEA_2880x480i60,
	CEA_2880x240p60,
	CEA_1440x480p60,
	CEA_1920x1080p60,
	CEA_720x576p50,
	CEA_1280x720p50,
	CEA_1920x1080i50,
	CEA_720x576i50,
	CEA_1440x576i50,
	CEA_720x288p50,
	CEA_1440x288p50,
	CEA_2880x576i50,
	CEA_2880x288p50,
	CEA_1440x576p50,
	CEA_1920x1080p50,
	CEA_1920x1080p24,
	CEA_1920x1080p25,
	CEA_1920x1080p30,
	VESA_640x350p85,
	VESA_640x400p85,
	VESA_720x400p85,
	VESA_640x480p60,
	VESA_640x480p72,
	VESA_640x480p75,
	VESA_640x480p85,
	VESA_800x600p56,
	VESA_800x600p60,
	VESA_800x600p72,
	VESA_800x600p75,
	VESA_800X600p85,
	VESA_840X480p60,
	VESA_1024x768p60,
	VESA_1024x768p70,
	VESA_1024x768p75,
	VESA_1024x768p85,
	VESA_1152x864p75,
	VESA_1280x768p60R,
	VESA_1280x768p60,
	VESA_1280x768p75,
	VESA_1280x768p85,
	VESA_1280x960p60,
	VESA_1280x960p85,
	VESA_1280x1024p60,
	VESA_1280x1024p75,
	VESA_1280X1024p85,
	VESA_1360X768p60,
	VESA_1400x768p60R,
	VESA_1400x768p60,
	VESA_1400x1050p75,
	VESA_1400x1050p85,
	VESA_1440x900p60R,
	VESA_1440x900p60,
	VESA_1440x900p75,
	VESA_1440x900p85,
	VESA_1600x1200p60,
	VESA_1600x1200p65,
	VESA_1600x1200p70,
	VESA_1600x1200p75,
	VESA_1600x1200p85,
	VESA_1680x1050p60R,
	VESA_1680x1050p60,
	VESA_1680x1050p75,
	VESA_1680x1050p85,
	VESA_1792x1344p60,
	VESA_1792x1344p75,
	VESA_1856x1392p60,
	VESA_1856x1392p75,
	VESA_1920x1200p60R,
	VESA_1920x1200p60,
	VESA_1920x1200p75,
	VESA_1920x1200p85,
	VESA_1920x1440p60,
	VESA_1920x1440p75,
};
#define MODE_ID enum _MODE_ID

/* ~jj_tseng@chipadvanced.com */

struct _RegSetEntry {
	unsigned char offset;
	unsigned char invAndMask;
	unsigned char OrMask;
};
#define RegSetEntry struct _RegSetEntry

#include "hdmitx_hdcp.h"
#include "hdmitx_input.h"

/* Audio Channel Control */
#define REG_TX_AUDIO_CTRL0 0xE0
#define M_TX_AUD_SWL (3 << 6)
#define M_TX_AUD_16BIT (0 << 6)
#define M_TX_AUD_18BIT (1 << 6)
#define M_TX_AUD_20BIT (2 << 6)
#define M_TX_AUD_24BIT (3 << 6)

#define B_TX_SPDIFTC (1 << 5)

#define B_TX_AUD_SPDIF (1 << 4)
#define B_TX_AUD_I2S (0 << 4)
#define B_TX_AUD_EN_I2S3 (1 << 3)
#define B_TX_AUD_EN_I2S2 (1 << 2)
#define B_TX_AUD_EN_I2S1 (1 << 1)
#define B_TX_AUD_EN_I2S0 (1 << 0)
#define B_TX_AUD_EN_SPDIF 1

#define REG_TX_AUDIO_CTRL1 0xE1
#define B_TX_AUD_FULLPKT (1 << 6)

#define B_TX_AUDFMT_STD_I2S (0 << 0)
#define B_TX_AUDFMT_32BIT_I2S (1 << 0)
#define B_TX_AUDFMT_LEFT_JUSTIFY (0 << 1)
#define B_TX_AUDFMT_RIGHT_JUSTIFY (1 << 1)
#define B_TX_AUDFMT_DELAY_1T_TO_WS (0 << 2)
#define B_TX_AUDFMT_NO_DELAY_TO_WS (1 << 2)
#define B_TX_AUDFMT_WS0_LEFT (0 << 3)
#define B_TX_AUDFMT_WS0_RIGHT (1 << 3)
#define B_TX_AUDFMT_MSB_SHIFT_FIRST (0 << 4)
#define B_TX_AUDFMT_LSB_SHIFT_FIRST (1 << 4)
#define B_TX_AUDFMT_RISE_EDGE_SAMPLE_WS (0 << 5)
#define B_TX_AUDFMT_FALL_EDGE_SAMPLE_WS (1 << 5)

#define REG_TX_AUDIO_FIFOMAP 0xE2
#define O_TX_FIFO3SEL 6
#define O_TX_FIFO2SEL 4
#define O_TX_FIFO1SEL 2
#define O_TX_FIFO0SEL 0
#define B_TX_SELSRC3 3
#define B_TX_SELSRC2 2
#define B_TX_SELSRC1 1
#define B_TX_SELSRC0 0

#define REG_TX_AUDIO_CTRL3 0xE3
#define B_TX_AUD_MULCH (1 << 7)
#define B_TX_EN_ZERO_CTS (1 << 6)
#define B_TX_CHSTSEL (1 << 4)
#define B_TX_S3RLCHG (1 << 3)
#define B_TX_S2RLCHG (1 << 2)
#define B_TX_S1RLCHG (1 << 1)
#define B_TX_S0RLCHG (1 << 0)

#define REG_TX_AUD_SRCVALID_FLAT 0xE4
#define B_TX_AUD_SPXFLAT_SRC3 (1 << 7)
#define B_TX_AUD_SPXFLAT_SRC2 (1 << 6)
#define B_TX_AUD_SPXFLAT_SRC1 (1 << 5)
#define B_TX_AUD_SPXFLAT_SRC0 (1 << 4)
#define B_TX_AUD_ERR2FLAT (1 << 3)
#define B_TX_AUD_S3VALID (1 << 2)
#define B_TX_AUD_S2VALID (1 << 1)
#define B_TX_AUD_S1VALID (1 << 0)

#define REG_TX_AUD_HDAUDIO 0xE5
#define B_TX_HBR (1 << 3)
#define B_TX_DSD (1 << 1)

/* //////////////////////////////////////// */
/* Bank 1 */
/* //////////////////////////////////////// */

#define REGPktAudCTS0 0x30    /* 7:0 */
#define REGPktAudCTS1 0x31    /* 15:8 */
#define REGPktAudCTS2 0x32    /* 19:16 */
#define REGPktAudN0 0x33      /* 7:0 */
#define REGPktAudN1 0x34      /* 15:8 */
#define REGPktAudN2 0x35      /* 19:16 */
#define REGPktAudCTSCnt0 0x35 /* 3:0 */
#define REGPktAudCTSCnt1 0x36 /* 11:4 */
#define REGPktAudCTSCnt2 0x37 /* 19:12 */

#define REG_TX_AUDCHST_MODE 0x91    /* 191 REG_TX_AUD_CHSTD[2:0] 6:4 */
				    /* REG_TX_AUD_CHSTC 3 */
				    /* REG_TX_AUD_NLPCM 2 */
				    /* REG_TX_AUD_MONO 0 */
#define REG_TX_AUDCHST_CAT 0x92     /* 192 REG_TX_AUD_CHSTCAT 7:0 */
#define REG_TX_AUDCHST_SRCNUM 0x93  /* 193 REG_TX_AUD_CHSTSRC 3:0 */
#define REG_TX_AUD0CHST_CHTNUM 0x94 /* 194 REG_TX_AUD0_CHSTCHR 7:4 */
				    /* REG_TX_AUD0_CHSTCHL 3:0 */
#define REG_TX_AUD1CHST_CHTNUM 0x95 /* 195 REG_TX_AUD1_CHSTCHR 7:4 */
				    /* REG_TX_AUD1_CHSTCHL 3:0 */
#define REG_TX_AUD2CHST_CHTNUM 0x96 /* 196 REG_TX_AUD2_CHSTCHR 7:4 */
				    /* REG_TX_AUD2_CHSTCHL 3:0 */
#define REG_TX_AUD3CHST_CHTNUM 0x97 /* 197 REG_TX_AUD3_CHSTCHR 7:4 */
				    /* REG_TX_AUD3_CHSTCHL 3:0 */
#define REG_TX_AUDCHST_CA_FS 0x98   /* 198 REG_TX_AUD_CHSTCA 5:4 */
				    /* REG_TX_AUD_CHSTFS 3:0 */
#define REG_TX_AUDCHST_OFS_WL 0x99  /* 199 REG_TX_AUD_CHSTOFS 7:4 */
				    /* REG_TX_AUD_CHSTWL 3:0 */

#define REG_TX_PKT_SINGLE_CTRL 0xC5
#define B_TX_SINGLE_PKT 1
#define B_TX_BURST_PKT
#define B_TX_SW_CTS (1 << 1)

#define REG_TX_NULL_CTRL 0xC9
#define REG_TX_ACP_CTRL 0xCA
#define REG_TX_ISRC1_CTRL 0xCB
#define REG_TX_ISRC2_CTRL 0xCC
#define REG_TX_AVI_INFOFRM_CTRL 0xCD
#define REG_TX_AUD_INFOFRM_CTRL 0xCE
#define REG_TX_SPD_INFOFRM_CTRL 0xCF
#define REG_TX_MPG_INFOFRM_CTRL 0xD0
#define B_TX_ENABLE_PKT 1
#define B_TX_REPEAT_PKT (1 << 1)

#define REG_TX_3D_INFO_CTRL 0xD2

/* //////////////////////////////////////// */
/* COMMON PACKET for NULL,ISRC1,ISRC2,SPD */
/* //////////////////////////////////////// */

#define REG_TX_PKT_HB00 0x38
#define REG_TX_PKT_HB01 0x39
#define REG_TX_PKT_HB02 0x3A

#define REG_TX_PKT_PB00 0x3B
#define REG_TX_PKT_PB01 0x3C
#define REG_TX_PKT_PB02 0x3D
#define REG_TX_PKT_PB03 0x3E
#define REG_TX_PKT_PB04 0x3F
#define REG_TX_PKT_PB05 0x40
#define REG_TX_PKT_PB06 0x41
#define REG_TX_PKT_PB07 0x42
#define REG_TX_PKT_PB08 0x43
#define REG_TX_PKT_PB09 0x44
#define REG_TX_PKT_PB10 0x45
#define REG_TX_PKT_PB11 0x46
#define REG_TX_PKT_PB12 0x47
#define REG_TX_PKT_PB13 0x48
#define REG_TX_PKT_PB14 0x49
#define REG_TX_PKT_PB15 0x4A
#define REG_TX_PKT_PB16 0x4B
#define REG_TX_PKT_PB17 0x4C
#define REG_TX_PKT_PB18 0x4D
#define REG_TX_PKT_PB19 0x4E
#define REG_TX_PKT_PB20 0x4F
#define REG_TX_PKT_PB21 0x50
#define REG_TX_PKT_PB22 0x51
#define REG_TX_PKT_PB23 0x52
#define REG_TX_PKT_PB24 0x53
#define REG_TX_PKT_PB25 0x54
#define REG_TX_PKT_PB26 0x55
#define REG_TX_PKT_PB27 0x56

#define REG_TX_AVIINFO_DB1 0x58
#define REG_TX_AVIINFO_DB2 0x59
#define REG_TX_AVIINFO_DB3 0x5A
#define REG_TX_AVIINFO_DB4 0x5B
#define REG_TX_AVIINFO_DB5 0x5C
#define REG_TX_AVIINFO_DB6 0x5E
#define REG_TX_AVIINFO_DB7 0x5F
#define REG_TX_AVIINFO_DB8 0x60
#define REG_TX_AVIINFO_DB9 0x61
#define REG_TX_AVIINFO_DB10 0x62
#define REG_TX_AVIINFO_DB11 0x63
#define REG_TX_AVIINFO_DB12 0x64
#define REG_TX_AVIINFO_DB13 0x65
#define REG_TX_AVIINFO_SUM 0x5D

#define REG_TX_PKT_AUDINFO_CC 0x68 /* [2:0] */
#define REG_TX_PKT_AUDINFO_SF 0x69 /* [4:2] */
#define REG_TX_PKT_AUDINFO_CA 0x6B /* [7:0] */

#define REG_TX_PKT_AUDINFO_DM_LSV 0x6C /* [7][6:3] */
#define REG_TX_PKT_AUDINFO_SUM 0x6D    /* [7:0] */

/* Source Product Description Info Frame */
#define REG_TX_PKT_SPDINFO_SUM 0x70
#define REG_TX_PKT_SPDINFO_PB1 0x71
#define REG_TX_PKT_SPDINFO_PB2 0x72
#define REG_TX_PKT_SPDINFO_PB3 0x73
#define REG_TX_PKT_SPDINFO_PB4 0x74
#define REG_TX_PKT_SPDINFO_PB5 0x75
#define REG_TX_PKT_SPDINFO_PB6 0x76
#define REG_TX_PKT_SPDINFO_PB7 0x77
#define REG_TX_PKT_SPDINFO_PB8 0x78
#define REG_TX_PKT_SPDINFO_PB9 0x79
#define REG_TX_PKT_SPDINFO_PB10 0x7A
#define REG_TX_PKT_SPDINFO_PB11 0x7B
#define REG_TX_PKT_SPDINFO_PB12 0x7C
#define REG_TX_PKT_SPDINFO_PB13 0x7D
#define REG_TX_PKT_SPDINFO_PB14 0x7E
#define REG_TX_PKT_SPDINFO_PB15 0x7F
#define REG_TX_PKT_SPDINFO_PB16 0x80
#define REG_TX_PKT_SPDINFO_PB17 0x81
#define REG_TX_PKT_SPDINFO_PB18 0x82
#define REG_TX_PKT_SPDINFO_PB19 0x83
#define REG_TX_PKT_SPDINFO_PB20 0x84
#define REG_TX_PKT_SPDINFO_PB21 0x85
#define REG_TX_PKT_SPDINFO_PB22 0x86
#define REG_TX_PKT_SPDINFO_PB23 0x87
#define REG_TX_PKT_SPDINFO_PB24 0x88
#define REG_TX_PKT_SPDINFO_PB25 0x89

#define REG_TX_PKT_MPGINFO_FMT 0x8A
#define B_TX_MPG_FR 1
#define B_TX_MPG_MF_I (1 << 1)
#define B_TX_MPG_MF_B (2 << 1)
#define B_TX_MPG_MF_P (3 << 1)
#define B_TX_MPG_MF_MASK (3 << 1)
#define REG_TX_PKG_MPGINFO_DB0 0x8B
#define REG_TX_PKG_MPGINFO_DB1 0x8C
#define REG_TX_PKG_MPGINFO_DB2 0x8D
#define REG_TX_PKG_MPGINFO_DB3 0x8E
#define REG_TX_PKG_MPGINFO_SUM 0x8F

#define Frame_Pcaking 0
#define Top_and_Botton 6
#define Side_by_Side 8

/* ////////////////////////////////////////////////// */
/* Function Prototype */
/* ////////////////////////////////////////////////// */
#define hdmitx_ENABLE_NULL_PKT()                                               \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_NULL_CTRL,                         \
				     B_TX_ENABLE_PKT | B_TX_REPEAT_PKT);       \
	}
#define hdmitx_ENABLE_ACP_PKT()                                                \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_ACP_CTRL,                          \
				     B_TX_ENABLE_PKT | B_TX_REPEAT_PKT);       \
	}
#define hdmitx_ENABLE_ISRC1_PKT()                                              \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_ISRC1_CTRL,                        \
				     B_TX_ENABLE_PKT | B_TX_REPEAT_PKT);       \
	}
#define hdmitx_ENABLE_ISRC2_PKT()                                              \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_ISRC2_CTRL,                        \
				     B_TX_ENABLE_PKT | B_TX_REPEAT_PKT);       \
	}
#define hdmitx_ENABLE_AVI_INFOFRM_PKT()                                        \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_AVI_INFOFRM_CTRL,                  \
				     B_TX_ENABLE_PKT | B_TX_REPEAT_PKT);       \
	}
#define hdmitx_ENABLE_AUD_INFOFRM_PKT()                                        \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_AUD_INFOFRM_CTRL,                  \
				     B_TX_ENABLE_PKT | B_TX_REPEAT_PKT);       \
	}
#define hdmitx_ENABLE_SPD_INFOFRM_PKT()                                        \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_SPD_INFOFRM_CTRL,                  \
				     B_TX_ENABLE_PKT | B_TX_REPEAT_PKT);       \
	}
#define hdmitx_ENABLE_MPG_INFOFRM_PKT()                                        \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_MPG_INFOFRM_CTRL,                  \
				     B_TX_ENABLE_PKT | B_TX_REPEAT_PKT);       \
	}
#define hdmitx_ENABLE_GeneralPurpose_PKT()                                     \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_NULL_CTRL,                         \
				     B_TX_ENABLE_PKT | B_TX_REPEAT_PKT);       \
	}
#define hdmitx_DISABLE_VSDB_PKT()                                              \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_3D_INFO_CTRL, 0);                  \
	}
#define hdmitx_DISABLE_NULL_PKT()                                              \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_NULL_CTRL, 0);                     \
	}
#define hdmitx_DISABLE_ACP_PKT()                                               \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_ACP_CTRL, 0);                      \
	}
#define hdmitx_DISABLE_ISRC1_PKT()                                             \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_ISRC1_CTRL, 0);                    \
	}
#define hdmitx_DISABLE_ISRC2_PKT()                                             \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_ISRC2_CTRL, 0);                    \
	}
#define hdmitx_DISABLE_AVI_INFOFRM_PKT()                                       \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_AVI_INFOFRM_CTRL, 0);              \
	}
#define hdmitx_DISABLE_AUD_INFOFRM_PKT()                                       \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_AUD_INFOFRM_CTRL, 0);              \
	}
#define hdmitx_DISABLE_SPD_INFOFRM_PKT()                                       \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_SPD_INFOFRM_CTRL, 0);              \
	}
#define hdmitx_DISABLE_MPG_INFOFRM_PKT()                                       \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_MPG_INFOFRM_CTRL, 0);              \
	}
#define hdmitx_DISABLE_GeneralPurpose_PKT()                                    \
	{                                                                      \
		HDMITX_WriteI2C_Byte(REG_TX_NULL_CTRL, 0);                     \
	}


/* External Interface */

enum _VIDEOPCLKLEVEL { PCLK_LOW = 0, PCLK_MEDIUM, PCLK_HIGH };
#define VIDEOPCLKLEVEL enum _VIDEOPCLKLEVEL

/* 2008/08/18 added by jj_tseng@chipadvanced.com */

/* HDMITX function prototype */
void InitHDMITX(void);
void HDMITX_InitTxDev(const HDMITXDEV *pInstance);
unsigned char CheckHDMITX(unsigned char *pHPD, unsigned char *pHPDChange);
bool getHDMITX_LinkStatus(void);
void HDMITX_PowerOn(void);
void HDMITX_PowerDown(void);

void hdmitx_LoadRegSetting(RegSetEntry table[]);

/* HDMITX video function prototype */

void HDMITX_DisableVideoOutput(void);
bool HDMITX_EnableVideoOutput(VIDEOPCLKLEVEL level,
			      unsigned char inputColorMode,
			      unsigned char outputColorMode,
			      unsigned char bHDMI);
bool setHDMITX_VideoSignalType(unsigned char inputSignalType);
void setHDMITX_ColorDepthPhase(unsigned char ColorDepth, unsigned char bPhase);
void hdmitx_LoadRegSetting(RegSetEntry table[]);

extern HDMITXDEV hdmiTxDev[1];

void hdmitx_SetInputMode(unsigned char InputMode,
			 unsigned char bInputSignalType);
void hdmitx_SetCSCScale(unsigned char bInputMode, unsigned char bOutputMode);
void hdmitx_SetupAFE(VIDEOPCLKLEVEL PCLKLevel);
void hdmitx_FireAFE(void);

#ifndef DISABLE_HDMITX_CSC
#if (defined(SUPPORT_OUTPUTYUV)) && (defined(SUPPORT_INPUTRGB))
extern unsigned char bCSCMtx_RGB2YUV_ITU601_16_235[];
extern unsigned char bCSCMtx_RGB2YUV_ITU601_0_255[];
extern unsigned char bCSCMtx_RGB2YUV_ITU709_16_235[];
extern unsigned char bCSCMtx_RGB2YUV_ITU709_0_255[];
#endif

#if (defined(SUPPORT_OUTPUTRGB)) && (defined(SUPPORT_INPUTYUV))
extern unsigned char bCSCMtx_YUV2RGB_ITU601_16_235[];
extern unsigned char bCSCMtx_YUV2RGB_ITU601_0_255[];
extern unsigned char bCSCMtx_YUV2RGB_ITU709_16_235[];
extern unsigned char bCSCMtx_YUV2RGB_ITU709_0_255[];

#endif
#endif /* DISABLE_HDMITX_CSC */

/* TBD ... */
/* #ifdef SUPPORT_DEGEN */
/* bool ProgramDEGenModeByID(MODE_ID id,unsigned char bInputSignalType); */
/* #endif // SUPPORT_DEGEN */

#ifdef SUPPORT_SYNCEMBEDDED
bool setHDMITX_SyncEmbeddedByVIC(unsigned char VIC,
				 unsigned char bInputSignalType);
#endif

void hdmitx_SetInputMode(unsigned char InputMode,
			 unsigned char bInputSignalType);
void hdmitx_SetCSCScale(unsigned char bInputMode, unsigned char bOutputMode);
void hdmitx_SetupAFE(VIDEOPCLKLEVEL level);
void hdmitx_FireAFE(void);


/* HDMITX audio function prototype */

void HDMITX_DisableAudioOutput(void);
void HDMITX_EnableAudioOutput(unsigned char AudioType, bool bSPDIF,
			      ULONG SampleFreq, unsigned char ChNum,
			      unsigned char *pIEC60958ChStat, ULONG TMDSClock);

void setHDMITX_AudioChannelEnable(bool EnableAudio_b);
void setHDMITX_ChStat(unsigned char ucIEC60958ChStat[]);
void setHDMITX_DSDAudio(void);
void setHDMITX_HBRAudio(bool bSPDIF);
void setHDMITX_LPCMAudio(unsigned char AudioSrcNum, unsigned char AudSWL,
			 bool bSPDIF);
void setHDMITX_NCTS(unsigned char Fs);
void setHDMITX_NLPCMAudio(bool bSPDIF);
void setHDMITX_UpdateChStatFs(ULONG Fs);

bool hdmitx_IsAudioChang(void);
void hdmitx_AutoAdjustAudio(void);


/* HDMITX hdcp function prototype */

bool HDMITX_EnableHDCP(unsigned char bEnable);
bool getHDMITX_AuthenticationDone(void);


/* HDMITX pkt/infoframe function prototype */

void setHDMITX_AVMute(unsigned char bEnable);
bool HDMITX_EnableAVIInfoFrame(unsigned char bEnable,
			       unsigned char *pAVIInfoFrame);
bool HDMITX_EnableAudioInfoFrame(unsigned char bEnable,
				 unsigned char *pAudioInfoFrame);
bool HDMITX_EnableVSInfoFrame(unsigned char bEnable,
			      unsigned char *pVSInfoFrame);

SYS_STATUS hdmitx_SetAVIInfoFrame(AVI_InfoFrame *pAVIInfoFrame);
SYS_STATUS hdmitx_SetAudioInfoFrame(Audio_InfoFrame *pAudioInfoFrame);
SYS_STATUS hdmitx_SetSPDInfoFrame(SPD_InfoFrame *pSPDInfoFrame);
SYS_STATUS hdmitx_SetMPEGInfoFrame(MPEG_InfoFrame *pMPGInfoFrame);
SYS_STATUS hdmitx_SetVSIInfoFrame(VendorSpecific_InfoFrame *pVSIInfoFrame);
SYS_STATUS hdmitx_Set_GeneralPurpose_PKT(unsigned char *pData);

/* HDMITX ddc/edid function prototype */

bool getHDMITX_EDIDBlock(int EDIDBlockID, unsigned char *pEDIDData);
SYS_STATUS getHDMITX_EDIDBytes(unsigned char *pData, unsigned char bSegment,
			       unsigned char offset, short Count);

void hdmitx_GenerateDDCSCLK(void);
void hdmitx_ClearDDCFIFO(void);
void hdmitx_AbortDDC(void);

#if 1 /* defined(Debug_message) && (Debug_message==1) */
void DumpHDMITXReg(void);
#else
#define DumpHDMITXReg()
#endif


/* Required Interfance */

unsigned char HDMITX_ReadI2C_Byte(unsigned char RegAddr);
SYS_STATUS HDMITX_WriteI2C_Byte(unsigned char RegAddr, unsigned char d);
SYS_STATUS HDMITX_ReadI2C_ByteN(unsigned char RegAddr, unsigned char *pData,
				int N);
SYS_STATUS HDMITX_WriteI2C_ByteN(unsigned char RegAddr, unsigned char *pData,
				 int N);
SYS_STATUS HDMITX_SetI2C_Byte(unsigned char Reg, unsigned char Mask,
			      unsigned char Value);
SYS_STATUS HDMITX_ToggleBit(unsigned char Reg, unsigned char n);

#endif /* _HDMITX_DRV_H_ */
