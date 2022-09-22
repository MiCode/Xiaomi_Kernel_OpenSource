/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_THERMAL_TIMER_H_
#define _MTK_THERMAL_TIMER_H_
extern int mtkTTimer_register
(const char *name, void (*start_timer) (void), void (*cancel_timer) (void));

extern int mtkTTimer_unregister(const char *name);
#endif	/* _MTK_THERMAL_TIMER_H_ */
