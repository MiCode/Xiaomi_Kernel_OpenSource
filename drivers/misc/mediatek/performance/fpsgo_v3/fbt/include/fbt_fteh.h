/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FTEH_H__
#define __FTEH_H__

#include "fpsgo_base.h"

int __init fbt_fteh_init(void);
void __exit fbt_fteh_exit(void);

int fpsgo_fbt2fteh_judge_ceiling(struct render_info *thread_info,
		unsigned int blc_wt);
int fpsgo_fteh_get_state(int *pid);

extern int fpsgo_fteh2xgf_get_dep_list_num(int pid);
extern int fpsgo_fteh2xgf_get_dep_list(int pid, int count,
		struct fpsgo_loading *arr);
extern int fpsgo_fteh2minitop_start(int count, struct fpsgo_loading *fl);
extern int fpsgo_fteh2minitop_query(int count, struct fpsgo_loading *fl);
extern int fpsgo_fteh2minitop_end(void);


#endif
