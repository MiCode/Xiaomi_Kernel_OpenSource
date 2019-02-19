// SPDX-License-Identifier: GPL-2.0
/* kernel/rwsem.c: R/W semaphores, public implementation
 *
 * Written by David Howells (dhowells@redhat.com).
 * Derived from asm-i386/semaphore.h
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <linux/export.h>
#include <linux/rwsem.h>
#include <linux/atomic.h>
#include <linux/jiffies.h>
#include <linux/sched/clock.h>

#include "rwsem.h"

static void rwsem_read_acquire_debug(struct rw_semaphore *sem)
{
	struct task_struct *task = current;
	int index = 0;
	int found = 0;
	unsigned long nanosec_rem;
	unsigned long timestamp;

	//pr_err("prateek read acquire: free index :%d sem:%p task:%p\n",task->fill_count, sem, task);
	for (index=0; index<NUM_DEBUG_ENTRIES; index++) {
		if (task->debug_info_rwsem[index].rwsem == sem
				&& rwsem_read_down == task->debug_info_rwsem[index].operation) {
			timestamp = task->debug_info_rwsem[index].timestamp;
			nanosec_rem = do_div(timestamp,1000000000);
			printk("sem:%p task:%p op:%d time: %lu.%06lu index:%d sem->count:%ld\n", sem, task, task->debug_info_rwsem[index].operation, timestamp, nanosec_rem, index, sem->count);
			//BUG_ON(1);
		}
	}

	for (index=0; index<NUM_DEBUG_ENTRIES; index++) {
		if (task->debug_info_rwsem[index].rwsem == NULL) {
			found = 1;
			break;
		}
	}

	if (index >= NUM_DEBUG_ENTRIES)
		BUG_ON(1);

	if (found) {
		task->debug_info_rwsem[index].operation = rwsem_read_down;
		task->debug_info_rwsem[index].rwsem = sem;
		task->debug_info_rwsem[index].task = task;
		task->debug_info_rwsem[index].timestamp = sched_clock();
		task->fill_count++;
	}

	return;
}

static void rwsem_read_release_debug(struct rw_semaphore *sem)
{
	struct task_struct *task = current;
	int index = 0;

	for (index=0; index<NUM_DEBUG_ENTRIES; index++) {
		if ((task->debug_info_rwsem[index].rwsem == sem) &&
				(task->debug_info_rwsem[index].operation == rwsem_read_down)) {
			//pr_err("prateek read release: free index :%d sem:%p task:%p fromdebugsem:%p  op:%d\n",task->fill_count, sem, task, task->debug_info_rwsem[index].rwsem, task->debug_info_rwsem[index].operation);
			task->debug_info_rwsem[index].operation = -1;
			task->debug_info_rwsem[index].rwsem = NULL;
			task->debug_info_rwsem[index].task = NULL;
			task->debug_info_rwsem[index].timestamp = 0;
			task->fill_count--;
			break;
		}
	}

	if (index >= NUM_DEBUG_ENTRIES)
		WARN_ON(1);

	return;
}

static void rwsem_write_acquire_debug(struct rw_semaphore *sem)
{
	struct task_struct *task = current;
	int index = 0;
	int found = 0;
	unsigned long nanosec_rem;
	unsigned long timestamp;

	//pr_err("prateek write acquire: free index :%d sem:%p task:%p\n",task->fill_count, sem, task);
	for (index=0; index<NUM_DEBUG_ENTRIES; index++) {
		if (task->debug_info_rwsem[index].rwsem == sem
				&& rwsem_write_down == task->debug_info_rwsem[index].operation) {
			timestamp = task->debug_info_rwsem[index].timestamp;
			nanosec_rem = do_div(timestamp,1000000000);
			printk("sem:%p task:%p  op:%d time: %lu.%06lu index:%d sem->count:%ld\n",sem, task, task->debug_info_rwsem[index].operation,timestamp,nanosec_rem, index, sem->count);
			BUG_ON(1);
		}
	}
	for (index=0; index<NUM_DEBUG_ENTRIES; index++) {
		if (task->debug_info_rwsem[index].rwsem == NULL) {
			found = 1;
			break;
		}
	}

	if (index >= NUM_DEBUG_ENTRIES)
		BUG_ON(1);

	if (found) {
		task->debug_info_rwsem[index].operation = rwsem_write_down;
		task->debug_info_rwsem[index].rwsem = sem;
		task->debug_info_rwsem[index].task = task;
		task->debug_info_rwsem[index].timestamp = sched_clock();
		task->fill_count++;
	}

	return;
}

static void rwsem_write_release_debug(struct rw_semaphore *sem)
{
	struct task_struct *task = current;
	int index = 0;

	for (index=0; index<NUM_DEBUG_ENTRIES; index++) {
		if ((task->debug_info_rwsem[index].rwsem == sem) &&
				(task->debug_info_rwsem[index].operation == rwsem_write_down)) {
			//pr_err("prateek write release: free index :%d sem:%p task:%p fromdebugsem:%p  op:%d\n",task->fill_count, sem, task, task->debug_info_rwsem[index].rwsem, task->debug_info_rwsem[index].operation);
			task->debug_info_rwsem[index].operation = -1;
			task->debug_info_rwsem[index].rwsem = NULL;
			task->debug_info_rwsem[index].task = NULL;
			task->debug_info_rwsem[index].timestamp = 0;
			task->fill_count--;
			break;
		}
	}

	if (index >= NUM_DEBUG_ENTRIES)
		WARN_ON(1);

	return;
}

/*
 * lock for reading
 */
void __sched down_read(struct rw_semaphore *sem)
{
	might_sleep();
	rwsem_acquire_read(&sem->dep_map, 0, 0, _RET_IP_);

	LOCK_CONTENDED(sem, __down_read_trylock, __down_read);
	rwsem_set_reader_owned(sem);
	rwsem_read_acquire_debug(sem);
}

EXPORT_SYMBOL(down_read);

/*
 * trylock for reading -- returns 1 if successful, 0 if contention
 */
int down_read_trylock(struct rw_semaphore *sem)
{
	int ret = __down_read_trylock(sem);

	if (ret == 1) {
		rwsem_acquire_read(&sem->dep_map, 0, 1, _RET_IP_);
		rwsem_set_reader_owned(sem);
		rwsem_read_acquire_debug(sem);
	}
	return ret;
}

EXPORT_SYMBOL(down_read_trylock);

/*
 * lock for writing
 */
void __sched down_write(struct rw_semaphore *sem)
{
	might_sleep();
	rwsem_acquire(&sem->dep_map, 0, 0, _RET_IP_);

	LOCK_CONTENDED(sem, __down_write_trylock, __down_write);
	rwsem_set_owner(sem);
	rwsem_write_acquire_debug(sem);
}

EXPORT_SYMBOL(down_write);

/*
 * lock for writing
 */
int __sched down_write_killable(struct rw_semaphore *sem)
{
	might_sleep();
	rwsem_acquire(&sem->dep_map, 0, 0, _RET_IP_);
	rwsem_write_acquire_debug(sem);

	if (LOCK_CONTENDED_RETURN(sem, __down_write_trylock, __down_write_killable)) {
		rwsem_release(&sem->dep_map, 1, _RET_IP_);
		rwsem_write_release_debug(sem);
		return -EINTR;
	}

	rwsem_set_owner(sem);
	return 0;
}

EXPORT_SYMBOL(down_write_killable);

/*
 * trylock for writing -- returns 1 if successful, 0 if contention
 */
int down_write_trylock(struct rw_semaphore *sem)
{
	int ret = __down_write_trylock(sem);

	if (ret == 1) {
		rwsem_acquire(&sem->dep_map, 0, 1, _RET_IP_);
		rwsem_set_owner(sem);
		rwsem_write_acquire_debug(sem);
	}

	return ret;
}

EXPORT_SYMBOL(down_write_trylock);

/*
 * release a read lock
 */
void up_read(struct rw_semaphore *sem)
{
	rwsem_release(&sem->dep_map, 1, _RET_IP_);

	__up_read(sem);
	rwsem_read_release_debug(sem);
}

EXPORT_SYMBOL(up_read);

/*
 * release a write lock
 */
void up_write(struct rw_semaphore *sem)
{
	rwsem_release(&sem->dep_map, 1, _RET_IP_);

	rwsem_clear_owner(sem);
	__up_write(sem);
	rwsem_write_release_debug(sem);
}

EXPORT_SYMBOL(up_write);

/*
 * downgrade write lock to read lock
 */
void downgrade_write(struct rw_semaphore *sem)
{
	lock_downgrade(&sem->dep_map, _RET_IP_);

	rwsem_set_reader_owned(sem);
	__downgrade_write(sem);
}

EXPORT_SYMBOL(downgrade_write);

#ifdef CONFIG_DEBUG_LOCK_ALLOC

void down_read_nested(struct rw_semaphore *sem, int subclass)
{
	might_sleep();
	rwsem_acquire_read(&sem->dep_map, subclass, 0, _RET_IP_);

	LOCK_CONTENDED(sem, __down_read_trylock, __down_read);
	rwsem_set_reader_owned(sem);
}

EXPORT_SYMBOL(down_read_nested);

void _down_write_nest_lock(struct rw_semaphore *sem, struct lockdep_map *nest)
{
	might_sleep();
	rwsem_acquire_nest(&sem->dep_map, 0, 0, nest, _RET_IP_);

	LOCK_CONTENDED(sem, __down_write_trylock, __down_write);
	rwsem_set_owner(sem);
}

EXPORT_SYMBOL(_down_write_nest_lock);

void down_read_non_owner(struct rw_semaphore *sem)
{
	might_sleep();

	__down_read(sem);
}

EXPORT_SYMBOL(down_read_non_owner);

void down_write_nested(struct rw_semaphore *sem, int subclass)
{
	might_sleep();
	rwsem_acquire(&sem->dep_map, subclass, 0, _RET_IP_);

	LOCK_CONTENDED(sem, __down_write_trylock, __down_write);
	rwsem_set_owner(sem);
}

EXPORT_SYMBOL(down_write_nested);

int __sched down_write_killable_nested(struct rw_semaphore *sem, int subclass)
{
	might_sleep();
	rwsem_acquire(&sem->dep_map, subclass, 0, _RET_IP_);

	if (LOCK_CONTENDED_RETURN(sem, __down_write_trylock, __down_write_killable)) {
		rwsem_release(&sem->dep_map, 1, _RET_IP_);
		return -EINTR;
	}

	rwsem_set_owner(sem);
	return 0;
}

EXPORT_SYMBOL(down_write_killable_nested);

void up_read_non_owner(struct rw_semaphore *sem)
{
	__up_read(sem);
}

EXPORT_SYMBOL(up_read_non_owner);

#endif
