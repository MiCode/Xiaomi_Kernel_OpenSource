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

enum XGF_ERROR {
	XGF_NOTIFY_OK,
	XGF_SLPTIME_OK,
	XGF_DISABLE,
	XGF_THREAD_NOT_FOUND,
	XGF_PARAM_ERR
};

enum {
	XGF_QUEUE_START = 0,
	XGF_QUEUE_END,
	XGF_DEQUEUE_START,
	XGF_DEQUEUE_END
};

struct xgf_tick {
	unsigned long long ts; /* timestamp */
	unsigned long long runtime;
};

struct xgf_intvl {
	struct xgf_tick *start, *end;
};

struct xgf_sect {
	struct hlist_node hlist;
	struct xgf_intvl un;
};

struct xgf_proc {
	struct hlist_node hlist;
	pid_t parent;
	pid_t render;
	struct hlist_head timer_head;
	struct rb_root timer_rec;

	struct xgf_tick queue;
	struct xgf_tick deque;

	unsigned long long slptime;
	unsigned long long quetime;
	unsigned long long deqtime;

	struct rb_root deps_rec;
	int render_thread_called_count;
	int dep_timer_count;
};

struct xgf_timer {
	struct hlist_node hlist;
	struct rb_node rb_node;
	const void *hrtimer;
	struct xgf_tick fire;
	struct xgf_tick expire;
	union {
		int expired;
		int blacked;
	};
};

struct xgf_deps {
	struct rb_node rb_node;
	pid_t tid;
	int render_pre_count;
	int render_count;
	int render_dep;
	int render_dep_deep;
	int has_timer;

	struct xgf_tick queue;
	struct xgf_tick deque;

	unsigned long long slptime;
	unsigned long long quetime;
	unsigned long long deqtime;
	unsigned long long has_timer_renew_ts;
};

struct render_dep {
	pid_t currentpid;
	pid_t currenttgid;
	pid_t becalledpid;
	pid_t becalledtgid;
};

extern int (*xgf_est_slptime_fp)(struct xgf_proc *proc,
		unsigned long long *slptime, struct xgf_tick *ref,
		struct xgf_tick *now, pid_t r_pid);

void xgf_lockprove(const char *tag);
int xgf_est_slptime(struct xgf_proc *proc, unsigned long long *slptime,
		    struct xgf_tick *ref, struct xgf_tick *now, pid_t r_pid);
void xgf_trace(const char *fmt, ...);
void xgf_reset_render(struct xgf_proc *proc);


void *xgf_kzalloc(size_t size);
void xgf_kfree(const void *block);

void fpsgo_ctrl2xgf_switch_xgf(int val);
int fpsgo_comp2xgf_qudeq_notify(int rpid, int cmd,
		unsigned long long *sleep_time);
void fpsgo_fstb2xgf_do_recycle(int fstb_active);
void fpsgo_create_render_dep(void);

int has_xgf_dep(pid_t tid);

int __init init_xgf(void);

#endif
