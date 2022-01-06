// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__

#include <linux/atomic.h>
#include <linux/debug_locks.h>
#include <linux/delay.h>
#include <linux/kprobes.h>
#include <linux/kthread.h>
#include <linux/lockdep.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <mt-plat/aee.h>

//lockdep_test.c
int lockdep_test_init(void);

static atomic_t warned;

static const char * const critical_lock_list[] = {
	/* runqueue */
	"&rq->lock",
	/* workqueue */
	"&(&pool->lock)->rlock",
	/* kmalloc */
	"&(&n->list_lock)->rlock",
	"&(&zone->lock)->rlock",
	/* stacktrace */
	"depot_lock",
	/* console */
	"&(&port->lock)->rlock",
	/* try_to_wake_up */
	"&p->pi_lock",
	"&sg_policy->update_lock",
};

static int is_critical_lock_held(void)
{
	int i, j;
	struct held_lock *hlock;

	/* check locks held by current task */
	if (!current->lockdep_depth)
		return false;

	hlock = current->held_locks;
	for (i = 0; i < current->lockdep_depth; i++) {
		struct lockdep_map *instance = (hlock + i)->instance;

		if (!instance->name)
			continue;

		for (j = 0; j < ARRAY_SIZE(critical_lock_list); j++)
			if (!strcmp(instance->name, critical_lock_list[j]))
				return true;
	}
	return false;
}

static int debug_locks_off_kretprobe_handler(struct kretprobe_instance *ri,
		struct pt_regs *regs)
{
	int retval = regs_return_value(regs);
	u32 val_warned = 0;

	if (!retval)
		return 0;

	if (is_critical_lock_held())
		return 0;

	if (likely(atomic_try_cmpxchg(&warned, &val_warned, 1U))) {
		char aee_str[40];

		pr_info("LockProve Warning, AEE!\n");
		scnprintf(aee_str, 40, "[%s]LockProve Warning", current->comm);
		aee_kernel_warning_api(__FILE__, __LINE__,
				DB_OPT_DUMMY_DUMP | DB_OPT_FTRACE,
				aee_str, "LockProve Debug\n");
	}
	return 0;
}

static struct kretprobe debug_locks_off_kretprobe = {
	.kp = {
		.symbol_name = "debug_locks_off",
	},
	.handler = debug_locks_off_kretprobe_handler,
	.maxactive = 1,
};

static int locking_aee_thread(void *data)
{
	u32 val_warned = 0;

	for (;;) {
		msleep(2000);

		/* is lockdep disabled? */
		if (likely(debug_locks))
			continue;

		/* Have we called the AEE? */
		val_warned = atomic_read(&warned);
		if (likely(val_warned))
			break;
		if (likely(atomic_try_cmpxchg(&warned, &val_warned, 1U)))
			break;
	}

	WARN_ON(debug_locks);

	if (!val_warned) {
		char aee_str[40];

		pr_info("LockProve Warning, AEE!\n");
		scnprintf(aee_str, 40, "[%s]LockProve Warning", current->comm);
		aee_kernel_warning_api(__FILE__, __LINE__,
				DB_OPT_DUMMY_DUMP | DB_OPT_FTRACE,
				aee_str, "LockProve Debug\n");
	}
	return 0;
}

static void ldt_disable_aee(void)
{
	atomic_set(&warned, 1);
}

static int __init locking_aee_init(void)
{
	monitor_hang_regist_ldt(ldt_disable_aee);
	kthread_run(locking_aee_thread, NULL, "locking_aee");
	lockdep_test_init();
	return 0;
}

static void __exit locking_aee_exit(void)
{
	unregister_kretprobe(&debug_locks_off_kretprobe);
	monitor_hang_regist_ldt(NULL);
}

module_init(locking_aee_init);
module_exit(locking_aee_exit);
MODULE_DESCRIPTION("MEDIATEK LOCKING AEE TOOL");
MODULE_LICENSE("GPL v2");
