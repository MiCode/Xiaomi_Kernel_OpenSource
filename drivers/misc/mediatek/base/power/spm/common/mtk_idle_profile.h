/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MTK_IDLE_PROFILE_H__
#define __MTK_IDLE_PROFILE_H__

#include <linux/kernel.h>
#include <linux/cpumask.h>
#include <mtk_idle_internal.h>
#include <mtk_idle_module.h>

unsigned long long idle_get_current_time_ms(void);

void mtk_idle_latency_profile_result(struct MTK_IDLE_MODEL_CLERK *clerk);

void mtk_idle_block_reason_report(struct MTK_IDLE_MODEL_CLERK const *clerk);

#endif
