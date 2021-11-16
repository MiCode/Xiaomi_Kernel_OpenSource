/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef _FPSGO_MINI_TOP_H_
#define _FPSGO_MINI_TOP_H_

#include <linux/rbtree.h>
#include <linux/list.h>

struct tid_util {
	pid_t tid;
	int util;
};

struct minitop_work {
	pid_t tid[NR_CPUS];
	int util[NR_CPUS];
	struct work_struct work;
	struct list_head link;
};

#define MINITOP_SCHED		(0x1 << 0)
#define MINITOP_FTEH		(0x1 << 1)
#define MINITOP_FBT		(0x1 << 2)

struct minitop_rec {
	pid_t tid;
	struct rb_node node;

	u64 init_runtime;
	u64 init_timestamp;
	u64 ratio;
	u32 life;
	int debnc;
	int ever;

	int source;
	int debnc_fteh; /* irrelative to ceiling switch */
	int debnc_fbt;

	u64 runtime_inst;
	u64 ratio_inst;
};

int __init minitop_init(void);
void __exit minitop_exit(void);

extern void (*fpsgo_sched_nominate_fp)(pid_t *tid, int *util);

#endif
