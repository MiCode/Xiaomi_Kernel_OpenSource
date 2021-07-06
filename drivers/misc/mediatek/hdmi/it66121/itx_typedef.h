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
#include "hdmitx.h"
#ifndef _TYPEDEF_H_
#define _TYPEDEF_H_

#if defined(CONFIG_ARCH_MT6575)
#include <mach/mt6575_typedefs.h>
#endif
#if defined(CONFIG_ARCH_MT6577)
/* #include <mach/mt_typedefs.h> */
#endif
#if defined(CONFIG_ARCH_MT6589)
/* #include <mach/mt_typedefs.h> */
#endif

#if defined(CONFIG_ARCH_MT6592)
/* #include <mach/mt_typedefs.h> */
#endif

/* //////////////////////////////////////////////// */
/* data type */
/* //////////////////////////////////////////////// */

#define _CODE const

#ifndef ULONG
#define ULONG unsigned long
#endif
#ifndef WORD
#define WORD unsigned short
#endif

#define SUCCESS 0
#define FAIL -1

#define ON 1
#define OFF 0

#define LO_ACTIVE TRUE
#define HI_ACTIVE FALSE

enum _SYS_STATUS { ER_SUCCESS = 0, ER_FAIL, ER_RESERVED };
#define SYS_STATUS enum _SYS_STATUS

#define ABS(x) (((x) >= 0) ? (x) : (-(x)))

/* ///////////////////////////////////////////////////////////////////// */
/* Video Data Type */
/* ///////////////////////////////////////////////////////////////////// */

#define F_MODE_RGB444 0
#define F_MODE_YUV422 1
#define F_MODE_YUV444 2
#define F_MODE_CLRMOD_MASK 3

#define F_MODE_INTERLACE 1

#define F_VIDMODE_ITU709 (1 << 4)
#define F_VIDMODE_ITU601 0

#define F_VIDMODE_0_255 0
#define F_VIDMODE_16_235 (1 << 5)

#define F_VIDMODE_EN_UDFILT (1 << 6)
#define F_VIDMODE_EN_DITHER (1 << 7)

#define T_MODE_CCIR656 (1 << 0)
#define T_MODE_SYNCEMB (1 << 1)
#define T_MODE_INDDR (1 << 2)
#define T_MODE_PCLKDIV2 (1 << 3)
#define T_MODE_DEGEN (1 << 4)
#define T_MODE_SYNCGEN (1 << 5)
/* /////////////////////////////////////////////////////////////////// */
/* Packet and Info Frame definition and datastructure. */
/* /////////////////////////////////////////////////////////////////// */

#define VENDORSPEC_INFOFRAME_TYPE 0x81
#define AVI_INFOFRAME_TYPE 0x82
#define SPD_INFOFRAME_TYPE 0x83
#define AUDIO_INFOFRAME_TYPE 0x84
#define MPEG_INFOFRAME_TYPE 0x85

#define VENDORSPEC_INFOFRAME_VER 0x01
#define AVI_INFOFRAME_VER 0x02
#define SPD_INFOFRAME_VER 0x01
#define AUDIO_INFOFRAME_VER 0x01
#define MPEG_INFOFRAME_VER 0x01

#define VENDORSPEC_INFOFRAME_LEN 5
#define AVI_INFOFRAME_LEN 13
#define SPD_INFOFRAME_LEN 25
#define AUDIO_INFOFRAME_LEN 10
#define MPEG_INFOFRAME_LEN 10

#define ACP_PKT_LEN 9
#define ISRC1_PKT_LEN 16
#define ISRC2_PKT_LEN 16
/*#define unsigned char unsigned char*/

union _VendorSpecific_InfoFrame {
	struct {
		unsigned char Type;
		unsigned char Ver;
		unsigned char Len;

		unsigned char CheckSum;

		unsigned char IEEE_0; /* PB1 */
		unsigned char IEEE_1; /* PB2 */
		unsigned char IEEE_2; /* PB3 */

		unsigned char Rsvd : 5; /* PB4 */
		unsigned char HDMI_Video_Format : 3;

		unsigned char Reserved_PB5 : 4; /* PB5 */
		unsigned char _3D_Structure : 4;

		unsigned char Reserved_PB6 : 4; /* PB6 */
		unsigned char _3D_Ext_Data : 4;
	} info;
	struct {
		unsigned char VS_HB[3];
		unsigned char CheckSum;
		unsigned char VS_DB[28];
	} pktbyte;
};
#define VendorSpecific_InfoFrame union _VendorSpecific_InfoFrame

union _AVI_InfoFrame {

	struct {
		unsigned char Type;
		unsigned char Ver;
		unsigned char Len;

		unsigned char checksum;

		unsigned char Scan : 2;
		unsigned char BarInfo : 2;
		unsigned char ActiveFmtInfoPresent : 1;
		unsigned char ColorMode : 2;
		unsigned char FU1 : 1;

		unsigned char ActiveFormatAspectRatio : 4;
		unsigned char PictureAspectRatio : 2;
		unsigned char Colorimetry : 2;

		unsigned char Scaling : 2;
		unsigned char FU2 : 6;

		unsigned char VIC : 7;
		unsigned char FU3 : 1;

		unsigned char PixelRepetition : 4;
		unsigned char FU4 : 4;

		short Ln_End_Top;
		short Ln_Start_Bottom;
		short Pix_End_Left;
		short Pix_Start_Right;
	} info;

	struct {
		unsigned char AVI_HB[3];
		unsigned char checksum;
		unsigned char AVI_DB[AVI_INFOFRAME_LEN];
	} pktbyte;
};
#define AVI_InfoFrame union _AVI_InfoFrame

union _Audio_InfoFrame {

	struct {
		unsigned char Type;
		unsigned char Ver;
		unsigned char Len;
		unsigned char checksum;

		unsigned char AudioChannelCount : 3;
		unsigned char RSVD1 : 1;
		unsigned char AudioCodingType : 4;

		unsigned char SampleSize : 2;
		unsigned char SampleFreq : 3;
		unsigned char Rsvd2 : 3;

		unsigned char FmtCoding;

		unsigned char SpeakerPlacement;

		unsigned char Rsvd3 : 3;
		unsigned char LevelShiftValue : 4;
		unsigned char DM_INH : 1;
	} info;

	struct {
		unsigned char AUD_HB[3];
		unsigned char checksum;
		unsigned char AUD_DB[5];
	} pktbyte;
};
#define Audio_InfoFrame union _Audio_InfoFrame

union _MPEG_InfoFrame {
	struct {
		unsigned char Type;
		unsigned char Ver;
		unsigned char Len;
		unsigned char checksum;

		ULONG MpegBitRate;

		unsigned char MpegFrame : 2;
		unsigned char Rvsd1 : 2;
		unsigned char FieldRepeat : 1;
		unsigned char Rvsd2 : 3;
	} info;
	struct {
		unsigned char MPG_HB[3];
		unsigned char checksum;
		unsigned char MPG_DB[MPEG_INFOFRAME_LEN];
	} pktbyte;
};
#define MPEG_InfoFrame union _MPEG_InfoFrame

union _SPD_InfoFrame {
	struct {
		unsigned char Type;
		unsigned char Ver;
		unsigned char Len;
		unsigned char checksum;

		char VN[8];
		char PD[16];
		unsigned char SourceDeviceInfomation;
	} info;
	struct {
		unsigned char SPD_HB[3];
		unsigned char checksum;
		unsigned char SPD_DB[SPD_INFOFRAME_LEN];
	} pktbyte;
};
#define SPD_InfoFrame union _SPD_InfoFrame

/* ///////////////////////////////////////////////////////////////////////// */
/* Using for interface. */
/* ///////////////////////////////////////////////////////////////////////// */

#define PROG 1
#define INTERLACE 0
#define Vneg 0
#define Hneg 0
#define Vpos 1
#define Hpos 1

struct _CEAVTiming {
	WORD H_ActiveStart;
	WORD H_ActiveEnd;
	WORD H_SyncStart;
	WORD H_SyncEnd;
	WORD V_ActiveStart;
	WORD V_ActiveEnd;
	WORD V_SyncStart;
	WORD V_SyncEnd;
	WORD V2_ActiveStart;
	WORD V2_ActiveEnd;
	WORD HTotal;
	WORD VTotal;
};
#define CEAVTiming struct _CEAVTiming

struct _HDMI_VTiming {
	unsigned char VIC;
	unsigned char PixelRep;
	WORD HActive;
	WORD VActive;
	WORD HTotal;
	WORD VTotal;
	ULONG PCLK;
	unsigned char xCnt;
	WORD HFrontPorch;
	WORD HSyncWidth;
	WORD HBackPorch;
	unsigned char VFrontPorch;
	unsigned char VSyncWidth;
	unsigned char VBackPorch;
	unsigned char ScanMode : 1;
	unsigned char VPolarity : 1;
	unsigned char HPolarity : 1;
};
#define HDMI_VTiming struct _HDMI_VTiming

/* //////////////////////////////////////////////////////////////// */
/* Audio relate definition and macro. */
/* //////////////////////////////////////////////////////////////// */

/* 2008/08/15 added by jj_tseng@chipadvanced */
#define F_AUDIO_ON (1 << 7)
#define F_AUDIO_HBR (1 << 6)
#define F_AUDIO_DSD (1 << 5)
#define F_AUDIO_NLPCM (1 << 4)
#define F_AUDIO_LAYOUT_1 (1 << 3)
#define F_AUDIO_LAYOUT_0 (0 << 3)

/* HBR - 1100 */
/* DSD - 1010 */
/* NLPCM - 1001 */
/* LPCM - 1000 */

#define T_AUDIO_MASK 0xF0
#define T_AUDIO_OFF 0
#define T_AUDIO_HBR (F_AUDIO_ON | F_AUDIO_HBR)
#define T_AUDIO_DSD (F_AUDIO_ON | F_AUDIO_DSD)
#define T_AUDIO_NLPCM (F_AUDIO_ON | F_AUDIO_NLPCM)
#define T_AUDIO_LPCM (F_AUDIO_ON)

/* for sample clock */
#define AUDFS_22p05KHz 4
#define AUDFS_44p1KHz 0
#define AUDFS_88p2KHz 8
#define AUDFS_176p4KHz 12

#define AUDFS_24KHz 6
#define AUDFS_48KHz 2
#define AUDFS_96KHz 10
#define AUDFS_192KHz 14

#define AUDFS_768KHz 9

#define AUDFS_32KHz 3
#define AUDFS_OTHER 1

/* Audio Enable */
#define ENABLE_SPDIF (1 << 4)
#define ENABLE_I2S_SRC3 (1 << 3)
#define ENABLE_I2S_SRC2 (1 << 2)
#define ENABLE_I2S_SRC1 (1 << 1)
#define ENABLE_I2S_SRC0 (1 << 0)

#define AUD_SWL_NOINDICATE 0x0
#define AUD_SWL_16 0x2
#define AUD_SWL_17 0xC
#define AUD_SWL_18 0x4
#define AUD_SWL_20 0xA /* for maximum 20 bit */
#define AUD_SWL_21 0xD
#define AUD_SWL_22 0x5
#define AUD_SWL_23 0x9
#define AUD_SWL_24 0xB

#endif /* _TYPEDEF_H_ */
