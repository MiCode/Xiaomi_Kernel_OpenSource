/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2024 Xiaomi. All rights reserved.
 *
 * This header file is based on workqueue.c from the Linux kernel version 6.6.
 * Do not use this header file with other kernel versions.
 */

#ifndef _MI_WQ_DYNAMIC_PRIORITY_H_
#define _MI_WQ_DYNAMIC_PRIORITY_H_

enum {
	/*
	 * worker_pool flags
	 *
	 * A bound pool is either associated or disassociated with its CPU.
	 * While associated (!DISASSOCIATED), all workers are bound to the
	 * CPU and none has %WORKER_UNBOUND set and concurrency management
	 * is in effect.
	 *
	 * While DISASSOCIATED, the cpu may be offline and all workers have
	 * %WORKER_UNBOUND set and concurrency management disabled, and may
	 * be executing on any CPU.  The pool behaves as an unbound one.
	 *
	 * Note that DISASSOCIATED should be flipped only while holding
	 * wq_pool_attach_mutex to avoid changing binding state while
	 * worker_attach_to_pool() is in progress.
	 */
	POOL_MANAGER_ACTIVE	= 1 << 0,	/* being managed */
	POOL_DISASSOCIATED	= 1 << 2,	/* cpu can't serve workers */

	/* worker flags */
	WORKER_DIE		= 1 << 1,	/* die die die */
	WORKER_IDLE		= 1 << 2,	/* is idle */
	WORKER_PREP		= 1 << 3,	/* preparing to run works */
	WORKER_CPU_INTENSIVE	= 1 << 6,	/* cpu intensive */
	WORKER_UNBOUND		= 1 << 7,	/* worker is unbound */
	WORKER_REBOUND		= 1 << 8,	/* worker was rebound */

	WORKER_NOT_RUNNING	= WORKER_PREP | WORKER_CPU_INTENSIVE |
				  WORKER_UNBOUND | WORKER_REBOUND,

	NR_STD_WORKER_POOLS	= 2,		/* # standard pools per cpu */

	UNBOUND_POOL_HASH_ORDER	= 6,		/* hashed by pool->attrs */
	BUSY_WORKER_HASH_ORDER	= 6,		/* 64 pointers */

	MAX_IDLE_WORKERS_RATIO	= 4,		/* 1/4 of busy can be idle */
	IDLE_WORKER_TIMEOUT	= 300 * HZ,	/* keep idle ones for 5 mins */

	MAYDAY_INITIAL_TIMEOUT  = HZ / 100 >= 2 ? HZ / 100 : 2,
						/* call for help after 10ms
						   (min two ticks) */
	MAYDAY_INTERVAL		= HZ / 10,	/* and then every 100ms */
	CREATE_COOLDOWN		= HZ,		/* time to breath after fail */

	/*
	 * Rescue workers are used only on emergencies and shared by
	 * all cpus.  Give MIN_NICE.
	 */
	RESCUER_NICE_LEVEL	= MIN_NICE,
	HIGHPRI_NICE_LEVEL	= MIN_NICE,

	WQ_NAME_LEN		= 24,
};

/*
 * The externally visible workqueue.  It relays the issued work items to
 * the appropriate worker_pool through its pool_workqueues.
 */
struct workqueue_struct {
	struct list_head	pwqs;		/* WR: all pwqs of this wq */
	struct list_head	list;		/* PR: list of all workqueues */

	struct mutex		mutex;		/* protects this wq */
	int			work_color;	/* WQ: current work color */
	int			flush_color;	/* WQ: current flush color */
	atomic_t		nr_pwqs_to_flush; /* flush in progress */
	struct wq_flusher	*first_flusher;	/* WQ: first flusher */
	struct list_head	flusher_queue;	/* WQ: flush waiters */
	struct list_head	flusher_overflow; /* WQ: flush overflow list */

	struct list_head	maydays;	/* MD: pwqs requesting rescue */
	struct worker		*rescuer;	/* MD: rescue worker */

	int			nr_drainers;	/* WQ: drain in progress */
	int			saved_max_active; /* WQ: saved pwq max_active */

	struct workqueue_attrs	*unbound_attrs;	/* PW: only for unbound wqs */
	struct pool_workqueue	*dfl_pwq;	/* PW: only for unbound wqs */

#ifdef CONFIG_SYSFS
	struct wq_device	*wq_dev;	/* I: for sysfs interface */
#endif
#ifdef CONFIG_LOCKDEP
	char			*lock_name;
	struct lock_class_key	key;
	struct lockdep_map	lockdep_map;
#endif
	char			name[WQ_NAME_LEN]; /* I: workqueue name */

	/*
	 * Destruction of workqueue_struct is RCU protected to allow walking
	 * the workqueues list without grabbing wq_pool_mutex.
	 * This is used to dump all workqueues from sysrq.
	 */
	struct rcu_head		rcu;

	/* hot fields used during command issue, aligned to cacheline */
	unsigned int		flags ____cacheline_aligned; /* WQ: WQ_* flags */
	struct pool_workqueue __percpu __rcu **cpu_pwq; /* I: per-cpu pwqs */
};

#endif /* _MI_WQ_DYNAMIC_PRIORITY_H_ */

