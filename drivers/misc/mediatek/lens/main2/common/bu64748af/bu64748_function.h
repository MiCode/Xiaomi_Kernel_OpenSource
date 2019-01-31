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

#ifndef __BU64748_FUNCTION_H
#define __BU64748_FUNCTION_H

typedef unsigned short u16;
typedef unsigned char u8;

#define _SLV_FBAF_ 0x76

#define ADJ_OK 0
#define ADJ_ERR -1
#define PROG_DL_ERR -2
#define COEF_DL_ERR -3

#define _OP_FIRM_DWNLD 0x80
#define _OP_Periphe_RW 0x82
#define _OP_Memory__RW 0x84
#define _OP_COEF_DWNLD 0x88
#define _OP_SpecialCMD 0x8C

#define _cmd_8C_EI 0x0001
#define _cmd_8C_DI 0x0002
#define _cmd_8C_STRB 0x0004
#define _cmd_8C_TRI_SHT 0x0008
#define _cmd_8C_TRI_con 0x0010

#define _P_31_CURRENT_ADC__ 0x31
#define _P_37_OTHER___ADC__ 0x37

#define _P_2C_BTL_DAC__DATA 0x2C
#define _P_30_HALL_CurntDAC 0x30
#define _P_31_HALL___PREDAC 0x31
#define _P_34_CURAMP_OFSDAC 0x34
#define _P_39_HALL__POSTDAC 0x39
#define _P_3C_HALL__GAIN__1 0x3C
#define _P_3F_HALL__GAIN__2 0x3F
#define _P_41_ADC_Averaging 0x41
#define _P_51_AF_SLP_Trimng 0x51
#define _P_5A_ADC_MPX_FIX__ 0x5A
#define _P_72_BTL_____RANGE 0x72
#define _P_73_BTL____Enable 0x73
#define _P_75_BTL_ExtendBit 0x75

#define _M_FIRMVER 0xF6
#define _M_00_Kg00 0x00
#define _M_01_Kg01 0x01
#define _M_02_Kg02 0x02
#define _M_03_Kg03 0x03
#define _M_04_wD04 0x04
#define _M_05_wDEQH 0x05
#define _M_06_TRILMT 0x06
#define _M_07_KgWv 0x07
#define _M_08_Kg08 0x08
#define _M_09_Kg09 0x09
#define _M_0A_Kg0A 0x0A
#define _M_0B_Kg0B 0x0B
#define _M_0C_wD0C 0x0C
#define _M_0D_wDEQ1 0x0D
#define _M_0E_C_LMT 0x0E
#define _M_0F_C_LMO 0x0F
#define _M_10_Kg10 0x10
#define _M_11_Kg11 0x11
#define _M_12_Kg12 0x12
#define _M_13_Kg13 0x13
#define _M_14_wD14 0x14
#define _M_15_wDEQL 0x15
#define _M_16_CUROFS 0x16
#define _M_17_Kf0A 0x17
#define _M_18_Kg18 0x18
#define _M_19_Kg19 0x19
#define _M_1A_Kg1A 0x1A
#define _M_1B_Kg1B 0x1B
#define _M_1C_Kg1C 0x1C
#define _M_1D_wD1D 0x1D
#define _M_1E_wD1E 0x1E
#define _M_1F_BPFOUT 0x1F
#define _M_20_HOFS 0x20
#define _M_21_KgHG 0x21
#define _M_22_wTRGT 0x22
#define _M_23_wGIN 0x23
#define _M_24_wGOUT 0x24
#define _M_25_KgLPG 0x25
#define _M_26_wEQIN 0x26
#define _M_27_wGAS 0x27
#define _M_28_wDgEQA 0x28
#define _M_29_Kf0C 0x29
#define _M_2A_wDf0X 0x2A
#define _M_2B_wDg09 0x2B
#define _M_2C_NRMHAL 0x2C
#define _M_2D_Kg2D 0x2D
#define _M_2E__tmp0 0x2E
#define _M_2F_Kg2F 0x2F
#define _M_30_EQCTL 0x30

#define _EQ_ALL_OFF 0x0000
#define _EQ_ALL__ON 0x0004
#define _EQ_CUR_FBK 0x0004
#define _EQ_POS_SRV_SR_fil__O 0x000C
#define _EQ_POS_SRV_SR_fil_of 0x000D
/* -- _EQ_C_FB_CL	= 0x0004 */
/* -- _EQ_C_FB_OP	= 0x0005  */
/*-- _EQ_C_FBOFF	= 0x0014  */
#define _M_31_PRFCNT 0x31
#define _M_32_PRFNUM 0x32
#define _M_33_SO_ADR 0x33
#define _M_34_PRESFT 0x34
#define _M_35_PSTSFT 0x35
#define _M_36_fs_mod 0x36
#define _M_37_CURSFT 0x37
#define _M_38_PRFCEF 0x38
#define _M_39_CURSET 0x39
#define _M_3A_AXSOFS 0x3A
#define _M_3B_CURDET 0x3B
#define _M_3C_STRCNT 0x3C
#define _M_3D_Kwvat 0x3D
#define _M_3E_CNVTRG 0x3E
#define _M_3F_TRIDAT 0x3F
#define _M_40_COEF__00 0x40
#define _M_41_COEF__01 0x41
#define _M_42_COEF__02 0x42
#define _M_43_COEF__03 0x43
#define _M_44_COEF__04 0x44
#define _M_45_COEF__05 0x45
#define _M_46_COEF__06 0x46
#define _M_47_SETTRGT 0x47
#define _M_48_Kg48 0x48
#define _M_49_Kg49 0x49
#define _M_4A_Kg4A 0x4A
#define _M_4B_Kg4B 0x4B
#define _M_4C_Kg4C 0x4C
#define _M_4D_wD4D 0x4D
#define _M_4E_wD4E 0x4E
#define _M_4F_2ndOUT 0x4F
#define _M_50_POS_SFT_1 0x50
#define _M_51_POS_SFT_2 0x51
#define _M_52_POS_SFT_3 0x52
#define _M_53_POS_SFT_4 0x53
#define _M_54_POS_SFT_5 0x54
#define _M_55_POS_SFT_6 0x55
#define _M_56_POS_SFT_7 0x56
#define _M_57_POS_SFT_C 0x57
#define _M_58_POS_SFT11 0x58
#define _M_59_POS_SFT12 0x59
#define _M_5A_POS_SFT13 0x5A
#define _M_5B_POS_SFT14 0x5B
#define _M_5C_POS_SFT15 0x5C
#define _M_5D_POS_SFT16 0x5D
#define _M_5E_POS_SFT17 0x5E
#define _M_5F_HALIN 0x5F
#define _M_CD_CEFTYP 0xCD
#define _M_CF_tmpTRGT 0xCF
#define _M_D0_Hall_0 0xD0
#define _M_D1_Hall_1 0xD1
#define _M_D2_Hall_2 0xD2
#define _M_D3_Hall_3 0xD3
#define _M_D4_Hall_4 0xD4
#define _M_D5_Hall_5 0xD5
#define _M_D6_Hall_6 0xD6
#define _M_D7_Hall_7 0xD7
#define _M_D8_Hall_8 0xD8
#define _M_D9_Hall_9 0xD9
#define _M_DA_Hall_A 0xDA
#define _M_DB_Hall_B 0xDB
#define _M_DC_Hall_C 0xDC
#define _M_DD_Hall_D 0xDD
#define _M_DE_Hall_E 0xDE
#define _M_DF_Hall_F 0xDF
#define _M_E0_Hall10 0xE0
#define _M_E1_Hall11 0xE1
#define _M_E2_Hall12 0xE2
#define _M_E3_Hall13 0xE3
#define _M_E4_Hall14 0xE4
#define _M_E5_Hall15 0xE5
#define _M_E6_Hall16 0xE6
#define _M_E7_Hall17 0xE7
#define _M_E8_Hall18 0xE8
#define _M_E9_Hall19 0xE9
#define _M_EA_Hall1A 0xEA
#define _M_EB_Hall1B 0xEB
#define _M_EC_Hall1C 0xEC
#define _M_ED_Hall1D 0xED
#define _M_EE_Hall1E 0xEE
#define _M_EF_Hall1F 0xEF
#define _M_F0_CUR_IN 0xF0
#define _M_F6_FIRMVER 0xF6
#define _M_F7_FBAF_STS 0xF7

extern int main2_SOutEx(u8 slaveAddress, u8 *dat, int size);
extern int main2_SInEx(u8 slaveAddress, u8 *dat, int size, u8 *ret,
		       int ret_size);

void main2_AF_TARGET(u16 target);
int BU64748_main2_Initial(void);
u16 bu64748_main2_af_cur_pos(void);
void BU64748_main2_soft_power_ctrl(int On);

#endif
