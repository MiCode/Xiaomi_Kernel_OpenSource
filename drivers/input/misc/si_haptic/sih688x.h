/*
 *  Silicon Integrated Co., Ltd haptic sih688x header file
 *
 *  Copyright (c) 2021 kugua <canzhen.peng@si-in.com>
 *  Copyright (c) 2021 tianchi <tianchi.zheng@si-in.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#ifndef _SIH6887_H_
#define _SIH6887_H_

#include "haptic.h"

#define SIH688X_CHIPID_REG_VALUE                    0x87
#define SIH688X_CHIPID_REG_ADDR                     0x00
#define SIH688X_BRK_VBOOST_COE						10
#define SIH688X_VBOOST_MUL_COE						4096
#define SIH688X_DETECT_FIFO_ARRAY_MAX               50
#define SIH688X_READ_FIFO_MAX_DATA_LEN				9
#define SIH688X_FIFO_PACK_SIZE						5
#define SIH688X_FIFO_READ_DATA_LEN					6
#define SIH688X_LPF_GAIN_COE						4
#define SIH688X_DETECT_BEMF_COE						4096
#define SIH688X_DETECT_ADCV							16
#define SIH688X_BEMF_MV_COE							1000
#define SIH688X_DETECT_ADCV_COE						10
#define SIH688X_DETECT_F0_COE						786432000
#define SIH688X_DETECT_F0_AMPLI_COE					10
#define SIH688X_TRACKING_F0_COE						1966080000
#define SIH688X_F0_CAL_COE							1000000
#define SIH688X_F0_AMPLI_COE						10
#define SIH688X_F0_DELTA							28800000
#define SIH688X_F0_CALI_DELTA						2880
#define SIH688X_F0_VAL_MAX							1800
#define SIH688X_F0_VAL_MIN							1600
#define SIH688X_RL_AMP_COE							78125
#define SIH688X_RL_DIV_COE							336
#define SIH688X_B0_RL_AMP_COE						10000
#define SIH688X_B0_RL_DIV_COE						189
#define SIH688X_RL_SAR_CODE_DIVIDER					84
#define SIH688X_RL_CONFIG_REG_NUM					3
#define SIH688X_ADC_COE								320
#define SIH688X_ADC_AMPLIFY_COE						1000
#define SIH688X_RL_MODIFY							4
#define SIH688X_RL_MODIFY_COE						10
#define SIH688X_LPF_GAIN_COE						4
#define SIH688X_OSC_CALI_COE						100000
#define SIH688X_DRV_BOOST_BASE						35
#define SIH688X_DRV_BOOST_SETP_COE					1000
#define SIH688X_DRV_BOOST_SETP						625
#define SIH688X_STANDARD_VBAT						4000
#define SIH688X_VBAT_MIN							3000
#define SIH688X_VBAT_MAX							5500
#define SIH688X_VBAT_AMPLIFY_COE					1000
#define SIH688X_OSC_RTL_DATA_LEN					1000
#define SIh688X_PWM_SAMPLE_48KHZ					48
#define SIh688X_PWM_SAMPLE_24KHZ					24
#define SIh688X_PWM_SAMPLE_12KHZ					12
#define SIH688X_READ_CHIP_ID_MAX_TRY				8
#define SIH688X_RL_DETECT_MAX_TRY					10
#define SIH688X_GET_VBAT_MAX_TRY					10
#define SIH688X_DRV_VBOOST_MIN						6
#define SIH688X_DRV_VBOOST_MAX						11
#define SIH688X_DRV_VBOOST_COEFFICIENT				10
#define SIH688X_RL_OFFSET							500

typedef enum SIH688X_CONT_PARA {
	SIH688X_CONT_PARA_SEQ0 = 0,
	SIH688X_CONT_PARA_SEQ1 = 1,
	SIH688X_CONT_PARA_SEQ2 = 2,
	SIH688X_CONT_PARA_ASMOOTH = 3,
	SIH688X_CONT_PARA_TH_LEN = 4,
	SIH688X_CONT_PARA_TH_NUM = 5,
	SIH688X_CONT_PARA_AMPLI = 6,
} sih688x_cont_para_e;

#endif

