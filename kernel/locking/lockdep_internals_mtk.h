/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#undef MAX_LOCKDEP_CHAINS_BITS
#define MAX_LOCKDEP_CHAINS_BITS	17

#ifdef CONFIG_MTK_LOCKING_AEE
#include <mt-plat/aee.h>
bool is_critical_lock_held(void);
static const char * const critical_lock_list[] = {
	/* workqueue */
	"&(&pool->lock)->rlock",
	/* kmalloc */
	"&(&n->list_lock)->rlock",
	"&(&zone->lock)->rlock",
	/* stacktrace */
	"depot_lock",
	/* console */
	"&(&port->lock)->rlock"
};
#endif

#ifdef MTK_INTERNAL_SPINLOCK /* for spinlock_debug.c */
static void spin_aee(const char *msg, raw_spinlock_t *lock);
static inline void debug_spin_lock_after(raw_spinlock_t *lock);
static inline void debug_spin_unlock(raw_spinlock_t *lock);
#endif

#ifdef MTK_INTERNAL_LCOKDEP /* for lockdep.c */
#include <asm/stacktrace.h>
#include <kernel/sched/sched.h>

#ifdef CONFIG_MTK_BOOT
#include <mt-plat/mtk_boot_common.h>
#endif
#ifdef CONFIG_MTK_AEE_IPANIC
#include <mt-plat/mboot_params.h>
#endif

static void lockdep_aee(void);

#ifdef CONFIG_PROVE_LOCKING /* MTK_LOCK_LOG_TO_FTRACE */
static noinline int trace_circular_bug(struct lock_list *this,
				       struct lock_list *target,
				       struct held_lock *check_src,
				       struct held_lock *check_tgt,
				       struct stack_trace *trace);
static bool is_log_lock_held(struct task_struct *curr);
#endif
#ifdef MTK_LOCK_DEBUG_NEW_DEPENDENCY
static void check_new_dependency(struct held_lock *prev,
				 struct held_lock *next);
#endif
#ifdef MTK_LOCK_DEBUG_HELD_LOCK
static void lockdep_print_held_locks(struct task_struct *p);
static void held_lock_save_trace(struct stack_trace *trace,
				 unsigned long *entries);
#endif
static unsigned int lock_mon_enable;
#ifdef MTK_LOCK_MONITOR
static void __add_held_lock(struct lockdep_map *lock, unsigned int subclass,
			    int trylock, int read, int check, int hardirqs_off,
			    struct lockdep_map *nest_lock, unsigned long ip,
			    int references, int pin_count);
static void __del_held_lock(struct lockdep_map *lock);
#endif
#endif
