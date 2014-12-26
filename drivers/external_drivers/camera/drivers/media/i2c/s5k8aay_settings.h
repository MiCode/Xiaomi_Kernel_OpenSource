/*
 * Copyright (c) 2013 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

/* Source:
 * 8AA_EVT1prev_Cap_1280x960_30fps_M19 2_S58_P116_130530_1_FINAL_REAL.TSET
 *
 *  [8AA_EVT1]Preview 640x480_30fps_Capture 1280x960_30fps_M19.2_S58_P116.nset
 */

/* 01.Start Setting */
static struct s5k8aay_reg const s5k8aay_regs_1[] = {
	/* $MIPI[Width:1280,Height:720,Format:YUV422,Lane:1,
	ErrorCheck:0,PolarityData:0,PolarityClock:0,Buffer:2] */

	{ S5K8AAY_TOK_16BIT, 0xFCFC, 0xD000 }, /*Default page address setting */
	{ S5K8AAY_TOK_16BIT, 0x0004, 0x0000 }, /*Enable Address Auto-Increase */
	{ S5K8AAY_TOK_16BIT, 0xFCFC, 0xD000 }, /*Default page address setting */
	{ S5K8AAY_TOK_16BIT, 0x0004, 0x0000 }, /*Enable Address Auto-Increase */

	{ S5K8AAY_TOK_16BIT, 0xFCFC, 0xD000 }, /*Default page address setting */
	{ S5K8AAY_TOK_16BIT, 0x0004, 0x0000 }, /*Enable Address Auto-Increase */

	{ S5K8AAY_TOK_16BIT, 0xFCFC, 0xD000 },
	{ S5K8AAY_TOK_16BIT, 0x0010, 0x0001 }, /* S/W Reset */
	{ S5K8AAY_TOK_16BIT, 0xFCFC, 0x0000 },
	{ S5K8AAY_TOK_16BIT, 0x0000, 0x0000 }, /* Simmian bug workaround */

	{ S5K8AAY_TOK_16BIT, 0xFCFC, 0xD000 },
	{ S5K8AAY_TOK_16BIT, 0x1030, 0x0000 }, /* contint_host_int */
	{ S5K8AAY_TOK_16BIT, 0xFCFC, 0xD000 },
	{ S5K8AAY_TOK_16BIT, 0x0014, 0x0001 },

	{ S5K8AAY_TOK_DELAY, 0, 20 },	       /* Delay 20ms */

	{ S5K8AAY_TOK_TERM, 0, 0 }
};

/* 19.Input Size Setting */
static struct s5k8aay_reg const s5k8aay_regs_19_1056x864[] = {
	{ S5K8AAY_TOK_16BIT, 0x0138, 0x0420 },	/* REG_TC_IPRM_InputWidthSize */
	{ S5K8AAY_TOK_16BIT, 0x013A, 0x0360 },	/* REG_TC_IPRM_InputHeightSize*/
	{ S5K8AAY_TOK_16BIT, 0x013C, 0x0070 },	/* REG_TC_IPRM_InputWidthOfs */
	{ S5K8AAY_TOK_16BIT, 0x013E, 0x0030 },	/* REG_TC_IPRM_InputHeightOfs */


	{ S5K8AAY_TOK_16BIT, 0x03A6, 0x04B0 },	/* REG_TC_PZOOM_ZoomInputWidth*/
	{ S5K8AAY_TOK_16BIT, 0x03A8, 0x0320 },	/*REG_TC_PZOOM_ZoomInputHeight*/
	{ S5K8AAY_TOK_16BIT, 0x03AA, 0x0000 },/*REG_TC_PZOOM_ZoomInputWidthOfs*/
	/* REG_TC_PZOOM_ZoomInputHeightOfs */
	{ S5K8AAY_TOK_16BIT, 0x03AC, 0x0000 },
	/* REG_TC_PZOOM_ZoomPanTiltRequest */
	{ S5K8AAY_TOK_16BIT, 0x03A0, 0x0001 },

	/* Preview config[0] 1056X864  xxfps // */
	/* REG_0TC_PCFG_usWidth 500:1280; 280:640  0500 */
	{ S5K8AAY_TOK_16BIT, 0x01BE, 0x0420 },
	/* REG_0TC_PCFG_usHeight 3C0:960; 1E0:480 */
	{ S5K8AAY_TOK_16BIT, 0x01C0, 0x0360 },
	/* REG_0TC_PCFG_Format 5:YUV422; 7:RAW10 */
	{ S5K8AAY_TOK_16BIT, 0x01C2, 0x0005 },
	/* REG_0TC_PCFG_uClockInd */
	{ S5K8AAY_TOK_16BIT, 0x01C8, 0x0000 },
	/* debug liao */
	/* REG_0TC_PCFG_PVIMask 52:YUV422, 42:RAW10   42;40;4E;4A;46 */
	{ S5K8AAY_TOK_16BIT, 0x01C4, 0x0052 },
	/* REG_0TC_PCFG_FrRateQualityType  1b:FR(bin) 2b:Quality(no-bin) */
	{ S5K8AAY_TOK_16BIT, 0x01D4, 0x0002 },
	/* REG_0TC_PCFG_usFrTimeType
	0:dynamic; 1:fixed not accurate; 2:fixed accurate */
	{ S5K8AAY_TOK_16BIT, 0x01D2, 0x0000 },
	/* 02BA	// REG_0TC_PCFG_usMaxFrTimeMsecMult10
	30fps-014D; 15fps-029A; 7.5-0535; 6.0-0682; 3.75-0A6A */
	{ S5K8AAY_TOK_16BIT, 0x01D8, 0x014D },
	/* 014D// REG_0TC_PCFG_usMinFrTimeMsecMult10 */
	{ S5K8AAY_TOK_16BIT, 0x01D6, 0x014D },
	{ S5K8AAY_TOK_16BIT, 0x01E8, 0x0000 },	/* REG_0TC_PCFG_uPrevMirror */
	/* REG_0TC_PCFG_uCaptureMirror */
	{ S5K8AAY_TOK_16BIT, 0x01EA, 0x0000 },

	/* // Capture config[0] 1056x864  xxfps */
	{ S5K8AAY_TOK_16BIT, 0x02AE, 0x0001 },	/* Capture mode AE On */
	/* REG_0TC_CCFG_usWidth 500:1280; 280:640 */
	{ S5K8AAY_TOK_16BIT, 0x02B0, 0x0420 },
	/* REG_0TC_CCFG_usHeight 3C0:960; 1E0:480 */
	{ S5K8AAY_TOK_16BIT, 0x02B2, 0x0360 },
	/* REG_0TC_CCFG_Format 5:YUV422; 7:RAW10 */
	{ S5K8AAY_TOK_16BIT, 0x02B4, 0x0005 },
	{ S5K8AAY_TOK_16BIT, 0x02BA, 0x0000 },	/* REG_0TC_CCFG_uClockInd */
	/* debug liao */
	/* REG_0TC_CCFG_PVIMask 52:YUV422; 42:RAW10  42;40;4E;4A;46 */
	{ S5K8AAY_TOK_16BIT, 0x02B6, 0x0042 },
	/* REG_0TC_CCFG_FrRateQualityType  1b:FR(bin) 2b:Quality(no-bin) */
	{ S5K8AAY_TOK_16BIT, 0x02C6, 0x0002 },
	/* REG_0TC_CCFG_usFrTimeType
	0:dynamic; 1:fixed not accurate;	2:fixed accurate */
	{ S5K8AAY_TOK_16BIT, 0x02C4, 0x0002 },
	/* 014D	// REG_0TC_CCFG_usMaxFrTimeMsecMult10
	30fps-014D; 15fps-029A; 7.5-0535; 6.0-0682; 3.75-0A6A */
	{ S5K8AAY_TOK_16BIT, 0x02CA, 0x014D },
	/* REG_0TC_CCFG_usMinFrTimeMsecMult10 */
	{ S5K8AAY_TOK_16BIT, 0x02C8, 0x0000 },

	{ S5K8AAY_TOK_TERM, 0, 0 }
};

/* 19.Input Size Setting */
static struct s5k8aay_reg const s5k8aay_regs_19_1200x800[] = {
	{ S5K8AAY_TOK_16BIT, 0x0138, 0x04B0 },	/* REG_TC_IPRM_InputWidthSize */
	{ S5K8AAY_TOK_16BIT, 0x013A, 0x0320 },	/* REG_TC_IPRM_InputHeightSize*/
	{ S5K8AAY_TOK_16BIT, 0x013C, 0x0028 },	/* REG_TC_IPRM_InputWidthOfs */
	{ S5K8AAY_TOK_16BIT, 0x013E, 0x0080 },	/* REG_TC_IPRM_InputHeightOfs */

	{ S5K8AAY_TOK_16BIT, 0x03A6, 0x04B0 },	/* REG_TC_PZOOM_ZoomInputWidth*/
	{ S5K8AAY_TOK_16BIT, 0x03A8, 0x0320 },	/*REG_TC_PZOOM_ZoomInputHeight*/
	/* REG_TC_PZOOM_ZoomInputWidthOfs */
	{ S5K8AAY_TOK_16BIT, 0x03AA, 0x0000 },
	/* REG_TC_PZOOM_ZoomInputHeightOfs */
	{ S5K8AAY_TOK_16BIT, 0x03AC, 0x0080 },
	/* REG_TC_PZOOM_ZoomPanTiltRequest */
	{ S5K8AAY_TOK_16BIT, 0x03A0, 0x0001 },

	/* Preview config[0] 1200X800  xxfps // */
	/* REG_0TC_PCFG_usWidth 500:1280; 280:640  0500 */
	{ S5K8AAY_TOK_16BIT, 0x01BE, 0x04B0 },
	/* REG_0TC_PCFG_usHeight 3C0:960; 1E0:480 */
	{ S5K8AAY_TOK_16BIT, 0x01C0, 0x0320 },
	/* REG_0TC_PCFG_Format 5:YUV422; 7:RAW10 */
	{ S5K8AAY_TOK_16BIT, 0x01C2, 0x0005 },
	{ S5K8AAY_TOK_16BIT, 0x01C8, 0x0000 },	/* REG_0TC_PCFG_uClockInd */
	/* debug liao */
	/* REG_0TC_PCFG_PVIMask 52:YUV422, 42:RAW10   42;40;4E;4A;46 */
	{ S5K8AAY_TOK_16BIT, 0x01C4, 0x0052 },
	/* REG_0TC_PCFG_FrRateQualityType  1b:FR(bin) 2b:Quality(no-bin) */
	{ S5K8AAY_TOK_16BIT, 0x01D4, 0x0002 },
	/* REG_0TC_PCFG_usFrTimeType
	0:dynamic; 1:fixed not accurate; 2:fixed accurate */
	{ S5K8AAY_TOK_16BIT, 0x01D2, 0x0000 },
	/* 02BA	// REG_0TC_PCFG_usMaxFrTimeMsecMult10
	 30fps-014D; 15fps-029A; 7.5-0535; 6.0-0682; 3.75-0A6A */
	{ S5K8AAY_TOK_16BIT, 0x01D8, 0x014D },
	/* 014D// REG_0TC_PCFG_usMinFrTimeMsecMult10 */
	{ S5K8AAY_TOK_16BIT, 0x01D6, 0x014D },
	{ S5K8AAY_TOK_16BIT, 0x01E8, 0x0000 },	/* REG_0TC_PCFG_uPrevMirror */
	{ S5K8AAY_TOK_16BIT, 0x01EA, 0x0000 },/* REG_0TC_PCFG_uCaptureMirror */

	/* // Capture config[0] 1200x800  xxfps */
	{ S5K8AAY_TOK_16BIT, 0x02AE, 0x0001 },	/* Capture mode AE On */
	/* REG_0TC_CCFG_usWidth 500:1280; 280:640 */
	{ S5K8AAY_TOK_16BIT, 0x02B0, 0x04B0 },
	/* REG_0TC_CCFG_usHeight 3C0:960; 1E0:480 */
	{ S5K8AAY_TOK_16BIT, 0x02B2, 0x0320 },
	/* REG_0TC_CCFG_Format 5:YUV422; 7:RAW10 */
	{ S5K8AAY_TOK_16BIT, 0x02B4, 0x0005 },
	{ S5K8AAY_TOK_16BIT, 0x02BA, 0x0000 },	/* REG_0TC_CCFG_uClockInd */
	/* debug liao */
	/* REG_0TC_CCFG_PVIMask 52:YUV422; 42:RAW10  42;40;4E;4A;46 */
	{ S5K8AAY_TOK_16BIT, 0x02B6, 0x0042 },
	/* REG_0TC_CCFG_FrRateQualityType  1b:FR(bin) 2b:Quality(no-bin) */
	{ S5K8AAY_TOK_16BIT, 0x02C6, 0x0002 },
	/* REG_0TC_CCFG_usFrTimeType
	0:dynamic; 1:fixed not accurate;	2:fixed accurate */
	{ S5K8AAY_TOK_16BIT, 0x02C4, 0x0002 },
	/* 014D	// REG_0TC_CCFG_usMaxFrTimeMsecMult10
	30fps-014D; 15fps-029A; 7.5-0535; 6.0-0682; 3.75-0A6A */
	{ S5K8AAY_TOK_16BIT, 0x02CA, 0x014D },
	/* REG_0TC_CCFG_usMinFrTimeMsecMult10 */
	{ S5K8AAY_TOK_16BIT, 0x02C8, 0x0000 },

	{ S5K8AAY_TOK_TERM, 0, 0 }
};

/* 19.Input Size Setting */
static struct s5k8aay_reg const s5k8aay_regs_19_1280x720[] = {
	{ S5K8AAY_TOK_16BIT, 0x0138, 0x0500 },	/* REG_TC_IPRM_InputWidthSize */
	/* 03C0 //REG_TC_IPRM_InputHeightSize */
	{ S5K8AAY_TOK_16BIT, 0x013A, 0x02D0 },
	{ S5K8AAY_TOK_16BIT, 0x013C, 0x0000 },	/* REG_TC_IPRM_InputWidthOfs */
	{ S5K8AAY_TOK_16BIT, 0x013E, 0x0078 },	/* REG_TC_IPRM_InputHeightOfs */


	{ S5K8AAY_TOK_16BIT, 0x03A6, 0x0500 },	/* REG_TC_PZOOM_ZoomInputWidth*/
	/* 03C0 //REG_TC_PZOOM_ZoomInputHeight */
	{ S5K8AAY_TOK_16BIT, 0x03A8, 0x02D0 },
	/* REG_TC_PZOOM_ZoomInputWidthOfs */
	{ S5K8AAY_TOK_16BIT, 0x03AA, 0x0000 },
	/* REG_TC_PZOOM_ZoomInputHeightOfs */
	{ S5K8AAY_TOK_16BIT, 0x03AC, 0x0078 },
	/* REG_TC_PZOOM_ZoomPanTiltRequest */
	{ S5K8AAY_TOK_16BIT, 0x03A0, 0x0001 },

	/* Preview config[0] 1280X720  30fps // */
	/* REG_0TC_PCFG_usWidth 500:1280; 280:640  0500 */
	{ S5K8AAY_TOK_16BIT, 0x01BE, 0x0500 },
	/* REG_0TC_PCFG_usHeight 3C0:960; 1E0:480 */
	{ S5K8AAY_TOK_16BIT, 0x01C0, 0x02D0 },
	/* REG_0TC_PCFG_Format 5:YUV422; 7:RAW10 */
	{ S5K8AAY_TOK_16BIT, 0x01C2, 0x0005 },
	{ S5K8AAY_TOK_16BIT, 0x01C8, 0x0000 },	/* REG_0TC_PCFG_uClockInd */
	/* debug liao */
	/* REG_0TC_PCFG_PVIMask 52:YUV422, 42:RAW10   42;40;4E;4A;46 */
	{ S5K8AAY_TOK_16BIT, 0x01C4, 0x0052 },
	/* REG_0TC_PCFG_FrRateQualityType  1b:FR(bin) 2b:Quality(no-bin) */
	{ S5K8AAY_TOK_16BIT, 0x01D4, 0x0002 },
	/* REG_0TC_PCFG_usFrTimeType
	0:dynamic; 1:fixed not accurate; 2:fixed accurate */
	{ S5K8AAY_TOK_16BIT, 0x01D2, 0x0000 },
	/* 02BA	// REG_0TC_PCFG_usMaxFrTimeMsecMult10
	30fps-014D; 15fps-029A; 7.5-0535; 6.0-0682; 3.75-0A6A */
	{ S5K8AAY_TOK_16BIT, 0x01D8, 0x014D },
	/* 014D// REG_0TC_PCFG_usMinFrTimeMsecMult10 */
	{ S5K8AAY_TOK_16BIT, 0x01D6, 0x014D },
	{ S5K8AAY_TOK_16BIT, 0x01E8, 0x0000 },	/* REG_0TC_PCFG_uPrevMirror */
	{ S5K8AAY_TOK_16BIT, 0x01EA, 0x0000 },/* REG_0TC_PCFG_uCaptureMirror */

	/* // Capture config[0] 1280x720  30fps */
	{ S5K8AAY_TOK_16BIT, 0x02AE, 0x0001 },	/* Capture mode AE On */
	/* REG_0TC_CCFG_usWidth 500:1280; 280:640 */
	{ S5K8AAY_TOK_16BIT, 0x02B0, 0x0500 },
	/* REG_0TC_CCFG_usHeight 3C0:960; 1E0:480 */
	{ S5K8AAY_TOK_16BIT, 0x02B2, 0x02D0 },
	/* REG_0TC_CCFG_Format 5:YUV422; 7:RAW10 */
	{ S5K8AAY_TOK_16BIT, 0x02B4, 0x0005 },
	{ S5K8AAY_TOK_16BIT, 0x02BA, 0x0000 },	/* REG_0TC_CCFG_uClockInd */
	/* debug liao */
	/* REG_0TC_CCFG_PVIMask 52:YUV422; 42:RAW10  42;40;4E;4A;46 */
	{ S5K8AAY_TOK_16BIT, 0x02B6, 0x0042 },
	/* REG_0TC_CCFG_FrRateQualityType  1b:FR(bin) 2b:Quality(no-bin) */
	{ S5K8AAY_TOK_16BIT, 0x02C6, 0x0002 },
	/* REG_0TC_CCFG_usFrTimeType
	0:dynamic; 1:fixed not accurate;	2:fixed accurate */
	{ S5K8AAY_TOK_16BIT, 0x02C4, 0x0002 },
	/* 014D	// REG_0TC_CCFG_usMaxFrTimeMsecMult10
	30fps-014D; 15fps-029A; 7.5-0535; 6.0-0682; 3.75-0A6A */
	{ S5K8AAY_TOK_16BIT, 0x02CA, 0x014D },
	/* REG_0TC_CCFG_usMinFrTimeMsecMult10 */
	{ S5K8AAY_TOK_16BIT, 0x02C8, 0x0000 },

	{ S5K8AAY_TOK_TERM, 0, 0 }
};

/* 19.Input Size Setting */
static struct s5k8aay_reg const s5k8aay_regs_19_1280x960[] = {

	{ S5K8AAY_TOK_16BIT, 0x0138, 0x0500 },	/* REG_TC_IPRM_InputWidthSize */
	{ S5K8AAY_TOK_16BIT, 0x013A, 0x03C0 },	/* REG_TC_IPRM_InputHeightSize*/
	{ S5K8AAY_TOK_16BIT, 0x013C, 0x0000 },	/* REG_TC_IPRM_InputWidthOfs */
	{ S5K8AAY_TOK_16BIT, 0x013E, 0x0000 },	/* REG_TC_IPRM_InputHeightOfs */

	{ S5K8AAY_TOK_16BIT, 0x03A6, 0x0500 },	/* REG_TC_PZOOM_ZoomInputWidth*/
	{ S5K8AAY_TOK_16BIT, 0x03A8, 0x03C0 },	/* REG_TC_PZOOM_ZoomInputHeigh*/
	/* REG_TC_PZOOM_ZoomInputWidthOfs */
	{ S5K8AAY_TOK_16BIT, 0x03AA, 0x0000 },
	/* REG_TC_PZOOM_ZoomInputHeightOfs */
	{ S5K8AAY_TOK_16BIT, 0x03AC, 0x0000 },
	/* REG_TC_PZOOM_ZoomPanTiltRequest */
	{ S5K8AAY_TOK_16BIT, 0x03A0, 0x0001 },

	/* Preview config[0] 1280X960  30fps // */
	/* REG_0TC_PCFG_usWidth 500:1280; 280:640  0500 */
	{ S5K8AAY_TOK_16BIT, 0x01BE, 0x0500 },
	/* REG_0TC_PCFG_usHeight 3C0:960; 1E0:480 */
	{ S5K8AAY_TOK_16BIT, 0x01C0, 0x03C0 },
	/* REG_0TC_PCFG_Format 5:YUV422; 7:RAW10 */
	{ S5K8AAY_TOK_16BIT, 0x01C2, 0x0005 },
	{ S5K8AAY_TOK_16BIT, 0x01C8, 0x0000 },	/* REG_0TC_PCFG_uClockInd */
	/* debug liao */
	/* REG_0TC_PCFG_PVIMask 52:YUV422, 42:RAW10   42;40;4E;4A;46 */
	{ S5K8AAY_TOK_16BIT, 0x01C4, 0x0052 },
	/* REG_0TC_PCFG_FrRateQualityType  1b:FR(bin) 2b:Quality(no-bin) */
	{ S5K8AAY_TOK_16BIT, 0x01D4, 0x0002 },
	/* REG_0TC_PCFG_usFrTimeType
	0:dynamic; 1:fixed not accurate; 2:fixed accurate */
	{ S5K8AAY_TOK_16BIT, 0x01D2, 0x0001 },
	/* 02BA	// REG_0TC_PCFG_usMaxFrTimeMsecMult10
	 30fps-014D; 15fps-029A; 7.5-0535; 6.0-0682; 3.75-0A6A */
	{ S5K8AAY_TOK_16BIT, 0x01D8, 0x014D },
	/* 014D// REG_0TC_PCFG_usMinFrTimeMsecMult10 */
	{ S5K8AAY_TOK_16BIT, 0x01D6, 0x0000 },
	{ S5K8AAY_TOK_16BIT, 0x01E8, 0x0000 },	/* REG_0TC_PCFG_uPrevMirror */
	{ S5K8AAY_TOK_16BIT, 0x01EA, 0x0000 },	/* REG_0TC_PCFG_uCaptureMirror*/

	/* // Capture config[0] 1280x960  xxfps */
	{ S5K8AAY_TOK_16BIT, 0x02AE, 0x0001 },	/* Capture mode AE On */
	/* REG_0TC_CCFG_usWidth 500:1280; 280:640 */
	{ S5K8AAY_TOK_16BIT, 0x02B0, 0x0500 },
	/* REG_0TC_CCFG_usHeight 3C0:960; 1E0:480 */
	{ S5K8AAY_TOK_16BIT, 0x02B2, 0x03C0 },
	/* REG_0TC_CCFG_Format 5:YUV422; 7:RAW10 */
	{ S5K8AAY_TOK_16BIT, 0x02B4, 0x0005 },
	{ S5K8AAY_TOK_16BIT, 0x02BA, 0x0000 },	/* REG_0TC_CCFG_uClockInd */
	/* debug liao */
	/* REG_0TC_CCFG_PVIMask 52:YUV422; 42:RAW10  42;40;4E;4A;46 */
	{ S5K8AAY_TOK_16BIT, 0x02B6, 0x0052 },
	/* REG_0TC_CCFG_FrRateQualityType  1b:FR(bin) 2b:Quality(no-bin) */
	{ S5K8AAY_TOK_16BIT, 0x02C6, 0x0002 },
	/* REG_0TC_CCFG_usFrTimeType
	0:dynamic; 1:fixed not accurate;	2:fixed accurate */
	{ S5K8AAY_TOK_16BIT, 0x02C4, 0x0001 },
	/* 014D	// REG_0TC_CCFG_usMaxFrTimeMsecMult10
	30fps-014D; 15fps-029A; 7.5-0535; 6.0-0682; 3.75-0A6A */
	{ S5K8AAY_TOK_16BIT, 0x02CA, 0x014D },
	/* REG_0TC_CCFG_usMinFrTimeMsecMult10 */
	{ S5K8AAY_TOK_16BIT, 0x02C8, 0x0000 },

	{ S5K8AAY_TOK_TERM, 0, 0 }
};

/* 21.Select Cofigration Display */
static struct s5k8aay_reg const s5k8aay_regs_21[] = {
	/* REG_TC_GP_ActivePreviewConfig */
	{ S5K8AAY_TOK_16BIT, 0x01A8, 0x0000 },
	/* REG_TC_GP_PrevOpenAfterChange */
	{ S5K8AAY_TOK_16BIT, 0x01AC, 0x0001 },
	{ S5K8AAY_TOK_16BIT, 0x01A6, 0x0001 },	/* REG_TC_GP_NewConfigSync */
	/* REG_TC_GP_PrevOpenAfterChange */
	{ S5K8AAY_TOK_16BIT, 0x01AC, 0x0001 },
	{ S5K8AAY_TOK_16BIT, 0x01A6, 0x0001 },	/* REG_TC_GP_NewConfigSync */
	/* REG_TC_GP_PreviewConfigChanged */
	{ S5K8AAY_TOK_16BIT, 0x01AA, 0x0001 },
	{ S5K8AAY_TOK_16BIT, 0x019E, 0x0001 },	/* REG_TC_GP_EnablePreview */
	/* REG_TC_GP_EnablePreviewChanged */
	{ S5K8AAY_TOK_16BIT, 0x01A0, 0x0001 },

	{ S5K8AAY_TOK_16BIT, 0xFCFC, 0xD000 },
	{ S5K8AAY_TOK_16BIT, 0x1000, 0x0001 },	/* Set host interrupt */

	{ S5K8AAY_TOK_DELAY, 0, 150 },	/* mdelay(150  //p150 //delay 150ms */

	{ S5K8AAY_TOK_16BIT, 0xFCFC, 0xD000 },
	{ S5K8AAY_TOK_16BIT, 0x0004, 0x0001 },
	{ S5K8AAY_TOK_16BIT, 0xFCFC, 0xD000 },
	{ S5K8AAY_TOK_16BIT, 0x0004, 0x0001 },
	{ S5K8AAY_TOK_16BIT, 0xFCFC, 0xD000 },
	{ S5K8AAY_TOK_16BIT, 0x0004, 0x0001 },

	{ S5K8AAY_TOK_TERM, 0, 0 }
};
