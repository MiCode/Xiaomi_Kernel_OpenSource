/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_IDLE_PROFILE_H__
#define __MTK_IDLE_PROFILE_H__

#include <linux/kernel.h>
#include <linux/cpumask.h>
#include <mtk_idle_internal.h>

unsigned long long idle_get_current_time_ms(void);

//void mtk_idle_latency_profile_result(struct MTK_IDLE_MODEL_CLERK *clerk);

//void mtk_idle_block_reason_report(struct MTK_IDLE_MODEL_CLERK const *clerk);

#endif
