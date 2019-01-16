/*
 * Mutexes: blocking mutual exclusion locks
 *
 * started by Ingo Molnar:
 *
 *  Copyright (C) 2004, 2005, 2006 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 *
 * This file contains mutex debugging related internal declarations,
 * prototypes and inline functions, for the CONFIG_DEBUG_MUTEXES case.
 * More details are in kernel/mutex-debug.c.
 */

/*
 * This must be called with lock->wait_lock held.
 */
extern void debug_mutex_lock_common(struct mutex *lock,
				    struct mutex_waiter *waiter);
extern void debug_mutex_wake_waiter(struct mutex *lock,
				    struct mutex_waiter *waiter);
extern void debug_mutex_free_waiter(struct mutex_waiter *waiter);
extern void debug_mutex_add_waiter(struct mutex *lock,
				   struct mutex_waiter *waiter,
				   struct thread_info *ti);
extern void mutex_remove_waiter(struct mutex *lock, struct mutex_waiter *waiter,
				struct thread_info *ti);
extern void debug_mutex_unlock(struct mutex *lock);
extern void debug_mutex_init(struct mutex *lock, const char *name,
			     struct lock_class_key *key);

static inline void mutex_set_owner(struct mutex *lock)
{
	lock->owner = current;
}

static inline void mutex_clear_owner(struct mutex *lock)
{
	lock->owner = NULL;
}
#include <linux/aee.h>
#define MT_DEBUG_LOCKS_WARN_ON(c)                       \
({                                  \
    int __ret = 0;                          \
                                    \
    if (!oops_in_progress && unlikely(c)) {             \
        aee_kernel_warning(#c,"Mutex Debug\n");\
        if (debug_locks_off() && !debug_locks_silent)       \
            WARN_ON(1);                 \
        else \
            dump_stack();\
        __ret = 1;                      \
    }                               \
    __ret;                              \
})

#ifdef DEBUG_LOCKS_WARN_ON
#undef DEBUG_LOCKS_WARN_ON
#define DEBUG_LOCKS_WARN_ON(c) MT_DEBUG_LOCKS_WARN_ON(c)
#endif

#define spin_lock_mutex(lock, flags)			\
	do {						\
		struct mutex *l = container_of(lock, struct mutex, wait_lock); \
							\
        if(DEBUG_LOCKS_WARN_ON(in_interrupt())){    \
            printk("[MUTEX WARN!!] mutex lock in interrupt context!\n");\
        }\
		local_irq_save(flags);			\
		arch_spin_lock(&(lock)->rlock.raw_lock);\
		DEBUG_LOCKS_WARN_ON(l->magic != l);	\
	} while (0)

#define spin_unlock_mutex(lock, flags)				\
	do {							\
		arch_spin_unlock(&(lock)->rlock.raw_lock);	\
		local_irq_restore(flags);			\
		preempt_check_resched();			\
	} while (0)
