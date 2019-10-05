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

#ifndef _XGF_H_
#define _XGF_H_

#include <linux/rbtree.h>

#define HW_EVENT_NUM 2
#define HW_MONITER_WINDOW 10
#define HW_MONITER_LEVEL 8
#define EMA_DIVISOR 10
#define EMA_DIVIDEND 7
#define EMA_REST_DIVIDEND (EMA_DIVISOR - EMA_DIVIDEND)
#define SP_ALLOW_NAME "UnityMain"

enum XGF_ERROR {
	XGF_NOTIFY_OK,
	XGF_SLPTIME_OK,
	XGF_DISABLE,
	XGF_THREAD_NOT_FOUND,
	XGF_PARAM_ERR,
	XGF_BUFFER_ERR,
	XGF_BUFFER_OW,
	XGF_BUFFER_ZERO,
	XGF_DEP_LIST_ERR
};

enum {
	XGF_QUEUE_START = 0,
	XGF_QUEUE_END,
	XGF_DEQUEUE_START,
	XGF_DEQUEUE_END
};

enum HW_EVENT {
	HW_AI = 0,
	HW_CV
};

enum XGF_DEPS_CAT {
	INNER_DEPS = 0,
	OUTER_DEPS,
	PREVI_DEPS
};

struct xgf_sub_sect {
	struct hlist_node hlist;
	unsigned long long start_ts;
	unsigned long long end_ts;
};

struct xgf_render {
	struct hlist_node hlist;
	pid_t parent;
	pid_t render;

	struct hlist_head sector_head;
	int sector_nr;

	struct hlist_head hw_head;

	int prev_index;
	unsigned long long prev_ts;
	int curr_index;
	unsigned long long curr_ts;
	int event_count;

	int frame_count;

	struct rb_root deps_list;
	struct rb_root out_deps_list;
	struct rb_root prev_deps_list;

	struct xgf_sub_sect deque;
	struct xgf_sub_sect queue;

	unsigned long long ema_runtime;

	int spid;
};

struct xgf_dep {
	struct rb_node rb_node;

	pid_t tid;
	int render_dep;
};

struct xgf_runtime_sect {
	struct hlist_node hlist;
	unsigned long long start_ts;
	unsigned long long end_ts;
};

struct xgf_render_sector {
	struct hlist_node hlist;
	int sector_id;
	int counted;
	struct hlist_head path_head;

	unsigned long long head_ts;
	unsigned long long tail_ts;
	int head_index;
	int tail_index;

	int got_next_index;
	int stop_at_irq;

	int raw_head_index;
	int raw_tail_index;

	int special_rs;
};

struct xgf_pid_rec {
	struct hlist_node hlist;
	pid_t pid;
};

struct xgf_hw_rec {
	struct hlist_node hlist;
	unsigned long long mid;
	int event_type;
	int hw_type;
};

struct xgf_irq_stack {
	int index;
	int event;

	struct xgf_irq_stack *next;
};

struct xgf_hw_event {
	struct hlist_node hlist;

	int event_type;
	int event_index;
	pid_t tid;
	pid_t render;
	unsigned long long mid;
	int valid;

	unsigned long long head_ts;
	unsigned long long tail_ts;
};

extern int (*xgf_est_runtime_fp)(pid_t r_pid,
		struct xgf_render *render,
		unsigned long long *runtime,
		unsigned long long ts);

extern int (*xgf_stat_xchg_fp)(int enable);

void xgf_lockprove(const char *tag);
void xgf_trace(const char *fmt, ...);
void xgf_reset_renders(void);
int xgf_est_runtime(pid_t r_pid, struct xgf_render *render,
			unsigned long long *runtime, unsigned long long ts);
int xgf_stat_xchg(int enable);
void *xgf_alloc(int size);
void xgf_free(void *block);
void *xgf_atomic_val_assign(int select);
int xgf_atomic_read(atomic_t *val);
int xgf_atomic_inc_return(atomic_t *val);
void xgf_atomic_set(atomic_t *val, int i);
int xgf_get_task_wake_cpu(struct task_struct *t);
int xgf_get_task_pid(struct task_struct *t);
long xgf_get_task_state(struct task_struct *t);
unsigned long xgf_lookup_name(const char *name);
void notify_xgf_ko_ready(void);
unsigned long long xgf_get_time(void);
struct xgf_dep *xgf_get_dep(pid_t tid, struct xgf_render *render,
	int pos, int force);
void xgf_clean_deps_list(struct xgf_render *render, int pos);
int xgf_hw_events_update(int rpid, struct xgf_render *render);

void fpsgo_ctrl2xgf_switch_xgf(int val);
void fpsgo_ctrl2xgf_nn_job_begin(unsigned int tid, unsigned long long mid);
int fpsgo_ctrl2xgf_nn_job_end(unsigned int tid, unsigned long long mid);

int fpsgo_comp2xgf_qudeq_notify(int rpid, int cmd,
	unsigned long long *run_time, unsigned long long *mid,
	unsigned long long ts);
void fpsgo_fstb2xgf_do_recycle(int fstb_active);
void fpsgo_create_render_dep(void);
int has_xgf_dep(pid_t tid);

int __init init_xgf(void);
#endif
