/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __TMP_BTS_H__
#define __TMP_BTS_H__
/* chip dependent */
#define APPLY_PRECISE_NTC_TABLE
#define APPLY_AUXADC_CALI_DATA
#define APPLY_PRECISE_BTS_TEMP

/* N17 code for HQ-296383 by liunianliang at 2023/05/17 start */
#define AUX_IN0_NTC (0)
#define AUX_IN1_NTC (1)
#define AUX_IN2_NTC (2)
#define AUX_IN3_NTC (3)

#define BTS_RAP_PULL_UP_R		100000 /* 100K, pull up resister */
#define BTS_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp default value -40 deg */
#define BTS_RAP_PULL_UP_VOLTAGE		1800 /* 1.8V ,pull up voltage */
#define BTS_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */
#define BTS_RAP_ADC_CHANNEL		AUX_IN3_NTC /* default is 3 */

#define BTSMDPA_RAP_PULL_UP_R		100000 /* 100K, pull up resister */
#define BTSMDPA_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp default value -40 deg */
#define BTSMDPA_RAP_PULL_UP_VOLTAGE	1800 /* 1.8V ,pull up voltage */
#define BTSMDPA_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */
#define BTSMDPA_RAP_ADC_CHANNEL		AUX_IN1_NTC /* default is 1 */

#define BTSNRPA_RAP_PULL_UP_R		100000	/* 100K,pull up resister */
#define BTSNRPA_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp default value -40 deg */
#define BTSNRPA_RAP_PULL_UP_VOLTAGE	1800	/* 1.8V ,pull up voltage */
#define BTSNRPA_RAP_NTC_TABLE		7
#define BTSNRPA_RAP_ADC_CHANNEL		AUX_IN1_NTC

#define BTSCHARGER_RAP_PULL_UP_R	100000	/* 100K,pull up resister */
#define BTSCHARGER_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp default value -40 deg */
#define BTSCHARGER_RAP_PULL_UP_VOLTAGE	1800	/* 1.8V ,pull up voltage */
#define BTSCHARGER_RAP_NTC_TABLE	7
#define BTSCHARGER_RAP_ADC_CHANNEL	AUX_IN2_NTC

#define CSCPU_RAP_PULL_UP_R		100000 /* 100K, pull up resister */
#define CSCPU_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp default value -40 deg */
#define CSCPU_RAP_PULL_UP_VOLTAGE	1800 /* 1.8V ,pull up voltage */
#define CSCPU_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */
#define CSCPU_RAP_ADC_CHANNEL		AUX_IN0_NTC /* default is 0 */

#define MAIN_SUPPLY_RAP_NTC_TABLE	8
#define MAIN_SUPPLY 0
#define SECONDARY_SUPPLY 1

extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_IsAdcInitReady(void);
extern int get_supply_rank(void);
/* N17 code for HQ-296383 by liunianliang at 2023/05/17 end */
#endif	/* __TMP_BTS_H__ */
