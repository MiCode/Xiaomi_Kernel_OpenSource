// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Unisoc Communications Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/android_debug_symbols.h>
#include <linux/mm.h>
#include <linux/mm/emem.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/swap.h>
#include <linux/workqueue.h>

static BLOCKING_NOTIFIER_HEAD(unisoc_show_mem_notify_list);

int register_unisoc_show_mem_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&unisoc_show_mem_notify_list, nb);
}
EXPORT_SYMBOL_GPL(register_unisoc_show_mem_notifier);

int unregister_unisoc_show_mem_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&unisoc_show_mem_notify_list, nb);
}
EXPORT_SYMBOL_GPL(unregister_unisoc_show_mem_notifier);

void unisoc_enhanced_show_mem(void)
{
	struct sysinfo si;
	void (*fun)(unsigned int filter, nodemask_t *nodemask);

	pr_info("Enhanced Mem-Info:E_SHOW_MEM_ALL\n");
	fun = android_debug_symbol(ADS_SHOW_MEM);
	if (fun != NULL)
		(*fun)(0, NULL);
	si_meminfo(&si);
	pr_info("MemTotal:       %8lu kB\n"
		"Buffers:        %8lu kB\n"
		"SwapCached:     %8lu kB\n",
		(si.totalram) << (PAGE_SHIFT - 10),
		(si.bufferram) << (PAGE_SHIFT - 10),
		total_swapcache_pages() << (PAGE_SHIFT - 10));
}

void unisoc_emem_notify_workfn(struct work_struct *work)
{
	unsigned long used = 0;

	blocking_notifier_call_chain(&unisoc_show_mem_notify_list, 0, &used);
	pr_info("+++++++++++++++++++++++UNISOC_SHOW_MEM_END+++++++++++++++++++++\n");
}
