/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */



/*******************************************************************************
 *  OisDef.h - Header file for LC898122
 *
 *  SANYO Semiconductor
 *
 *  REVISION:
 *      2013/01/07 - First Edition, Y.Shigeoka
 ******************************************************************************/

/* ========================================================================= */
/* PWM Register */
/* ========================================================================= */
/* #define                                                       0x0000 */
#define		DRVFC				0x0001
#define		DRVFC2				0x0002
#define		DRVSELX				0x0003
#define		DRVSELY				0x0004
#define		DRVCH1SEL			0x0005
#define		DRVCH2SEL			0x0006
/* #define                                                       0x0007 */
/* #define                                                       0x0008 */
/* #define                                                       0x0009 */
/* #define                                                       0x000A */
/* #define                                                       0x000B */
/* #define                                                       0x000C */
/* #define                                                       0x000D */
/* #define                                                       0x000E */
/* #define                                                       0x000F */
#define		PWMA				0x0010
#define		PWMFC				0x0011
#define		PWMDLYX				0x0012
#define		PWMDLYY				0x0013
#define		PWMDLYTIMX			0x0014
#define		PWMDLYTIMY			0x0015
#define		PWMFC2				0x0016
/* #define                                                       0x0017 */
#define		PWMPERIODX			0x0018	/* none (122) */
#define		PWMPERIODX2			0x0019	/* none (122) */
#define		PWMPERIODY			0x001A	/* PWMPERIODX (122) */
#define		PWMPERIODY2			0x001B	/* PWMPERIODY (122) */
#define		STROBEFC			0x001C
#define		STROBEDLYX			0x001D
#define		STROBEDLYY			0x001E
/* #define                                                       0x001F */
#define		CVA					0x0020
#define		CVFC				0x0021
#define		CVFC2				0x0022
#define		CVSMTHX				0x0023
#define		CVSMTHY				0x0024
/* #define                                                       0x0025 */
/* #define                                                       0x0026 */
/* #define                                                       0x0027 */
/* #define                                                       0x0028 */
/* #define                                                       0x0029 */
/* #define                                                       0x002A */
/* #define                                                       0x002B */
/* #define                                                       0x002C */
/* #define                                                       0x002D */
/* #define                                                       0x002E */
/* #define                                                       0x002F */
#define		PWMMONA				0x0030
#define		PWMMONFC			0x0031
#define		DACMONFC			0x0032
/* #define                                                       0x0033 */
/* #define                                                       0x0034 */
/* #define                                                       0x0035 */
/* #define                                                       0x0036 */
/* #define                                                       0x0037 */
/* #define                                                       0x0038 */
/* #define                                                       0x0039 */
/* #define                                                       0x003A */
/* #define                                                       0x003B */
/* #define                                                       0x003C */
/* #define                                                       0x003D */
/* #define                                                       0x003E */
/* #define                                                       0x003F */
#define		DACSLVADD			0x0040
#define		DACMSTCODE			0x0041
#define		DACFSCKRATE			0x0042
#define		DACHSCKRATE			0x0043
#define		DACI2CFC			0x0044
#define		DACI2CA				0x0045
/* #define                                                       0x0046 */
/* #define                                                       0x0047 */
/* #define                                                       0x0048 */
/* #define                                                       0x0049 */
/* #define                                                       0x004A */
/* #define                                                       0x004B */
/* #define                                                       0x004C */
/* #define                                                       0x004D */
/* #define                                                       0x004E */
/* #define                                                       0x004F */
/* #define                                                       0x0050 */
/* #define                                                       0x0051 */
/* #define                                                       0x0052 */
/* #define                                                       0x0053 */
/* #define                                                       0x0054 */
/* #define                                                       0x0055 */
/* #define                                                       0x0056 */
/* #define                                                       0x0057 */
/* #define                                                       0x0058 */
/* #define                                                       0x0059 */
/* #define                                                       0x005A */
/* #define                                                       0x005B */
/* #define                                                       0x005C */
/* #define                                                       0x005D */
/* #define                                                       0x005E */
/* #define                                                       0x005F */
/* #define                                                       0x0060 */
/* #define                                                       0x0061 */
/* #define                                                       0x0062 */
/* #define                                                       0x0063 */
/* #define                                                       0x0064 */
/* #define                                                       0x0065 */
/* #define                                                       0x0066 */
/* #define                                                       0x0067 */
/* #define                                                       0x0068 */
/* #define                                                       0x0069 */
/* #define                                                       0x006A */
/* #define                                                       0x006B */
/* #define                                                       0x006C */
/* #define                                                       0x006D */
/* #define                                                       0x006E */
/* #define                                                       0x006F */
/* #define                                                       0x0070 */
/* #define                                                       0x0071 */
/* #define                                                       0x0072 */
/* #define                                                       0x0073 */
/* #define                                                       0x0074 */
/* #define                                                       0x0075 */
/* #define                                                       0x0076 */
/* #define                                                       0x0077 */
/* #define                                                       0x0078 */
/* #define                                                       0x0079 */
/* #define                                                       0x007A */
/* #define                                                       0x007B */
/* #define                                                       0x007C */
/* #define                                                       0x007D */
/* #define                                                       0x007E */
/* #define                                                       0x007F */
/* #define                                                       0x0080 */
#define		DRVFCAF				0x0081
#define		DRVFC2AF			0x0082
#define		DRVFC3AF			0x0083
#define		DRVFC4AF			0x0084
#define		DRVCH3SEL			0x0085
/* #define                                                       0x0086 */
/* #define                                                       0x0087 */
#define		AFFC				0x0088
/* #define                                                       0x0089 */
/* #define                                                       0x008A */
/* #define                                                       0x008B */
/* #define                                                       0x008C */
/* #define                                                       0x008D */
/* #define                                                       0x008E */
/* #define                                                       0x008F */
#define		PWMAAF				0x0090
#define		PWMFCAF				0x0091
#define		PWMDLYAF			0x0092
#define		PWMDLYTIMAF			0x0093
/* #define                                                       0x0094 */
/* #define                                                       0x0095 */
/* #define                                                       0x0096 */
/* #define                                                       0x0097 */
/* #define                                                       0x0098 */
#define		PWMPERIODAF			0x0099
/* #define                                                       0x009A */
/* #define                                                       0x009B */
/* #define                                                       0x009C */
/* #define                                                       0x009D */
/* #define                                                       0x009E */
/* #define                                                       0x009F */
#define		CCAAF				0x00A0
#define		CCFCAF				0x00A1
/* #define                                                       0x00A2 */
/* #define                                                       0x00A3 */
/* #define                                                       0x00A4 */
/* #define                                                       0x00A5 */
/* #define                                                       0x00A6 */
/* #define                                                       0x00A7 */
/* #define                                                       0x00A8 */
/* #define                                                       0x00A9 */
/* #define                                                       0x00AA */
/* #define                                                       0x00AB */
/* #define                                                       0x00AC */
/* #define                                                       0x00AD */
/* #define                                                       0x00AE */
/* #define                                                       0x00AF */
/* #define                                                       0x00B0 */
/* #define                                                       0x00B1 */
/* #define                                                       0x00B2 */
/* #define                                                       0x00B3 */
/* #define                                                       0x00B4 */
/* #define                                                       0x00B5 */
/* #define                                                       0x00B6 */
/* #define                                                       0x00B7 */
/* #define                                                       0x00B8 */
/* #define                                                       0x00B9 */
/* #define                                                       0x00BA */
/* #define                                                       0x00BB */
/* #define                                                       0x00BC */
/* #define                                                       0x00BD */
/* #define                                                       0x00BE */
/* #define                                                       0x00BF */
/* #define                                                       0x00C0 */
/* #define                                                       0x00C1 */
/* #define                                                       0x00C2 */
/* #define                                                       0x00C3 */
/* #define                                                       0x00C4 */
/* #define                                                       0x00C5 */
/* #define                                                       0x00C6 */
/* #define                                                       0x00C7 */
/* #define                                                       0x00C8 */
/* #define                                                       0x00C9 */
/* #define                                                       0x00CA */
/* #define                                                       0x00CB */
/* #define                                                       0x00CC */
/* #define                                                       0x00CD */
/* #define                                                       0x00CE */
/* #define                                                       0x00CF */
/* #define                                                       0x00D0 */
/* #define                                                       0x00D1 */
/* #define                                                       0x00D2 */
/* #define                                                       0x00D3 */
/* #define                                                       0x00D4 */
/* #define                                                       0x00D5 */
/* #define                                                       0x00D6 */
/* #define                                                       0x00D7 */
/* #define                                                       0x00D8 */
/* #define                                                       0x00D9 */
/* #define                                                       0x00DA */
/* #define                                                       0x00DB */
/* #define                                                       0x00DC */
/* #define                                                       0x00DD */
/* #define                                                       0x00DE */
/* #define                                                       0x00DF */
/* #define                                                       0x00E0 */
/* #define                                                       0x00E1 */
/* #define                                                       0x00E2 */
/* #define                                                       0x00E3 */
/* #define                                                       0x00E4 */
/* #define                                                       0x00E5 */
/* #define                                                       0x00E6 */
/* #define                                                       0x00E7 */
/* #define                                                       0x00E8 */
/* #define                                                       0x00E9 */
/* #define                                                       0x00EA */
/* #define                                                       0x00EB */
/* #define                                                       0x00EC */
/* #define                                                       0x00ED */
/* #define                                                       0x00EE */
/* #define                                                       0x00EF */
/* #define                                                       0x00F0 */
/* #define                                                       0x00F1 */
/* #define                                                       0x00F2 */
/* #define                                                       0x00F3 */
/* #define                                                       0x00F4 */
/* #define                                                       0x00F5 */
/* #define                                                       0x00F6 */
/* #define                                                       0x00F7 */
/* #define                                                       0x00F8 */
/* #define                                                       0x00F9 */
/* #define                                                       0x00FA */
/* #define                                                       0x00FB */
/* #define                                                       0x00FC */
/* #define                                                       0x00FD */
/* #define                                                       0x00FE */
#define		MDLREG				0x00FF


/* ===================================================================== */
/* Filter Register */
/* ===================================================================== */
/* #define                                                       0x0100 */
#define		WC_EQON				0x0101
#define		WC_RAMINITON		0x0102
#define		WC_CPUOPEON			0x0103
#define		WC_VMON				0x0104
#define		WC_DPON				0x0105
/* #define                                                       0x0106 */
#define		WG_SHTON			0x0107
#define		WG_ADJGANGO			0x0108
#define		WG_PANON			0x0109
#define		WG_PANSTT6			0x010A
#define		WG_NPANSTFRC		0x010B
#define		WG_CNTPICGO			0x010C
#define		WG_NPANINION		0x010D
#define		WG_NPANSTOFF		0x010E
/* #define                                                       0x010F */
#define		WG_EQSW				0x0110
#define		WG_DWNSMP1			0x0111
#define		WG_DWNSMP2			0x0112
#define		WG_DWNSMP3			0x0113
#define		WG_DWNSMP4			0x0114
/* #define                                                       0x0115 */
#define		WG_SHTMOD			0x0116
#define		WG_SHTDLYTMR		0x0117
#define		WG_LMT3MOD			0x0118
#define		WG_VREFADD			0x0119
/* #define                                                       0x011A */
#define		WG_HCHR				0x011B
#define		WG_GADSMP			0x011C
/* #define                                                       0x011D */
/* #define                                                       0x011E */
/* #define                                                       0x011F */
#define		WG_LEVADD			0x0120
#define		WG_LEVTMRLOW		0x0121
#define		WG_LEVTMRHGH		0x0122
#define		WG_LEVTMR			0x0123
/* #define                                                       0x0124 */
/* #define                                                       0x0125 */
/* #define                                                       0x0126 */
/* #define                                                       0x0127 */
#define		WG_ADJGANADD		0x0128
#define		WG_ADJGANGXATO		0x0129
#define		WG_ADJGANGYATO		0x012A
/* #define                                                       0x012B */
/* #define                                                       0x012C */
/* #define                                                       0x012D */
/* #define                                                       0x012E */
/* #define                                                       0x012F */
#define		WG_PANADDA			0x0130
#define		WG_PANADDB			0x0131
#define		WG_PANTRSON0		0x0132
#define		WG_PANLEVABS		0x0133
#define		WG_PANSTT1DWNSMP0	0x0134
#define		WG_PANSTT1DWNSMP1	0x0135
#define		WG_PANSTT2DWNSMP0	0x0136
#define		WG_PANSTT2DWNSMP1	0x0137
#define		WG_PANSTT3DWNSMP0	0x0138
#define		WG_PANSTT3DWNSMP1	0x0139
#define		WG_PANSTT4DWNSMP0	0x013A
#define		WG_PANSTT4DWNSMP1	0x013B
#define		WG_PANSTT2TMR0		0x013C
#define		WG_PANSTT2TMR1		0x013D
#define		WG_PANSTT4TMR0		0x013E
#define		WG_PANSTT4TMR1		0x013F
#define		WG_PANSTT21JUG0		0x0140
#define		WG_PANSTT21JUG1		0x0141
#define		WG_PANSTT31JUG0		0x0142
#define		WG_PANSTT31JUG1		0x0143
#define		WG_PANSTT41JUG0		0x0144
#define		WG_PANSTT41JUG1		0x0145
#define		WG_PANSTT12JUG0		0x0146
#define		WG_PANSTT12JUG1		0x0147
#define		WG_PANSTT13JUG0		0x0148
#define		WG_PANSTT13JUG1		0x0149
#define		WG_PANSTT23JUG0		0x014A
#define		WG_PANSTT23JUG1		0x014B
#define		WG_PANSTT43JUG0		0x014C
#define		WG_PANSTT43JUG1		0x014D
#define		WG_PANSTT34JUG0		0x014E
#define		WG_PANSTT34JUG1		0x014F
#define		WG_PANSTT24JUG0		0x0150
#define		WG_PANSTT24JUG1		0x0151
#define		WG_PANSTT42JUG0		0x0152
#define		WG_PANSTT42JUG1		0x0153
#define		WG_PANSTTSETGYRO	0x0154
#define		WG_PANSTTSETGAIN	0x0155
#define		WG_PANSTTSETISTP	0x0156
#define		WG_PANSTTSETIFTR	0x0157
#define		WG_PANSTTSETLFTR	0x0158
/* #define                                                       0x0159 */
#define		WG_PANSTTXXXTH		0x015A
#define		WG_PANSTT1LEVTMR	0x015B
#define		WG_PANSTT2LEVTMR	0x015C
#define		WG_PANSTT3LEVTMR	0x015D
#define		WG_PANSTT4LEVTMR	0x015E
#define		WG_PANSTTSETILHLD	0x015F
#define		WG_STT3MOD			0x0160
#define		WG_STILMOD			0x0161
#define		WG_PLAYON			0x0162
#define		WG_NPANJ2DWNSMP		0x0163
#define		WG_NPANTST0			0x0164
#define		WG_NPANDWNSMP		0x0165
#define		WG_NPANST3RTMR		0x0166
#define		WG_NPANST12BTMR		0x0167
#define		WG_NPANST12TMRX		0x0168
#define		WG_NPANST12TMRY		0x0169
#define		WG_NPANST3TMRX		0x016A
#define		WG_NPANST3TMRY		0x016B
#define		WG_NPANST4TMRX		0x016C
#define		WG_NPANST4TMRY		0x016D
#define		WG_NPANFUN			0x016E
#define		WG_NPANINITMR		0x016F
#define		WH_EQSWX			0x0170
#define		WH_EQSWY			0x0171
#define		EQSINSW				0x3C
#define		WH_DWNSMP1			0x0172
#define		WH_G2SDLY			0x0173
#define		WH_HOFCON			0x0174
/* #define                                                       0x0175 */
/* #define                                                       0x0176 */
/* #define                                                       0x0177 */
#define		WH_EMGSTPON			0x0178
/* #define                                                       0x0179 */
#define		WH_EMGSTPTMR		0x017A
/* #define                                                       0x017B */
#define		WH_SMTSRVON			0x017C
#define		WH_SMTSRVSMP		0x017D
#define		WH_SMTTMR			0x017E
/* #define                                                       0x017F */
#define		WC_SINON			0x0180
#define		WC_SINFRQ0			0x0181
#define		WC_SINFRQ1			0x0182
#define		WC_SINPHSX			0x0183
#define		WC_SINPHSY			0x0184
/* #define                                                       0x0185 */
/* #define                                                       0x0186 */
/* #define                                                       0x0187 */
#define		WC_ADMODE			0x0188
/* #define                                                       0x0189 */
#define		WC_CPUOPE1ADD		0x018A
#define		WC_CPUOPE2ADD		0x018B
#define		WC_RAMACCMOD		0x018C
#define		WC_RAMACCXY			0x018D
#define		WC_RAMDLYMOD0		0x018E
#define		WC_RAMDLYMOD1		0x018F
#define		WC_MESMODE			0x0190
#define		WC_MESSINMODE		0x0191
#define		WC_MESLOOP0			0x0192
#define		WC_MESLOOP1			0x0193
#define		WC_MES1ADD0			0x0194
#define		WC_MES1ADD1			0x0195
#define		WC_MES2ADD0			0x0196
#define		WC_MES2ADD1			0x0197
#define		WC_MESABS			0x0198
#define		WC_MESWAIT			0x0199
/* #define                                                       0x019A */
/* #define                                                       0x019B */
/* #define                                                       0x019C */
#define		RC_MESST			0x019D
#define		RC_MESLOOP0			0x019E
#define		RC_MESLOOP1			0x019F
#define		WC_AMJMODE			0x01A0
#define		WC_AMJDF			0x01A1
#define		WC_AMJLOOP0			0x01A2
#define		WC_AMJLOOP1			0x01A3
#define		WC_AMJIDL0			0x01A4
#define		WC_AMJIDL1			0x01A5
#define		WC_AMJ1ADD0			0x01A6
#define		WC_AMJ1ADD1			0x01A7
#define		WC_AMJ2ADD0			0x01A8
#define		WC_AMJ2ADD1			0x01A9
/* #define                                                       0x01AA */
/* #define                                                       0x01AB */
#define		RC_AMJST			0x01AC
#define		RC_AMJERROR			0x01AD
#define		RC_AMJLOOP0			0x01AE
#define		RC_AMJLOOP1			0x01AF
#define		WC_DPI1ADD0			0x01B0
#define		WC_DPI1ADD1			0x01B1
#define		WC_DPI2ADD0			0x01B2
#define		WC_DPI2ADD1			0x01B3
#define		WC_DPI3ADD0			0x01B4
#define		WC_DPI3ADD1			0x01B5
#define		WC_DPI4ADD0			0x01B6
#define		WC_DPI4ADD1			0x01B7
#define		WC_DPO1ADD0			0x01B8
#define		WC_DPO1ADD1			0x01B9
#define		WC_DPO2ADD0			0x01BA
#define		WC_DPO2ADD1			0x01BB
#define		WC_DPO3ADD0			0x01BC
#define		WC_DPO3ADD1			0x01BD
#define		WC_DPO4ADD0			0x01BE
#define		WC_DPO4ADD1			0x01BF
#define		WC_PINMON1			0x01C0
#define		WC_PINMON2			0x01C1
#define		WC_PINMON3			0x01C2
#define		WC_PINMON4			0x01C3
#define		WC_DLYMON10			0x01C4
#define		WC_DLYMON11			0x01C5
#define		WC_DLYMON20			0x01C6
#define		WC_DLYMON21			0x01C7
#define		WC_DLYMON30			0x01C8
#define		WC_DLYMON31			0x01C9
#define		WC_DLYMON40			0x01CA
#define		WC_DLYMON41			0x01CB
/* #define                                                       0x01CC */
/* #define                                                       0x01CD */
#define		WC_INTMSK			0x01CE
/* #define                                                       0x01CF */
#define		WC_FRCAD			0x01D0
#define		WC_FRCADEN			0x01D1
#define		WC_ADRES			0x01D2
#define		WC_TSTMON			0x01D3
#define		WC_RAMACCTM0		0x01D4
#define		WC_RAMACCTM1		0x01D5
/* #define                                                       0x01D6 */
/* #define                                                       0x01D7 */
/* #define                                                       0x01D8 */
/* #define                                                       0x01D9 */
/* #define                                                       0x01DA */
/* #define                                                       0x01DB */
/* #define                                                       0x01DC */
/* #define                                                       0x01DD */
/* #define                                                       0x01DE */
/* #define                                                       0x01DF */
#define		WC_EQSW				0x01E0
#define		WC_STPMV			0x01E1
#define		WC_STPMVMOD			0x01E2
#define		WC_DWNSMP1			0x01E3
#define		WC_DWNSMP2			0x01E4
#define		WC_DWNSMP3			0x01E5
#define		WC_LEVTMP			0x01E6
#define		WC_DIFTMP			0x01E7
#define		WC_L10				0x01E8
#define		WC_L11				0x01E9
/* #define                                                       0x01EA */
/* #define                                                       0x01EB */
/* #define                                                       0x01EC */
/* #define                                                       0x01ED */
/* #define                                                       0x01EE */
/* #define                                                       0x01EF */
#define		RG_XPANFIL			0x01F0
#define		RG_YPANFIL			0x01F1
#define		RG_XPANRAW			0x01F2
#define		RG_YPANRAW			0x01F3
#define		RG_LEVJUGE			0x01F4
#define		RG_NXPANST			0x01F5
#define		RC_RAMACC			0x01F6
#define		RH_EMLEV			0x01F7
#define		RH_SMTSRVSTT		0x01F8
#define		RC_CNTPIC			0x01F9
#define		RC_LEVDIF			0x01FA
/* #define                                                       0x01FB */
/* #define                                                       0x01FC */
/* #define                                                       0x01FD */
#define		RC_FLG0				0x01FE
#define		RC_INT				0x01FF


/* ==================================================================== */
/* System Register */
/* ==================================================================== */
/* #define                                                       0x0200 */
/* #define                                                       0x0201 */
/* #define                                                       0x0202 */
/* #define                                                       0x0203 */
/* #define                                                       0x0204 */
/* #define                                                       0x0205 */
/* #define                                                       0x0206 */
/* #define                                                       0x0207 */
/* #define                                                       0x0208 */
/* #define                                                       0x0209 */
#define		CLKTST				0x020A
#define		CLKON				0x020B
#define		CLKSEL				0x020C
/* #define                                                       0x020D */
/* #define                                                       0x020E */
/* #define                                                       0x020F */
#define		PWMDIV				0x0210
#define		SRVDIV				0x0211
#define		GIFDIV				0x0212
#define		AFPWMDIV			0x0213
#define		OPAFDIV				0x0214
/* #define                                                       0x0215 */
/* #define                                                       0x0216 */
/* #define                                                       0x0217 */
/* #define                                                       0x0218 */
/* #define                                                       0x0219 */
/* #define                                                       0x021A */
/* #define                                                       0x021B */
/* #define                                                       0x021C */
/* #define                                                       0x021D */
/* #define                                                       0x021E */
/* #define                                                       0x021F */
#define		P0LEV				0x0220
#define		P0DIR				0x0221
#define		P0PON				0x0222
#define		P0PUD				0x0223
/* #define                                                       0x0224 */
/* #define                                                       0x0225 */
/* #define                                                       0x0226 */
/* #define                                                       0x0227 */
/* #define                                                       0x0228 */
/* #define                                                       0x0229 */
/* #define                                                       0x022A */
/* #define                                                       0x022B */
/* #define                                                       0x022C */
/* #define                                                       0x022D */
/* #define                                                       0x022E */
/* #define                                                       0x022F */
#define		IOP0SEL				0x0230
#define		IOP1SEL				0x0231
#define		IOP2SEL				0x0232
#define		IOP3SEL				0x0233
#define		IOP4SEL				0x0234
#define		IOP5SEL				0x0235
#define		DGINSEL				0x0236
/* #define                                                       0x0237 */
#define		IOP_CNT				0x0238
#define		OUT56MON			0x0239
/* #define                                                       0x023A */
/* #define                                                       0x023B */
/* #define                                                       0x023C */
/* #define                                                       0x023D */
/* #define                                                       0x023E */
/* #define                                                       0x023F */
#define		BSYSEL				0x0240
/* #define                                                       0x0241 */
/* #define                                                       0x0242 */
/* #define                                                       0x0243 */
/* #define                                                       0x0244 */
/* #define                                                       0x0245 */
/* #define                                                       0x0246 */
/* #define                                                       0x0247 */
#define		I2CSEL				0x0248
#define		DLMODE				0x0249
/* #define                                                       0x024A */
/* #define                                                       0x024B */
/* #define                                                       0x024C */
/* #define                                                       0x024D */
#define		TSTREG0				0x024E
#define		TSTREG1				0x024F
#define		STBB0				0x0250
#define		CMSDAC0				0x0251
#define		CMSDAC1				0x0252
#define		OPGSEL0				0x0253
#define		OPGSEL1				0x0254
#define		OPGSEL2				0x0255
#define		OSCSTOP				0x0256
#define		OSCSET				0x0257
#define		OSCCNTEN			0x0258
#define		LDO_C_SET			0x0259
#define		VGA_SW0				0x025A
#define		VGA_SW1				0x025B
#define		RSTRLSCNTL			0x025C
#define		RSTRLSCNTH			0x025D
#define		OSCCK_CNTR0			0x025E
#define		OSCCK_CNTR1			0x025F
#define		EXTCNTEN			0x0260
#define		EXTCLKLOW			0x0261
#define		ADCTEST				0x0262
#define		LDSTB				0x0263
#define		STBB1				0x0264
/* #define                                                       0x0265 */
/* #define                                                       0x0266 */
/* #define                                                       0x0267 */
/* #define                                                       0x0268 */
/* #define                                                       0x0269 */
/* #define                                                       0x026A */
/* #define                                                       0x026B */
/* #define                                                       0x026C */
/* #define                                                       0x026D */
/* #define                                                       0x026E */
/* #define                                                       0x026F */
#define		MONSELA				0x0270
#define		MONSELB				0x0271
#define		MONSELC				0x0272
#define		MONSELD				0x0273
#define		CmMonTst			0x0274
/* #define                                                       0x0275 */
/* #define                                                       0x0276 */
/* #define                                                       0x0277 */
#define		SOFTRES1			0x0278
#define		SOFTRES2			0x0279
/* #define                                                       0x027A */
/* #define                                                       0x027B */
/* #define                                                       0x027C */
/* #define                                                       0x027D */
#define		CVER				0x027E
#define		TESTRD				0x027F


/* =================================================================== */
/* Digital Gyro I/F Register */
/* =================================================================== */
#define		GRSEL				0x0280
#define		GRINI				0x0281
#define		SLOWMODE			0x04
#define		GRACC				0x0282
#define		GRADR0				0x0283
#define		GRADR1				0x0284
#define		GRADR2				0x0285
#define		GRADR3				0x0286
#define		GRADR4				0x0287
#define		GRADR5				0x0288
#define		GRADR6				0x0289
#define		GSETDT				0x028A
#define		RDSEL				0x028B
#define		REVB7				0x028C
#define		LSBF				0x028D
#define		PANAM				0x028E
#define		SPIM				0x028F
#define		GRDAT0H				0x0290
#define		GRDAT0L				0x0291
#define		GRDAT1H				0x0292
#define		GRDAT1L				0x0293
#define		GRDAT2H				0x0294
#define		GRDAT2L				0x0295
#define		GRDAT3H				0x0296
#define		GRDAT3L				0x0297
#define		GRDAT4H				0x0298
#define		GRDAT4L				0x0299
#define		GRDAT5H				0x029A
#define		GRDAT5L				0x029B
#define		GRDAT6H				0x029C
#define		GRDAT6L				0x029D
/* #define                                                       0x029E */
/* #define                                                       0x029F */
#define		IZAH				0x02A0
#define		IZAL				0x02A1
#define		IZBH				0x02A2
#define		IZBL				0x02A3
/* #define                                                       0x02A4 */
/* #define                                                       0x02A5 */
/* #define                                                       0x02A6 */
/* #define                                                       0x02A7 */
/* #define                                                       0x02A8 */
/* #define                                                       0x02A9 */
/* #define                                                       0x02AA */
/* #define                                                       0x02AB */
/* #define                                                       0x02AC */
/* #define                                                       0x02AD */
/* #define                                                       0x02AE */
/* #define                                                       0x02AF */
/* #define                                                       0x02B0 */
/* #define                                                       0x02B1 */
/* #define                                                       0x02B2 */
/* #define                                                       0x02B3 */
/* #define                                                       0x02B4 */
/* #define                                                       0x02B5 */
/* #define                                                       0x02B6 */
/* #define                                                       0x02B7 */
#define		GRFLG0				0x02B8
#define		GRFLG1				0x02B9
/* #define                                                       0x02BA */
/* #define                                                       0x02BB */
/* #define                                                       0x02BC */
/* #define                                                       0x02BD */
/* #define                                                       0x02BE */
/* #define                                                       0x02BF */
/* #define                                                       0x02C0 */
#define		DGSTAT0				0x02C1
#define		DGSTAT1				0x02C2
/* #define                                                       0x02C3 */
/* #define                                                       0x02C4 */
/* #define                                                       0x02C5 */
/* #define                                                       0x02C6 */
/* #define                                                       0x02C7 */
/* #define                                                       0x02C8 */
/* #define                                                       0x02C9 */
/* #define                                                       0x02CA */
/* #define                                                       0x02CB */
/* #define                                                       0x02CC */
/* #define                                                       0x02CD */
/* #define                                                       0x02CE */
/* #define                                                       0x02CF */
#define		VRREG				0x02D0	/* USE TEST REG */
/* #define                                                       0x02D1 */
/* #define                                                       0x02D2 */
/* #define                                                       0x02D3 */
/* #define                                                       0x02D4 */
/* #define                                                       0x02D5 */
/* #define                                                       0x02D6 */
/* #define                                                       0x02D7 */
/* #define                                                       0x02D8 */
/* #define                                                       0x02D9 */
/* #define                                                       0x02DA */
/* #define                                                       0x02DB */
/* #define                                                       0x02DC */
/* #define                                                       0x02DD */
/* #define                                                       0x02DE */
/* #define                                                       0x02DF */
/* #define                                                       0x02E0 */
/* #define                                                       0x02E1 */
/* #define                                                       0x02E2 */
/* #define                                                       0x02E3 */
/* #define                                                       0x02E4 */
/* #define                                                       0x02E5 */
/* #define                                                       0x02E6 */
/* #define                                                       0x02E7 */
/* #define                                                       0x02E8 */
/* #define                                                       0x02E9 */
/* #define                                                       0x02EA */
/* #define                                                       0x02EB */
/* #define                                                       0x02EC */
/* #define                                                       0x02ED */
/* #define                                                       0x02EE */
/* #define                                                       0x02EF */
/* #define                                                       0x02F0 */
/* #define                                                       0x02F1 */
/* #define                                                       0x02F2 */
/* #define                                                       0x02F3 */
/* #define                                                       0x02F4 */
/* #define                                                       0x02F5 */
/* #define                                                       0x02F6 */
/* #define                                                       0x02F7 */
/* #define                                                       0x02F8 */
/* #define                                                       0x02F9 */
/* #define                                                       0x02FA */
/* #define                                                       0x02FB */
/* #define                                                       0x02FC */
/* #define                                                       0x02FD */
/* #define                                                       0x02FE */
/* #define                                                       0x02FF */


/* ==================================================================== */
/* Open AF Register */
/* ==================================================================== */
/* #define                                                       0x0300 */
/* #define                                                       0x0301 */
#define		FSTMODE				0x0302
#define		FSTCTIME			0x0303
#define		TCODEH				0x0304
#define		TCODEL				0x0305
#define		LTHDH				0x0306
#define		LTHDL				0x0307
/* #define                                                       0x0308 */
/* #define                                                       0x0309 */
/* #define                                                       0x030A */
/* #define                                                       0x030B */
/* #define                                                       0x030C */
/* #define                                                       0x030D */
/* #define                                                       0x030E */
/* #define                                                       0x030F */
#define		FSTOPTION			0x0310
/* #define                                                       0x0311 */
/* #define                                                       0x0312 */
/* #define                                                       0x0313 */
/* #define                                                       0x0314 */
/* #define                                                       0x0315 */
/* #define                                                       0x0316 */
/* #define                                                       0x0317 */
/* #define                                                       0x0318 */
/* #define                                                       0x0319 */
/* #define                                                       0x031A */
/* #define                                                       0x031B */
/* #define                                                       0x031C */
/* #define                                                       0x031D */
/* #define                                                       0x031E */
/* #define                                                       0x031F */
#define		OPAFEN				0x0320
/* #define                                                       0x0321 */
/* #define                                                       0x0322 */
/* #define                                                       0x0323 */
/* #define                                                       0x0324 */
/* #define                                                       0x0325 */
/* #define                                                       0x0326 */
/* #define                                                       0x0327 */
/* #define                                                       0x0328 */
/* #define                                                       0x0329 */
/* #define                                                       0x032A */
/* #define                                                       0x032B */
/* #define                                                       0x032C */
/* #define                                                       0x032D */
/* #define                                                       0x032E */
/* #define                                                       0x032F */
#define		OPAFSW				0x0330
/* #define                                                       0x0331 */
/* #define                                                       0x0332 */
/* #define                                                       0x0333 */
/* #define                                                       0x0334 */
#define		OPAFST				0x0335

/* #define                                                       0x0380 */
/* #define                                                       0x0381 */
/* #define                                                       0x0382 */
/* #define                                                       0x0383 */
/* #define                                                       0x0384 */
/* #define                                                       0x0385 */
/* #define                                                       0x0386 */
/* #define                                                       0x0387 */
/* #define                                                       0x0388 */
/* #define                                                       0x0389 */
/* #define                                                       0x038A */
/* #define                                                       0x038B */
/* #define                                                       0x038C */
/* #define                                                       0x038D */
/* #define                                                       0x038E */
/* #define                                                       0x038F */
/* #define                                                       0x0390 */
/* #define                                                       0x0391 */
/* #define                                                       0x0392 */
/* #define                                                       0x0393 */
/* #define                                                       0x0394 */
/* #define                                                       0x0395 */
#define		RWEXD1_L			0x0396	/* 2Byte access */
/* #define                                                       0x0397 */
#define		RWEXD2_L			0x0398	/* 2Byte access */
/* #define                                                       0x0399 */
#define		RWEXD3_L			0x039A	/* 2Byte access */
/* #define                                                       0x039B */
/* #define                                                       0x039C */
/* #define                                                       0x039D */
/* #define                                                       0x039E */
/* #define                                                       0x039F */

/* ==================================================================== */
/* FILTER COEFFICIENT RAM */
/* ==================================================================== */
#define		gx45g				0x1000
#define		gx45x				0x1001
#define		gx45y				0x1002
#define		gxgyro				0x1003
#define		gxia				0x1004
#define		gxib				0x1005
#define		gxic				0x1006
#define		gxggain				0x1007
#define		gxigain				0x1008
#define		gxggain2			0x1009
#define		gx2x4xf				0x100A
#define		gxadj				0x100B
#define		gxgain				0x100C
#define		gxl3				0x100D
#define		gxhc_tmp			0x100E
#define		npxlev1				0x100F
#define		gxh1a				0x1010
#define		gxh1b				0x1011
#define		gxh1c				0x1012
#define		gxh2a				0x1013
#define		gxh2b				0x1014
#define		gxh2c				0x1015
#define		gxh3a				0x1016
#define		gxh3b				0x1017
#define		gxh3c				0x1018
#define		gxla				0x1019
#define		gxlb				0x101A
#define		gxlc				0x101B
#define		gxhgain				0x101C
#define		gxlgain				0x101D
#define		gxigainstp			0x101E
#define		npxlev2				0x101F
#define		gxzoom				0x1020
#define		gx2x4xb				0x1021
#define		gxlens				0x1022
#define		gxta				0x1023
#define		gxtb				0x1024
#define		gxtc				0x1025
#define		gxtd				0x1026
#define		gxte				0x1027
#define		gxlmt1H				0x1028
#define		gxlmt3HS0			0x1029
#define		gxlmt3HS1			0x102A
#define		gxlmt4HS0			0x102B
#define		gxlmt4HS1			0x102C
#define		gxlmt6L				0x102D
#define		gxlmt6H				0x102E
#define		npxlev3				0x102F
#define		gxj1a				0x1030
#define		gxj1b				0x1031
#define		gxj1c				0x1032
#define		gxj2a				0x1033
#define		gxj2b				0x1034
#define		gxj2c				0x1035
#define		gxk1a				0x1036
#define		gxk1b				0x1037
#define		gxk1c				0x1038
#define		gxk2a				0x1039
#define		gxk2b				0x103A
#define		gxk2c				0x103B
#define		gxoa				0x103C
#define		gxob				0x103D
#define		gxoc				0x103E
#define		npxlev4				0x103F
#define		MSABS1				0x1040
#define		MSABS1AV			0x1041
#define		MSPP1AV				0x1042
#define		gxia_1				0x1043
#define		gxib_1				0x1044
#define		gxic_1				0x1045
#define		gxia_a				0x1046
#define		gxib_a				0x1047
#define		gxic_a				0x1048
#define		gxia_b				0x1049
#define		gxib_b				0x104A
#define		gxic_b				0x104B
#define		gxia_c				0x104C
#define		gxib_c				0x104D
#define		gxic_c				0x104E
#define		Sttx12aM			0x104F
#define		MSMAX1				0x1050
#define		MSMAX1AV			0x1051
#define		MSCT1AV				0x1052
#define		gxla_1				0x1053
#define		gxlb_1				0x1054
#define		gxlc_1				0x1055
#define		gxla_a				0x1056
#define		gxlb_a				0x1057
#define		gxlc_a				0x1058
#define		gxla_b				0x1059
#define		gxlb_b				0x105A
#define		gxlc_b				0x105B
#define		gxla_c				0x105C
#define		gxlb_c				0x105D
#define		gxlc_c				0x105E
#define		Sttx12aH			0x105F
#define		MSMIN1				0x1060
#define		MSMIN1AV			0x1061
#define		MS1AV				0x1062
#define		gxgyro_1			0x1063
#define		gxgyro_1d			0x1064
#define		gxgyro_1u			0x1065
#define		gxgyro_a			0x1066
#define		gxgyro_2d			0x1067
#define		gxgyro_2u			0x1068
#define		gxgyro_b			0x1069
#define		gxgyro_3d			0x106A
#define		gxgyro_3u			0x106B
#define		gxgyro_c			0x106C
#define		gxgyro_4d			0x106D
#define		gxgyro_4u			0x106E
#define		Sttx12bM			0x106F
#define		HOStp				0x1070
#define		HOMin				0x1071
#define		HOMax				0x1072
#define		gxgain_1			0x1073
#define		gxgain_1d			0x1074
#define		gxgain_1u			0x1075
#define		gxgain_a			0x1076
#define		gxgain_2d			0x1077
#define		gxgain_2u			0x1078
#define		gxgain_b			0x1079
#define		gxgain_3d			0x107A
#define		gxgain_3u			0x107B
#define		gxgain_c			0x107C
#define		gxgain_4d			0x107D
#define		gxgain_4u			0x107E
#define		Sttx12bH			0x107F
#define		HBStp				0x1080
#define		HBMin				0x1081
#define		HBMax				0x1082
#define		gxistp_1			0x1083
#define		gxistp_1d			0x1084
#define		gxistp_1u			0x1085
#define		gxistp_a			0x1086
#define		gxistp_2d			0x1087
#define		gxistp_2u			0x1088
#define		gxistp_b			0x1089
#define		gxistp_3d			0x108A
#define		gxistp_3u			0x108B
#define		gxistp_c			0x108C
#define		gxistp_4d			0x108D
#define		gxistp_4u			0x108E
#define		Sttx34aM			0x108F
#define		LGStp				0x1090
#define		LGMin				0x1091
#define		LGMax				0x1092
#define		gxistp				0x1093
#define		gxadjmin			0x1094
#define		gxadjmax			0x1095
#define		gxadjdn				0x1096
#define		gxadjup				0x1097
#define		gxog3				0x1098
#define		gxog5				0x1099
#define		gxog7				0x109A
#define		npxlev8				0x109B
#define		sxlmtb1				0x109C
#define		SttxaL				0x109D
#define		SttxbL				0x109E
#define		Sttx34aH			0x109F
#define		sxlmtb2				0x10A0
#define		pxmaa				0x10A1
#define		pxmab				0x10A2
#define		pxmac				0x10A3
#define		pxmba				0x10A4
#define		pxmbb				0x10A5
#define		pxmbc				0x10A6
#define		gxma				0x10A7
#define		gxmb				0x10A8
#define		gxmc				0x10A9
#define		gxmg				0x10AA
#define		gxleva				0x10AB
#define		gxlevb				0x10AC
#define		gxlevc				0x10AD
#define		gxlevlow			0x10AE
#define		Sttx34bM			0x10AF
#define		sxria				0x10B0
#define		sxrib				0x10B1
#define		sxric				0x10B2
#define		sxinx				0x10B3
#define		sxiny				0x10B4
#define		sxggf				0x10B5
#define		sxag				0x10B6
#define		sxpr				0x10B7
#define		sxgx				0x10B8
#define		sxgy				0x10B9
#define		sxiexp3				0x10BA
#define		sxiexp2				0x10BB
#define		sxiexp1				0x10BC
#define		sxiexp0				0x10BD
#define		sxiexp				0x10BE
#define		Sttx34bH			0x10BF
#define		sxda				0x10C0
#define		sxdb				0x10C1
#define		sxdc				0x10C2
#define		sxea				0x10C3
#define		sxeb				0x10C4
#define		sxec				0x10C5
#define		sxua				0x10C6
#define		sxub				0x10C7
#define		sxuc				0x10C8
#define		sxia				0x10C9
#define		sxib				0x10CA
#define		sxic				0x10CB
#define		sxja				0x10CC
#define		sxjb				0x10CD
#define		sxjc				0x10CE
#define		npxlev1_i			0x10CF
#define		sxfa				0x10D0
#define		sxfb				0x10D1
#define		sxfc				0x10D2
#define		sxg					0x10D3
#define		sxg2				0x10D4
#define		sxsin				0x10D5
#define		sxggf_tmp			0x10D6
#define		sxsa				0x10D7
#define		sxsb				0x10D8
#define		sxsc				0x10D9
#define		sxoa				0x10DA
#define		sxob				0x10DB
#define		sxoc				0x10DC
#define		sxod				0x10DD
#define		sxoe				0x10DE
#define		npxlev2_i			0x10DF
#define		sxpa				0x10E0
#define		sxpb				0x10E1
#define		sxpc				0x10E2
#define		sxpd				0x10E3
#define		sxpe				0x10E4
#define		sxq					0x10E5
#define		sxlmta1				0x10E6
#define		sxlmta2				0x10E7
#define		smxga				0x10E8
#define		smxgb				0x10E9
#define		smxa				0x10EA
#define		smxb				0x10EB
#define		sxemglev			0x10EC
#define		sxsmtav				0x10ED
#define		sxsmtstp			0x10EE
#define		npxlev3_i			0x10EF
#define		mes1aa				0x10F0
#define		mes1ab				0x10F1
#define		mes1ac				0x10F2
#define		mes1ad				0x10F3
#define		mes1ae				0x10F4
#define		mes1ba				0x10F5
#define		mes1bb				0x10F6
#define		mes1bc				0x10F7
#define		mes1bd				0x10F8
#define		mes1be				0x10F9
#define		sxoexp3				0x10FA
#define		sxoexp2				0x10FB
#define		sxoexp1				0x10FC
#define		sxoexp0				0x10FD
#define		sxoexp				0x10FE
#define		npxlev4_i			0x10FF
#define		gy45g				0x1100
#define		gy45y				0x1101
#define		gy45x				0x1102
#define		gygyro				0x1103
#define		gyia				0x1104
#define		gyib				0x1105
#define		gyic				0x1106
#define		gyggain				0x1107
#define		gyigain				0x1108
#define		gyggain2			0x1109
#define		gy2x4xf				0x110A
#define		gyadj				0x110B
#define		gygain				0x110C
#define		gyl3				0x110D
#define		gyhc_tmp			0x110E
#define		npylev1				0x110F
#define		gyh1a				0x1110
#define		gyh1b				0x1111
#define		gyh1c				0x1112
#define		gyh2a				0x1113
#define		gyh2b				0x1114
#define		gyh2c				0x1115
#define		gyh3a				0x1116
#define		gyh3b				0x1117
#define		gyh3c				0x1118
#define		gyla				0x1119
#define		gylb				0x111A
#define		gylc				0x111B
#define		gyhgain				0x111C
#define		gylgain				0x111D
#define		gyigainstp			0x111E
#define		npylev2				0x111F
#define		gyzoom				0x1120
#define		gy2x4xb				0x1121
#define		gylens				0x1122
#define		gyta				0x1123
#define		gytb				0x1124
#define		gytc				0x1125
#define		gytd				0x1126
#define		gyte				0x1127
#define		gylmt1H				0x1128
#define		gylmt3HS0			0x1129
#define		gylmt3HS1			0x112A
#define		gylmt4HS0			0x112B
#define		gylmt4HS1			0x112C
#define		gylmt6L				0x112D
#define		gylmt6H				0x112E
#define		npylev3				0x112F
#define		gyj1a				0x1130
#define		gyj1b				0x1131
#define		gyj1c				0x1132
#define		gyj2a				0x1133
#define		gyj2b				0x1134
#define		gyj2c				0x1135
#define		gyk1a				0x1136
#define		gyk1b				0x1137
#define		gyk1c				0x1138
#define		gyk2a				0x1139
#define		gyk2b				0x113A
#define		gyk2c				0x113B
#define		gyoa				0x113C
#define		gyob				0x113D
#define		gyoc				0x113E
#define		npylev4				0x113F
#define		MSABS2				0x1140
#define		MSABS2AV			0x1141
#define		MSPP2AV				0x1142
#define		gyia_1				0x1143
#define		gyib_1				0x1144
#define		gyic_1				0x1145
#define		gyia_a				0x1146
#define		gyib_a				0x1147
#define		gyic_a				0x1148
#define		gyia_b				0x1149
#define		gyib_b				0x114A
#define		gyic_b				0x114B
#define		gyia_c				0x114C
#define		gyib_c				0x114D
#define		gyic_c				0x114E
#define		Stty12aM			0x114F
#define		MSMAX2				0x1150
#define		MSMAX2AV			0x1151
#define		MSCT2AV				0x1152
#define		gyla_1				0x1153
#define		gylb_1				0x1154
#define		gylc_1				0x1155
#define		gyla_a				0x1156
#define		gylb_a				0x1157
#define		gylc_a				0x1158
#define		gyla_b				0x1159
#define		gylb_b				0x115A
#define		gylc_b				0x115B
#define		gyla_c				0x115C
#define		gylb_c				0x115D
#define		gylc_c				0x115E
#define		Stty12aH			0x115F
#define		MSMIN2				0x1160
#define		MSMIN2AV			0x1161
#define		MS2AV				0x1162
#define		gygyro_1			0x1163
#define		gygyro_1d			0x1164
#define		gygyro_1u			0x1165
#define		gygyro_a			0x1166
#define		gygyro_2d			0x1167
#define		gygyro_2u			0x1168
#define		gygyro_b			0x1169
#define		gygyro_3d			0x116A
#define		gygyro_3u			0x116B
#define		gygyro_c			0x116C
#define		gygyro_4d			0x116D
#define		gygyro_4u			0x116E
#define		Stty12bM			0x116F
#define		GGStp				0x1170
#define		GGMin				0x1171
#define		GGMax				0x1172
#define		gygain_1			0x1173
#define		gygain_1d			0x1174
#define		gygain_1u			0x1175
#define		gygain_a			0x1176
#define		gygain_2d			0x1177
#define		gygain_2u			0x1178
#define		gygain_b			0x1179
#define		gygain_3d			0x117A
#define		gygain_3u			0x117B
#define		gygain_c			0x117C
#define		gygain_4d			0x117D
#define		gygain_4u			0x117E
#define		Stty12bH			0x117F
#define		GGStp2				0x1180
#define		GGMin2				0x1181
#define		GGMax2				0x1182
#define		gyistp_1			0x1183
#define		gyistp_1d			0x1184
#define		gyistp_1u			0x1185
#define		gyistp_a			0x1186
#define		gyistp_2d			0x1187
#define		gyistp_2u			0x1188
#define		gyistp_b			0x1189
#define		gyistp_3d			0x118A
#define		gyistp_3u			0x118B
#define		gyistp_c			0x118C
#define		gyistp_4d			0x118D
#define		gyistp_4u			0x118E
#define		Stty34aM			0x118F
/* #define		vma					0x1190 */
#define		vmb					0x1191
#define		vmc					0x1192
#define		gyistp				0x1193
#define		gyadjmin			0x1194
#define		gyadjmax			0x1195
#define		gyadjdn				0x1196
#define		gyadjup				0x1197
#define		gyog3				0x1198
#define		gyog5				0x1199
#define		gyog7				0x119A
#define		npylev8				0x119B
#define		sylmtb1				0x119C
#define		SttyaL				0x119D
#define		SttybL				0x119E
#define		Stty34aH			0x119F
#define		sylmtb2				0x11A0
#define		pymaa				0x11A1
#define		pymab				0x11A2
#define		pymac				0x11A3
#define		pymba				0x11A4
#define		pymbb				0x11A5
#define		pymbc				0x11A6
#define		gyma				0x11A7
#define		gymb				0x11A8
#define		gymc				0x11A9
#define		gymg				0x11AA
#define		gyleva				0x11AB
#define		gylevb				0x11AC
#define		gylevc				0x11AD
#define		gylevlow			0x11AE
#define		Stty34bM			0x11AF
#define		syria				0x11B0
#define		syrib				0x11B1
#define		syric				0x11B2
#define		syiny				0x11B3
#define		syinx				0x11B4
#define		syggf				0x11B5
#define		syag				0x11B6
#define		sypr				0x11B7
#define		sygy				0x11B8
#define		sygx				0x11B9
#define		syiexp3				0x11BA
#define		syiexp2				0x11BB
#define		syiexp1				0x11BC
#define		syiexp0				0x11BD
#define		syiexp				0x11BE
#define		Stty34bH			0x11BF
#define		syda				0x11C0
#define		sydb				0x11C1
#define		sydc				0x11C2
#define		syea				0x11C3
#define		syeb				0x11C4
#define		syec				0x11C5
#define		syua				0x11C6
#define		syub				0x11C7
#define		syuc				0x11C8
#define		syia				0x11C9
#define		syib				0x11CA
#define		syic				0x11CB
#define		syja				0x11CC
#define		syjb				0x11CD
#define		syjc				0x11CE
#define		npylev1_i			0x11CF
#define		syfa				0x11D0
#define		syfb				0x11D1
#define		syfc				0x11D2
#define		syg					0x11D3
#define		syg2				0x11D4
#define		sysin				0x11D5
#define		syggf_tmp			0x11D6
#define		sysa				0x11D7
#define		sysb				0x11D8
#define		sysc				0x11D9
#define		syoa				0x11DA
#define		syob				0x11DB
#define		syoc				0x11DC
#define		syod				0x11DD
#define		syoe				0x11DE
#define		npylev2_i			0x11DF
#define		sypa				0x11E0
#define		sypb				0x11E1
#define		sypc				0x11E2
#define		sypd				0x11E3
#define		sype				0x11E4
#define		syq					0x11E5
#define		sylmta1				0x11E6
#define		sylmta2				0x11E7
#define		smyga				0x11E8
#define		smygb				0x11E9
#define		smya				0x11EA
#define		smyb				0x11EB
#define		syemglev			0x11EC
#define		sysmtav				0x11ED
#define		sysmtstp			0x11EE
#define		npylev3_i			0x11EF
#define		mes2aa				0x11F0
#define		mes2ab				0x11F1
#define		mes2ac				0x11F2
#define		mes2ad				0x11F3
#define		mes2ae				0x11F4
#define		mes2ba				0x11F5
#define		mes2bb				0x11F6
#define		mes2bc				0x11F7
#define		mes2bd				0x11F8
#define		mes2be				0x11F9
#define		syoexp3				0x11FA
#define		syoexp2				0x11FB
#define		syoexp1				0x11FC
#define		syoexp0				0x11FD
#define		syoexp				0x11FE
#define		npylev4_i			0x11FF
#define		afsin				0x1200
#define		afing				0x1201
#define		afstmg				0x1202
#define		afag				0x1203
#define		afda				0x1204
#define		afdb				0x1205
#define		afdc				0x1206
#define		afea				0x1207
#define		afeb				0x1208
#define		afec				0x1209
#define		afua				0x120A
#define		afub				0x120B
#define		afuc				0x120C
#define		afia				0x120D
#define		afib				0x120E
#define		afic				0x120F
#define		afja				0x1210
#define		afjb				0x1211
#define		afjc				0x1212
#define		affa				0x1213
#define		affb				0x1214
#define		affc				0x1215
#define		afg					0x1216
#define		afg2				0x1217
#define		afpa				0x1218
#define		afpb				0x1219
#define		afpc				0x121A
#define		afpd				0x121B
#define		afpe				0x121C
#define		afstma				0x121D
#define		afstmb				0x121E
#define		afstmc				0x121F
#define		aflmt				0x1220
#define		aflmt2				0x1221
#define		afssmv1				0x1222
#define		afssmv2				0x1223
#define		afsjlev				0x1224
#define		afsjdif				0x1225
#define		SttxHis				0x1226
#define		tmpa				0x1227
#define		af_cc				0x1228
#define		a_df				0x1229
#define		b_df				0x122A
#define		c_df				0x122B
#define		d_df				0x122C
#define		e_df				0x122D
#define		f_df				0x122E
#define		pi					0x122F
#define		msmean				0x1230
#define		vmlevhis			0x1231
#define		vmlev				0x1232
#define		vmtl				0x1233
#define		vmth				0x1234
#define		st1mean				0x1235
#define		st2mean				0x1236
#define		st3mean				0x1237
#define		st4mean				0x1238
#define		dm1g				0x1239
#define		dm2g				0x123A
#define		dm3g				0x123B
#define		dm4g				0x123C
#define		zero				0x123D
#define		com10				0x123E
#define		cop10				0x123F

/* =============================================================== */
/* FILTER DELAY RAM */
/* =============================================================== */
#define		SINXZ				0x1400
#define		GX45Z				0x1401
#define		GXINZ				0x1402
#define		GXI1Z1				0x1403
#define		GXI1Z2				0x1404
#define		GXI2Z1				0x1405
#define		GXI2Z2				0x1406
#define		GXMZ1				0x1407
#define		GXMZ2				0x1408
#define		GXIZ				0x1409
#define		GXXFZ				0x140A
#define		GXADJZ				0x140B
#define		GXGAINZ				0x140C
#define		GXLEV1Z1			0x140D
#define		GXLEV1Z2			0x140E
#define		TMPX				0x140F
#define		SXDOFFZ2			0x1410
#define		GXH1Z1				0x1411
#define		GXH1Z2				0x1412
/* #define                                                       0x1413 */
#define		GXH2Z1				0x1414
#define		GXH2Z2				0x1415
#define		GXLEV2Z1			0x1416
#define		GXH3Z1				0x1417
#define		GXH3Z2				0x1418
#define		GXL1Z1				0x1419
#define		GXL1Z2				0x141A
#define		GXL2Z1				0x141B
#define		GXL2Z2				0x141C
#define		GXL3Z				0x141D
#define		GXLZ				0x141E
#define		GXI3Z				0x141F
#define		GXZOOMZ				0x1420
#define		GXXBZ				0x1421
#define		GXLENSZ				0x1422
#define		GXLMT3Z				0x1423
#define		GXTZ1				0x1424
#define		GXTZ2				0x1425
#define		GXTZ3				0x1426
#define		GXTZ4				0x1427
#define		GX2SXZ				0x1428
#define		SXOVRZ				0x1429
#define		PXAMZ				0x142A
#define		PXMAZ1				0x142B
#define		PXMAZ2				0x142C
#define		PXBMZ				0x142D
#define		PXMBZ1				0x142E
#define		PXMBZ2				0x142F
#define		DAXHLOtmp			0x1430
#define		GXJ1Z1				0x1431
#define		GXJ1Z2				0x1432
#define		SXINZ1				0x1433
#define		GXJ2Z1				0x1434
#define		GXJ2Z2				0x1435
#define		SXINZ2				0x1436
#define		GXK1Z1				0x1437
#define		GXK1Z2				0x1438
#define		SXTRZ				0x1439
#define		GXK2Z1				0x143A
#define		GXK2Z2				0x143B
#define		SXIEXPZ				0x143C
#define		GXOZ1				0x143D
#define		GXOZ2				0x143E
#define		GXLEV2Z2			0x143F
#define		AD0Z				0x1440
#define		SXRIZ1				0x1441
#define		SXRIZ2				0x1442
#define		SXAGZ				0x1443
#define		SXSMTZ				0x1444
#define		MES1AZ1				0x1445
#define		MES1AZ2				0x1446
#define		MES1AZ3				0x1447
#define		MES1AZ4				0x1448
#define		SXTRZ1				0x1449
#define		AD2Z				0x144A
#define		MES1BZ1				0x144B
#define		MES1BZ2				0x144C
#define		MES1BZ3				0x144D
#define		MES1BZ4				0x144E
#define		AD4Z				0x144F
#define		OFF0Z				0x1450
#define		SXDZ1				0x1451
#define		SXDZ2				0x1452
#define		NPXDIFZ				0x1453
#define		SXEZ1				0x1454
#define		SXEZ2				0x1455
#define		SX2HXZ2				0x1456
#define		SXUZ1				0x1457
#define		SXUZ2				0x1458
#define		SXTRZ2				0x1459
#define		OFF2Z				0x145A
#define		SXIZ1				0x145B
#define		SXIZ2				0x145C
#define		SXJZ1				0x145D
#define		SXJZ2				0x145E
#define		OFF4Z				0x145F
#define		AD0OFFZ				0x1460
#define		SXOFFZ1				0x1461
#define		SXOFFZ2				0x1462
#define		SXFZ				0x1463
#define		SXGZ				0x1464
#define		NPXTMPZ				0x1465
#define		SXG3Z				0x1466
#define		SXSZ1				0x1467
#define		SXSZ2				0x1468
#define		SXTRZ3				0x1469
#define		AD2OFFZ				0x146A
#define		SXOZ1				0x146B
#define		SXOZ2				0x146C
#define		SXOZ3				0x146D
#define		SXOZ4				0x146E
#define		AD4OFFZ				0x146F
#define		SXDOFFZ				0x1470
#define		SXPZ1				0x1471
#define		SXPZ2				0x1472
#define		SXPZ3				0x1473
#define		SXPZ4				0x1474
#define		SXQZ				0x1475
#define		SXOEXPZ				0x1476
#define		SXLMT				0x1477
#define		SX2HXZ				0x1478
#define		DAXHLO				0x1479
#define		DAXHLB				0x147A
#define		TMPX2				0x147B
#define		TMPX3				0x147C
/* #define                                                       0x147D */
/* #define                                                       0x147E */
/* #define                                                       0x147F */
#define		SINYZ				0x1480
#define		GY45Z				0x1481
#define		GYINZ				0x1482
#define		GYI1Z1				0x1483
#define		GYI1Z2				0x1484
#define		GYI2Z1				0x1485
#define		GYI2Z2				0x1486
#define		GYMZ1				0x1487
#define		GYMZ2				0x1488
#define		GYIZ				0x1489
#define		GYXFZ				0x148A
#define		GYADJZ				0x148B
#define		GYGAINZ				0x148C
#define		GYLEV1Z1			0x148D
#define		GYLEV1Z2			0x148E
#define		TMPY				0x148F
#define		SYDOFFZ2			0x1490
#define		GYH1Z1				0x1491
#define		GYH1Z2				0x1492
/* #define                                                       0x1493 */
#define		GYH2Z1				0x1494
#define		GYH2Z2				0x1495
#define		GYLEV2Z1			0x1496
#define		GYH3Z1				0x1497
#define		GYH3Z2				0x1498
#define		GYL1Z1				0x1499
#define		GYL1Z2				0x149A
#define		GYL2Z1				0x149B
#define		GYL2Z2				0x149C
#define		GYL3Z				0x149D
#define		GYLZ				0x149E
#define		GYI3Z				0x149F
#define		GYZOOMZ				0x14A0
#define		GYXBZ				0x14A1
#define		GYLENSZ				0x14A2
#define		GYLMT3Z				0x14A3
#define		GYTZ1				0x14A4
#define		GYTZ2				0x14A5
#define		GYTZ3				0x14A6
#define		GYTZ4				0x14A7
#define		GY2SYZ				0x14A8
#define		SYOVRZ				0x14A9
#define		PYAMZ				0x14AA
#define		PYMAZ1				0x14AB
#define		PYMAZ2				0x14AC
#define		PYBMZ				0x14AD
#define		PYMBZ1				0x14AE
#define		PYMBZ2				0x14AF
#define		DAYHLOtmp			0x14B0
#define		GYJ1Z1				0x14B1
#define		GYJ1Z2				0x14B2
#define		SYINZ1				0x14B3
#define		GYJ2Z1				0x14B4
#define		GYJ2Z2				0x14B5
#define		SYINZ2				0x14B6
#define		GYK1Z1				0x14B7
#define		GYK1Z2				0x14B8
#define		SYTRZ				0x14B9
#define		GYK2Z1				0x14BA
#define		GYK2Z2				0x14BB
#define		SYIEXPZ				0x14BC
#define		GYOZ1				0x14BD
#define		GYOZ2				0x14BE
#define		GYLEV2Z2			0x14BF
#define		AD1Z				0x14C0
#define		SYRIZ1				0x14C1
#define		SYRIZ2				0x14C2
#define		SYAGZ				0x14C3
#define		SYSMTZ				0x14C4
#define		MES2AZ1				0x14C5
#define		MES2AZ2				0x14C6
#define		MES2AZ3				0x14C7
#define		MES2AZ4				0x14C8
#define		SYTRZ1				0x14C9
#define		AD3Z				0x14CA
#define		MES2BZ1				0x14CB
#define		MES2BZ2				0x14CC
#define		MES2BZ3				0x14CD
#define		MES2BZ4				0x14CE
#define		AD5Z				0x14CF
#define		OFF1Z				0x14D0
#define		SYDZ1				0x14D1
#define		SYDZ2				0x14D2
#define		NPYDIFZ				0x14D3
#define		SYEZ1				0x14D4
#define		SYEZ2				0x14D5
#define		SY2HYZ2				0x14D6
#define		SYUZ1				0x14D7
#define		SYUZ2				0x14D8
#define		SYTRZ2				0x14D9
#define		OFF3Z				0x14DA
#define		SYIZ1				0x14DB
#define		SYIZ2				0x14DC
#define		SYJZ1				0x14DD
#define		SYJZ2				0x14DE
#define		OFF5Z				0x14DF
#define		AD1OFFZ				0x14E0
#define		SYOFFZ1				0x14E1
#define		SYOFFZ2				0x14E2
#define		SYFZ				0x14E3
#define		SYGZ				0x14E4
#define		NPYTMPZ				0x14E5
#define		SYG3Z				0x14E6
#define		SYSZ1				0x14E7
#define		SYSZ2				0x14E8
#define		SYTRZ3				0x14E9
#define		AD3OFFZ				0x14EA
#define		SYOZ1				0x14EB
#define		SYOZ2				0x14EC
#define		SYOZ3				0x14ED
#define		SYOZ4				0x14EE
#define		AD5OFFZ				0x14EF
#define		SYDOFFZ				0x14F0
#define		SYPZ1				0x14F1
#define		SYPZ2				0x14F2
#define		SYPZ3				0x14F3
#define		SYPZ4				0x14F4
#define		SYQZ				0x14F5
#define		SYOEXPZ				0x14F6
#define		SYLMT				0x14F7
#define		SY2HYZ				0x14F8
#define		DAYHLO				0x14F9
#define		DAYHLB				0x14FA
#define		TMPY2				0x14FB
#define		TMPY3				0x14FC
/* #define                                                       0x14FD */
/* #define                                                       0x14FE */
/* #define                                                       0x14FF */
#define		AFSINZ				0x1500
#define		AFDIFTMP			0x1501
#define		AFINZ				0x1502
#define		AFINZ2				0x1503
#define		AFAGZ				0x1504
#define		AFDZ1				0x1505
#define		AFDZ2				0x1506
#define		AFSTMGTSS			0x1507
#define		AFEZ1				0x1508
#define		AFEZ2				0x1509
#define		OFSTAFZ				0x150A
#define		AFUZ1				0x150B
#define		AFUZ2				0x150C
#define		AD4OFFZ2			0x150D
#define		AFIZ1				0x150E
#define		AFIZ2				0x150F
#define		OFF6Z				0x1510
#define		AFJZ1				0x1511
#define		AFJZ2				0x1512
#define		AFSTMTGT			0x1513
#define		AFSTMSTP			0x1514
#define		AFSTMTGTtmp			0x1515
#define		AFFZ				0x1516
#define		AFGZ				0x1517
#define		AFG3Z				0x1518
#define		AFPZ1				0x1519
#define		AFPZ2				0x151A
#define		AFPZ3				0x151B
#define		AFPZ4				0x151C
#define		AFLMTZ				0x151D
#define		AF2PWM				0x151E
#define		AFSTMZ2				0x151F
#define		VMXYZ				0x1520
#define		VMZ1				0x1521
#define		VMZ2				0x1522
/* #define                                                       0x1523 */
#define		OAFTHL				0x1524
#define		PR					0x1525
#define		AFRATO1				0x1526
#define		ADRATO2				0x1527
#define		AFRATO3				0x1528
#define		DAZHLO				0x1529
#define		DAZHLB				0x152A
#define		AFL1Z				0x152B
#define		AFL2Z				0x152C
#define		AFDFZ				0x152D
#define		pi_L1				0x152E
#define		pi_L2				0x152F
