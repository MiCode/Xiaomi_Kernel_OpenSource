/*
 * Copyright (C) 2012 Invensense, Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/**
 *  @addtogroup  DRIVERS
 *  @brief       Hardware drivers.
 *
 *  @{
 *      @file    dmpDefaultMPU6050.c
 *      @brief   dmp Default data
 *      @details This file is part of invensense mpu driver code
 *
 */

#include "dmpKey.h"
#include "dmpmap.h"

#define CFG_LP_QUAT             (2710)
#define END_ORIENT_TEMP         (1874)
#define CFG_27                  (2738)
#define CFG_20                  (2232)
#define CFG_23                  (2741)
#define CFG_FIFO_ON_EVENT       (2687)
#define END_PREDICTION_UPDATE   (1769)
#define CGNOTICE_INTR           (2629)
#define X_GRT_Y_TMP             (1366)
#define CFG_DR_INT              (1029)
#define CFG_AUTH                (1035)
#define UPDATE_PROP_ROT         (1843)
#define FCFG_1                  (1062)
#define SKIP_X_GRT_Y_TMP        (1367)
#define SKIP_END_COMPARE        (1443)
#define FCFG_3                  (1110)
#define FCFG_2                  (1066)
#define END_COMPARE_Y_X_TMP2    (1463)
#define END_COMPARE_Y_X_TMP3    (1442)
#define FCFG_7                  (1076)
#define FCFG_6                  (1128)
#define FLAT_STATE_END          (1721)
#define SWING_END_4             (1624)
#define SWING_END_2             (1573)
#define SWING_END_3             (1595)
#define SWING_END_1             (1558)
#define CFG_8                   (2716)
#define CFG_15                  (2724)
#define CFG_16                  (2742)
#define FLAT_STATE_END_TEMP     (1691)
#define END_COMPARE_Y_X_TMP     (1415)
#define DO_NOT_UPDATE_PROP_ROT  (1847)
#define CFG_7                   (1221)
#define END_COMPARE_Y_X         (1492)
#define SKIP_SWING_END_1        (1559)
#define SKIP_SWING_END_3        (1596)
#define SKIP_SWING_END_2        (1574)
#define TILTG75_START           (1680)
#define CFG_6                   (2749)
#define TILTL75_END             (1677)
#define END_ORIENT              (1892)
#define CFG_FLICK_IN            (2582)
#define TILTL75_START           (1651)
#define CFG_MOTION_BIAS         (1224)
#define X_GRT_Y                 (1416)
#define TEMPLABEL               (2332)
#define CFG_DISPLAY_ORIENT_INT  (1861)
#define X_GRT_Y_TMP2            (1387)

#define D_0_22                  (22+512)
#define D_0_24                  (24+512)

#define D_0_36                  (36)
#define D_0_52                  (52)
#define D_0_96                  (96)
#define D_0_104                 (104)
#define D_0_108                 (108)
#define D_0_163                 (163)
#define D_0_188                 (188)
#define D_0_192                 (192)
#define D_0_224                 (224)
#define D_0_228                 (228)
#define D_0_232                 (232)
#define D_0_236                 (236)

#define D_1_2                   (256 + 2)
#define D_1_4                   (256 + 4)
#define D_1_8                   (256 + 8)
#define D_1_10                  (256 + 10)
#define D_1_24                  (256 + 24)
#define D_1_28                  (256 + 28)
#define D_1_36                  (256 + 36)
#define D_1_40                  (256 + 40)
#define D_1_44                  (256 + 44)
#define D_1_72                  (256 + 72)
#define D_1_74                  (256 + 74)
#define D_1_79                  (256 + 79)
#define D_1_88                  (256 + 88)
#define D_1_90                  (256 + 90)
#define D_1_92                  (256 + 92)
#define D_1_96                  (256 + 96)
#define D_1_98                  (256 + 98)
#define D_1_106                 (256 + 106)
#define D_1_108                 (256 + 108)
#define D_1_112                 (256 + 112)
#define D_1_128                 (256 + 144)
#define D_1_152                 (256 + 12)
#define D_1_160                 (256 + 160)
#define D_1_176                 (256 + 176)
#define D_1_178                 (256 + 178)
#define D_1_218                 (256 + 218)
#define D_1_232                 (256 + 232)
#define D_1_236                 (256 + 236)
#define D_1_240                 (256 + 240)
#define D_1_244                 (256 + 244)
#define D_1_250                 (256 + 250)
#define D_1_252                 (256 + 252)
#define D_2_12                  (512 + 12)
#define D_2_96                  (512 + 96)
#define D_2_108                 (512 + 108)
#define D_2_208                 (512 + 208)
#define D_2_224                 (512 + 224)
#define D_2_236                 (512 + 236)
#define D_2_244                 (512 + 244)
#define D_2_248                 (512 + 248)
#define D_2_252                 (512 + 252)

#define CPASS_BIAS_X            (35 * 16 + 4)
#define CPASS_BIAS_Y            (35 * 16 + 8)
#define CPASS_BIAS_Z            (35 * 16 + 12)
#define CPASS_MTX_00            (36 * 16)
#define CPASS_MTX_01            (36 * 16 + 4)
#define CPASS_MTX_02            (36 * 16 + 8)
#define CPASS_MTX_10            (36 * 16 + 12)
#define CPASS_MTX_11            (37 * 16)
#define CPASS_MTX_12            (37 * 16 + 4)
#define CPASS_MTX_20            (37 * 16 + 8)
#define CPASS_MTX_21            (37 * 16 + 12)
#define CPASS_MTX_22            (43 * 16 + 12)
#define D_ACT0                  (40 * 16)
#define D_ACSX                  (40 * 16 + 4)
#define D_ACSY                  (40 * 16 + 8)
#define D_ACSZ                  (40 * 16 + 12)

#define FLICK_MSG               (45 * 16 + 4)
#define FLICK_COUNTER           (45 * 16 + 8)
#define FLICK_LOWER             (45 * 16 + 12)
#define FLICK_UPPER             (46 * 16 + 12)

#define D_AUTH_OUT              (992)
#define D_AUTH_IN               (996)
#define D_AUTH_A                (1000)
#define D_AUTH_B                (1004)

#define D_PEDSTD_BP_B           (768 + 0x1C)
#define D_PEDSTD_HP_A           (768 + 0x78)
#define D_PEDSTD_HP_B           (768 + 0x7C)
#define D_PEDSTD_BP_A4          (768 + 0x40)
#define D_PEDSTD_BP_A3          (768 + 0x44)
#define D_PEDSTD_BP_A2          (768 + 0x48)
#define D_PEDSTD_BP_A1          (768 + 0x4C)
#define D_PEDSTD_INT_THRSH      (768 + 0x68)
#define D_PEDSTD_CLIP           (768 + 0x6C)
#define D_PEDSTD_SB             (768 + 0x28)
#define D_PEDSTD_SB_TIME        (768 + 0x2C)
#define D_PEDSTD_PEAKTHRSH      (768 + 0x98)
#define D_PEDSTD_TIML           (768 + 0x2A)
#define D_PEDSTD_TIMH           (768 + 0x2E)
#define D_PEDSTD_PEAK           (768 + 0X94)
#define D_PEDSTD_STEPCTR        (768 + 0x60)
#define D_PEDSTD_TIMECTR        (964)
#define D_PEDSTD_DECI           (768 + 0xA0)

#define D_HOST_NO_MOT           (976)
#define D_ACCEL_BIAS            (660)

#define D_ORIENT_GAP            (76)

#define D_TILT0_H               (48)
#define D_TILT0_L               (50)
#define D_TILT1_H               (52)
#define D_TILT1_L               (54)
#define D_TILT2_H               (56)
#define D_TILT2_L               (58)
#define D_TILT3_H               (60)
#define D_TILT3_L               (62)

static const struct tKeyLabel dmpTConfig[] = {
	{KEY_CFG_27, CFG_27},
	{KEY_CFG_20, CFG_20},
	{KEY_CFG_23, CFG_23},
	{KEY_CFG_FIFO_ON_EVENT, CFG_FIFO_ON_EVENT},
	{KEY_CGNOTICE_INTR, CGNOTICE_INTR},
	{KEY_X_GRT_Y_TMP, X_GRT_Y_TMP},
	{KEY_CFG_DR_INT, CFG_DR_INT},
	{KEY_CFG_AUTH, CFG_AUTH},
	{KEY_FCFG_1, FCFG_1},
	{KEY_SKIP_X_GRT_Y_TMP, SKIP_X_GRT_Y_TMP},
	{KEY_SKIP_END_COMPARE, SKIP_END_COMPARE},
	{KEY_FCFG_3, FCFG_3},
	{KEY_FCFG_2, FCFG_2},
	{KEY_END_COMPARE_Y_X_TMP2, END_COMPARE_Y_X_TMP2},
	{KEY_CFG_DISPLAY_ORIENT_INT, CFG_DISPLAY_ORIENT_INT},
	{KEY_FCFG_7, FCFG_7},
	{KEY_FCFG_6, FCFG_6},
	{KEY_CFG_8, CFG_8},
	{KEY_CFG_15, CFG_15},
	{KEY_CFG_16, CFG_16},
	{KEY_END_COMPARE_Y_X_TMP, END_COMPARE_Y_X_TMP},
	{KEY_CFG_6, CFG_6},
	{KEY_END_COMPARE_Y_X, END_COMPARE_Y_X},
	{KEY_CFG_LP_QUAT, CFG_LP_QUAT},
	{KEY_END_ORIENT, END_ORIENT},
	{KEY_CFG_FLICK_IN, CFG_FLICK_IN},
	{KEY_CFG_7, CFG_7},
	{KEY_CFG_MOTION_BIAS, CFG_MOTION_BIAS},
	{KEY_X_GRT_Y, X_GRT_Y},
	{KEY_TEMPLABEL, TEMPLABEL},
	{KEY_END_COMPARE_Y_X_TMP3, END_COMPARE_Y_X_TMP3},
	{KEY_X_GRT_Y_TMP2, X_GRT_Y_TMP2},
	{KEY_D_0_22, D_0_22},
	{KEY_D_0_96, D_0_96},
	{KEY_D_0_104, D_0_104},
	{KEY_D_0_108, D_0_108},
	{KEY_D_1_36, D_1_36},
	{KEY_D_1_40, D_1_40},
	{KEY_D_1_44, D_1_44},
	{KEY_D_1_72, D_1_72},
	{KEY_D_1_74, D_1_74},
	{KEY_D_1_79, D_1_79},
	{KEY_D_1_88, D_1_88},
	{KEY_D_1_90, D_1_90},
	{KEY_D_1_92, D_1_92},
	{KEY_D_1_160, D_1_160},
	{KEY_D_1_176, D_1_176},
	{KEY_D_1_178, D_1_178},
	{KEY_D_1_218, D_1_218},
	{KEY_D_1_232, D_1_232},
	{KEY_D_1_250, D_1_250},
	{KEY_DMP_TAPW_MIN, DMP_TAPW_MIN},
	{KEY_DMP_TAP_THR_X, DMP_TAP_THX},
	{KEY_DMP_TAP_THR_Y, DMP_TAP_THY},
	{KEY_DMP_TAP_THR_Z, DMP_TAP_THZ},
	{KEY_DMP_SH_TH_Y, DMP_SH_TH_Y},
	{KEY_DMP_SH_TH_X, DMP_SH_TH_X},
	{KEY_DMP_SH_TH_Z, DMP_SH_TH_Z},
	{KEY_DMP_ORIENT, DMP_ORIENT},
	{KEY_D_AUTH_OUT, D_AUTH_OUT},
	{KEY_D_AUTH_IN, D_AUTH_IN},
	{KEY_D_AUTH_A, D_AUTH_A},
	{KEY_D_AUTH_B, D_AUTH_B},
	{KEY_CPASS_BIAS_X, CPASS_BIAS_X},
	{KEY_CPASS_BIAS_Y, CPASS_BIAS_Y},
	{KEY_CPASS_BIAS_Z, CPASS_BIAS_Z},
	{KEY_CPASS_MTX_00, CPASS_MTX_00},
	{KEY_CPASS_MTX_01, CPASS_MTX_01},
	{KEY_CPASS_MTX_02, CPASS_MTX_02},
	{KEY_CPASS_MTX_10, CPASS_MTX_10},
	{KEY_CPASS_MTX_11, CPASS_MTX_11},
	{KEY_CPASS_MTX_12, CPASS_MTX_12},
	{KEY_CPASS_MTX_20, CPASS_MTX_20},
	{KEY_CPASS_MTX_21, CPASS_MTX_21},
	{KEY_CPASS_MTX_22, CPASS_MTX_22},
	{KEY_D_ACT0, D_ACT0},
	{KEY_D_ACSX, D_ACSX},
	{KEY_D_ACSY, D_ACSY},
	{KEY_D_ACSZ, D_ACSZ},
	{KEY_FLICK_MSG, FLICK_MSG},
	{KEY_FLICK_COUNTER, FLICK_COUNTER},
	{KEY_FLICK_LOWER, FLICK_LOWER},
	{KEY_FLICK_UPPER, FLICK_UPPER},
	{KEY_D_PEDSTD_BP_B, D_PEDSTD_BP_B},
	{KEY_D_PEDSTD_HP_A, D_PEDSTD_HP_A},
	{KEY_D_PEDSTD_HP_B, D_PEDSTD_HP_B},
	{KEY_D_PEDSTD_BP_A4, D_PEDSTD_BP_A4},
	{KEY_D_PEDSTD_BP_A3, D_PEDSTD_BP_A3},
	{KEY_D_PEDSTD_BP_A2, D_PEDSTD_BP_A2},
	{KEY_D_PEDSTD_BP_A1, D_PEDSTD_BP_A1},
	{KEY_D_PEDSTD_INT_THRSH, D_PEDSTD_INT_THRSH},
	{KEY_D_PEDSTD_CLIP, D_PEDSTD_CLIP},
	{KEY_D_PEDSTD_SB, D_PEDSTD_SB},
	{KEY_D_PEDSTD_SB_TIME, D_PEDSTD_SB_TIME},
	{KEY_D_PEDSTD_PEAKTHRSH, D_PEDSTD_PEAKTHRSH},
	{KEY_D_PEDSTD_TIML, D_PEDSTD_TIML},
	{KEY_D_PEDSTD_TIMH, D_PEDSTD_TIMH},
	{KEY_D_PEDSTD_PEAK, D_PEDSTD_PEAK},
	{KEY_D_PEDSTD_STEPCTR, D_PEDSTD_STEPCTR},
	{KEY_D_PEDSTD_TIMECTR, D_PEDSTD_TIMECTR},
	{KEY_D_PEDSTD_DECI, D_PEDSTD_DECI},
	{KEY_D_HOST_NO_MOT, D_HOST_NO_MOT},
	{KEY_D_ACCEL_BIAS, D_ACCEL_BIAS},
};

#define NUM_LOCAL_KEYS (sizeof(dmpTConfig)/sizeof(dmpTConfig[0]))

static struct tKeyLabel keys[NUM_KEYS];

unsigned short inv_dmp_get_address(unsigned short key)
{
	static int isSorted;
	if (!isSorted) {
		int kk;
		for (kk = 0; kk < NUM_KEYS; ++kk) {
			keys[kk].addr = 0xffff;
			keys[kk].key = kk;
		}
		for (kk = 0; kk < NUM_LOCAL_KEYS; ++kk)
			keys[dmpTConfig[kk].key].addr = dmpTConfig[kk].addr;
		isSorted = 1;
	}
	if (key >= NUM_KEYS)
		return 0xffff;
	return keys[key].addr;
}

/**
 *  @}
 */
