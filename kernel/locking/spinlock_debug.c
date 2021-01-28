/*
 * Copyright 2005, Red Hat, Inc., Ingo Molnar
 * Released under the General Public License (GPL).
 *
 * This file contains the spinlock/rwlock implementations for
 * DEBUG_SPINLOCK.
 */

#include <linux/spinlock.h>
#include <linux/nmi.h>
#include <linux/interrupt.h>
#include <linux/debug_locks.h>
#include <linux/delay.h>
#include <linux/export.h>

#define MTK_INTERNAL_SPINLOCK
#include "lockdep_internals_mtk.h"

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
	printk(KERN_EMERG "BUG: spinlock %s on CPU#%d, %s/%d\n",
		msg, raw_smp_processor_id(),
		current->comm, task_pid_nr(current));
	printk(KERN_EMERG " lock: %pS, .magic: %08x, .owner: %s/%d, "
			".owner_cpu: %d\n",
		lock, lock->magic,
		owner ? owner->comm : "<none>",
		owner ? task_pid_nr(owner) : -1,
		lock->owner_cpu);
	dump_stack();
}

static void spin_bug(raw_spinlock_t *lock, const char *msg)
{
	if (!debug_locks_off())
		return;

	spin_dump(lock, msg);
	spin_aee(msg, lock);
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

#if !defined(MTK_DEBUG_SPINLOCK_V1) && !defined(MTK_DEBUG_SPINLOCK_V2)
static inline void debug_spin_lock_after(raw_spinlock_t *lock)
{
	WRITE_ONCE(lock->owner_cpu, raw_smp_processor_id());
	WRITE_ONCE(lock->owner, current);
}

static inline void debug_spin_unlock(raw_spinlock_t *lock)
{
	SPIN_BUG_ON(lock->magic != SPINLOCK_MAGIC, lock, "bad magic");
	SPIN_BUG_ON(!raw_spin_is_locked(lock), lock, "already unlocked");
	SPIN_BUG_ON(lock->owner != current, lock, "wrong owner");
	SPIN_BUG_ON(lock->owner_cpu != raw_smp_processor_id(),
							lock, "wrong CPU");
	WRITE_ONCE(lock->owner, SPINLOCK_OWNER_INIT);
	WRITE_ONCE(lock->owner_cpu, -1);
}
#endif

/*
 * We are now relying on the NMI watchdog to detect lockup instead of doing
 * the detection here with an unfair lock which can cause problem of its own.
 */
#if !defined(MTK_DEBUG_SPINLOCK_V1) && !defined(MTK_DEBUG_SPINLOCK_V2)
void do_raw_spin_lock(raw_spinlock_t *lock)
{
	debug_spin_lock_before(lock);
	arch_spin_lock(&lock->raw_lock);
	debug_spin_lock_after(lock);
}
#endif

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

	printk(KERN_EMERG "BUG: rwlock %s on CPU#%d, %s/%d, %p\n",
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

#if defined(MTK_DEBUG_SPINLOCK_V1) || defined(MTK_DEBUG_SPINLOCK_V2)
#include <linux/sched/clock.h>
#include <linux/sched/debug.h>
#define MAX_LOCK_NAME 128
#define WARNING_TIME 1000000000 /* 1 seconds */

static long long msec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000);
		return -nsec;
	}
	do_div(nsec, 1000000);

	return nsec;
}

static long long sec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000000);
		return -nsec;
	}
	do_div(nsec, 1000000000);

	return nsec;
}

static unsigned long sec_low(unsigned long long nsec)
{
	if ((long long)nsec < 0)
		nsec = -nsec;
	/* exclude part of nsec */
	return do_div(nsec, 1000000000) / 1000;
}

static void get_spin_lock_name(raw_spinlock_t *lock, char *name)
{
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	snprintf(name, MAX_LOCK_NAME, "%s", lock->dep_map.name);
#else
	snprintf(name, MAX_LOCK_NAME, "%ps", lock);
#endif
}

static inline void debug_spin_lock_after(raw_spinlock_t *lock)
{
	WRITE_ONCE(lock->lock_t, sched_clock());
	WRITE_ONCE(lock->owner_cpu, raw_smp_processor_id());
	WRITE_ONCE(lock->owner, current);
}

#ifdef MTK_DEBUG_SPINLOCK_V2
static void spin_lock_check_holding_time(raw_spinlock_t *lock);
#endif
static inline void debug_spin_unlock(raw_spinlock_t *lock)
{
	SPIN_BUG_ON(lock->magic != SPINLOCK_MAGIC, lock, "bad magic");
	SPIN_BUG_ON(!raw_spin_is_locked(lock), lock, "already unlocked");
	SPIN_BUG_ON(lock->owner != current, lock, "wrong owner");
	SPIN_BUG_ON(lock->owner_cpu != raw_smp_processor_id(),
		    lock, "wrong CPU");
	WRITE_ONCE(lock->owner, SPINLOCK_OWNER_INIT);
	WRITE_ONCE(lock->owner_cpu, -1);
	WRITE_ONCE(lock->unlock_t, sched_clock());
#ifdef MTK_DEBUG_SPINLOCK_V2
	spin_lock_check_holding_time(lock);
#endif
}
#endif /* MTK_DEBUG_SPINLOCK_V1 || MTK_DEBUG_SPINLOCK_V2 */

#ifdef MTK_DEBUG_SPINLOCK_V1
static DEFINE_PER_CPU(call_single_data_t, spinlock_debug_csd);

struct spinlock_debug_info {
	int detector_cpu;
	raw_spinlock_t lock;
};

static DEFINE_PER_CPU(struct spinlock_debug_info, sp_dbg) = {
	-1, __RAW_SPIN_LOCK_UNLOCKED(sp_dbg.lock) };

static void show_cpu_backtrace(void *info)
{
	call_single_data_t *csd;

	pr_info("========== The call trace of lock owner on CPU%d ==========\n",
		raw_smp_processor_id());
	dump_stack();

#ifdef CONFIG_MTK_LOCKING_AEE
	do {
		raw_spinlock_t *lock = (raw_spinlock_t *)info;
		char name[MAX_LOCK_NAME];
		char msg[64];

		get_spin_lock_name(lock, name);
		snprintf(msg, sizeof(msg), "spinlock lockup: (%s)", name);
		spin_aee(msg, lock);
	} while (0);
#endif

	csd = this_cpu_ptr(&spinlock_debug_csd);
	csd->info = NULL;
}

bool is_logbuf_lock_held(raw_spinlock_t *lock)
{
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	/* The lock is needed by kmalloc and aee_kernel_warning_api */
	if (!strcmp(lock->dep_map.name, "logbuf_lock"))
		return true;
#endif
	return false;
}

static void __spin_lock_debug(raw_spinlock_t *lock)
{
	u64 one_second = (u64)loops_per_jiffy * msecs_to_jiffies(1000);
	int owner_cpu = -1, target_cpu = -1;
	int curr_cpu = raw_smp_processor_id();
	int print_once = 1, cnt = 0;
	int is_warning_owner = 0;
	char lock_name[MAX_LOCK_NAME];
	unsigned long long t1, t2, t3;
	struct task_struct *owner = NULL;
	cycles_t exit_cycles;

	/* skip debugging */
	if (is_logbuf_lock_held(lock)) {
		arch_spin_lock(&lock->raw_lock);
		return;
	}

	t1 = sched_clock();
	t2 = t1;
	exit_cycles = get_cycles() + one_second;

	for (;;) {
		while (get_cycles() < exit_cycles) {
			if (arch_spin_trylock(&lock->raw_lock)) {
				if (is_warning_owner) {
					struct spinlock_debug_info *sdi;

					sdi = per_cpu_ptr(&sp_dbg, target_cpu);
					sdi->detector_cpu = -1;
				}
				return;
			}
		}
		exit_cycles += one_second;

		t3 = sched_clock();
		if (t3 < t2)
			continue;
		t2 = t3;

		owner = lock->owner;
		owner_cpu = lock->owner_cpu;

		/* lock is already released */
		if (owner == SPINLOCK_OWNER_INIT || owner_cpu == -1)
			continue;

		get_spin_lock_name(lock, lock_name);
		pr_info("(%s)(%p) spin time: %llu ms(from %lld.%06lu), raw_lock: 0x%08x, magic: %08x, held by %s/%d on CPU#%d(from %lld.%06lu)\n",
			lock_name, lock,
			msec_high(t2 - t1), sec_high(t1), sec_low(t1),
			*((unsigned int *)&lock->raw_lock), lock->magic,
			owner->comm, task_pid_nr(owner), owner_cpu,
			sec_high(lock->lock_t), sec_low(lock->lock_t));

		/* print held lock information per 5 sec */
		if (cnt == 0) {
			struct spinlock_debug_info *sdi;

			sdi = per_cpu_ptr(&sp_dbg, owner_cpu);
			if (sdi->detector_cpu == -1 &&
			    raw_spin_trylock(&sdi->lock)) {
				is_warning_owner = 1;
				sdi->detector_cpu = curr_cpu;
				target_cpu = owner_cpu;
				raw_spin_unlock(&sdi->lock);
			}

			if (sdi->detector_cpu == curr_cpu)
				debug_show_held_locks(owner);
		}
		cnt = (++cnt == 5) ? 0 : cnt;

		if (oops_in_progress != 0)
			/* in exception follow, pr_xxx maybe spinlock error */
			continue;

		if (!print_once || !is_warning_owner)
			continue;
		print_once = 0;

		if (owner_cpu != curr_cpu) {
			call_single_data_t *csd;

			csd = per_cpu_ptr(&spinlock_debug_csd, owner_cpu);

			/* already warned by another cpu */
			if (csd->info)
				continue;

			/* mark csd is in use */
			csd->info = lock;
			csd->func = show_cpu_backtrace;
			csd->flags = 0;

			smp_call_function_single_async(owner_cpu, csd);
		} else {
			pr_info("(%s) recursive deadlock on CPU%d\n",
				lock_name, owner_cpu);
		}
	}
}

void do_raw_spin_lock(raw_spinlock_t *lock)
{
	debug_spin_lock_before(lock);
	if (unlikely(!arch_spin_trylock(&lock->raw_lock)))
		__spin_lock_debug(lock);
	debug_spin_lock_after(lock);
}
#endif /* MTK_DEBUG_SPINLOCK_V1 */

#ifdef MTK_DEBUG_SPINLOCK_V2
static void spin_lock_get_timestamp(unsigned long long *ts)
{
	*ts = sched_clock();
}

static void
spin_lock_check_spinning_time(raw_spinlock_t *lock, unsigned long long ts)
{
	unsigned long long te;

	te = sched_clock();
	if (te - ts > WARNING_TIME) {
		char lock_name[MAX_LOCK_NAME];

		get_spin_lock_name(lock, lock_name);
		pr_info("spinning for (%s)(%p) from [%lld.%06lu] to [%lld.%06lu], total %llu ms\n",
			lock_name, lock,
			sec_high(ts), sec_low(ts),
			sec_high(te), sec_low(te),
			msec_high(te - ts));
	}
}

static void spin_lock_check_holding_time(raw_spinlock_t *lock)
{
	char name[MAX_LOCK_NAME];

	/* check if holding time over 1 second */
	if (lock->unlock_t - lock->lock_t < WARNING_TIME)
		return;

	get_spin_lock_name(lock, name);
	pr_info("hold spinlock (%s)(%p) from [%lld.%06lu] to [%lld.%06lu], total %llu ms\n",
		name, lock,
		sec_high(lock->lock_t), sec_low(lock->lock_t),
		sec_high(lock->unlock_t), sec_low(lock->unlock_t),
		msec_high(lock->unlock_t - lock->lock_t));

	pr_info("========== The call trace of lock owner on CPU%d ==========\n",
		raw_smp_processor_id());
	dump_stack();

#ifdef CONFIG_MTK_LOCKING_AEE
	do {
		char msg[64];

		snprintf(msg, sizeof(msg), "spinlock lockup: (%s)", name);
		spin_aee(msg, lock);
	} while (0);
#endif
}

void do_raw_spin_lock(raw_spinlock_t *lock)
{
	unsigned long long ts = 0;

	debug_spin_lock_before(lock);
	spin_lock_get_timestamp(&ts);
	arch_spin_lock(&lock->raw_lock);
	spin_lock_check_spinning_time(lock, ts);
	debug_spin_lock_after(lock);
}
#endif /* MTK_DEBUG_SPINLOCK_V2 */

#ifdef CONFIG_MTK_LOCKING_AEE
static const char * const spinlock_white_list[] = {
	/* debug only, showacpu() */
	"show_lock"
};

/* check current spinlock in the lists or not */
static bool is_critical_spinlock(raw_spinlock_t *lock)
{
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	int i;

	/* can not do aee dump */
	for (i = 0; i < ARRAY_SIZE(critical_lock_list); i++)
		if (!strcmp(lock->dep_map.name, critical_lock_list[i]))
			return true;

	/* needless to do aee dump */
	for (i = 0; i < ARRAY_SIZE(spinlock_white_list); i++)
		if (!strcmp(lock->dep_map.name, spinlock_white_list[i]))
			return true;
#endif
	return false;
}
#endif

static void spin_aee(const char *msg, raw_spinlock_t *lock)
{
	if (!strcmp(msg, "bad magic") ||
	    !strcmp(msg, "wrong CPU") ||
	    !strcmp(msg, "wrong owner") ||
	    !strcmp(msg, "recursion") ||
	    !strcmp(msg, "cpu recursion") ||
	    !strcmp(msg, "already unlocked"))
		BUG_ON(1);

#ifdef CONFIG_MTK_LOCKING_AEE
	if (!is_critical_lock_held() &&
	    !is_critical_spinlock(lock)) {
		char aee_str[64];

		snprintf(aee_str, sizeof(aee_str),
			"%s: %s\n", current->comm, msg);
		aee_kernel_warning_api(__FILE__, __LINE__,
			DB_OPT_DUMMY_DUMP | DB_OPT_FTRACE,
			aee_str, "spinlock debugger\n");
	}
#endif
}
