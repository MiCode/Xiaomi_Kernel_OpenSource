/*
 * Copyright (C) 2018 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#define AUX_IN0_NTC (0)
#define AUX_IN1_NTC (1)
/* 2021.02.04 longcheer jiangshitian change for pd-chg and main-cam thermal begin */
#if defined(CONFIG_TARGET_PROJECT_K7B)
#define AUX_IN3_NTC (3)
#define AUX_IN4_NTC (4)
#endif
/* 2021.02.04 longcheer jiangshitian change for pd-chg and main-cam thermal end */

#define BTS_RAP_PULL_UP_R		390000 /* 390K, pull up resister */

#define BTS_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp
						 * default value -40 deg
						 */

#define BTS_RAP_PULL_UP_VOLTAGE		1800 /* 1.8V ,pull up voltage */

#define BTS_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */

#define BTS_RAP_ADC_CHANNEL		AUX_IN0_NTC /* default is 0 */

#define BTSMDPA_RAP_PULL_UP_R		390000 /* 390K, pull up resister */

#define BTSMDPA_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp
						 * default value -40 deg
						 */

#define BTSMDPA_RAP_PULL_UP_VOLTAGE	1800 /* 1.8V ,pull up voltage */

#define BTSMDPA_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */

#define BTSMDPA_RAP_ADC_CHANNEL		AUX_IN1_NTC /* default is 1 */

/* 2021.02.04 longcheer jiangshitian change for pd-chg and main-cam thermal begin */
#if defined(CONFIG_TARGET_PROJECT_K7B)
#define BTSMCAM_RAP_PULL_UP_R		390000 /* 390K, pull up resister */
#define BTSMCAM_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp   default value -40 deg  */
#define BTSMCAM_RAP_PULL_UP_VOLTAGE	1800 /* 1.8V ,pull up voltage */
#define BTSMCAM_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */
#define BTSMCAM_RAP_ADC_CHANNEL		AUX_IN3_NTC /* default is 1 */

#define BTSPDCHG_RAP_PULL_UP_R		390000 /* 390K, pull up resister */
#define BTSPDCHG_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp   default value -40 deg  */
#define BTSPDCHG_RAP_PULL_UP_VOLTAGE	1800 /* 1.8V ,pull up voltage */
#define BTSPDCHG_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */
#define BTSPDCHG_RAP_ADC_CHANNEL		AUX_IN4_NTC /* default is 1 */
#endif
/* 2021.02.04 longcheer jiangshitian change for pd-chg and main-cam thermal end */

#define BTS_BLKNTC_RAP_PULL_UP_R	390000	/*390k PULL UP resister*/
#define	BTS_BLKNTC_TAP_OVER_CRITICAL_LOW	4397119	/*BASE ON 100K NTC TEMP DEFAULT VALUE - 40 DEG*/
#define	BTS_BLKNTC_RAP_PULL_UP_VOLTAGE		1800	/*1.8v PULL UP VOLTAGE*/
#define	BTS_BLKNTC_RAP_NTC_TABLE		7	/*default is ncp 15wf104f03rc 100k*/

extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_IsAdcInitReady(void);

#endif	/* __TMP_BTS_H__ */
