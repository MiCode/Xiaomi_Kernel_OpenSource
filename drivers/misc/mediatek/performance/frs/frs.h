/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __FRS_H__
#define __FRS_H__

extern int (*eara_pre_change_fp)(void);
extern int (*eara_pre_change_single_fp)(int pid, unsigned long long bufID,
			int target_fps);
int eara_nl_send_to_user(void *buf, int size);

#endif
