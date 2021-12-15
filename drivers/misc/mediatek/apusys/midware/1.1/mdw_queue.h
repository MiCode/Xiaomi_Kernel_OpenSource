// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_MDW_QUEUE_H__
#define __APUSYS_MDW_QUEUE_H__

#include "mdw_cmd.h"

struct mdw_queue_ops {
	int (*task_start)(struct mdw_apu_sc *sc, void *q);
	int (*task_end)(struct mdw_apu_sc *sc, void *q);
	struct mdw_apu_sc *(*pop)(void *q);
	int (*insert)(struct mdw_apu_sc *sc, void *q, int is_front);
	int (*delete)(struct mdw_apu_sc *sc, void *q);
	int (*len)(void *q);
	void (*destroy)(void *q);
};

/* deadline sched */
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

	atomic_t cnt;
	struct mdw_queue_ops ops;
};

int mdw_queue_deadline_init(struct deadline_root *root);

/* normal sched */
struct mdw_queue_norm {
	uint32_t cnt;
	struct mutex mtx;
	struct list_head p_list; //pid list
	struct list_head pi_list; //pid item list
	struct mdw_queue_ops ops;
};

int mdw_queue_norm_init(struct mdw_queue_norm *nq);

/* mdw queue */
struct mdw_queue {
	struct mutex mtx;
	uint32_t normal_task_num;
	uint32_t deadline_task_num;
	struct mdw_queue_norm norm;
	struct deadline_root deadline;
};

int mdw_queue_task_start(struct mdw_apu_sc *sc);
int mdw_queue_task_end(struct mdw_apu_sc *sc);
struct mdw_apu_sc *mdw_queue_pop(int type);
int mdw_queue_insert(struct mdw_apu_sc *sc, int is_front);
int mdw_queue_len(int type, int is_deadline);
int mdw_queue_delete(struct mdw_apu_sc *sc);
int mdw_queue_boost(struct mdw_apu_sc *sc);
void mdw_queue_destroy(struct mdw_queue *mq);
int mdw_queue_init(struct mdw_queue *mq);

extern struct dentry *mdw_dbg_root;

#endif
