/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */




#ifndef OIS_DEFINITION_H
#define OIS_DEFINITION_H

/* ================================================================= */
/* Common */
/* ================================================================= */
#define __ON_ 1
#define _OFF_ 0

#define __IN_ 1
#define _MON_ 2

#define _PWM32__ 3
#define _PWM3_1_ 4
#define _PWM3__0 5
#define _PWM_21_ 6
#define _PWM_2_0 7
#define _PWM__10 8

#define _PRN_ 4
#define _m_RSP 1
#define _m_CLK_GEN 2
#define _m_CLK_TRI 3
#define _m_I2C_SLV 4
#define _m_SPI_MAS 5
#define _m_PWM_CNT 6
#define _m_ADA_WRP 7
#define _m_EXT_SIG 8

#define _HIGH_ 1
#define __LOW_ 0

#define _FOCUS_ 1
#define _ZOOM__ 0

#define _FACTORY_ 1
#define _NORMADJ_ 2
#define _NORMAL__ 0

#define _NOT_READY 0

/* ================================================================= */
/* PIC uP port assign */
/* ================================================================= */
#define _PIO_O_L_ 0
#define _PIO_O_H_ 1
#define _PIO_INP_ 2

#define _PIC_RB_0_ 0x00
#define _PIC_RB_1_ 0x01
#define _PIC_RB_2_ 0x02
#define _PIC_RB_3_ 0x03
#define _PIC_RB_4_ 0x04
#define _PIC_RB_5_ 0x05
#define _PIC_RB_6_ 0x06
#define _PIC_RB_7_ 0x07
#define _PIC_RB_8_ 0x08
#define _PIC_RB_9_ 0x09
#define _PIC_RB_A_ 0x0A
#define _PIC_RB_B_ 0x0B
#define _PIC_RB_C_ 0x0C
#define _PIC_RB_D_ 0x0D
#define _PIC_RB_E_ 0x0E
#define _PIC_RB_F_ 0x0F

#define _PIC_RC_0_ 0x10
#define _PIC_RC_1_ 0x11
#define _PIC_RC_2_ 0x12
#define _PIC_RC_3_ 0x13
#define _PIC_RC_4_ 0x14
#define _PIC_RC_5_ 0x15
#define _PIC_RC_6_ 0x16
#define _PIC_RC_7_ 0x17
#define _PIC_RC_8_ 0x18
#define _PIC_RC_9_ 0x19
#define _PIC_RC_A_ 0x1A
#define _PIC_RC_B_ 0x1B
#define _PIC_RC_C_ 0x1C
#define _PIC_RC_D_ 0x1D
#define _PIC_RC_E_ 0x1E
#define _PIC_RC_F_ 0x1F

#define _PIC_RD_0_ 0x20
#define _PIC_RD_1_ 0x21
#define _PIC_RD_2_ 0x22
#define _PIC_RD_3_ 0x23
#define _PIC_RD_4_ 0x24
#define _PIC_RD_5_ 0x25
#define _PIC_RD_6_ 0x26
#define _PIC_RD_7_ 0x27
#define _PIC_RD_8_ 0x28
#define _PIC_RD_9_ 0x29
#define _PIC_RD_A_ 0x2A
#define _PIC_RD_B_ 0x2B
#define _PIC_RD_C_ 0x2C
#define _PIC_RD_D_ 0x2D
#define _PIC_RD_E_ 0x2E
#define _PIC_RD_F_ 0x2F

#define _PIC_RE_0_ 0x30
#define _PIC_RE_1_ 0x31
#define _PIC_RE_2_ 0x32
#define _PIC_RE_3_ 0x33
#define _PIC_RE_4_ 0x34
#define _PIC_RE_5_ 0x35
#define _PIC_RE_6_ 0x36
#define _PIC_RE_7_ 0x37
#define _PIC_RE_8_ 0x38
#define _PIC_RE_9_ 0x39
#define _PIC_RE_A_ 0x3A
#define _PIC_RE_B_ 0x3B
#define _PIC_RE_C_ 0x3C
#define _PIC_RE_D_ 0x3D
#define _PIC_RE_E_ 0x3E
#define _PIC_RE_F_ 0x3F

#define _PIC_RF_0_ 0x40
#define _PIC_RF_1_ 0x41
#define _PIC_RF_2_ 0x42
#define _PIC_RF_3_ 0x43
#define _PIC_RF_4_ 0x44
#define _PIC_RF_5_ 0x45
#define _PIC_RF_6_ 0x46
#define _PIC_RF_7_ 0x47
#define _PIC_RF_8_ 0x48
#define _PIC_RF_9_ 0x49
#define _PIC_RF_A_ 0x4A
#define _PIC_RF_B_ 0x4B
#define _PIC_RF_C_ 0x4C
#define _PIC_RF_D_ 0x4D
#define _PIC_RF_E_ 0x4E
#define _PIC_RF_F_ 0x4F

#define _PIC_RG_6_ 0x56

#define _PIC_AD_0_ 8
#define _PIC_AD_1_ 9
#define _PIC_AD_2_ 10
#define _PIC_AD_3_ 11
#define _PIC_AD_4_ 12
#define _PIC_AD_5_ 13
#define _PIC_AD_6_ 14

/* _picsig_PS_POL = _PIC_RB_8_ */
#define _picsig_PS____ _PIC_RE_5_
#define _picsig_TRIG__ _PIC_RE_4_

#define _picsig_AF____ _PIC_RC_E_
/* _picsig_Shutr_ = _PIC_RD_1_ */

#define _picsig_FPGAcs _PIC_RB_6_

/* _picsig_CRNR_0 = _PIC_RD_2_ */
/* _picsig_CRNR_1 = _PIC_RD_1_ */
/* _picsig_CRNR_2 = _PIC_RC_E_ */
/* _picsig_CRNR_3 = _PIC_RC_D_ */

/* ================================================================= */
/* OIS controller assign */
/* ================================================================= */
/* Slave Address */
#define _SLV_OIS_ 0x0E

/* Packet Command OP code */
#define _OP_FIRM_DWNLD 0x80
#define _OP_Periphe_RW 0x82
#define _OP_Memory__RW 0x84
#define _OP_AD_TRNSFER 0x86
#define _OP_COEF_DWNLD 0x88
#define _OP_PrgMem__RD 0x8A
#define _OP_SpecialCMD 0x8C

#define _cmd_8C_EI 0x0001
#define _cmd_8C_DI 0x0002
#define _cmd_8C_STRB 0x0004
#define _cmd_8C_TRI_SHT 0x0008
#define _cmd_8C_TRI_con 0x0010

#define _OP_FX_Cmnd_0 0xF0
#define _OP_FX_Cmnd_1 0xF1
#define _OP_FX_Cmnd_2 0xF2
#define _OP_FX_Cmnd_3 0xF3
#define _OP_FX_Cmnd_8 0xF8
#define _OP_FX_Cmnd_A 0xFA

/* Peripheral Adr Mapping */
#define _P_00_CLK_CONTRL 0x00
#define _P_01_CLK_PWMCNT 0x01
#define _P_02_CLK_TMRCNT 0x02
#define _P_03_CLK_DWNSMP 0x03
#define _P_04_CLK_CALDIV 0x04
#define _P_05_CLK_CLKSEL 0x05
#define _P_06_CLK_MONITR 0x06
/* _P_07) */
#define _P_08_TRI_CNTRL_ 0x08
#define _P_09_TRI_TARGET 0x09
#define _P_0A_TRI_PHSCNT 0x0A
/* _P_0B) */
/* _P_0C) */
/* _P_0D) */
/* _P_0E) */
/* _P_0F) */
#define _P_10_I2C_SLVADR 0x10
#define _P_11_I2C__DATA_ 0x11
#define _P_12_I2C_STSADR 0x12
#define _P_13_I2C_MONSEL 0x13
#define _P_14_I2C_OFSREG 0x14
/* _P_15) */
/* _P_16) */
/* _P_17) */
#define _P_18_SPI_TRSNUM 0x18
#define _P_19_SPI_CONTRL 0x19
/* _P_1A_SPI_D_000H = 0x1A */
#define _P_1A_SPI_D_HHHL 0x1B
#define _P_1B_SPI_D_LHLL 0x1C
/* _P_1B) */
/* _P_1C) */
/* _P_1D) */
/* _P_1E) */
/* _P_1F) */
#define _P_20_MSC_MONCTL1 0x20
#define _P_21_MSC_MONCTL2 0x21
#define _P_22_MSC_PORTCTL 0x22
#define _P_23_MSC_PORTSTS 0x23
#define _P_24_MSC_PWMREGA 0x24
#define _P_25_MSC_PWMREGB 0x25
#define _P_26_MSC_SIGMON 0x26
/* _P_27) */
#define _P_28_PWM_ENB_SR 0x28
#define _P_29_PWM_FRQSEL 0x29
#define _P_2A_PWM_SFT_YX 0x2A
#define _P_2B_PWM_SFT__Z 0x2B
#define _P_2C_PWM_ChX_DAT 0x2C
#define _P_2D_PWM_ChY_DAT 0x2D
#define _P_2E_PWM_ChZ_DAT 0x2E
#define _P_2F_PWM_SMPLSFT 0x2F
#define _P_30_ADC_CH0 0x30
#define _P_31_ADC_CH1 0x31
#define _P_32_ADC_CH2 0x32
#define _P_33_ADC_CH3 0x33
#define _P_34_ADC_CH4 0x34
#define _P_35_ADC_CH5 0x35
#define _P_36_ADC_CH6 0x36
#define _P_37_ADC_CH7 0x37

#define _P_38_Ch3_VAL_0 0x38
#define _P_39_Ch3_VAL_1 0x39
#define _P_3A_Ch3_VAL_2 0x3A
#define _P_3B_Ch3_VAL_3 0x3B

#define _P_3C_ADC_GAIN 0x3C
#define _P_3D_ADC_CTL 0x3D
/* _P_3E) */
/* _P_3F) */
#define _P_40_OUT_CH 0x40
#define _P_41_OUT_CTL 0x41
#define _P_42_OUT_DAC 0x42
#define _P_43_OUT_SW 0x43
/* _P_44) */
/* _P_45) */
/* _P_46) */
/* _P_47) */
#define _P_48_TOP_00 0x48
/* _P_49) */
/* _P_4A) */
/* _P_4B) */
/* _P_4C) */
/* _P_4D) */
/* _P_4E) */
/* _P_4F) */
#define _P_50_TST_TSTCMD 0x50
#define _P_51_TST_AF_SLP 0x51
#define _P_52_TST_AF_OFS 0x52
#define _P_53_TST_SH_SLP 0x53
#define _P_54_TST_OSC___ 0x54
#define _P_55_TST_VREF__ 0x55
#define _P_56_TST_MUXCRN 0x56
#define _P_57_TST_TSADAC 0x57
#define _P_58_TST_FIXHBR 0x58
#define _P_59_TST_SMPPWR 0x59
#define _P_5A_TST_MPXFIX 0x5A
/* _P_5B_)             ,0x5B */
/* _P_5C_)             ,0x5C */
/* _P_5D_)             ,0x5D */
/* _P_5E_)             ,0x5E */
#define _P_5F_TST_INFVER 0x5F

#define _P_F0_I2CnSTR 0xF0
#define _P_F1_I2CnSTR 0xF1
#define _P_F2_I2CnSTR 0xF2
#define _P_F3_I2CnSTR 0xF3

/* Memory Adr Mapping */
#define _M_Kgx01 0x00
#define _M_Kgx02 0x01
#define _M_Kgx03 0x02
#define _M_Kgx04 0x03
#define _M_wDgx00 0x04
#define _M_X_HPF0 0x05
#define _M_Kgx00 0x06
#define _M_wDgxof 0x07
#define _M_Kgx05 0x08
#define _M_Kgx06 0x09
#define _M_Kgx07 0x0A
#define _M_Kgx08 0x0B
#define _M_wDgx01 0x0C
#define _M_X_HPF1 0x0D
#define _M_KgxWv 0x0E
#define _M_KgxG 0x0F
#define _M_Kgx09 0x10
#define _M_Kgx0A 0x11
#define _M_Kgx0B 0x12
#define _M_Kgx0C 0x13
#define _M_wDgx02 0x14
#define _M_X_HPF2 0x15
#define _M_wDgx04 0x16
#define _M_X_TRGT 0x17
#define _M_Kgx0D 0x18
#define _M_Kgx0E 0x19
#define _M_Kgx0F 0x1A
#define _M_Kgx10 0x1B
#define _M_wDgx03 0x1C
#define _M_X_HPF3 0x1D
#define _M_X_H_ofs 0x1E
#define _M_X_wGIN 0x1F
#define _M_Kgx1F 0x20
#define _M_Kgx20 0x21
#define _M_Kgx21 0x22
#define _M_Kgx22 0x23
#define _M_wDgx0B 0x24
#define _M_wDgx_EQL1 0x25
#define _M_X_wGOU 0x26
#define _M_wDgx09 0x27
#define _M_Kgx1B 0x28
#define _M_Kgx1C 0x29
#define _M_Kgx1D 0x2A
#define _M_Kgx1E 0x2B
#define _M_wDgx0A 0x2C
#define _M_wDgx_EQH 0x2D
#define _M_X_wGAS 0x2E
#define _M_X_GYIN 0x2F
#define _M_Kgx23 0x30
#define _M_Kgx24 0x31
#define _M_Kgx25 0x32
#define _M_Kgx26 0x33
#define _M_wDgx0C 0x34
#define _M_wDgx_EQL 0x35
#define _M_Kgxdr 0x36
#define _M_X_axofs 0x37
#define _M_Kgx11 0x38
#define _M_Kgx12 0x39
#define _M_Kgx13 0x3A
#define _M_Kgx14 0x3B
#define _M_Kgx15 0x3C
#define _M_wDgx05 0x3D
#define _M_wDgx06 0x3E
#define _M_X_H_Lmt 0x3F
#define _M_X_LMT 0x40
#define _M_X_LMO 0x41
#define _M_X_DRP 0x42
#define _M_X_TGT 0x43
#define _M_Kgx19 0x44
#define _M_Kgx1A 0x45
#define _M_KgxHG 0x46
#define _M_KgxTG 0x47
#define _M_Kgx01t 0x48
#define _M_Kgx02t 0x49
#define _M_Kgx03t 0x4A
#define _M_Kgx04t 0x4B
#define _M_Kgx05t 0x4C
#define _M_wDgx01t 0x4D
#define _M_wDgx02t 0x4E
#define _M_wDgx_EQ 0x4F
/* #define _M_rsv       0x50 */
/* #define _M_rsv       0x51 */
/* #define _M_rsv       0x52 */
/* #define _M_rsv       0x53 */
/* #define _M_rsv       0x54 */
#define _M_DigGx 0x55
#define _M_DigGy 0x56
#define _M_CURIN 0x57
/* #define _M_rsv       0x58 */
/* #define _M_rsv       0x59 */
/* #define _M_rsv       0x5A */
/* #define _M_rsv       0x5B */
#define _M_wDf00 0x5C
#define _M_FOTRIM 0x5D
#define _M_CUROFS 0x5E
#define _M_Kf0A 0x5F
#define _M_wCRFED 0x60
#define _M_wFTRGT 0x61
#define _M_wFSUB 0x62
/* #define _M_rsv       0x63 */
#define _M_Kf0C 0x64
#define _M_wDf03 0x65
#define _M_rsv 0x66
#define _M_CUROFO 0x67
#define _M_Kf00 0x68
#define _M_Kf01 0x69
#define _M_TMP_X_ 0x6A /* RHM_HT 2013/11/25    Modified */
#define _M_TMP_Y_ 0x6B /* RHM_HT 2013/11/25    Modified */
#define _M_wDf01 0x6C
#define _M_AFLPF 0x6D
/* #define _M_rsv       0x6E */
/* #define _M_rsv       0x6F */
#define _M_KgxH0 0x70 /* RHM_HT 2013/11/25    Modified */
#define _M_X_PEO 0x71
#define _M_KgyH0 0x72 /* RHM_HT 2013/11/25    Modified */
#define _M_Y_PEO 0x73
#define _M_FOC_CNTRL 0x74
/* #define _M_rsv       0x75 */
#define _M_CEFTYP 0x76
#define _M_GYRSNS 0x77
#define _M_SHDAC0 0x78
#define _M_SHDAC1 0x79
#define _M_Ksh0 0x7A
#define _M_Ksh1 0x7B
#define _M_mon0 0x7C
#define _M_mon1 0x7D
#define _M_GLBFLG 0x7E
#define _M_EQCTL 0x7F
#define _M_Kgy01 0x80
#define _M_Kgy02 0x81
#define _M_Kgy03 0x82
#define _M_Kgy04 0x83
#define _M_wDgy00 0x84
#define _M_Y_HPF0 0x85
#define _M_Kgy00 0x86
#define _M_wDgyof 0x87
#define _M_Kgy05 0x88
#define _M_Kgy06 0x89
#define _M_Kgy07 0x8A
#define _M_Kgy08 0x8B
#define _M_wDgy01 0x8C
#define _M_Y_HPF1 0x8D
#define _M_KgyWv 0x8E
#define _M_KgyG 0x8F
#define _M_Kgy09 0x90
#define _M_Kgy0A 0x91
#define _M_Kgy0B 0x92
#define _M_Kgy0C 0x93
#define _M_wDgy02 0x94
#define _M_Y_HPF2 0x95
#define _M_wDgy04 0x96
#define _M_Y_TRGT 0x97
#define _M_Kgy0D 0x98
#define _M_Kgy0E 0x99
#define _M_Kgy0F 0x9A
#define _M_Kgy10 0x9B
#define _M_wDgy03 0x9C
#define _M_Y_HPF3 0x9D
#define _M_Y_H_ofs 0x9E
#define _M_Y_wGIN 0x9F
#define _M_Kgy1F 0xA0
#define _M_Kgy20 0xA1
#define _M_Kgy21 0xA2
#define _M_Kgy22 0xA3
#define _M_wDgy0B 0xA4
#define _M_wDgy_EQL1 0xA5
#define _M_Y_wGOU 0xA6
#define _M_wDgy09 0xA7
#define _M_Kgy1B 0xA8
#define _M_Kgy1C 0xA9
#define _M_Kgy1D 0xAA
#define _M_Kgy1E 0xAB
#define _M_wDgy0A 0xAC
#define _M_wDgy_EQH 0xAD
#define _M_Y_wGAS 0xAE
#define _M_Y_GYIN 0xAF
#define _M_Kgy23 0xB0
#define _M_Kgy24 0xB1
#define _M_Kgy25 0xB2
#define _M_Kgy26 0xB3
#define _M_wDgy0C 0xB4
#define _M_wDgy_EQL 0xB5
#define _M_Kgydr 0xB6
#define _M_Y_axofs 0xB7
#define _M_Kgy11 0xB8
#define _M_Kgy12 0xB9
#define _M_Kgy13 0xBA
#define _M_Kgy14 0xBB
#define _M_Kgy15 0xBC
#define _M_wDgy05 0xBD
#define _M_wDgy06 0xBE
#define _M_Y_H_Lmt 0xBF
#define _M_Y_LMT 0xC0
#define _M_Y_LMO 0xC1
#define _M_Y_DRP 0xC2
#define _M_Y_TGT 0xC3
#define _M_Kgy19 0xC4
#define _M_Kgy1A 0xC5
#define _M_KgyHG 0xC6
#define _M_KgyTG 0xC7
#define _M_Kgy01t 0xC8
#define _M_Kgy02t 0xC9
#define _M_Kgy03t 0xCA
#define _M_Kgy04t 0xCB
#define _M_Kgy05t 0xCC
#define _M_wDgy01t 0xCD
#define _M_wDgy02t 0xCE
#define _M_wDgy_EQ 0xCF
#define _M_ISRC0 0xD0
#define _M_ISRC1 0xD1
#define _M_ISRC2 0xD2
#define _M_ISRC3 0xD3
#define _M_ISRC4 0xD4
#define _M_ISRC5 0xD5
#define _M_ISRC6 0xD6
#define _M_ISRC7 0xD7
#define _M_ISRC8 0xD8
#define _M_ISRC9 0xD9
#define _M_ISRCA 0xDA
#define _M_ISRCB 0xDB
#define _M_ISRCC 0xDC
#define _M_ISRCD 0xDD
#define _M_ISRCE 0xDE
#define _M_ISRCF 0xDF
#define _M_Kgy16 0xE0
#define _M_Kgy17 0xE1
#define _M_Kgy18 0xE2
#define _M_0x0000 0xE3
#define _M_0x8000 0xE4
#define _M_wDgy07 0xE5
#define _M_wDgy08 0xE6
#define _M_VMTRGT 0xE7
#define _M_Kv00 0xE8
#define _M_Kv01 0xE9
#define _M_Kv02 0xEA
#define _M_Kv03 0xEB
#define _M_wDv00 0xEC
#define _M_VMLPF 0xED
#define _M_wDv01 0xEE
#define _M_Kv04 0xEF
#define _M_SHND_sts 0xF0
#define _M_SHND_tim_org 0xF1
#define _M_SHND_tim 0xF2
#define _M_SHND_CWCCW 0xF3
#define _M_BPFOUT 0xF4
#define _M_BPFMON 0xF5
#define _M_FIRMVER 0xF6
#define _M_OIS_STS 0xF7
#define _def_OIS____________BOOT 0x00
#define _def_OIS__while_FWdwnlod 0x01
#define _def_OIS__FW_NG_NG_NG_NG 0x02
#define _def_OIS____________FWOK 0x03
#define _def_OIS_main_______IDLE 0x04
#define _M_I2C_dat 0xF8
#define _M_I2C_cmd 0xF9
#define _M_I2C_STS 0xFA
#define _M____tmp4 0xFB
#define _M____tmp3 0xFC
#define _M____tmp2 0xFD
#define _M____tmp1 0xFE
#define _M____tmp0 0xFF

/* ================================================================= */
/* Another definition */
/* ================================================================= */

/* Scene parameter */
#define _SCENE_NIGHT_1 1
#define _SCENE_NIGHT_2 2
#define _SCENE_NIGHT_3 3

#define _SCENE_D_A_Y_1 4
#define _SCENE_D_A_Y_2 5
#define _SCENE_D_A_Y_3 6

#define _SCENE_SPORT_1 7
#define _SCENE_SPORT_2 8
#define _SCENE_SPORT_3 9

#define _SCENE_TEST___ 10

/* for Factory Adjustment */
#define _CUR100mA 0x00A0
#define _CUR125mA 0x00C8
#define _CUR150mA 0x00F0
#define _CUR175mA 0x0118
#define _CUR200mA 0x0140
#define _CUR225mA 0x0168
#define _CUR250mA 0x0190
#define _CUR275mA 0x01B8
#define _CUR300mA 0x01E0
#define _CUR325mA 0x0208
#define _CUR350mA 0x0230
#define _CUR375mA 0x0258
#define _CUR400mA 0x0280

#define _VHTRGT 0x0BA2

#define _OTHR_IN_HALPX 0x0006
#define _OTHR_IN_HALPY 0x0008
#define _OTHR_HXpreOUT 0x0001
#define _OTHR_HYpreOUT 0x0002

/* Factory Adjustment data */
struct _FACT_ADJ {
	unsigned short int gl_CURDAT;
	unsigned short int gl_HALOFS_X;
	unsigned short int gl_HALOFS_Y;
	unsigned short int gl_HX_OFS;
	unsigned short int gl_HY_OFS;
	unsigned short int gl_PSTXOF;
	unsigned short int gl_PSTYOF;
	unsigned short int gl_GX_OFS;
	unsigned short int gl_GY_OFS;
	unsigned short int gl_KgxHG;
	unsigned short int gl_KgyHG;
	unsigned short int gl_KGXG;
	unsigned short int gl_KGYG;
	unsigned short int gl_SFTHAL_X; /* RHM_HT 2013/11/25    Added */
	unsigned short int gl_SFTHAL_Y; /* RHM_HT 2013/11/25    Added */
	unsigned short int gl_TMP_X_;   /* RHM_HT 2013/11/25    Added */
	unsigned short int gl_TMP_Y_;   /* RHM_HT 2013/11/25    Added */
	unsigned short int gl_KgxH0;    /* RHM_HT 2013/11/25    Added */
	unsigned short int gl_KgyH0;    /* RHM_HT 2013/11/25    Added */
};

/* Default Parameter of FACTORY Adjust data */
/* --------------------------------------------- */
extern const struct _FACT_ADJ FADJ_DEF
#ifdef OIS_MAIN_C
= {

			0x0200, /* gl_CURDAT; */
			0x0200, /* gl_HALOFS_X; */
			0x0200, /* gl_HALOFS_Y; */
			0x0000, /* gl_HX_OFS; */
			0x0000, /* gl_HY_OFS; */
			0x0080,
			0x0080,
			0x0000, /* gl_GX_OFS; */
			0x0000, /* gl_GY_OFS; */

			0x2000,
			0x2000,
			0x2000,
			0x2000,
			0x0200,
			0x0200,
			0x0000,
			0x0000,
			0x0000,
			0x0000,
}
#endif
;

/* FACTORY Adjusted data */
/* These data are stored at the non-vollatile */
/* memory inside of the CMOS sensor. */
/* The Host ( ISP or I2C master ) read these */
/* data from above memory and write to the OIS */
/* controller. */
/* --------------------------------------------- */
extern struct _FACT_ADJ FADJ_MEM
#ifdef OIS_MAIN_C
= {

			0x0201, /* gl_CURDAT; */
			0x0200, /* gl_HALOFS_X; */
			0x0200, /* gl_HALOFS_Y; */
			0x0000, /* gl_HX_OFS; */
			0x0000, /* gl_HY_OFS; */
			0x0080,
			0x0080,
			0x0000, /* gl_GX_OFS; */
			0x0000, /* gl_GY_OFS; */

			0x2000,
			0x2000,
			0x2000,
			0x2000,
			0x0200,
			0x0200,
			0x0000,
			0x0000,
			0x0000,
			0x0000,
}
#endif
;

/* Parameters for expanding OIS range */
/* --------------------------------------------- */
extern double p_x, q_x;
extern double p_y, q_y;
extern short int zero_X;
extern short int zero_Y;
extern short int PREOUT_X_P, PREOUT_X_N;
extern short int PREOUT_Y_P, PREOUT_Y_N;
extern double alfa_X, beta_X;
extern double alfa_Y, beta_Y;

#endif /* OIS_DEFINITION_H */
