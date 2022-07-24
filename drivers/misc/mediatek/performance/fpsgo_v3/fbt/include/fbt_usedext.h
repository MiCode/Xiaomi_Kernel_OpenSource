/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FBT_USEDEXT_H__
#define __FBT_USEDEXT_H__

extern void set_user_nice(struct task_struct *p, long nice);

extern int fpsgo_fbt2minitop_start(int count, struct fpsgo_loading *fl);
extern int fpsgo_fbt2minitop_query(int count, struct fpsgo_loading *fl);
extern int fpsgo_fbt2minitop_end(void);
extern int fpsgo_fbt2minitop_query_single(pid_t pid);


#endif
