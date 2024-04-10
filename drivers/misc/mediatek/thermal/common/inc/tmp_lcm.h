/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __TMP_LCM_H__
#define __TMP_LCM_H__

/* chip dependent */

#define APPLY_PRECISE_NTC_TABLE
#define APPLY_AUXADC_CALI_DATA

#define AUX_IN4_NTC (4)

#define LCM_RAP_PULL_UP_R		390000 /* 390K, pull up resister */

#define LCM_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp
						 * default value -40 deg
						 */

#define LCM_RAP_PULL_UP_VOLTAGE		1800 /* 1.8V ,pull up voltage */

#define LCM_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */

#define LCM_RAP_ADC_CHANNEL		AUX_IN4_NTC /* default is 4 */


extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_IsAdcInitReady(void);

#endif	/* __TMP_BTS_H__ */
