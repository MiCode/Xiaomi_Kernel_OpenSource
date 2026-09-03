/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/mm.h>
#include <linux/notifier.h>
#include <linux/swap.h>

static BLOCKING_NOTIFIER_HEAD(e_show_mem_notify_list);

int register_e_show_mem_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&e_show_mem_notify_list, nb);
}
EXPORT_SYMBOL_GPL(register_e_show_mem_notifier);

int unregister_e_show_mem_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&e_show_mem_notify_list, nb);
}
EXPORT_SYMBOL_GPL(unregister_e_show_mem_notifier);

void enhanced_show_mem(void)
{
	/* Module used pages */
	unsigned long used = 0;
	struct sysinfo si;

	pr_info("Enhanced Mem-Info:E_SHOW_MEM_ALL\n");
	show_mem(SHOW_MEM_FILTER_NODES, NULL);
	si_meminfo(&si);
	pr_info("MemTotal:       %8lu kB\n"
		"Buffers:        %8lu kB\n"
		"SwapCached:     %8lu kB\n",
		(si.totalram) << (PAGE_SHIFT - 10),
		(si.bufferram) << (PAGE_SHIFT - 10),
		total_swapcache_pages() << (PAGE_SHIFT - 10));

	blocking_notifier_call_chain(&e_show_mem_notify_list, 0, &used);
}

