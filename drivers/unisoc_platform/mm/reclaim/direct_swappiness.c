// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 Unisoc Communications Inc.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <trace/hooks/vmscan.h>
#include <linux/swap.h>
#include <linux/sysctl.h>

static int two_hundred = 200;
int sysctl_direct_swappiness = 60;
EXPORT_SYMBOL_GPL(sysctl_direct_swappiness);

struct ctl_table mm_table[] = {
	{
		.procname	= "direct_swappiness",
		.data		= &sysctl_direct_swappiness,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &two_hundred,
	},
	{ },
};

struct ctl_table sys_mm_table[] = {
	{
		.procname	= "unisoc_mm",
		.mode		= 0555,
		.child		= mm_table,
	},
	{ },
};

static void tune_swappiness(void *data, int *swappiness)
{
	if (!current_is_kswapd())
		*swappiness = sysctl_direct_swappiness;
}

void register_direct_swappiness_vendor_hooks(void)
{
	register_trace_android_vh_tune_swappiness(tune_swappiness, NULL);
}

void unregister_direct_swappiness_vendor_hooks(void)
{
	unregister_trace_android_vh_tune_swappiness(tune_swappiness, NULL);
}

int unisoc_enhance_reclaim_init(void)
{
	struct ctl_table_header *hdr;

	register_direct_swappiness_vendor_hooks();
	hdr = register_sysctl_table(sys_mm_table);

	if (unlikely(!hdr))
		pr_info("failed register_sysctl_table to /proc/sys/unisoc_mm/direct_swappiness!\n");

	pr_info("UNISOC enhance reclaim init succeed!\n");
	return 0;
}

void unisoc_enhance_reclaim_exit(void)
{
	unregister_direct_swappiness_vendor_hooks();
	pr_info("UNISOC enhance reclaim exit succeed!\n");
}
