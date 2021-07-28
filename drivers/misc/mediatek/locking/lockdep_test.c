// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/freezer.h>

static spinlock_t lockA;
static spinlock_t lockB;
static spinlock_t lockC;
static spinlock_t lockD;
static struct mutex mutexA;
static struct rw_semaphore rw_semA;
static struct timer_list lockdep_timer;
static struct lockdep_map dep_mapA;

void lockdep_test_recursive_lock(void)
{
	/* recursive deadlock */
	mutex_lock(&mutexA);
	mutex_lock(&mutexA);
	mutex_unlock(&mutexA);
	mutex_unlock(&mutexA);
}

struct lockdep_test_rcu {
	int val;
};

static struct lockdep_test_rcu __rcu *lockdep_test_rcu_data;

void lockdep_test_suspicious_rcu(void)
{
	/* RCU Updater */
	{
		struct lockdep_test_rcu *rcu_updater;

		rcu_updater =
			kmalloc(sizeof(struct lockdep_test_rcu), GFP_KERNEL);
		if (!rcu_updater)
			return;

		rcu_updater->val = 123;
		RCU_INIT_POINTER(lockdep_test_rcu_data, rcu_updater);
		synchronize_rcu();
	}
	/* RCU Reader */
	{
		struct lockdep_test_rcu *rcu_reader;

		/* rcu_read_lock() should be here */
		rcu_reader = rcu_dereference(lockdep_test_rcu_data);
		pr_info("data value is %d\n", rcu_reader->val);
		/* rcu_read_unlock() should be here */
	}
}

void lockdep_test_circular_deadlock(void)
{
	/* lockA -> lockB */
	spin_lock(&lockA);
	spin_lock(&lockB);
	spin_unlock(&lockB);
	spin_unlock(&lockA);

	/* lockB -> lockA */
	spin_lock(&lockB);
	spin_lock(&lockA);
	spin_unlock(&lockA);
	spin_unlock(&lockB);
}

static void lockdep_test_timer(struct timer_list *t)
{
	spin_lock(&lockA);
	spin_unlock(&lockA);
}

void lockdep_test_inconsistent_lock_a(void)
{
	/* {SOFTIRQ-ON-W} */
	spin_lock(&lockA);
	spin_unlock(&lockA);

	/* {IN-SOFTIRQ-W} */
	timer_setup(&lockdep_timer, lockdep_test_timer, 0);
	mod_timer(&lockdep_timer, jiffies + msecs_to_jiffies(10));
}

void lockdep_test_inconsistent_lock_b(void)
{
	/* {IN-SOFTIRQ-W} */
	timer_setup(&lockdep_timer, lockdep_test_timer, 0);
	mod_timer(&lockdep_timer, jiffies + msecs_to_jiffies(10));
	mdelay(100);

	/* {SOFTIRQ-ON-W} */
	spin_lock(&lockA);
	spin_unlock(&lockA);
}

void lockdep_test_irq_lock_inversion(void)
{
	unsigned long flags;

	/* lockB is used in SOFTIRQ-unsafe condition.
	 * The state of lockB is {SOFTIRQ-ON-W}.
	 * The state of lockB in SOFTIRQ field is marked as {+}.
	 * A new lock dependency is generated.
	 *   1. lockB
	 */
	spin_lock(&lockB);
	spin_unlock(&lockB);

	/* lockA and lockB are used in absolute safe condition.
	 * Because IRQ is disabled and they are not used in interrupt handler.
	 * The state of lockA and lockB are unconcerned.
	 * The state of lockA and lockB in SOFTIRQ field are marked as {.}.
	 * Two new lock dependencies are generated.
	 *   1. lockA
	 *   2. lockA -> lockB
	 */
	spin_lock_irqsave(&lockA, flags);
	spin_lock(&lockB);
	spin_unlock(&lockB);
	spin_unlock_irqrestore(&lockA, flags);

	/* lockA is used in lockdep_test_timer.
	 * It's a timer callback function and the condition is SOFTIRQ-safe.
	 * The state of lockA will change from unconcerned to {IN-SOFTIRQ-W}.
	 * The state of lockA in SOFTIRQ field will change from {.} to {-}.
	 * No lock dependency is generated.
	 */
	timer_setup(&lockdep_timer, lockdep_test_timer, 0);
	mod_timer(&lockdep_timer, jiffies + msecs_to_jiffies(10));
}

void lockdep_test_irq_lock_inversion_sp(void)
{
	unsigned long flags;

	/* This is a special case.
	 * The lock and unlock order is the problem.
	 * The wrong order make lockB to be in IRQ-unsafe condition.
	 *
	 * (X)  <IRQ-safe>
	 *     -> lock(A)
	 *     -> lock(B)
	 *     -> unlock(A)
	 *      <IRQ-unsafe>
	 *     -> unlock(B)
	 *
	 * (O)  <IRQ-safe>
	 *     -> lock(A)
	 *     -> lock(B)
	 *     -> unlock(B)
	 *     -> unlock(A)
	 *      <IRQ-unsafe>
	 */

	spin_lock_irqsave(&lockA, flags);
	spin_lock(&lockB);
	spin_unlock_irqrestore(&lockA, flags);
	spin_unlock(&lockB);

	timer_setup(&lockdep_timer, lockdep_test_timer, 0);
	mod_timer(&lockdep_timer, jiffies + msecs_to_jiffies(10));
}

void lockdep_test_safe_to_unsafe(void)
{
	unsigned long flags;

	/* SOFTIRQ-unsafe */
	spin_lock(&lockB);
	spin_unlock(&lockB);

	/* SOFTIRQ-safe */
	timer_setup(&lockdep_timer, lockdep_test_timer, 0);
	mod_timer(&lockdep_timer, jiffies + msecs_to_jiffies(10));

	/* wait for lockdep_test_timer to finish */
	mdelay(200);

	/* safe and unconcerned */
	spin_lock_irqsave(&lockA, flags);
	spin_lock(&lockB);
	spin_unlock(&lockB);
	spin_unlock_irqrestore(&lockA, flags);
}

void lockdep_test_uninitialized(void)
{
	/* miss spin_lock_init */
	spin_lock(&lockD);
	spin_unlock(&lockD);
}

void lockdep_test_bad_magic(void)
{
	/* without spin_lock_init */
	spin_lock(&lockD);
	spin_unlock(&lockD);
}

void lockdep_test_bad_unlock_balance(void)
{
	spin_lock(&lockA);
	spin_unlock(&lockA);
	spin_unlock(&lockA);
}

void lockdep_test_wrong_owner_cpu(void)
{
	spin_lock(&lockA);
	lockA.rlock.owner_cpu = -1;
	spin_unlock(&lockA);
}

void lockdep_test_held_lock_freed(void)
{
	spinlock_t *lockE; /* pointer */

	lockE = kmalloc(sizeof(spinlock_t), GFP_KERNEL);
	if (!lockE)
		return;

	spin_lock_init(lockE);
	spin_lock(lockE);
	kfree(lockE);
	/* should do spin_unlock before free memory */
}

static int lockdep_test_thread(void *data)
{
	spin_lock(&lockA);
	mdelay(8000);
	spin_unlock(&lockA);
	return 0;
}

void lockdep_test_spin_time(void)
{
	kthread_run(lockdep_test_thread, NULL, "lockdep_test_spin_time");
	mdelay(100);
	spin_lock(&lockA);
	spin_unlock(&lockA);
}

static int lock_monitor_thread1(void *data)
{
	lock_map_acquire(&dep_mapA);
	down_read(&rw_semA);
	mutex_lock(&mutexA);
	rcu_read_lock();
	mdelay(20000);
	rcu_read_unlock();
	mutex_unlock(&mutexA);
	up_read(&rw_semA);
	lock_map_release(&dep_mapA);
	return 0;
}

static int lock_monitor_thread2(void *arg)
{
	lock_map_acquire(&dep_mapA);
	mutex_lock(&mutexA);
	mutex_unlock(&mutexA);
	lock_map_release(&dep_mapA);
	return 0;
}

static int lock_monitor_thread3(void *arg)
{
	down_write(&rw_semA);
	up_write(&rw_semA);
	return 0;
}

void lockdep_test_lock_monitor(void)
{
	kthread_run(lock_monitor_thread1, NULL, "test_thread1");
	mdelay(100);
	kthread_run(lock_monitor_thread2, NULL, "test_thread2");
	mdelay(100);
	kthread_run(lock_monitor_thread3, NULL, "test_thread3");
}

void lockdep_test_freeze_with_lock(void)
{
	mutex_lock(&mutexA);
	/* should not freeze a task with locks held */
	try_to_freeze();
	mutex_unlock(&mutexA);
}

struct lockdep_test_func {
	char name[32];
	void (*func)(void);
};

struct lockdep_test_func lockdep_test_list[] = {
	/* KernelAPI Dump */
	{"circular_deadlock", lockdep_test_circular_deadlock},
	{"recursive_lock", lockdep_test_recursive_lock},
	{"suspicious_rcu", lockdep_test_suspicious_rcu},
	{"inconsistent_lock_a", lockdep_test_inconsistent_lock_a},
	{"inconsistent_lock_b", lockdep_test_inconsistent_lock_b},
	{"irq_lock_inversion", lockdep_test_irq_lock_inversion},
	{"irq_lock_inversion_sp", lockdep_test_irq_lock_inversion_sp},
	{"bad_unlock_balance", lockdep_test_bad_unlock_balance},
	{"safe_to_unsafe", lockdep_test_safe_to_unsafe},
	{"spin_time", lockdep_test_spin_time},
	{"lock_monitor", lockdep_test_lock_monitor},
	{"freeze_with_lock", lockdep_test_freeze_with_lock},
	/* KE */
	{"uninitialized", lockdep_test_uninitialized},
	{"bad_magic", lockdep_test_bad_magic},
	{"wrong_owner_cpu", lockdep_test_wrong_owner_cpu},
	{"held_lock_freed", lockdep_test_held_lock_freed}
};

static ssize_t
lockdep_test_write(struct file *file, const char *ubuf,
		   size_t count, loff_t *ppos)
{
	int i;
	char buf[32];

	if (count >= sizeof(buf) || count == 0)
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, count))
		return -EFAULT;

	buf[count] = '\0';

	for (i = 0; i < ARRAY_SIZE(lockdep_test_list); i++) {
		if (strlen(lockdep_test_list[i].name) != count - 1)
			continue;
		if (!strncmp(buf, lockdep_test_list[i].name, count - 1))
			lockdep_test_list[i].func();
	}

	return count;
}

static const struct proc_ops proc_lockdep_test_fops = {
	.proc_open  = simple_open,
	.proc_write = lockdep_test_write,
};

int lockdep_test_init(void)
{
	static struct lock_class_key key;

	spin_lock_init(&lockA);
	spin_lock_init(&lockB);
	spin_lock_init(&lockC);
	mutex_init(&mutexA);
	init_rwsem(&rw_semA);
	lockdep_init_map(&dep_mapA, "dep_mapA", &key, 0);

	proc_create("lockdep_test", 0220, NULL, &proc_lockdep_test_fops);

	return 0;
}
