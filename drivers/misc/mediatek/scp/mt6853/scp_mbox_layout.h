/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _SCP_MBOX_LAYOUT_H_
#define _SCP_MBOX_LAYOUT_H_

#define SCP_MBOX_TOTAL 5

/* core1 */
/* definition of slot size for send PINs */
#define PIN_OUT_SIZE_AUDIO_VOW_1        9 /* the following will use mbox 0 */

/* definition of slot size for received PINs */
#define PIN_IN_SIZE_AUDIO_VOW_ACK_1     2 /* the following will use mbox 0 */
#define PIN_IN_SIZE_AUDIO_VOW_1        26 /* the following will use mbox 0 */

/* core0 */
/* definition of slot size for send PINs */
#define PIN_OUT_SIZE_APCCCI_0		 2 /* the following will use mbox 1 */
#define PIN_OUT_SIZE_DVFS_SET_FREQ_0	1 /* the following will use mbox 1 */
#define PIN_OUT_C_SIZE_SLEEP_0          2 /* the following will use mbox 1 */
#define PIN_OUT_R_SIZE_SLEEP_0          1 /* the following will use mbox 1 */
#define PIN_OUT_SIZE_TEST_0		 1 /* the following will use mbox 1 */

/* definition of slot size for received PINs */
#define PIN_IN_SIZE_APCCCI_0		 2 /* the following will use mbox 1 */
#define PIN_IN_SIZE_SCP_ERROR_INFO_0	10 /* the following will use mbox 1 */
#define PIN_IN_SIZE_SCP_READY_0		 1 /* the following will use mbox 1 */
#define PIN_IN_SIZE_SCP_RAM_DUMP_0	 2 /* the following will use mbox 1 */
/* ============================================================ */

/* core1 */
/* definition of slot size for send PINs */
#define PIN_OUT_SIZE_AUDIO_ULTRA_SND_1	 2 /* the following will use mbox 3 */
#define PIN_OUT_SIZE_DVFS_SET_FREQ_1	 1 /* the following will use mbox 3 */
#define PIN_OUT_C_SIZE_SLEEP_1	         2 /* the following will use mbox 3 */
#define PIN_OUT_R_SIZE_SLEEP_1	         1 /* the following will use mbox 3 */
#define PIN_OUT_SIZE_TEST_1		 1 /* the following will use mbox 3 */
#define PIN_OUT_SIZE_LOGGER_CTRL	 6 /* the following will use mbox 3 */
#define PIN_OUT_SIZE_SCPCTL_1		 2 /* the following will use mbox 3 */

/* definition of slot size for received PINs */
#define PIN_IN_SIZE_AUDIO_ULTRA_SND_1	 2 /* the following will use mbox 3 */
#define PIN_IN_SIZE_SCP_ERROR_INFO_1	10 /* the following will use mbox 3 */
#define PIN_IN_SIZE_LOGGER_CTRL		 6 /* the following will use mbox 3 */
#define PIN_IN_SIZE_SCP_READY_1		 1 /* the following will use mbox 3 */
#define PIN_IN_SIZE_SCP_RAM_DUMP_1	 2 /* the following will use mbox 3 */
/* ============================================================ */

/* this is mbox pool for 2 cores */
#define PIN_OUT_SIZE_SCP_MPOOL         34 /* the following will use mbox 2,4 */
#define PIN_IN_SIZE_SCP_MPOOL          30 /* the following will use mbox 2,4 */
#define PIN_OUT_SIZE_CHRE_0            34
#define PIN_OUT_SIZE_CHREX_0           14
#define PIN_OUT_SIZE_SENSOR_0          14
#define PIN_IN_SIZE_CHRE_0             14
#define PIN_IN_SIZE_SENSOR_0           14

#endif
