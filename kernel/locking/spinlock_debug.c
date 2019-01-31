/*
 * Copyright 2005, Red Hat, Inc., Ingo Molnar
 * Released under the General Public License (GPL).
 *
 * This file contains the spinlock/rwlock implementations for
 * DEBUG_SPINLOCK.
 */
#define pr_fmt(fmt)	"spinlock_debug: " fmt

#include <linux/spinlock.h>
#include <linux/nmi.h>
#include <linux/interrupt.h>
#include <linux/debug_locks.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/printk.h>
#include <mt-plat/aee.h>

#ifdef CONFIG_MTK_SCHED_MONITOR
#include "mtk_sched_mon.h"
#endif

void __raw_spin_lock_init(raw_spinlock_t *lock, const char *name,
			  struct lock_class_key *key)
{
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	/*
	 * Make sure we are not reinitializing a held lock:
	 */
	debug_check_no_locks_freed((void *)lock, sizeof(*lock));
	lockdep_init_map(&lock->dep_map, name, key, 0);
#endif
	lock->raw_lock = (arch_spinlock_t)__ARCH_SPIN_LOCK_UNLOCKED;
	lock->magic = SPINLOCK_MAGIC;
	lock->owner = SPINLOCK_OWNER_INIT;
	lock->owner_cpu = -1;
}

EXPORT_SYMBOL(__raw_spin_lock_init);

void __rwlock_init(rwlock_t *lock, const char *name,
		   struct lock_class_key *key)
{
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	/*
	 * Make sure we are not reinitializing a held lock:
	 */
	debug_check_no_locks_freed((void *)lock, sizeof(*lock));
	lockdep_init_map(&lock->dep_map, name, key, 0);
#endif
	lock->raw_lock = (arch_rwlock_t) __ARCH_RW_LOCK_UNLOCKED;
	lock->magic = RWLOCK_MAGIC;
	lock->owner = SPINLOCK_OWNER_INIT;
	lock->owner_cpu = -1;
}

EXPORT_SYMBOL(__rwlock_init);

static void spin_dump(raw_spinlock_t *lock, const char *msg)
{
	struct task_struct *owner = NULL;

	if (lock->owner && lock->owner != SPINLOCK_OWNER_INIT)
		owner = lock->owner;
	pr_info("BUG: spinlock %s on CPU#%d, %s/%d\n",
		msg, raw_smp_processor_id(),
		current->comm, task_pid_nr(current));
	pr_info(" lock: %pS, .magic: %08x, .owner: %s/%d, "
			".owner_cpu: %d\n",
		lock, lock->magic,
		owner ? owner->comm : "<none>",
		owner ? task_pid_nr(owner) : -1,
		lock->owner_cpu);
	dump_stack();
}

static void spin_bug(raw_spinlock_t *lock, const char *msg)
{
	char aee_str[50];

	if (!debug_locks_off())
		return;

	spin_dump(lock, msg);
	snprintf(aee_str, 50, "%s: %s\n", current->comm, msg);
	if (!strcmp(msg, "bad magic") || !strcmp(msg, "already unlocked")
		|| !strcmp(msg, "wrong owner") || !strcmp(msg, "wrong CPU")) {
		pr_info("%s\n", aee_str);
		pr_info("maybe use an un-initial spin_lock or mem corrupt\n");
		pr_info("maybe already unlocked or wrong owner or wrong CPU\n");
		pr_info("maybe bad magic %08x, should be %08x\n",
			lock->magic, SPINLOCK_MAGIC);
		pr_info(">>>>>>>>>>>>>> Let's dump Kernel API <<<<<<<<<<<<<<\n");
	}
	aee_kernel_warning_api(__FILE__, __LINE__,
		DB_OPT_DUMMY_DUMP | DB_OPT_FTRACE,
		aee_str, "spinlock debugger\n");
}

#define SPIN_BUG_ON(cond, lock, msg) if (unlikely(cond)) spin_bug(lock, msg)

static inline void
debug_spin_lock_before(raw_spinlock_t *lock)
{
	SPIN_BUG_ON(lock->magic != SPINLOCK_MAGIC, lock, "bad magic");
	SPIN_BUG_ON(lock->owner == current, lock, "recursion");
	SPIN_BUG_ON(lock->owner_cpu == raw_smp_processor_id(),
							lock, "cpu recursion");
}

static inline void debug_spin_lock_after(raw_spinlock_t *lock)
{
	lock->owner_cpu = raw_smp_processor_id();
	lock->owner = current;
}

static inline void debug_spin_unlock(raw_spinlock_t *lock)
{
	SPIN_BUG_ON(lock->magic != SPINLOCK_MAGIC, lock, "bad magic");
	SPIN_BUG_ON(!raw_spin_is_locked(lock), lock, "already unlocked");
	SPIN_BUG_ON(lock->owner != current, lock, "wrong owner");
	SPIN_BUG_ON(lock->owner_cpu != raw_smp_processor_id(),
							lock, "wrong CPU");
	lock->owner = SPINLOCK_OWNER_INIT;
	lock->owner_cpu = -1;
}
#ifdef CONFIG_MTK_LOCK_DEBUG
static void show_cpu_backtrace(void *ignored)
{
	pr_info("========== The call trace of lock owner on CPU%d ==========\n",
		smp_processor_id());
	show_stack(NULL, NULL);
}
#endif

/*Select appropriate loop counts to 1~2sec*/
#if HZ == 100
#define LOOP_HZ 100 /* temp 10 */
#elif HZ == 10
#define LOOP_HZ 2 /* temp 2 */
#else
#define LOOP_HZ HZ
#endif
#define WARNING_TIME 1000000000		/* warning time 1 seconds */

static void __spin_lock_debug(raw_spinlock_t *lock)
{
#ifdef CONFIG_MTK_LOCK_DEBUG
	u64 i;
	u64 loops = loops_per_jiffy * LOOP_HZ;
	int print_once = 1;
	char aee_str[50];
	unsigned long long t1, t2, t3;
	struct task_struct *owner = NULL;

	if (is_logbuf_lock(lock)) { /* ignore to debug logbuf_lock */
		for (i = 0; i < loops; i++) {
			if (arch_spin_trylock(&lock->raw_lock))
				return;
			__delay(1);
		}
		return;
	}

#ifdef CONFIG_PREEMPT_MONITOR
	MT_trace_spin_lock_start(lock);
#endif

	t1 = sched_clock();
	t2 = t1;

	for (;;) {
		for (i = 0; i < loops; i++) {
			if (arch_spin_trylock(&lock->raw_lock)) {
#ifdef CONFIG_PREEMPT_MONITOR
				MT_trace_spin_lock_end(lock);
#endif
				return;
			}
			__delay(1);
		}
		t3 = sched_clock();
		if (t3 < t2)
			continue;
		else if (t3 - t2 < WARNING_TIME)
			continue;
		/* if(sched_clock() - t2 < WARNING_TIME) continue; */
		t2 = sched_clock();

		/* lockup suspected: */
		if (lock->owner && lock->owner != SPINLOCK_OWNER_INIT)
			owner = lock->owner;

		pr_info("(%ps) spin time: %llu ns(from %llu ns), raw_lock: 0x%08x, lock is held by %s/%d on CPU#%d\n",
		lock,
		sched_clock() - t1, t1,
		*((unsigned int *)&lock->raw_lock),
		owner ? owner->comm : "<none>",
		owner ? task_pid_nr(owner) : -1,
		lock->owner_cpu);

		if (oops_in_progress != 0)
			/* in exception follow, printk maybe spinlock error */
			continue;

		if (print_once) {
			print_once = 0;
			pr_info("(%ps) magic: %08x, owner: %s/%d, owner_cpu: %d\n",
				lock, lock->magic,
				owner ? owner->comm : "<none>",
				owner ? task_pid_nr(owner) : -1,
				lock->owner_cpu);
			pr_info("========== The call trace of spinning task ==========\n");
			dump_stack();
			if (owner) {
				pr_info("spinlock debug show lock owenr [%s/%d] info\n",
				owner->comm, owner->pid);
				smp_call_function_single(lock->owner_cpu,
					show_cpu_backtrace, NULL, 0);
				if (debug_locks)
					debug_show_held_locks(owner);
			}

			/* ensure debug_locks is true,then can call aee */
			if (debug_locks) {
				debug_show_all_locks();
				snprintf(aee_str, 50,
					"Spinlock lockup: %ps in %s\n",
					lock, current->comm);
				#if defined(CONFIG_MTK_AEE_FEATURE)
				aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_DUMMY_DUMP | DB_OPT_FTRACE,
					aee_str, "spinlock debugger\n");
				#endif
			}
		}
	}
#else /* CONFIG_MTK_LOCK_DEBUG*/
	u64 i;
	u64 loops = loops_per_jiffy * HZ;

	for (i = 0; i < loops; i++) {
		if (arch_spin_trylock(&lock->raw_lock))
			return;
		__delay(1);
	}
	/* lockup suspected: */
	spin_dump(lock, "lockup suspected");
#ifdef CONFIG_SMP
	trigger_all_cpu_backtrace();
#endif

	/*
	 * The trylock above was causing a livelock.  Give the lower level arch
	 * specific lock code a chance to acquire the lock. We have already
	 * printed a warning/backtrace at this point. The non-debug arch
	 * specific code might actually succeed in acquiring the lock.  If it is
	 * not successful, the end-result is the same - there is no forward
	 * progress.
	 */
	arch_spin_lock(&lock->raw_lock);
#endif /* CONFIG_MTK_LOCK_DEBUG */
}

/*
 * We are now relying on the NMI watchdog to detect lockup instead of doing
 * the detection here with an unfair lock which can cause problem of its own.
 */
void do_raw_spin_lock(raw_spinlock_t *lock)
{
	debug_spin_lock_before(lock);
	if (unlikely(!arch_spin_trylock(&lock->raw_lock)))
		__spin_lock_debug(lock);
	debug_spin_lock_after(lock);
}

int do_raw_spin_trylock(raw_spinlock_t *lock)
{
	int ret = arch_spin_trylock(&lock->raw_lock);

	if (ret)
		debug_spin_lock_after(lock);
#ifndef CONFIG_SMP
	/*
	 * Must not happen on UP:
	 */
	SPIN_BUG_ON(!ret, lock, "trylock failure on UP");
#endif
	return ret;
}

void do_raw_spin_unlock(raw_spinlock_t *lock)
{
	debug_spin_unlock(lock);
	arch_spin_unlock(&lock->raw_lock);
}

static void rwlock_bug(rwlock_t *lock, const char *msg)
{
	if (!debug_locks_off())
		return;

	pr_info("BUG: rwlock %s on CPU#%d, %s/%d, %p\n",
		msg, raw_smp_processor_id(), current->comm,
		task_pid_nr(current), lock);
	dump_stack();
}

#define RWLOCK_BUG_ON(cond, lock, msg) if (unlikely(cond)) rwlock_bug(lock, msg)

void do_raw_read_lock(rwlock_t *lock)
{
	RWLOCK_BUG_ON(lock->magic != RWLOCK_MAGIC, lock, "bad magic");
	arch_read_lock(&lock->raw_lock);
}

int do_raw_read_trylock(rwlock_t *lock)
{
	int ret = arch_read_trylock(&lock->raw_lock);

#ifndef CONFIG_SMP
	/*
	 * Must not happen on UP:
	 */
	RWLOCK_BUG_ON(!ret, lock, "trylock failure on UP");
#endif
	return ret;
}

void do_raw_read_unlock(rwlock_t *lock)
{
	RWLOCK_BUG_ON(lock->magic != RWLOCK_MAGIC, lock, "bad magic");
	arch_read_unlock(&lock->raw_lock);
}

static inline void debug_write_lock_before(rwlock_t *lock)
{
	RWLOCK_BUG_ON(lock->magic != RWLOCK_MAGIC, lock, "bad magic");
	RWLOCK_BUG_ON(lock->owner == current, lock, "recursion");
	RWLOCK_BUG_ON(lock->owner_cpu == raw_smp_processor_id(),
							lock, "cpu recursion");
}

static inline void debug_write_lock_after(rwlock_t *lock)
{
	lock->owner_cpu = raw_smp_processor_id();
	lock->owner = current;
}

static inline void debug_write_unlock(rwlock_t *lock)
{
	RWLOCK_BUG_ON(lock->magic != RWLOCK_MAGIC, lock, "bad magic");
	RWLOCK_BUG_ON(lock->owner != current, lock, "wrong owner");
	RWLOCK_BUG_ON(lock->owner_cpu != raw_smp_processor_id(),
							lock, "wrong CPU");
	lock->owner = SPINLOCK_OWNER_INIT;
	lock->owner_cpu = -1;
}

void do_raw_write_lock(rwlock_t *lock)
{
	debug_write_lock_before(lock);
	arch_write_lock(&lock->raw_lock);
	debug_write_lock_after(lock);
}

int do_raw_write_trylock(rwlock_t *lock)
{
	int ret = arch_write_trylock(&lock->raw_lock);

	if (ret)
		debug_write_lock_after(lock);
#ifndef CONFIG_SMP
	/*
	 * Must not happen on UP:
	 */
	RWLOCK_BUG_ON(!ret, lock, "trylock failure on UP");
#endif
	return ret;
}

void do_raw_write_unlock(rwlock_t *lock)
{
	debug_write_unlock(lock);
	arch_write_unlock(&lock->raw_lock);
}
