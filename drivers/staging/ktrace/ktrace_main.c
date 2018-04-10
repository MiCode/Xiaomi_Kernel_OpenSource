
#define pr_fmt(fmt)  "ktrace : " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/ktrace.h>

static struct ktrace __kt;

struct ktrace *get_ktrace(void)
{
	return &__kt;
}

static int __init ktrace_init(void)
{
	struct ktrace *kt = get_ktrace();

	if (!debugfs_initialized()) {
		pr_warn("debugfs not available, stat dir not created\n");
		return -ENOENT;
	}
	kt->dir = debugfs_create_dir("ktrace", NULL);
	if (!kt->dir) {
		pr_err("debugfs 'ktrace' stat dir creation failed\n");
		return -ENOMEM ;
	}

	ktrace_print_init(kt->dir, &kt->queue[QUEUE_STR]);

	ktrace_event_init(kt->dir);

	ktrace_mm_init(kt->dir, &kt->queue[QUEUE_MM]);

	ktrace_sched_init(kt->dir, &kt->queue[QUEUE_SCHED]);

	ktrace_binder_init(kt->dir, &kt->queue[QUEUE_BINDER]);

	ktrace_cpufreq_init(kt->dir, &kt->queue[QUEUE_CPUFREQ]);

	pr_info("ktrace init OK\n");

	return 0;
}

fs_initcall(ktrace_init);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Cai Liu <liucai@xiaomi.com>");
