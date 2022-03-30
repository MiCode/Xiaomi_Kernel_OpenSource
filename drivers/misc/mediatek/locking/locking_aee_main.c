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

module_init(locking_aee_init);
MODULE_DESCRIPTION("MEDIATEK LOCKING AEE TOOL");
MODULE_LICENSE("GPL v2");
