/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/memory_alloc.h>
#include <linux/notifier.h>
#include <mach/scm.h>
#include <mach/msm_cache_dump.h>
#include <mach/memory.h>
#include <mach/msm_iomap.h>

#define L2C_IMEM_ADDR 0x2a03f014

static unsigned long msm_cache_dump_addr;

/*
 * These are dummy pointers so the defintion of l1_cache_dump
 * and l2_cache_dump don't get optimized away. If they aren't
 * referenced, the structure definitions don't show up in the
 * debugging information which is needed for post processing.
 */
static struct l1_cache_dump __used *l1_dump;
static struct l2_cache_dump __used *l2_dump;

static int msm_cache_dump_panic(struct notifier_block *this,
				unsigned long event, void *ptr)
{
#ifdef CONFIG_MSM_CACHE_DUMP_ON_PANIC
	scm_call_atomic1(L1C_SERVICE_ID, CACHE_BUFFER_DUMP_COMMAND_ID, 2);
	scm_call_atomic1(L1C_SERVICE_ID, CACHE_BUFFER_DUMP_COMMAND_ID, 1);
#endif
	return 0;
}

static struct notifier_block msm_cache_dump_blk = {
	.notifier_call  = msm_cache_dump_panic,
	/*
	 * higher priority to ensure this runs before another panic handler
	 * flushes the caches.
	 */
	.priority = 1,
};

static int msm_cache_dump_probe(struct platform_device *pdev)
{
	struct msm_cache_dump_platform_data *d = pdev->dev.platform_data;
	int ret;
	struct {
		unsigned long buf;
		unsigned long size;
	} l1_cache_data;
#ifndef CONFIG_MSM_CACHE_DUMP_ON_PANIC
	unsigned int *imem_loc;
#endif
	void *temp;
	unsigned long total_size = d->l1_size + d->l2_size;

	msm_cache_dump_addr = allocate_contiguous_ebi_nomap(total_size, SZ_4K);

	if (!msm_cache_dump_addr) {
		pr_err("%s: Could not get memory for cache dumping\n",
			__func__);
		return -ENOMEM;
	}

	temp = ioremap(msm_cache_dump_addr, total_size);
	memset(temp, 0xFF, total_size);
	iounmap(temp);

	l1_cache_data.buf = msm_cache_dump_addr;
	l1_cache_data.size = d->l1_size;

	ret = scm_call(L1C_SERVICE_ID, L1C_BUFFER_SET_COMMAND_ID,
			&l1_cache_data, sizeof(l1_cache_data), NULL, 0);

	if (ret)
		pr_err("%s: could not register L1 buffer ret = %d.\n",
			__func__, ret);

#if defined(CONFIG_MSM_CACHE_DUMP_ON_PANIC)
	l1_cache_data.buf = msm_cache_dump_addr + d->l1_size;
	l1_cache_data.size = d->l2_size;

	ret = scm_call(L1C_SERVICE_ID, L2C_BUFFER_SET_COMMAND_ID,
			&l1_cache_data, sizeof(l1_cache_data), NULL, 0);

	if (ret)
		pr_err("%s: could not register L2 buffer ret = %d.\n",
			__func__, ret);
#else
	imem_loc = ioremap(L2C_IMEM_ADDR, SZ_4K);
	__raw_writel(msm_cache_dump_addr + d->l1_size, imem_loc);
	iounmap(imem_loc);
#endif

	atomic_notifier_chain_register(&panic_notifier_list,
						&msm_cache_dump_blk);
	return 0;
}

static int msm_cache_dump_remove(struct platform_device *pdev)
{
	atomic_notifier_chain_unregister(&panic_notifier_list,
					&msm_cache_dump_blk);
	return 0;
}

static struct platform_driver msm_cache_dump_driver = {
	.remove		= __devexit_p(msm_cache_dump_remove),
	.driver         = {
		.name = "msm_cache_dump",
		.owner = THIS_MODULE
	},
};

static int __init msm_cache_dump_init(void)
{
	return platform_driver_probe(&msm_cache_dump_driver,
					msm_cache_dump_probe);
}

static void __exit msm_cache_dump_exit(void)
{
	platform_driver_unregister(&msm_cache_dump_driver);
}
late_initcall(msm_cache_dump_init);
module_exit(msm_cache_dump_exit)
