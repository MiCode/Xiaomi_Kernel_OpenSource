/*
 * Copyright (c) 2020 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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


#ifndef __DRTX_HAL_H__
#define __DRTX_HAL_H__

#include "mtk_dp_common.h"


#define AUX_CMD_I2C_R_MOT0		0x01
#define AUX_CMD_I2C_R			0x05
#define AUX_CMD_NATIVE_R		0x09
#define AuxWaitReplyLpCntNum		20000


#define MASKBIT(a)			(BIT((1 ? a) + 1) - BIT((0 ? a)))
#define _BITMASK(loc_msb, loc_lsb) \
	((1U << (loc_msb)) - (1U << (loc_lsb)) + (1U << (loc_msb)))
#define BITMASK(x) _BITMASK(1 ? x, 0 ? x)


enum DPTx_LANE_NUM {
	DPTx_LANE0 = 0x0,
	DPTx_LANE1 = 0x1,
	DPTx_LANE2 = 0x2,
	DPTx_LANE3 = 0x3,
	DPTx_LANE_MAX,
};

enum DPTx_LANE_Count {
	DPTx_LANE_0 = 0x0,
	DPTx_LANE_2 = 0x01,
	DPTx_LANE_4 = 0x02,
};

enum DPTx_LINK_Rate {
	DPTx_RATE_RBR  = 0x06,
	DPTx_RATE_HBR  = 0x0A,
	DPTx_RATE_HBR2 = 0x14,
	DPTx_RATE_HBR3 = 0x1E,
};

enum DPTx_SDP_PKG_TYPE {
	DPTx_SDPTYP_NONE = 0x00,
	DPTx_SDPTYP_ACM  = 0x01,
	DPTx_SDPTYP_ISRC = 0x02,
	DPTx_SDPTYP_AVI  = 0x03,
	DPTx_SDPTYP_AUI  = 0x04,
	DPTx_SDPTYP_SPD  = 0x05,
	DPTx_SDPTYP_MPEG = 0x06,
	DPTx_SDPTYP_NTSC = 0x07,
	DPTx_SDPTYP_VSP  = 0x08,
	DPTx_SDPTYP_VSC  = 0x09,
	DPTx_SDPTYP_EXT  = 0x0A,
	DPTx_SDPTYP_PPS0 = 0x0B,
	DPTx_SDPTYP_PPS1 = 0x0C,
	DPTx_SDPTYP_PPS2 = 0x0D,
	DPTx_SDPTYP_PPS3 = 0x0E,
	DPTx_SDPTYP_DRM  = 0x10,
	DPTx_SDPTYP_MAX_NUM
};

enum DPTx_SDP_HB1_PKG_TYPE {
	DP_SPEC_SDPTYP_RESERVE	= 0x00,
	DP_SPEC_SDPTYP_AUDIO_TS	= 0x01,
	DP_SPEC_SDPTYP_AUDIO	= 0x02,
	DP_SPEC_SDPTYP_EXT	= 0x04,
	DP_SPEC_SDPTYP_ACM	= 0x05,
	DP_SPEC_SDPTYP_ISRC	= 0x06,
	DP_SPEC_SDPTYP_VSC	= 0x07,
	DP_SPEC_SDPTYP_CAMERA   = 0x08,
	DP_SPEC_SDPTYP_PPS      = 0x10,
	DP_SPEC_SDPTYP_EXT_VESA = 0x20,
	DP_SPEC_SDPTYP_EXT_CEA  = 0x21,
	DP_SPEC_SDPTYP_NON_AINFO = 0x80,
	DP_SPEC_SDPTYP_VS_INFO	= 0x81,
	DP_SPEC_SDPTYP_AVI_INFO	= 0x82,
	DP_SPEC_SDPTYP_SPD_INFO = 0x83,
	DP_SPEC_SDPTYP_AINFO    = 0x84,
	DP_SPEC_SDPTYP_MPG_INFO = 0x85,
	DP_SPEC_SDPTYP_NTSC_INFO = 0x86,
	DP_SPEC_SDPTYP_DRM_INFO = 0x87,
	DP_SPEC_SDPTYP_MAX_NUM
};

enum DP_COLOR_FORMAT_TYPE {
	DP_COLOR_FORMAT_RGB_444     = 0,
	DP_COLOR_FORMAT_YUV_422     = 1,
	DP_COLOR_FORMAT_YUV_444     = 2,
	DP_COLOR_FORMAT_YUV_420     = 3,
	DP_COLOR_FORMAT_YONLY       = 4,
	DP_COLOR_FORMAT_RAW         = 5,
	DP_COLOR_FORMAT_RESERVED    = 6,
	DP_COLOR_FORMAT_DEFAULT     = DP_COLOR_FORMAT_RGB_444,
	DP_COLOR_FORMAT_UNKNOWN     = 15,
};

enum DP_COLOR_DEPTH_TYPE {
	DP_COLOR_DEPTH_6BIT       = 0,
	DP_COLOR_DEPTH_8BIT       = 1,
	DP_COLOR_DEPTH_10BIT      = 2,
	DP_COLOR_DEPTH_12BIT      = 3,
	DP_COLOR_DEPTH_16BIT      = 4,
	DP_COLOR_DEPTH_UNKNOWN    = 5,
};

enum AUDIO_FS {
	FS_22K = 0x0,
	FS_32K = 0x1,
	FS_44K = 0x2,
	FS_48K = 0x3,
	FS_88K = 0x4,
	FS_96K = 0x5,
	FS_176K = 0x6,
	FS_192K = 0x7,
	FS_MAX,
};

enum AUDIO_WORD_LEN {
	WL_16bit = 1,
	WL_20bit = 2,
	WL_24bit = 3,
	WL_MAX,
};

#define  IEC_CH_STATUS_LEN 5
union DPRX_AUDIO_CHSTS {
	struct{
		BYTE rev : 1;
		BYTE ISLPCM : 1;
		BYTE CopyRight : 1;
		BYTE AdditionFormatInfo : 3;
		BYTE ChannelStatusMode : 2;
		BYTE CategoryCode;
		BYTE SourceNumber : 4;
		BYTE ChannelNumber : 4;
		BYTE SamplingFreq : 4;
		BYTE ClockAccuary : 2;
		BYTE rev2 : 2;
		BYTE WordLen : 4;
		BYTE OriginalSamplingFreq : 4;
	} iec_ch_sts;

	BYTE AUD_CH_STS[IEC_CH_STATUS_LEN];
};

enum DPTx_PG_PURECOLOR {
	DPTX_PG_PURECOLOR_NONE		= 0x0,
	DPTX_PG_PURECOLOR_BLUE		= 0x1,
	DPTX_PG_PURECOLOR_GREEN		= 0x2,
	DPTX_PG_PURECOLOR_RED		= 0x3,
	DPTX_PG_PURECOLOR_MAX,
};

enum DPTx_PG_LOCATION {
	DPTX_PG_LOCATION_NONE            = 0x0,
	DPTX_PG_LOCATION_ALL             = 0x1,
	DPTX_PG_LOCATION_TOP             = 0x2,
	DPTX_PG_LOCATION_BOTTOM          = 0x3,
	DPTX_PG_LOCATION_LEFT_OF_TOP     = 0x4,
	DPTX_PG_LOCATION_LEFT_OF_BOTTOM  = 0x5,
	DPTX_PG_LOCATION_LEFT            = 0x6,
	DPTX_PG_LOCATION_RIGHT           = 0x7,
	DPTX_PG_LOCATION_LEFT_OF_LEFT    = 0x8,
	DPTX_PG_LOCATION_RIGHT_OF_LEFT   = 0x9,
	DPTX_PG_LOCATION_LEFT_OF_RIGHT   = 0xA,
	DPTX_PG_LOCATION_RIGHT_OF_RIGHT  = 0xB,
	DPTX_PG_LOCATION_MAX,
};

enum DPTx_PG_PIXEL_MASK {
	DPTX_PG_PIXEL_MASK_NONE         = 0x0,
	DPTX_PG_PIXEL_ODD_MASK          = 0x1,
	DPTX_PG_PIXEL_EVEN_MASK         = 0x2,
	DPTX_PG_PIXEL_MASK_MAX,
};

enum DPTx_PG_TYPESEL {
	DPTX_PG_NONE                    = 0x0,
	DPTX_PG_PURE_COLOR              = 0x1,
	DPTX_PG_VERTICAL_RAMPING        = 0x2,
	DPTX_PG_HORIZONTAL_RAMPING      = 0x3,
	DPTX_PG_VERTICAL_COLOR_BAR      = 0x4,
	DPTX_PG_HORIZONTAL_COLOR_BAR    = 0x5,
	DPTX_PG_CHESSBOARD_PATTERN      = 0x6,
	DPTX_PG_SUB_PIXEL_PATTERN       = 0x7,
	DPTX_PG_FRAME_PATTERN           = 0x8,
	DPTX_PG_MAX,
};


u32 mtk_dp_read(struct mtk_dp *mtk_dp, u32 offset);
void mtk_dp_write_byte(struct mtk_dp *mtk_dp, u32 addr, u8 val, u32 mask);
void mtk_dp_mask(struct mtk_dp *mtk_dp, u32 offset, u32 val, u32 mask);
void mtk_dp_write(struct mtk_dp *mtk_dp, u32 offset, u32 val);

#define msReadByte(mtk_dp, u32Reg) mtk_dp_read(mtk_dp, u32Reg)
#define msRead2Byte(mtk_dp, u32Reg) mtk_dp_read(mtk_dp, u32Reg)
#define msRead4Byte(mtk_dp, u32Reg) mtk_dp_read(mtk_dp, u32Reg)
#define msWriteByte(mtk_dp, u32Reg, u8Val) \
	mtk_dp_write_byte(mtk_dp, u32Reg, u8Val, 0xFF)
#define msWrite2Byte(mtk_dp, u32Reg, u16Val) \
	mtk_dp_mask(mtk_dp, u32Reg, u16Val, 0xFFFF)
#define msWrite4Byte(mtk_dp, u32Reg, u32Val) \
	mtk_dp_write(mtk_dp, u32Reg, u32Val)
#define msWriteByteMask(mtk_dp, addr, val, mask) \
	mtk_dp_write_byte(mtk_dp, addr, val, mask)
#define msWrite2ByteMask(mtk_dp, addr, val, mask) \
	mtk_dp_mask(mtk_dp, addr, val, mask)
#define msWrite4ByteMask(mtk_dp, addr, val, mask) \
	mtk_dp_mask(mtk_dp, addr, val, mask)

extern void mdrv_DPTx_HPD_ISREvent(struct mtk_dp *mtk_dp);
void mhal_DPTx_USBC_HPD(struct mtk_dp *mtk_dp, bool conn);
void mhal_DPTx_Fake_Plugin(struct mtk_dp *mtk_dp, bool conn);
void mhal_dump_reg(struct mtk_dp *mtk_dp);
void mhal_DPTx_Verify_Clock(struct mtk_dp *mtk_dp);
void mhal_DPTx_ISR(struct mtk_dp *mtk_dp);
BYTE mhal_DPTx_GetColorBpp(struct mtk_dp *mtk_dp);
bool mhal_DPTx_AuxRead_Bytes(struct mtk_dp *mtk_dp,
	BYTE ubCmd, DWORD usDPCDADDR, size_t ubLength, BYTE *pRxBuf);
bool mhal_DPTx_AuxWrite_Bytes(struct mtk_dp *mtk_dp,
	BYTE ubCmd, DWORD usDPCDADDR, size_t ubLength, BYTE *pData);
bool mhal_DPTx_SetSwingtPreEmphasis(struct mtk_dp *mtk_dp, int lane_num,
	int swingValue, int preEmphasis);
bool mhal_DPTx_ResetSwingtPreEmphasis(struct mtk_dp *mtk_dp);
void mhal_DPTx_DigitalSwReset(struct mtk_dp *mtk_dp);
bool mhal_DPTx_GetHPDPinLevel(struct mtk_dp *mtk_dp);
void mhal_DPTx_SSCOnOffSetting(struct mtk_dp *mtk_dp, bool bENABLE);
void mhal_DPTx_SWInterruptSet(struct mtk_dp *mtk_dp, WORD bstatus);
void mhal_DPTx_SWInterruptClr(struct mtk_dp *mtk_dp, WORD bstatus);
void mhal_DPTx_SWInterruptEnable(struct mtk_dp *mtk_dp, bool enable);
void mhal_DPTx_HPDInterruptClr(struct mtk_dp *mtk_dp, BYTE bstatus);
void mhal_DPTx_HPDInterruptEnable(struct mtk_dp *mtk_dp, bool enable);
void mhal_DPTx_HPDDetectSetting(struct mtk_dp *mtk_dp);
void mhal_DPTx_PHYSetting(struct mtk_dp *mtk_dp);
void mhal_DPTx_AuxSetting(struct mtk_dp *mtk_dp);
void mhal_DPTx_AdjustPHYSetting(struct mtk_dp *mtk_dp, BYTE c0, BYTE cp1);
void mhal_DPTx_DigitalSetting(struct mtk_dp *mtk_dp);
void mhal_DPTx_PSCTRL(bool AUXNHighEnable);
void mhal_DPTx_SetTxLane(struct mtk_dp *mtk_dp, int Value);
void mhal_DPTx_SetTxLaneToLane(struct mtk_dp *mtk_dp,
	BYTE ucLaneNum, BYTE ucSetLaneNum);
void mhal_DPTx_SetPGMSA(struct mtk_dp *mtk_dp, BYTE Address, WORD Data);
void mhal_DPTx_PHY_ResetPattern(struct mtk_dp *mtk_dp);
void mhal_DPTx_PHY_SetIdlePattern(struct mtk_dp *mtk_dp, bool bENABLE);
void mhal_DPTx_PHYD_Reset(struct mtk_dp *mtk_dp);
void mhal_DPTx_PRBSEnable(struct mtk_dp *mtk_dp, bool bENABLE);
void mhal_DPTx_PatternSelect(struct mtk_dp *mtk_dp, int Value);
void mhal_DPTx_ComplianceEyeEnSetting(struct mtk_dp *mtk_dp,  bool bENABLE);
void mhal_DPTx_SetProgramPattern(struct mtk_dp *mtk_dp,
	BYTE Value, BYTE  *usData);
void mhal_DPTx_ProgramPatternEnable(struct mtk_dp *mtk_dp, bool bENABLE);
void mhal_DPTx_SetTxTrainingPattern(struct mtk_dp *mtk_dp, int  Value);
void mhal_DPTx_SetEF_Mode(struct mtk_dp *mtk_dp, bool  bENABLE);
void mhal_DPTx_SetScramble(struct mtk_dp *mtk_dp, bool  bENABLE);
void mhal_DPTx_SetScramble_Type(struct mtk_dp *mtk_dp, bool bSelType);
void mhal_DPTx_ShutDownDPTxPort(struct mtk_dp *mtk_dp);
void mhal_DPTx_EnableFEC(struct mtk_dp *mtk_dp, bool bENABLE);
void mhal_DPTx_EnableDSC(struct mtk_dp *mtk_dp, bool bENABLE);
void mhal_DPTx_SetChunkSize(struct mtk_dp *mtk_dp,
	BYTE slice_num, WORD chunk_num,
	BYTE remainder, BYTE lane_count,
	BYTE hde_last_num, BYTE hde_num_even);
void mhal_DPTx_InitialSetting(struct mtk_dp *mtk_dp);
void mhal_DPTx_Set_Efuse_Value(struct mtk_dp *mtk_dp);
void mhal_DPTx_VideoMuteSW(struct mtk_dp *mtk_dp, bool bENABLE);
void mhal_DPTx_VideoMute(struct mtk_dp *mtk_dp, bool bENABLE);
void mhal_DPTx_AudioMute(struct mtk_dp *mtk_dp, bool bENABLE);
void mhal_DPTx_SetFreeSync(struct mtk_dp *mtk_dp, bool bENABLE);
void mhal_DPTx_Set_VideoInterlance(struct mtk_dp *mtk_dp, bool bENABLE);
void mhal_DPTx_EnableBypassMSA(struct mtk_dp *mtk_dp, bool bENABLE);
void mhal_DPTx_PGEnable(struct mtk_dp *mtk_dp, bool bENABLE);
void mhal_DPTx_PG_Pure_Color(struct mtk_dp *mtk_dp, BYTE BGR, DWORD ColorDepth);
void mhal_DPTx_PG_VerticalRamping(struct mtk_dp *mtk_dp, BYTE BGR,
	DWORD ColorDepth, BYTE Location);
void mhal_DPTx_PG_HorizontalRamping(struct mtk_dp *mtk_dp, BYTE BGR,
	DWORD ColorDepth, BYTE Location);
void mhal_DPTx_PG_VerticalColorBar(struct mtk_dp *mtk_dp, BYTE Location);
void mhal_DPTx_PG_HorizontalColorBar(struct mtk_dp *mtk_dp, BYTE Location);
void mhal_DPTx_PG_Chessboard(struct mtk_dp *mtk_dp, BYTE Location,
	WORD Hde, WORD Vde);
void mhal_DPTx_PG_SubPixel(struct mtk_dp *mtk_dp, BYTE Location);
void mhal_DPTx_PG_Frame(struct mtk_dp *mtk_dp, BYTE Location,
	WORD Hde, WORD Vde);
void mhal_DPTx_Set_MVIDx2(struct mtk_dp *mtk_dp, bool bEnable);
bool mhal_DPTx_OverWrite_MN(struct mtk_dp *mtk_dp,
	bool bEnable, DWORD ulVideo_M, DWORD ulVideo_N);
void mhal_DPTx_SetTU_SramRdStart(struct mtk_dp *mtk_dp, WORD uwValue);
void mhal_DPTx_SetSDP_DownCntinitInHblanking(struct mtk_dp *mtk_dp,
	WORD uwValue);
void mhal_DPTx_SetSDP_DownCntinit(struct mtk_dp *mtk_dp, WORD uwValue);
void mhal_DPTx_SetTU_SetEncoder(struct mtk_dp *mtk_dp);
void mhal_DPTx_SetMSA(struct mtk_dp *mtk_dp);
void mhal_DPTx_SetMISC(struct mtk_dp *mtk_dp, BYTE ucMISC[2]);
void mhal_DPTx_SetColorDepth(struct mtk_dp *mtk_dp, BYTE coloer_depth);
void mhal_DPTx_SetColorFormat(struct mtk_dp *mtk_dp, BYTE enOutColorFormat);
void mhal_DPTx_SPKG_SDP(struct mtk_dp *mtk_dp, bool bEnable, BYTE ucSDPType,
	BYTE *pHB, BYTE *pDB);
void mhal_DPTx_SPKG_VSC_EXT_VESA(struct mtk_dp *mtk_dp, bool bEnable,
	BYTE ucHDR_NUM, BYTE *pDB);
void mhal_DPTx_SPKG_VSC_EXT_CEA(struct mtk_dp *mtk_dp, bool bEnable,
	BYTE ucHDR_NUM, BYTE *pDB);
BYTE mhal_DPTx_GetHPDIRQStatus(struct mtk_dp *mtk_dp);
void mhal_DPTx_Audio_PG_EN(struct mtk_dp *mtk_dp, BYTE Channel, BYTE Fs,
	BYTE bEnable);
void mhal_DPTx_Audio_PG_EN(struct mtk_dp *mtk_dp, BYTE Channel, BYTE Fs,
	BYTE bEnable);
WORD mhal_DPTx_GetSWIRQStatus(struct mtk_dp *mtk_dp);
void mhal_DPTx_Audio_Ch_Status_Set(struct mtk_dp *mtk_dp, BYTE Channel,
	BYTE Fs, BYTE Wordlength);
void mhal_DPTx_Audio_SDP_Setting(struct mtk_dp *mtk_dp, BYTE Channel);
void mhal_DPTx_Audio_M_Divider_Setting(struct mtk_dp *mtk_dp, BYTE Div);
void mhal_DPTx_SetTxRate(struct mtk_dp *mtk_dp, int Value);
void mhal_DPTx_AnalogPowerOnOff(struct mtk_dp *mtk_dp, bool enable);
void mhal_DPTx_DataLanePNSwap(struct mtk_dp *mtk_dp, bool bENABLE);

#endif //__DRTX_HAL_H__
