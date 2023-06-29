/*
 * Copyright (C) 2018 MediaTek Inc.
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
#ifndef __TMP_BTS_H__
#define __TMP_BTS_H__

#define APPLY_PRECISE_NTC_TABLE
#define APPLY_AUXADC_CALI_DATA
#define APPLY_PRECISE_BTS_TEMP

#define AUX_IN0_NTC (0)
#define AUX_IN1_NTC (1)
#define AUX_IN2_NTC (2)
#define AUX_IN3_NTC (3)
#define AUX_IN4_NTC (4)


#define BTS_RAP_PULL_UP_R		100000 /* 100K, pull up resister */

#define BTS_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp
						 * default value -40 deg
						 */

#define BTS_RAP_PULL_UP_VOLTAGE		1800 /* 1.8V ,pull up voltage */

#define BTS_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */

#define BTS_RAP_ADC_CHANNEL		AUX_IN0_NTC /* default is 0 */

#define BTSMDPA_RAP_PULL_UP_R		100000 /* 100K, pull up resister */

#define BTSMDPA_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp
						 * default value -40 deg
						 */

#define BTSMDPA_RAP_PULL_UP_VOLTAGE	1800 /* 1.8V ,pull up voltage */

#define BTSMDPA_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */

#define BTSMDPA_RAP_ADC_CHANNEL		AUX_IN1_NTC /* default is 1 */


#define BTSMAINCHARGER_RAP_PULL_UP_R		100000	/* 100K,pull up resister */
#define BTSMAINCHARGER_OVER_CRITICAL_LOW	4397119	/* base on 100K NTC temp
						 *default value -40 deg
						 */

#define BTSMAINCHARGER_PULL_UP_VOLTAGE	1800	/* 1.8V ,pull up voltage */
#define BTSMAINCHARGER_RAP_NTC_TABLE		7

#define BTSMAINCHARGER_RAP_ADC_CHANNEL		AUX_IN3_NTC

#define BTSSUBCHARGER_RAP_PULL_UP_R		100000	/* 100K,pull up resister */
#define BTSSUBCHARGER_OVER_CRITICAL_LOW	4397119	/* base on 100K NTC temp
						 *default value -40 deg
						 */

#define BTSSUBCHARGER_PULL_UP_VOLTAGE	1800	/* 1.8V ,pull up voltage */
#define BTSSUBCHARGER_RAP_NTC_TABLE		7

#define BTSSUBCHARGER_RAP_ADC_CHANNEL		AUX_IN2_NTC

#define BTSFLED_RAP_PULL_UP_R		100000	/* 100K,pull up resister */
#define BTSFLED_OVER_CRITICAL_LOW	4397119	/* base on 100K NTC temp
						 *default value -40 deg
						 */

#define BTSFLED_PULL_UP_VOLTAGE	1800	/* 1.8V ,pull up voltage */
#define BTSFLED_RAP_NTC_TABLE		7

#define BTSFLED_RAP_ADC_CHANNEL		AUX_IN4_NTC




extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_IsAdcInitReady(void);

#endif	/* __TMP_BTS_H__ */
