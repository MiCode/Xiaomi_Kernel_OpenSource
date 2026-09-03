// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt)  "sprd_modules_notify: " fmt

#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/radix-tree.h>
#include <linux/seq_file.h>
#include <linux/sched/clock.h>

#include "native_hang_monitor.h"
#if IS_ENABLED(CONFIG_SPRD_MODULES_NOTIFY)

/*unit: ms*/
#ifdef CONFIG_SPRD_DEBUG
#define MODULES_TIME_OUT (1600)
#else
#define MODULES_TIME_OUT (2000)
#endif

static RADIX_TREE(modules_tree, GFP_KERNEL|GFP_ATOMIC);

static void sprd_module_time_out(struct timer_list *t)
{
	struct module *mod = (struct module *)t->android_kabi_reserved2;
#ifdef CONFIG_SPRD_SKYNET
	pr_debug("%s.ko loads too long time, panic timeout = %d ms\n",
			mod->name, MODULES_TIME_OUT);
#else
	panic("%s.ko loads too long time, panic timeout = %d ms, loglevel = %d\n",
			mod->name, MODULES_TIME_OUT, console_printk[0]);
#endif
}

/* Handle one module, either coming or alive. */
static int sprd_module_notify(struct notifier_block *self,
			unsigned long val, void *data)
{
	struct module *mod = data;
	struct timer_list *module_timer;
	int ret = 0;

	switch (val) {
	case MODULE_STATE_COMING:

		mod->android_kabi_reserved1 = ktime_get_boot_fast_ns();
		radix_tree_insert(&modules_tree, mod->android_kabi_reserved1, mod);

		module_timer = kmalloc(sizeof(struct timer_list), GFP_ATOMIC);
		if (!module_timer)
			break;

		timer_setup(module_timer, sprd_module_time_out, TIMER_DEFERRABLE);
		mod->android_kabi_reserved3 = (unsigned long)module_timer;
		module_timer->android_kabi_reserved2 = (unsigned long)mod;

		module_timer->android_kabi_reserved1 = 1;
		module_timer->expires = jiffies + msecs_to_jiffies(MODULES_TIME_OUT);
		add_timer(module_timer);
		break;

	case MODULE_STATE_LIVE:
		if (mod->android_kabi_reserved3) {
			module_timer = (struct timer_list *)mod->android_kabi_reserved3;
			del_timer(module_timer);
			kfree(module_timer);
			mod->android_kabi_reserved3 = 0;
		}

		mod->android_kabi_reserved2 = ktime_get_boot_fast_ns()
							- mod->android_kabi_reserved1;
		break;

	case  MODULE_STATE_GOING:
		/*only check loading too long, ignore loading fail*/
		if (mod->android_kabi_reserved3) {
			module_timer = (struct timer_list *)mod->android_kabi_reserved3;
			del_timer(module_timer);
			kfree(module_timer);
			mod->android_kabi_reserved3 = 0;
		}

		if (mod->android_kabi_reserved1)
			radix_tree_delete(&modules_tree, mod->android_kabi_reserved1);
		break;
	default:
		break;
	}
	return ret;
}

static struct notifier_block sprd_module_nb = {
	.notifier_call = sprd_module_notify,
	.priority = 99,
};

static int sprd_modules_show(struct seq_file *m, void *v)
{
	void __rcu **slot;
	struct radix_tree_iter iter;
	struct module *modules;

	seq_printf(m, "panic timeout = %d ms\n", MODULES_TIME_OUT);
	seq_puts(m, "module name, loading time(ns), kernel boottime(ns)\n");
	radix_tree_for_each_slot(slot, &modules_tree, &iter, 0) {
		if (slot != NULL)
			modules = *slot;

		if (modules) {
			seq_printf(m, "%-50s  %-20lld  %-20lld\n",
					modules->name, modules->android_kabi_reserved2,
					modules->android_kabi_reserved1);
			modules = NULL;
		}
	}
	return 0;
}

int sprd_modules_init(void)
{
	int ret;
	static struct proc_dir_entry *modules_entry;

	ret = register_module_notifier(&sprd_module_nb);
	if (ret) {
		pr_warn("Failed to register sprd module notifier\n");
		return -1;
	}

	modules_entry = proc_create_single("modules_list", 0444, NULL, sprd_modules_show);
	if (!modules_entry) {
		pr_err("Failed to create /proc/modules_list\n");
		unregister_module_notifier(&sprd_module_nb);
		return -1;
	}

	return ret;
}

void  sprd_modules_exit(void)
{
	unregister_module_notifier(&sprd_module_nb);
	remove_proc_entry("modules_list", NULL);
}
#endif
