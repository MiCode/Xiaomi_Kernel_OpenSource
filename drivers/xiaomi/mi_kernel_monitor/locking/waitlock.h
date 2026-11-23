// SPDX-License-Identifier: GPL-2.0+
/*
 * mi_locking will show kernel lock wait/hold time.
 *
 * Copyright (C) 2024 Xiaomi Ltd.
 */

#ifndef _WAITLOCK_H_
#define _WAITLOCK_H_

#include <linux/list.h>
#include <linux/mi_sched.h>
#include "locking_main.h"
#include "internal.h"

/* #define INCLUDE_UNUSE */
#define CONFIG_XM_INTERNAL_VERSION  1
////#define XIAOMI_FUTEX_MONITOR   1
////#define CONFIG_USE_HZ_STAT   1
#define LOCKSTAT_POINTS		        4
#define XXX_LOCK_USAGE_STATES		2
////#define LOCK_TRACE_STATES		    (2*4 + 2)
#define NR_LOCKDEP_CACHING_CLASSES	2
#define SHOW_STAT_BUF_SIZE          (3 * PAGE_SIZE)
/* use for waitlock sort */
/* 13 : 8192 */
#define MAX_LOCKDEP_KEYS_BITS	    13
#define MAX_LOCKDEP_KEYS		    (1UL << MAX_LOCKDEP_KEYS_BITS)
#define MAX_CPU_KEYS                8

#define REGISTER_HOOKS_HANDLE_RET(func_name, handler, data, err)	\
do {								\
	ret = func_name(handler, data);				\
	if (ret) {						\
		pr_err("[kern_lock_stat]:Failed to"		\
		"#func_name \n");				\
		goto err;					\
	}							\
} while (0)

#define DEFINE_PROC_CONTEXT(var_name, name_str, lock_type, cmp_fn_name) \
    struct proc_context var_name = {                               \
        .name = name_str,                                          \
        .type = lock_type,                                         \
        .cmp_fn = (cmp_fn_name),                                          \
    }

enum lock_type {
	MUTEX=0,
	RWSEM_READ,
	RWSEM_WRITE,
	FUTEX_ART,
	LOCK_TYPES,
};

struct track_stat {
	atomic_t	level[MAX_THRES];
	atomic_t	total_nr;                       /* total contended counts. */
	atomic64_t  max_time;
	atomic64_t	total_time;                     /* total contended time. */
	atomic64_t	exp_total_time;                 /* total time exceed low thres. */
};

struct lock_stats {
	struct track_stat       per_type_stat[LOCK_TYPES];
};

#define LOCK_TYPE_SHIFT (30)
#define INVALID_TYPE    (0)
#define LOCK_ART        (1)
#define LOCK_JUC        (2)

/*
 * There are many types os thread groups, For memory considerations,
 * divide multiple groups in 3 : UX/RT/OTHER.
 */
#define LIMIT_GRP_TYPES 		(GRP_OT + 1)
static __read_mostly u32 thres_array[LIMIT_GRP_TYPES][LOCK_TYPES][MAX_THRES]  = {
			/* UX */
			{
			{8 * NSEC_PER_MSEC, 15 * NSEC_PER_MSEC, 30 * NSEC_PER_MSEC},
			{8 * NSEC_PER_MSEC, 15 * NSEC_PER_MSEC, 30 * NSEC_PER_MSEC},
			 {8 * NSEC_PER_MSEC, 15 * NSEC_PER_MSEC, 30 * NSEC_PER_MSEC},
			{30 * NSEC_PER_MSEC, 50 * NSEC_PER_MSEC, 100 * NSEC_PER_MSEC},   ///futex_art
			},
			/* RT */
			{
			{2 * NSEC_PER_MSEC, 5 * NSEC_PER_MSEC, 20 * NSEC_PER_MSEC},
			{2 * NSEC_PER_MSEC, 5 * NSEC_PER_MSEC, 20 * NSEC_PER_MSEC},
			{2 * NSEC_PER_MSEC, 5 * NSEC_PER_MSEC, 20 * NSEC_PER_MSEC},  //2~1
			{2 * NSEC_PER_MSEC, 5 * NSEC_PER_MSEC, 20 * NSEC_PER_MSEC},  ///futex_art
			},
			/* OTHER */
			{
			{12 * NSEC_PER_MSEC, 25 * NSEC_PER_MSEC, 40 * NSEC_PER_MSEC},
			{12 * NSEC_PER_MSEC, 25 * NSEC_PER_MSEC, 50 * NSEC_PER_MSEC},
		    {12  * NSEC_PER_MSEC, 25 * NSEC_PER_MSEC, 50 * NSEC_PER_MSEC},  //12~1
			{80 * NSEC_PER_MSEC, 160 * NSEC_PER_MSEC, 300 * NSEC_PER_MSEC},  ///futex_art
			}
};

struct mi_lockdep_subclass_key {
	char __one_byte;
} __attribute__ ((__packed__));

struct mi_lockdep_map {
	const char					*name;
	unsigned long				key;
};

typedef int (*mi_lock_cmp_fn)(const struct mi_lockdep_map *a, const struct mi_lockdep_map *b);
typedef void (*mi_lock_print_fn)(const struct mi_lockdep_map *map);

struct mi_lock_class {
	/*
	 * class-hash:
	 */
	struct hlist_node		hash_entry;
	/*
	 * Entry in all_lock_classes when in use. Entry in free_lock_classes
	 * when not in use. Instances that are being freed are on one of the
	 * zapped_classes lists.
	 */
	struct list_head		lock_entry;
	/*
	 * These fields represent a directed graph of lock dependencies,
	 * to every node we attach a list of "forward" and a list of
	 * "backward" graph nodes.
	 */
	struct list_head		locks_after, locks_before;
	//const struct lockdep_subclass_key *key;
	mi_lock_print_fn			print_fn;
	/*
	 * Generation counter, when doing certain classes of graph walking,
	 * to ensure that we check one node only once:
	 */
	const char		*name;
	unsigned long	key;
	u8				lock_type;
} __no_randomize_layout;

struct mi_lock_time {
	s64				max;
	s64				total;
	s64 			avg;
	unsigned long 	wait_cnt;
	unsigned long 	rank;
	unsigned long	nr;
};

struct mi_lock_class_stats {
	struct mi_lock_time		mi_waittime;
};

struct proc_context {
    const char *name;
    int type;
    int (*cmp_fn)(const void *, const void *);
};

#define REGISTER_TRACE_VH(vender_hook, handler) \
	{ \
		ret = register_trace_##vender_hook(handler, NULL); \
		if (ret) { \
			pr_err("failed to register_trace_"#vender_hook", ret=%d\n", ret); \
			return ret; \
		} \
	}

#define UNREGISTER_TRACE_VH(vender_hook, handler) \
	{ \
		unregister_trace_##vender_hook(handler, NULL); \
	}

extern char *group_str[GRP_TYPES];
extern const struct proc_ops lock_stat_overview_fops;

extern struct proc_context mutex_max_ctx;
extern struct proc_context mutex_avg_ctx;
extern struct proc_context mutex_total_ctx;
extern struct proc_context mutex_cnt_ctx;
extern struct proc_context rwsemw_max_ctx;
extern struct proc_context rwsemw_avg_ctx;
extern struct proc_context rwsemw_total_ctx;
extern struct proc_context rwsemw_cnt_ctx;

int waittime_thres_exceed_type(int grp_idx, int type, u64 time);
int get_task_grp_idx(struct task_struct *tsk);
void handle_wait_delta(const char *name, u64 time, int type);
void handle_wait_stats(int type, u64 time);
#endif /* _WAITLOCK_H_ */
