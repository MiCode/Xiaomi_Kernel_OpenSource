/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Celine Liu <Celine.liu@mediatek.com>
 */

#ifndef _ADSP_TIMESYNC_H_
#define _ADSP_TIMESYNC_H_

void adsp_timesync_suspend(void);
void adsp_timesync_resume(void);
int __init adsp_timesync_init(void);

#endif // _ADSP_TIMESYNC_H_

