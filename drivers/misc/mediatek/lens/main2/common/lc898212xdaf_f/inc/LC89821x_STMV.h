/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __LC89821x_STMV__
#define	__LC89821x_STMV__


/* ************************** */
/* Definations */
/* ************************** */
/* Convergence Judgement */
#define INI_MSSET_211		((unsigned char)0x00)
#define CHTGX_THRESHOLD		((unsigned short)0x0200)
#define CHTGOKN_TIME		((unsigned char)0x80)
#define CHTGOKN_WAIT		3

/* StepMove */
#define STMV_SIZE		((unsigned short)0x0180)
#define STMV_INTERVAL		((unsigned char)0x01)

#define STMCHTG_ON		((unsigned char)0x08)
#define STMSV_ON		((unsigned char)0x04)
#define STMLFF_ON		((unsigned char)0x02)
#define STMVEN_ON		((unsigned char)0x01)
#define STMCHTG_OFF		((unsigned char)0x00)
#define STMSV_OFF		((unsigned char)0x00)
#define STMLFF_OFF		((unsigned char)0x00)
#define STMVEN_OFF		((unsigned char)0x00)

#define STMCHTG_SET		STMCHTG_ON
#define STMSV_SET		STMSV_ON
#define STMLFF_SET		STMLFF_OFF

#define	WAIT			0xFF

#define	ADHXI_211H	0x00
#define	ADHXI_211L	0x01
#define	PIDZO_211H	0x02
#define	PIDZO_211L	0x03
#define	RZ_211H		0x04
#define	RZ_211L		0x05
#define	DZ1_211H	0x06
#define	DZ1_211L	0x07
#define	DZ2_211H	0x08
#define	DZ2_211L	0x09
#define	UZ1_211H	0x0A
#define	UZ1_211L	0x0B
#define	UZ2_211H	0x0C
#define	UZ2_211L	0x0D
#define	IZ1_211H	0x0E
#define	IZ1_211L	0x0F
#define	IZ2_211H	0x10
#define	IZ2_211L	0x11
#define	MS1Z01_211H	0x12
#define	MS1Z01_211L	0x13
#define	MS1Z11_211H	0x14
#define	MS1Z11_211L	0x15
#define	MS1Z12_211H	0x16
#define	MS1Z12_211L	0x17
#define	MS1Z22_211H	0x18
#define	MS1Z22_211L	0x19
#define	MS2Z01_211H	0x1A
#define	MS2Z01_211L	0x1B
#define	MS2Z11_211H	0x1C
#define	MS2Z11_211L	0x1D
#define	MS2Z12_211H	0x1E
#define	MS2Z12_211L	0x1F
#define	MS2Z22_211H	0x20
#define	MS2Z22_211L	0x21
#define	MS2Z23_211H	0x22
#define	MS2Z23_211L	0x23
#define	OZ1_211H	0x24
#define	OZ1_211L	0x25
#define	OZ2_211H	0x26
#define	OZ2_211L	0x27
#define	DAHLXO_211H	0x28
#define	DAHLXO_211L	0x29
#define	OZ3_211H	0x2A
#define	OZ3_211L	0x2B
#define	OZ4_211H	0x2C
#define	OZ4_211L	0x2D
#define	OZ5_211H	0x2E
#define	OZ5_211L	0x2F
#define	oe_211H		0x30
#define	oe_211L		0x31
#define	MSR1CMAX_211H	0x32
#define	MSR1CMAX_211L	0x33
#define	MSR1CMIN_211H	0x34
#define	MSR1CMIN_211L	0x35
#define	MSR2CMAX_211H	0x36
#define	MSR2CMAX_211L	0x37
#define	MSR2CMIN_211H	0x38
#define	MSR2CMIN_211L	0x39
#define	OFFSET_211H	0x3A
#define	OFFSET_211L	0x3B
#define	ADOFFSET_211H	0x3C
#define	ADOFFSET_211L	0x3D
#define	EZ_211H		0x3E
#define	EZ_211L		0x3F

#define	ag_211H			0x40
#define	ag_211L			0x41
#define	da_211H			0x42
#define	da_211L			0x43
#define	db_211H			0x44
#define	db_211L			0x45
#define	dc_211H			0x46
#define	dc_211L			0x47
#define	dg_211H			0x48
#define	dg_211L			0x49
#define	pg_211H			0x4A
#define	pg_211L			0x4B
#define	gain1_211H		0x4C
#define	gain1_211L		0x4D
#define	gain2_211H		0x4E
#define	gain2_211L		0x4F
#define	ua_211H			0x50
#define	ua_211L			0x51
#define	uc_211H			0x52
#define	uc_211L			0x53
#define	ia_211H			0x54
#define	ia_211L			0x55
#define	ib_211H			0x56
#define	ib_211L			0x57
#define	ic_211H			0x58
#define	ic_211L			0x59
#define	ms11a_211H		0x5A
#define	ms11a_211L		0x5B
#define	ms11c_211H		0x5C
#define	ms11c_211L		0x5D
#define	ms12a_211H		0x5E
#define	ms12a_211L		0x5F
#define	ms12c_211H		0x60
#define	ms12c_211L		0x61
#define	ms21a_211H		0x62
#define	ms21a_211L		0x63
#define	ms21b_211H		0x64
#define	ms21b_211L		0x65
#define	ms21c_211H		0x66
#define	ms21c_211L		0x67
#define	ms22a_211H		0x68
#define	ms22a_211L		0x69
#define	ms22c_211H		0x6A
#define	ms22c_211L		0x6B
#define	ms22d_211H		0x6C
#define	ms22d_211L		0x6D
#define	ms22e_211H		0x6E
#define	ms22e_211L		0x6F
#define	ms23p_211H		0x70
#define	ms23p_211L		0x71
#define	oa_211H			0x72
#define	oa_211L			0x73
#define	oc_211H			0x74
#define	oc_211L			0x75
#define	PX12_211H		0x76
#define	PX12_211L		0x77
#define	PX3_211H		0x78
#define	PX3_211L		0x79
#define	MS2X_211H		0x7A
#define	MS2X_211L		0x7B
#define	CHTGX_211H		0x7C
#define	CHTGX_211L		0x7D
#define	CHTGN_211H		0x7E
#define	CHTGN_211L		0x7F

#define	CLKSEL_211		0x80
#define	ADSET_211		0x81
#define	PWMSEL_211		0x82
#define	SWTCH_211		0x83
#define	STBY_211		0x84
#define	CLR_211			0x85
#define	DSSEL_211		0x86
#define	ENBL_211		0x87
#define	ANA1_211		0x88
#define	AFSEND_211		0x8A
#define	STMVEN_211		0x8A
#define	STPT_211		0x8B
#define	SWFC_211		0x8C
#define	SWEN_211		0x8D
#define	MSNUM_211		0x8E
#define	MSSET_211		0x8F
#define	DLYMON_211		0x90
#define	MONA_211		0x91
#define	PWMLIMIT_211		0x92
#define	PINSEL_211		0x93
#define	PWMSEL2_211		0x94
#define	SFTRST_211		0x95
#define	TEST_211		0x96
#define	PWMZONE2_211		0x97
#define	PWMZONE1_211		0x98
#define	PWMZONE0_211		0x99
#define	ZONE3_211		0x9A
#define	ZONE2_211		0x9B
#define	ZONE1_211		0x9C
#define	ZONE0_211		0x9D
#define	GCTIM_211		0x9E
#define	GCTIM_211NU		0x9F
#define	STMINT_211		0xA0
#define	STMVENDH_211		0xA1
#define	STMVENDL_211		0xA2
#define	MSNUMR_211		0xA3
#define ANA2_211		0xA4

struct INIDATA {
	unsigned short addr;
	unsigned short data;
} IniData_F;

/* Camera Module Small */
/* IMX214 + LC898212XD */
/* IMX258 + LC898212XD */
const struct INIDATA Init_Table_F[] = {
	/* Addr,   Data */

	{0x0080, 0x34},		/* CLKSEL 1/1, CLKON */
	{0x0081, 0x20},		/* AD 4Time */
	{0x0084, 0xE0},		/* STBY   AD ON,DA ON,OP ON */
	{0x0087, 0x05},		/* PIDSW OFF,AF ON,MS2 ON */
	{0x00A4, 0x24},		/* Internal OSC Setup (No01=24.18MHz) */

	{0x003A, 0x0000},	/* OFFSET Clear */
	{0x0004, 0x0000},	/* RZ Clear(Target Value) */
	{0x0002, 0x0000},	/* PIDZO Clear */
	{0x0018, 0x0000},	/* MS1Z22 Clear(STMV Target Value) */

	/* Filter Setting: ST140911-1.h For TVC-651 */
	{0x0088, 0x70},
	{0x0028, 0x8080},
	{0x004C, 0x4000},
	{0x0083, 0x2C},
	{0x0085, 0xC0},
	{WAIT, 1},		/* Wait 1 ms */

	{0x0085, 0x00},
	{0x0084, 0xE3},
	{0x0097, 0x00},
	{0x0098, 0x42},
	{0x0099, 0x00},
	{0x009A, 0x00},

	{0x0086, 0x40},
	{0x0040, 0x7ff0},
	{0x0042, 0x7150},
	{0x0044, 0x8F90},
	{0x0046, 0x61B0},
	{0x0048, 0x3930},
	{0x004A, 0x2410},
	{0x004C, 0x4030},
	{0x004E, 0x7FF0},
	{0x0050, 0x04f0},
	{0x0052, 0x7610},
	{0x0054, 0x1450},
	{0x0056, 0x0000},
	{0x0058, 0x7FF0},
	{0x005A, 0x0680},
	{0x005C, 0x72f0},
	{0x005E, 0x7f70},
	{0x0060, 0x7ed0},
	{0x0062, 0x7ff0},
	{0x0064, 0x0000},
	{0x0066, 0x0000},
	{0x0068, 0x5130},
	{0x006A, 0x72f0},
	{0x006C, 0x8010},
	{0x006E, 0x0000},
	{0x0070, 0x0000},
	{0x0072, 0x18e0},
	{0x0074, 0x4e30},
	{0x0030, 0x0000},
	{0x0076, 0x0C50},
	{0x0078, 0x4000},
	{WAIT, 5},		/* Wait 5 ms */

	{0x0086, 0x60},

	{0x0087, 0x85} };

struct stSmvPar {
	unsigned short UsSmvSiz;
	unsigned char UcSmvItv;
	unsigned char UcSmvEnb;
};

extern int s4AF_WriteReg_LC898212XDAF_F(u8 *a_pSendData, u16 a_sizeSendData,
					u16 i2cId);
extern int s4AF_ReadReg_LC898212XDAF_F(u8 *a_pSendData, u16 a_sizeSendData,
				       u8 *a_pRecvData, u16 a_sizeRecvData,
				       u16 i2cId);

#endif
