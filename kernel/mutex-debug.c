/*
 * kernel/mutex-debug.c
 *
 * Debugging code for mutexes
 *
 * Started by Ingo Molnar:
 *
 *  Copyright (C) 2004, 2005, 2006 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 *
 * lock debugging, locking tree, deadlock detection started by:
 *
 *  Copyright (C) 2004, LynuxWorks, Inc., Igor Manyilov, Bill Huey
 *  Released under the General Public License (GPL).
 */
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/poison.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/kallsyms.h>
#include <linux/interrupt.h>
#include <linux/debug_locks.h>

#include "mutex-debug.h"

/*
 * Must be called with lock->wait_lock held.
 */
void debug_mutex_lock_common(struct mutex *lock, struct mutex_waiter *waiter)
{
	memset(waiter, MUTEX_DEBUG_INIT, sizeof(*waiter));
	waiter->magic = waiter;
	INIT_LIST_HEAD(&waiter->list);
}

void debug_mutex_wake_waiter(struct mutex *lock, struct mutex_waiter *waiter)
{
	SMP_DEBUG_LOCKS_WARN_ON(!spin_is_locked(&lock->wait_lock));
    if(DEBUG_LOCKS_WARN_ON(list_empty(&lock->wait_list))){
        printk("[MUTEX WARN!!]\n");
    }
    if(DEBUG_LOCKS_WARN_ON(waiter->magic != waiter)){
        printk("[MUTEX WARN!!] bad magic number %p:%p\n", waiter->magic, waiter);
    }
    if(DEBUG_LOCKS_WARN_ON(list_empty(&waiter->list))){
        printk("[MUTEX WARN!!] empt waiter list\n");
    }
}

void debug_mutex_free_waiter(struct mutex_waiter *waiter)
{
	DEBUG_LOCKS_WARN_ON(!list_empty(&waiter->list));
	memset(waiter, MUTEX_DEBUG_FREE, sizeof(*waiter));
}

void debug_mutex_add_waiter(struct mutex *lock, struct mutex_waiter *waiter,
			    struct thread_info *ti)
{
	SMP_DEBUG_LOCKS_WARN_ON(!spin_is_locked(&lock->wait_lock));

	/* Mark the current thread as blocked on the lock: */
	ti->task->blocked_on = waiter;
#ifdef CONFIG_MT_DEBUG_MUTEXES
    waiter->task_wait_on = lock->owner;
#endif
}

void mutex_remove_waiter(struct mutex *lock, struct mutex_waiter *waiter,
			 struct thread_info *ti)
{
    if(DEBUG_LOCKS_WARN_ON(list_empty(&waiter->list))){
        printk("[MUTEX WARN!!] empty waiter list\n");
    }
    if(DEBUG_LOCKS_WARN_ON(waiter->task != ti->task)){
        printk("[MUTEX WARN!!] waiter task is not the same![%d:%s] != [%d:%s]\n", waiter->task->pid, waiter->task->comm, ti->task->pid, ti->task->comm);
    }
    if(DEBUG_LOCKS_WARN_ON(ti->task->blocked_on != waiter)){
        printk("[MUTEX WARN!!] blocked on different waiter\n");
    }
	ti->task->blocked_on = NULL;

	list_del_init(&waiter->list);
	waiter->task = NULL;
#ifdef CONFIG_MT_DEBUG_MUTEXES
    waiter->task_wait_on = NULL;
#endif
}

void debug_mutex_unlock(struct mutex *lock)
{
//	if (unlikely(!debug_locks))
//		return;

    if(DEBUG_LOCKS_WARN_ON(lock->magic != lock)){
        printk("[MUTEX WARN!!] bad lock magic:%p\n", lock->magic);
    }
    if(DEBUG_LOCKS_WARN_ON(lock->owner != current)){
        if(lock->owner != NULL){
            printk("[MUTEX WARN!!] releasing mutex which is hold by another process, %p\n", lock->owner);
            printk("[MUTEX WARN!!] current process[%d:%s] is trying to release lock\n But it should be released by lock owner-process[%d:%s]\n", current->pid, current->comm, lock->owner->pid, lock->owner->comm);
        }else
            printk("\n[MUTEX WARN!!] imbalanced unlock\n");
    }
    if(DEBUG_LOCKS_WARN_ON(!lock->wait_list.prev && !lock->wait_list.next)){
        printk("[MUTEX WARN!!] wait_list both empty in prev and next \n");
    }
	mutex_clear_owner(lock);
}

void debug_mutex_init(struct mutex *lock, const char *name,
		      struct lock_class_key *key)
{
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	/*
	 * Make sure we are not reinitializing a held lock:
	 */
	debug_check_no_locks_freed((void *)lock, sizeof(*lock));
	lockdep_init_map(&lock->dep_map, name, key, 0);
#endif
#ifdef CONFIG_DEBUG_MUTEXES
    lock->name = name;
#endif
	lock->magic = lock;
}

/***
 * mutex_destroy - mark a mutex unusable
 * @lock: the mutex to be destroyed
 *
 * This function marks the mutex uninitialized, and any subsequent
 * use of the mutex is forbidden. The mutex must not be locked when
 * this function is called.
 */
void mutex_destroy(struct mutex *lock)
{
	DEBUG_LOCKS_WARN_ON(mutex_is_locked(lock));
	lock->magic = NULL;
}

EXPORT_SYMBOL_GPL(mutex_destroy);
