/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _WMT_ALARM_H_
#define _WMT_ALARM_H_

int wmt_alarm_init(void);
int wmt_alarm_deinit(void);

int wmt_alarm_start(unsigned int sec);
int wmt_alarm_cancel(void);

#endif
