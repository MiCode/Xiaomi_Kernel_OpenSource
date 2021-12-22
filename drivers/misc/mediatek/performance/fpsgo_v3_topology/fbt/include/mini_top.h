/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
