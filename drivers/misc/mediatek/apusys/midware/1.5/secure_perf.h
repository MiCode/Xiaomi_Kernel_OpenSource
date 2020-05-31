/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SECURE_PERF_H__
#define __SECURE_PERF_H__

void secure_perf_init(void);
void secure_perf_remove(void);
void secure_perf_raise(void);
void secure_perf_restore(void);

#endif
