/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/delay.h>

static spinlock_t lockA;
static spinlock_t lockB;
static spinlock_t lockC;
static spinlock_t lockD;
static struct mutex mutexA;
static struct rw_semaphore	rw_semA;
static struct timer_list lockdep_timer;

struct lockdep_test_rcu {
	int val;
};
static struct lockdep_test_rcu __rcu *lockdep_test_rcu_data;

void lockdep_test_recursive_lock(void)
{
	/* recursive deadlock */
	spin_lock(&lockA);
	spin_lock(&lockA);
	spin_unlock(&lockA);
	spin_unlock(&lockA);
}

void lockdep_test_suspicious_rcu(void)
{
	struct lockdep_test_rcu *rcu_p;

	rcu_p = kmalloc(sizeof(struct lockdep_test_rcu), GFP_KERNEL);
	if (rcu_p == NULL)
		return;
	RCU_INIT_POINTER(lockdep_test_rcu_data, rcu_p);
	synchronize_rcu();

	/* rcu_read_lock() should be here */
	rcu_p = rcu_dereference(lockdep_test_rcu_data);
	/* rcu_read_unlock() should be here */
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

static void lockdep_test_timer(unsigned long arg)
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
	setup_timer(&lockdep_timer, lockdep_test_timer, 0);
	lockdep_timer.expires = jiffies + msecs_to_jiffies(10);
	add_timer(&lockdep_timer);
}

void lockdep_test_inconsistent_lock_b(void)
{
	/* {IN-SOFTIRQ-W} */
	setup_timer(&lockdep_timer, lockdep_test_timer, 0);
	lockdep_timer.expires = jiffies + msecs_to_jiffies(10);
	add_timer(&lockdep_timer);
	mdelay(100);

	/* {SOFTIRQ-ON-W} */
	spin_lock(&lockA);
	spin_unlock(&lockA);
}

void lockdep_test_irq_lock_inversion(void)
{
	unsigned long flags;

	/* the order is the problem */
	spin_lock_irqsave(&lockA, flags);
	spin_lock(&lockB);
	spin_unlock_irqrestore(&lockA, flags);
	spin_unlock(&lockB);

	setup_timer(&lockdep_timer, lockdep_test_timer, 0);
	lockdep_timer.expires = jiffies + msecs_to_jiffies(10);
	add_timer(&lockdep_timer);
}

void lockdep_test_uninitialized(void)
{
	/* miss spin_lock_init */
	spin_lock(&lockD);
	spin_unlock(&lockD);
}

void lockdep_test_bad_magic(void)
{
	spin_lock_init(&lockD);
	lockD.rlock.magic = 0xdeaddead;
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
	spinlock_t *lockE;

	lockE = kmalloc(sizeof(spinlock_t), GFP_KERNEL);
	spin_lock_init(lockE);
	spin_lock(lockE);
	kfree(lockE);
	/* should do spin_unlock before free memory */
}

void lockdep_test_lock_monitor(void)
{
	mutex_lock(&mutexA);
	down_read(&rw_semA);
	rcu_read_lock();
	spin_lock(&lockA);

	mdelay(3000);

	spin_unlock(&lockA);
	rcu_read_unlock();
	up_read(&rw_semA);
	mutex_unlock(&mutexA);
}

struct lockdep_test_func {
	char name[32];
	void (*func)(void);
};

struct lockdep_test_func lockdep_test_list[] = {
	/* KernelAPI Dump */
	{"circular_deadlock", lockdep_test_circular_deadlock},
	{"suspicious_rcu", lockdep_test_suspicious_rcu},
	{"inconsistent_lock_a", lockdep_test_inconsistent_lock_a},
	{"inconsistent_lock_b", lockdep_test_inconsistent_lock_b},
	{"irq_lock_inversion", lockdep_test_irq_lock_inversion},
	{"bad_unlock_balance", lockdep_test_bad_unlock_balance},
	{"lock_monitor", lockdep_test_lock_monitor},
	/* KE */
	{"uninitialized", lockdep_test_uninitialized},
	{"bad_magic", lockdep_test_bad_magic},
	{"wrong_owner_cpu", lockdep_test_wrong_owner_cpu},
	{"held_lock_freed", lockdep_test_held_lock_freed},
	/* HWT */
	{"recursive_lock", lockdep_test_recursive_lock},
};

static ssize_t lockdep_test_write(struct file *file,
	const char *ubuf, size_t count, loff_t *ppos)
{
	int i;
	char buf[32];

	if (count >= sizeof(buf) || count == 0)
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, count))
		return -EFAULT;

	buf[count] = '\0';

	for (i = 0; i < ARRAY_SIZE(lockdep_test_list); i++) {
		if (!strncmp(buf, lockdep_test_list[i].name,
			strlen(lockdep_test_list[i].name)))
			lockdep_test_list[i].func();
	}

	return count;
}

static const struct file_operations proc_lockdep_test_fops = {
	.open  = simple_open,
	.write = lockdep_test_write,
};

void lockdep_test_init(void)
{
	spin_lock_init(&lockA);
	spin_lock_init(&lockB);
	spin_lock_init(&lockC);
	mutex_init(&mutexA);
	init_rwsem(&rw_semA);

	proc_create("lockdep_test", 0220, NULL, &proc_lockdep_test_fops);
}

