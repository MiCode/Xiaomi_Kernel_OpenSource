/* Copyright (c) 2012,2014-2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/stacktrace.h>
#include <linux/spinlock.h>

static DEFINE_SPINLOCK(alloc_lock);

#ifdef CONFIG_SLUB_DEBUG
#define WCNSS_MAX_STACK_TRACE			64
#endif

struct wcnss_prealloc {
	int occupied;
	unsigned int size;
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
	{0, 32 * 1024, NULL},
	{0, 32 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
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

void *wcnss_prealloc_get(unsigned int size)
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

	for (i = 0; i < ARRAY_SIZE(wcnss_allocs); i++) {
		if (!wcnss_allocs[i].occupied)
			continue;

		if (j == 0) {
			pr_err("wcnss_prealloc: Memory leak detected\n");
			j++;
		}

		pr_err("Size: %u, addr: %pK, backtrace:\n",
		       wcnss_allocs[i].size, wcnss_allocs[i].ptr);
		print_stack_trace(&wcnss_allocs[i].trace, 1);
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

static int __init wcnss_pre_alloc_init(void)
{
	return wcnss_prealloc_init();
}

static void __exit wcnss_pre_alloc_exit(void)
{
	wcnss_prealloc_deinit();
}

module_init(wcnss_pre_alloc_init);
module_exit(wcnss_pre_alloc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DEVICE "WCNSS Prealloc Driver");
