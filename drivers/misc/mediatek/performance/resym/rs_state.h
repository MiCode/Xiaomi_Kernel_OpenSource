/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef RS_STATE_H
#define RS_STATE_H

extern void (*rsu_getstate_fp)(int *throttled);

int __init rs_state_init(void);
void __exit rs_state_exit(void);

#endif

