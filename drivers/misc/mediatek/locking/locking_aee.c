// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__

#include <linux/debug_locks.h>
#include <linux/kprobes.h>
#include <linux/lockdep.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <mt-plat/aee.h>

static const char * const critical_lock_list[] = {
	/* runqueue */
	"&(&rq->lock)->rlock"
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

	if (!retval)
		return 0;

	if (!is_critical_lock_held()) {
		char aee_str[40];

		snprintf(aee_str, 40, "[%s]LockProve Warning", current->comm);
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

static int __init locking_aee_init(void)
{
	int ret;

	ret = register_kretprobe(&debug_locks_off_kretprobe);
	if (ret < 0) {
		pr_info("register debug_locks_off kretprobe failed, returned %d\n", ret);
		return -1;
	}
	pr_info("%register debug_locks_off kretprobe succeeded.\n");
	return 0;
}

static void __exit locking_aee_exit(void)
{
	unregister_kretprobe(&debug_locks_off_kretprobe);
}

module_init(locking_aee_init);
module_exit(locking_aee_exit);
MODULE_DESCRIPTION("MEDIATEK LOCKING AEE TOOL");
MODULE_LICENSE("GPL v2");
