/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
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
		struct fteh_loading *arr);
extern int fpsgo_fteh2minitop_start(int count, struct fteh_loading *fl);
extern int fpsgo_fteh2minitop_query(int count, struct fteh_loading *fl);
extern int fpsgo_fteh2minitop_end(void);


#endif
