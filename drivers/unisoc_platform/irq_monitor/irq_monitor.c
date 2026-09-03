// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/sched/clock.h>
#if defined(CONFIG_ARM)
#include <linux/math64.h>
#endif
#include "irq_consumption_monitor.h"

#define DEFAULT_SCAN_INTERVAL		3000
#define DEFAULT_THRESHOLD_VAL		3000

struct irq_monitor {
	unsigned int __percpu	*prev_kstat;
	unsigned int		threshold;
};

static RADIX_TREE(monitor_tree, GFP_KERNEL|GFP_ATOMIC);
static unsigned int scan_interval = DEFAULT_SCAN_INTERVAL;
static unsigned int threshold_global = DEFAULT_THRESHOLD_VAL;
static struct dentry *irq_monitor_dir;
static struct timer_list scan_timer;
static struct irq_domain *gic_domain;

/* The time in ms of scanning */
static u64 prev_time, curr_time, time_delta;

static void add_irq_monitor(struct irq_data *irq_data, bool atomic)
{
	struct irq_monitor *monitor;
	int hwirq = irq_data->hwirq;

	/* Sometimes we need add new monitors in interrupt context */
	gfp_t gfp_flag = atomic ? GFP_ATOMIC : GFP_KERNEL;

	monitor = kmalloc(sizeof(struct irq_monitor), gfp_flag);
	if (unlikely(!monitor))
		goto fail1;

	monitor->prev_kstat = alloc_percpu_gfp(unsigned int, gfp_flag);
	if (unlikely(!monitor->prev_kstat))
		goto fail2;

	monitor->threshold = threshold_global;

	if (radix_tree_insert(&monitor_tree, hwirq, monitor))
		goto insert_fail;

	pr_info("Add monitor for hwirq:%d\n", hwirq);
	return;

insert_fail:
	free_percpu(monitor->prev_kstat);
fail2:
	kfree(monitor);
fail1:
	pr_err("Failed to add monitor for hwirq:%d\n", hwirq);
}

static void
monitor_check_and_update(struct irq_desc *desc, struct irq_monitor *monitor)
{
	unsigned int curr_cnt, prev_cnt, cnt_delta, cnt_per_sec;
	int cpu;
	const char *name = "unknown";
#if defined(CONFIG_ARM)
	u64 tmp1, tmp2;
#endif

	for_each_possible_cpu(cpu) {
		curr_cnt = *per_cpu_ptr(desc->kstat_irqs, cpu);
		prev_cnt = *per_cpu_ptr(monitor->prev_kstat, cpu);
		cnt_delta = curr_cnt - prev_cnt;
#if defined(CONFIG_ARM)
		cnt_per_sec = cnt_delta * MSEC_PER_SEC;
		cnt_per_sec = div64_u64(cnt_per_sec, time_delta);
#else

		cnt_per_sec = cnt_delta * MSEC_PER_SEC / time_delta;
#endif

		/* Warn if the interrupts occur too many times */
		if (unlikely(cnt_per_sec > monitor->threshold)) {
			if (desc->action)
				name = desc->action->name;

#if defined(CONFIG_ARM)
			tmp1 = prev_time;
			tmp2 = div64_u64(tmp1, MSEC_PER_SEC);
			pr_warn("hwirq:%lu(%s) handled %u times on CPU%d from %llu.%03llus\n",
				desc->irq_data.hwirq, name, cnt_delta, cpu,
				tmp1,
				tmp2);
#else
			pr_warn("hwirq:%lu(%s) handled %u times on CPU%d from %llu.%03llus\n",
				desc->irq_data.hwirq, name, cnt_delta, cpu,
				prev_time / MSEC_PER_SEC,
				prev_time % MSEC_PER_SEC);
#endif
		}

		*per_cpu_ptr(monitor->prev_kstat, cpu) = curr_cnt;
	}
}

static void irq_monitor_scan(struct timer_list *t)
{
	struct irq_desc *irq_desc;
	struct irq_monitor *monitor;
	int i, hwirq;

	curr_time = local_clock();
	do_div(curr_time, NSEC_PER_MSEC);
	time_delta = curr_time - prev_time;

	for_each_irq_desc(i, irq_desc) {
		if (irq_desc->irq_data.domain != gic_domain)
			continue;

		hwirq = irq_desc->irq_data.hwirq;

		monitor = (struct irq_monitor *)radix_tree_lookup(&monitor_tree, hwirq);

		if (likely(monitor)) {
			monitor_check_and_update(irq_desc, monitor);
		} else {
			/* Add a monitor for the new interrupt */
			add_irq_monitor(&irq_desc->irq_data, true);
		}
	}

	prev_time = curr_time;
	mod_timer(&scan_timer, jiffies + msecs_to_jiffies(scan_interval));
}

/* file "threshold" operations */
static int threshold_get(void *data, u64 *val)
{
	*val = threshold_global;
	return 0;
}

static int threshold_set(void *data, u64 val)
{
	struct irq_monitor *monitor;
	struct irq_desc *irq_desc;
	int i, hwirq;

	threshold_global = (unsigned int)val;

	/* Setup threshold for every interrupts */
	for_each_irq_desc(i, irq_desc) {
		if (irq_desc->irq_data.domain != gic_domain)
			continue;
		hwirq = irq_desc->irq_data.hwirq;
		monitor = (struct irq_monitor *)radix_tree_lookup(&monitor_tree, hwirq);
		if (likely(monitor))
			monitor->threshold = threshold_global;
	}

	/* TODO: Setup threshold for specific interrupt */

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(threshold_fops, threshold_get, threshold_set, "%llu\n");

/* file "interval" operations */
static int interval_get(void *data, u64 *val)
{
	*val = scan_interval;
	return 0;
}

static int interval_set(void *data, u64 val)
{
	scan_interval = (unsigned int)val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(interval_fops, interval_get, interval_set, "%llu\n");


static int __init irq_monitor_init(void)
{
	struct irq_desc *irq_desc;
	int i;

	/* Create file nodes */
	irq_monitor_dir = debugfs_create_dir("irq_monitor", NULL);
	if (!irq_monitor_dir)
		return -ENOENT;
	debugfs_create_file("threshold", 0600, irq_monitor_dir, NULL, &threshold_fops);
	debugfs_create_file("interval", 0600, irq_monitor_dir, NULL, &interval_fops);

	/* Add monitors for the existing interrupts */
	for_each_irq_desc(i, irq_desc) {
		if (!gic_domain) {
			if (!strncmp(irq_desc->irq_data.chip->name, "GIC", 3))
				gic_domain = irq_desc->irq_data.domain;
			else
				continue;
		}

		/* Only care the interrupts in GIC domain */
		if (gic_domain && irq_desc->irq_data.domain != gic_domain)
			continue;

		/* It doesn't matter if failed here */
		add_irq_monitor(&irq_desc->irq_data, false);
	}

	timer_setup(&scan_timer, irq_monitor_scan, TIMER_DEFERRABLE);
	scan_timer.expires = jiffies + msecs_to_jiffies(scan_interval);
	add_timer(&scan_timer);

	consumption_monitor_init();

	pr_info("Initialized\n");
	return 0;
}

static void __exit irq_monitor_exit(void)
{
	struct irq_monitor *monitor;
	struct irq_desc *irq_desc;
	int i, hwirq;

	del_timer_sync(&scan_timer);

	debugfs_remove(irq_monitor_dir);

	for_each_irq_desc(i, irq_desc) {
		if (irq_desc->irq_data.domain != gic_domain)
			continue;

		hwirq = irq_desc->irq_data.hwirq;
		monitor = (struct irq_monitor *)radix_tree_lookup(&monitor_tree, hwirq);
		if (likely(monitor)) {
			free_percpu(monitor->prev_kstat);
			kfree(monitor);
		}
		radix_tree_delete(&monitor_tree, hwirq);
	}

	consumption_monitor_exit();

	pr_warn("Exited\n");
}

module_init(irq_monitor_init);
module_exit(irq_monitor_exit);

MODULE_AUTHOR("ben.dai@unisoc.com");
MODULE_LICENSE("GPL");
