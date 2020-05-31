/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef MIDWARE_1_0_SCHED_DEADLINE_H_
#define MIDWARE_1_0_SCHED_DEADLINE_H_
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include "cmd_parser.h"
#include "cmd_format.h"

struct deadline_root {
	char name[32];
	struct rb_root_cached root;
	int cores;
	uint64_t total_runtime;
	uint64_t total_period;
	uint64_t total_subcmd;
	uint64_t avg_load[3];
	struct mutex lock;
	struct delayed_work work;
	bool need_timer;
	bool load_boost;
	bool trace_boost;
};

extern struct dentry *apusys_dbg_root;
int deadline_queue_init(int type);
int deadline_task_insert(struct apusys_subcmd *sc);
int deadline_task_remove(struct apusys_subcmd *sc);
bool deadline_task_empty(int type);
struct apusys_subcmd *deadline_task_pop(int type);
int deadline_task_end(struct apusys_subcmd *sc);
int deadline_task_start(struct apusys_subcmd *sc);
void deadline_queue_destroy(int type);
int deadline_task_boost(struct apusys_subcmd *sc);


#endif /* MIDWARE_1_0_SCHED_DEADLINE_H_ */
