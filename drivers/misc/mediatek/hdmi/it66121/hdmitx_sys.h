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

#ifndef _HDMITX_SYS_H_
#define _HDMITX_SYS_H_

#include "extd_hdmi.h"
#include "hdmi_drv.h"
#include "itx_typedef.h"
#define I2S 0
#define SPDIF 1

#ifdef HDMITX_INPUT_INFO
void HDMITX_MonitorInputVideoChange(void);
void HDMITX_MonitorInputAudioChange(void);

#endif

unsigned char ParseEDID(void);
bool ParseCEAEDID(unsigned char *pCEAEDID);
void ConfigAVIInfoFrame(unsigned char VIC, unsigned char pixelrep);
void ConfigAudioInfoFrm(void);
void Config_GeneralPurpose_Infoframe(unsigned char *p3DInfoFrame);
void ConfigfHdmiVendorSpecificInfoFrame(unsigned char _3D_Stru);
void InitHDMITX_Variable(void);
void HDMITX_SetOutput(void);
void HDMITX_DevLoopProc(void);

/*extern struct switch_dev hdmi_switch_data;*/
extern void switch_host_interface_timing(int out);
extern void init_hdmi_disp_path(int out);
/* extern int start_output; */

#ifndef I2S_FORMAT
#define I2S_FORMAT 0x01 /* 32bit audio */
#endif

#ifndef INPUT_SAMPLE_FREQ
#define INPUT_SAMPLE_FREQ AUDFS_48KHz
#endif /* INPUT_SAMPLE_FREQ */

#ifndef INPUT_SAMPLE_FREQ_HZ
#define INPUT_SAMPLE_FREQ_HZ 48000L
#endif /* INPUT_SAMPLE_FREQ_HZ */

#ifndef OUTPUT_CHANNEL
#define OUTPUT_CHANNEL 2
#endif /* OUTPUT_CHANNEL */

#ifndef CNOFIG_INPUT_AUDIO_TYPE
#define CNOFIG_INPUT_AUDIO_TYPE T_AUDIO_LPCM
/* #define CNOFIG_INPUT_AUDIO_TYPE T_AUDIO_NLPCM */
/* #define CNOFIG_INPUT_AUDIO_TYPE T_AUDIO_HBR */
#endif /* CNOFIG_INPUT_AUDIO_TYPE */

#ifndef CONFIG_INPUT_AUDIO_SPDIF
#define CONFIG_INPUT_AUDIO_SPDIF I2S
/* #define CONFIG_INPUT_AUDIO_SPDIF  SPDIF */
#endif /* CONFIG_INPUT_AUDIO_SPDIF */

#ifndef INPUT_SIGNAL_TYPE
#define INPUT_SIGNAL_TYPE 0 /* 24 bit sync separate */
#endif


/* Internal Data Type */


enum _HDMI_Video_Type {
	HDMI_Unknown = 0,
	HDMI_640x480p60 = 1,
	HDMI_480p60,
	HDMI_480p60_16x9,
	HDMI_720p60,
	HDMI_1080i60,
	HDMI_480i60,
	HDMI_480i60_16x9,
	HDMI_1080p60 = 16,
	HDMI_576p50,
	HDMI_576p50_16x9,
	HDMI_720p50,
	HDMI_1080i50,
	HDMI_576i50,
	HDMI_576i50_16x9,
	HDMI_1080p50 = 31,
	HDMI_1080p24,
	HDMI_1080p25,
	HDMI_1080p30,
	HDMI_720p30 = 61,
};
#define HDMI_Video_Type enum _HDMI_Video_Type

enum _HDMI_Aspec { HDMI_4x3, HDMI_16x9 };
#define HDMI_Aspec enum _HDMI_Aspec

enum _HDMI_OutputColorMode { HDMI_RGB444, HDMI_YUV444, HDMI_YUV422 };
#define HDMI_OutputColorMode enum _HDMI_OutputColorMode

enum _HDMI_Colorimetry { HDMI_ITU601, HDMI_ITU709 };
#define HDMI_Colorimetry enum _HDMI_Colorimetry

struct VideoTiming {
	ULONG VideoPixelClock;
	unsigned char VIC;
	unsigned char pixelrep;
	unsigned char outputVideoMode;
};

enum _TXVideo_State_Type {
	TXVSTATE_Unplug = 0,
	TXVSTATE_HPD,
	TXVSTATE_WaitForMode,
	TXVSTATE_WaitForVStable,
	TXVSTATE_VideoInit,
	TXVSTATE_VideoSetup,
	TXVSTATE_VideoOn,
	TXVSTATE_Reserved
};
#define TXVideo_State_Type enum _TXVideo_State_Type

enum _TXAudio_State_Type {
	TXASTATE_AudioOff = 0,
	TXASTATE_AudioPrepare,
	TXASTATE_AudioOn,
	TXASTATE_AudioFIFOFail,
	TXASTATE_Reserved
};
#define TXAudio_State_Type enum _TXAudio_State_Type

/* /////////////////////////////////////// */
/* RX Capability. */
/* /////////////////////////////////////// */
struct _LPCM_BitWidth {
	unsigned char b16bit : 1;
	unsigned char b20bit : 1;
	unsigned char b24bit : 1;
	unsigned char Rsrv : 5;
};
#define LPCM_BitWidth struct _LPCM_BitWidth

enum _AUDIO_FORMAT_CODE {
	AUD_RESERVED_0 = 0,
	AUD_LPCM,
	AUD_AC3,
	AUD_MPEG1,
	AUD_MP3,
	AUD_MPEG2,
	AUD_AAC,
	AUD_DTS,
	AUD_ATRAC,
	AUD_ONE_BIT_AUDIO,
	AUD_DOLBY_DIGITAL_PLUS,
	AUD_DTS_HD,
	AUD_MAT_MLP,
	AUD_DST,
	AUD_WMA_PRO,
	AUD_RESERVED_15
};
#define AUDIO_FORMAT_CODE enum _AUDIO_FORMAT_CODE

union _AUDDESCRIPTOR {
	struct {
		unsigned char channel : 3;
		unsigned char AudioFormatCode : 4;
		unsigned char Rsrv1 : 1;

		unsigned char b32KHz : 1;
		unsigned char b44_1KHz : 1;
		unsigned char b48KHz : 1;
		unsigned char b88_2KHz : 1;
		unsigned char b96KHz : 1;
		unsigned char b176_4KHz : 1;
		unsigned char b192KHz : 1;
		unsigned char Rsrv2 : 1;
		unsigned char ucCode;
	} s;
	unsigned char uc[3];
};
#define AUDDESCRIPTOR union _AUDDESCRIPTOR

union _SPK_ALLOC {
	struct {
		unsigned char FL_FR : 1;
		unsigned char LFE : 1;
		unsigned char FC : 1;
		unsigned char RL_RR : 1;
		unsigned char RC : 1;
		unsigned char FLC_FRC : 1;
		unsigned char RLC_RRC : 1;
		unsigned char Reserve : 1;
		unsigned char Unuse[2];
	} s;
	unsigned char uc[3];
};
#define SPK_ALLOC union _SPK_ALLOC

#define CEA_SUPPORT_UNDERSCAN (1 << 7)
#define CEA_SUPPORT_AUDIO (1 << 6)
#define CEA_SUPPORT_YUV444 (1 << 5)
#define CEA_SUPPORT_YUV422 (1 << 4)
#define CEA_NATIVE_MASK 0xF

#define HDMI_DC_SUPPORT_AI (1 << 7)
#define HDMI_DC_SUPPORT_48 (1 << 6)
#define HDMI_DC_SUPPORT_36 (1 << 5)
#define HDMI_DC_SUPPORT_30 (1 << 4)
#define HDMI_DC_SUPPORT_Y444 (1 << 3)
#define HDMI_DC_SUPPORT_DVI_DUAL 1

union _DCSUPPORT {
	struct {
		unsigned char DVI_Dual : 1;
		unsigned char Rsvd : 2;
		unsigned char DC_Y444 : 1;
		unsigned char DC_30Bit : 1;
		unsigned char DC_36Bit : 1;
		unsigned char DC_48Bit : 1;
		unsigned char SUPPORT_AI : 1;
	} info;
	unsigned char uc;
};
#define DCSUPPORT union _DCSUPPORT

union _LATENCY_SUPPORT {
	struct {
		unsigned char Rsvd : 6;
		unsigned char I_Latency_Present : 1;
		unsigned char Latency_Present : 1;
	} info;
	unsigned char uc;
};
#define LATENCY_SUPPORT union _LATENCY_SUPPORT

#define HDMI_IEEEOUI 0x0c03
#define MAX_VODMODE_COUNT 32
#define MAX_AUDDES_COUNT 4

struct _RX_CAP {
	unsigned char VideoMode;
	unsigned char NativeVDOMode;
	unsigned char VDOMode[8];
	unsigned char AUDDesCount;
	AUDDESCRIPTOR AUDDes[MAX_AUDDES_COUNT];
	unsigned char PA[2];
	ULONG IEEEOUI;
	DCSUPPORT dc;
	unsigned char MaxTMDSClock;
	LATENCY_SUPPORT lsupport;
	SPK_ALLOC SpeakerAllocBlk;
	unsigned char ValidCEA : 1;
	unsigned char ValidHDMI : 1;
	unsigned char Valid3D : 1;
};
#define RX_CAP struct _RX_CAP

/* ///////////////////////////////////////////////////////////////////// */
/* Output Mode Type */
/* ///////////////////////////////////////////////////////////////////// */

#define RES_ASPEC_4x3 0
#define RES_ASPEC_16x9 1
#define F_MODE_REPT_NO 0
#define F_MODE_REPT_TWICE 1
#define F_MODE_REPT_QUATRO 3
#define F_MODE_CSC_ITU601 0
#define F_MODE_CSC_ITU709 1

void InitHDMITX_Variable(void);
void HDMITX_ChangeDisplayOption(HDMI_Video_Type VideoMode,
				HDMI_OutputColorMode OutputColorMode);
void HDMITX_SetOutput(void);
void HDMITX_DevLoopProc(void);
void ConfigfHdmiVendorSpecificInfoFrame(unsigned char _3D_Stru);
void HDMITX_ChangeAudioOption(unsigned char Option, unsigned char channelNum,
			      unsigned char AudioFs);
void HDMITX_SetAudioOutput(void);
void HDMITX_ChangeColorDepth(unsigned char colorDepth);

enum _HDMI_PLUG_STATE { HDMI_PLUG_NO_DEVICE, HDMI_PLUG_ACTIVE };
#define HDMI_PLUG_STATE enum _HDMI_PLUG_STATE
extern unsigned char HPDStatus;
extern unsigned int sink_support_resolution;
extern struct HDMI_UTIL_FUNCS hdmi_util;
extern void ite66121_AppGetEdidInfo(struct _HDMI_EDID_T *pv_get_info);
extern void hdmi_invoke_cable_callbacks(enum HDMI_STATE state);
#endif /* _HDMITX_SYS_H_ */
