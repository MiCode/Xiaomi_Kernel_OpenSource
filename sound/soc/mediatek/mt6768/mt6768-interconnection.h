/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Mediatek MT6768 audio driver interconnection definition
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

#ifndef _MT6768_INTERCONNECTION_H_
#define _MT6768_INTERCONNECTION_H_

/* in port define */
#define I_I2S0_CH1 0
#define I_I2S0_CH2 1
#define I_ADDA_UL_CH1 3
#define I_ADDA_UL_CH2 4
#define I_DL1_CH1 5
#define I_DL1_CH2 6
#define I_DL2_CH1 7
#define I_DL2_CH2 8
#define I_GAIN1_OUT_CH1 10
#define I_GAIN1_OUT_CH2 11
#define I_GAIN2_OUT_CH1 12
#define I_GAIN2_OUT_CH2 13
#define I_PCM_2_CAP_CH1 14
#define I_DL12_CH1 19
#define I_DL12_CH2 20
#define I_PCM_2_CAP_CH2 21
#define I_DL3_CH1 23
#define I_DL3_CH2 24
#define I_I2S2_CH1 25
#define I_I2S2_CH2 26

/* in port define >= 32 */
#define I_32_OFFSET 32
#define I_CONNSYS_I2S_CH1 (34 - I_32_OFFSET)
#define I_CONNSYS_I2S_CH2 (35 - I_32_OFFSET)

#endif
