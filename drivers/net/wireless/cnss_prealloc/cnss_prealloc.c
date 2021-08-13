// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2012,2014-2017,2019,2020 The Linux Foundation. All rights reserved. */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/stacktrace.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#ifdef CONFIG_WCNSS_SKB_PRE_ALLOC
#include <linux/skbuff.h>
#endif
#include <net/cnss_prealloc.h>

static DEFINE_SPINLOCK(alloc_lock);

#ifdef CONFIG_SLUB_DEBUG
#define WCNSS_MAX_STACK_TRACE			64
#endif

static struct kobject  *prealloc_kobject;

struct wcnss_prealloc {
	int occupied;
	size_t size;
	void *ptr;
#ifdef CONFIG_SLUB_DEBUG
	unsigned long stack_trace[WCNSS_MAX_STACK_TRACE];
	struct stack_trace trace;
#endif
};

/* pre-alloced mem for WLAN driver */
static struct wcnss_prealloc wcnss_allocs[] = {
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 8  * 1024, NULL},
	{0, 16 * 1024, NULL},
	{0, 16 * 1024, NULL},
	{0, 16 * 1024, NULL},
	{0, 16 * 1024, NULL},
	{0, 16 * 1024, NULL},
	{0, 16 * 1024, NULL},
	{0, 16 * 1024, NULL},
	{0, 16 * 1024, NULL},
	{0, 16 * 1024, NULL},
	{0, 16 * 1024, NULL},
	{0, 16 * 1024, NULL},
	{0, 16 * 1024, NULL},
	{0, 16 * 1024, NULL},
	{0, 16 * 1024, NULL},
	{0, 16 * 1024, NULL},
	{0, 16 * 1024, NULL},
	{0, 32 * 1024, NULL},
	{0, 32 * 1024, NULL},
	{0, 32 * 1024, NULL},
	{0, 32 * 1024, NULL},
	{0, 32 * 1024, NULL},
	{0, 32 * 1024, NULL},
	{0, 32 * 1024, NULL},
	{0, 32 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 128 * 1024, NULL},
	{0, 128 * 1024, NULL},
	{0, 128 * 1024, NULL},
};

int wcnss_prealloc_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(wcnss_allocs); i++) {
		wcnss_allocs[i].occupied = 0;
		wcnss_allocs[i].ptr = kmalloc(wcnss_allocs[i].size, GFP_KERNEL);
		if (!wcnss_allocs[i].ptr)
			return -ENOMEM;
	}

	return 0;
}

void wcnss_prealloc_deinit(void)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(wcnss_allocs); i++) {
		kfree(wcnss_allocs[i].ptr);
		wcnss_allocs[i].ptr = NULL;
	}
}

#ifdef CONFIG_SLUB_DEBUG
static void wcnss_prealloc_save_stack_trace(struct wcnss_prealloc *entry)
{
	struct stack_trace *trace = &entry->trace;

	memset(&entry->stack_trace, 0, sizeof(entry->stack_trace));
	trace->nr_entries = 0;
	trace->max_entries = WCNSS_MAX_STACK_TRACE;
	trace->entries = entry->stack_trace;
	trace->skip = 2;

	save_stack_trace(trace);
}
#else
static inline
void wcnss_prealloc_save_stack_trace(struct wcnss_prealloc *entry) {}
#endif

void *wcnss_prealloc_get(size_t size)
{
	int i = 0;
	unsigned long flags;

	spin_lock_irqsave(&alloc_lock, flags);
	for (i = 0; i < ARRAY_SIZE(wcnss_allocs); i++) {
		if (wcnss_allocs[i].occupied)
			continue;

		if (wcnss_allocs[i].size >= size) {
			/* we found the slot */
			wcnss_allocs[i].occupied = 1;
			spin_unlock_irqrestore(&alloc_lock, flags);
			wcnss_prealloc_save_stack_trace(&wcnss_allocs[i]);
			return wcnss_allocs[i].ptr;
		}
	}
	spin_unlock_irqrestore(&alloc_lock, flags);

	return NULL;
}
EXPORT_SYMBOL(wcnss_prealloc_get);

int wcnss_prealloc_put(void *ptr)
{
	int i = 0;
	unsigned long flags;

	spin_lock_irqsave(&alloc_lock, flags);
	for (i = 0; i < ARRAY_SIZE(wcnss_allocs); i++) {
		if (wcnss_allocs[i].ptr == ptr) {
			wcnss_allocs[i].occupied = 0;
			spin_unlock_irqrestore(&alloc_lock, flags);
			return 1;
		}
	}
	spin_unlock_irqrestore(&alloc_lock, flags);

	return 0;
}
EXPORT_SYMBOL(wcnss_prealloc_put);

#ifdef CONFIG_SLUB_DEBUG
void wcnss_prealloc_check_memory_leak(void)
{
	int i, j = 0;
	struct stack_trace *trace = NULL;

	for (i = 0; i < ARRAY_SIZE(wcnss_allocs); i++) {
		if (!wcnss_allocs[i].occupied)
			continue;

		if (j == 0) {
			pr_err("wcnss_prealloc: Memory leak detected\n");
			j++;
		}

		pr_err("Size: %zu, addr: %pK, backtrace:\n",
		       wcnss_allocs[i].size, wcnss_allocs[i].ptr);
		trace = &wcnss_allocs[i].trace;
		stack_trace_print(trace->entries, trace->nr_entries, 1);
	}
}
#else
void wcnss_prealloc_check_memory_leak(void) {}
#endif
EXPORT_SYMBOL(wcnss_prealloc_check_memory_leak);

int wcnss_pre_alloc_reset(void)
{
	int i, n = 0;

	for (i = 0; i < ARRAY_SIZE(wcnss_allocs); i++) {
		if (!wcnss_allocs[i].occupied)
			continue;

		wcnss_allocs[i].occupied = 0;
		n++;
	}
	return n;
}
EXPORT_SYMBOL(wcnss_pre_alloc_reset);

static ssize_t status_show(struct kobject *kobj, struct kobj_attribute *attr,
			   char *buffer)
{
	int i = 0;
	int used_slots = 0, free_slots = 0;
	unsigned int tsize = 0, tused = 0, size = 0;
	int len = 0;
	char *buf;

	buf = buffer;
	len += scnprintf(&buf[len], PAGE_SIZE - len,
			"\nSlot_Size(Kb)\t\t[Used : Free]\n");
	for (i = 0; i < ARRAY_SIZE(wcnss_allocs); i++) {
		tsize += wcnss_allocs[i].size;
		if (size != wcnss_allocs[i].size) {
			if (size) {
				len += scnprintf(&buf[len], PAGE_SIZE - len,
						"[%d : %d]\n", used_slots,
						free_slots);

			}

			size = wcnss_allocs[i].size;
			used_slots = 0;
			free_slots = 0;
			len += scnprintf(&buf[len], PAGE_SIZE - len,
					"%d Kb\t\t\t", size / 1024);

		}

		if (wcnss_allocs[i].occupied) {
			tused += wcnss_allocs[i].size;
			++used_slots;
		} else {
			++free_slots;
		}
	}
	len += scnprintf(&buf[len], PAGE_SIZE - len,
			"[%d : %d]\n", used_slots, free_slots);

	/* Convert byte to Kb */
	if (tsize)
		tsize = tsize / 1024;
	if (tused)
		tused = tused / 1024;
		len += scnprintf(&buf[len], PAGE_SIZE - len,
				"\nMemory Status:\nTotal Memory: %dKb\n",
				tsize);
		len += scnprintf(&buf[len], PAGE_SIZE - len,
				"Used: %dKb\nFree: %dKb\n", tused,
				tsize - tused);

	return len;
}

static struct kobj_attribute status_attribute = __ATTR_RO(status);

static int create_prealloc_status_sysfs(void)
{
	int ret;

	prealloc_kobject = kobject_create_and_add("cnss-prealloc",
						  kernel_kobj);
	if (!prealloc_kobject) {
		pr_err("Failed to create cnss-prealloc kernel object\n");
		return -ENOMEM;
	}

	ret = sysfs_create_file(prealloc_kobject,
				&status_attribute.attr);
	if (ret) {
		pr_err("%s: Failed to create sysfs cnss-prealloc file\n",
		       __func__);
		kobject_put(prealloc_kobject);
	}

	return ret;
}

static void remove_prealloc_status_sysfs(void)
{
	if (prealloc_kobject) {
		sysfs_remove_file(prealloc_kobject,
				  &status_attribute.attr);
		kobject_put(prealloc_kobject);
	}
}

static int __init wcnss_pre_alloc_init(void)
{
	int ret;

	ret = wcnss_prealloc_init();
	if (ret) {
		pr_err("%s: Failed to init the prealloc pool\n", __func__);
		return ret;
	}

	create_prealloc_status_sysfs();

	return ret;
}

static void __exit wcnss_pre_alloc_exit(void)
{
	wcnss_prealloc_deinit();
	remove_prealloc_status_sysfs();
}

module_init(wcnss_pre_alloc_init);
module_exit(wcnss_pre_alloc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("WCNSS Prealloc Driver");
