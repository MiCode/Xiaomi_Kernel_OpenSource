/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _GPUEB_TIMESYNC_H_
#define _GPUEB_TIMESYNC_H_

void gpueb_timesync_suspend(void);
void gpueb_timesync_resume(void);
unsigned int gpueb_timesync_init(void);

#endif // _GPUEB_TIMESYNC_H_
