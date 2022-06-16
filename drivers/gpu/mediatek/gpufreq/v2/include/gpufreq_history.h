/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_HISTORY_H__
#define __GPUFREQ_HISTORY_H__

/**************************************************
 * Definition
 **************************************************/
#define GPUFREQ_HISTORY_SIZE            (0x2000 >> 2)

/**************************************************
 * Structure
 **************************************************/


/**************************************************
 * Function
 **************************************************/
int gpufreq_history_init(void);
const unsigned int *gpufreq_history_get_log(void);

#endif /* __GPUFREQ_HISTORY_H__ */
