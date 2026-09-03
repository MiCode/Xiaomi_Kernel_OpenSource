// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched/clock.h>
#include <linux/syscore_ops.h>

#define NSEC_PER_SEC		1000000000L

static void resume_kts_sync(void)
{
	u64 local_t = local_clock();
	u64 boot_t = ktime_get_boot_fast_ns();
	unsigned long local_us = do_div(local_t, NSEC_PER_SEC) / 1000;
	unsigned long boot_us = do_div(boot_t, NSEC_PER_SEC) / 1000;

	pr_info("boottime_sleep: %llu.%06lu localtime: %llu.%06lu\n",
		boot_t, boot_us, local_t, local_us);
}

static struct syscore_ops kts_sync_ops = {
	.resume = resume_kts_sync,
};

static int kts_sync_show(struct seq_file *m, void *v)
{
	u64 local_t = local_clock();
	u64 boot_t = ktime_get_boot_fast_ns();
	unsigned long local_us = do_div(local_t, NSEC_PER_SEC) / 1000;
	unsigned long boot_us = do_div(boot_t, NSEC_PER_SEC) / 1000;

	seq_printf(m, "boottime_sleep: %llu.%06lu localtime: %llu.%06lu\n",
		   boot_t, boot_us, local_t, local_us);

	return 0;
}

static int __init kts_sync_init(void)
{
	static struct proc_dir_entry *kts_sync_entry;

	kts_sync_entry = proc_create_single("kts_sync", 0, NULL, kts_sync_show);
	if (!kts_sync_entry) {
		pr_err("Failed to create /proc/kts_sync\n");
		return -ENOENT;
	}

	register_syscore_ops(&kts_sync_ops);

	pr_info("Initialized\n");
	return 0;
}

static void __exit kts_sync_exit(void)
{
	remove_proc_entry("kts_sync", NULL);
	unregister_syscore_ops(&kts_sync_ops);
	pr_warn("Exited!\n");
}

module_init(kts_sync_init);
module_exit(kts_sync_exit);

MODULE_AUTHOR("ben.dai@unisoc.com");
MODULE_DESCRIPTION("The kernel timestamp synchronization module");
MODULE_LICENSE("GPL");
