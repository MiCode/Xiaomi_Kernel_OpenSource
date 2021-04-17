/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _TIMESYNC_H_
#define _TIMESYNC_H_

void timesync_filter_set(int64_t scp_timestamp, int64_t scp_archcounter);
int64_t timesync_filter_get(void);
void timesync_start(void);
void timesync_stop(void);
void timesync_resume(void);
void timesync_suspend(void);
int timesync_init(void);
void timesync_exit(void);

#endif
