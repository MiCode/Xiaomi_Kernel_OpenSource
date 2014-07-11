/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <soc/qcom/scm.h>
#include <asm/cacheflush.h>
#include <soc/qcom/cache_dump.h>
#include <soc/qcom/memory_dump.h>

#define L2_DUMP_OFFSET 0x14

static dma_addr_t msm_cache_dump_addr;
static void *msm_cache_dump_vaddr;

/*
 * These should not actually be dereferenced. There's no
 * need for a virtual mapping, but the physical address is
 * necessary.
 */
static struct l1_cache_dump *l1_dump;
static struct l2_cache_dump *l2_dump;

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
	struct msm_client_dump l1_dump_entry, l2_dump_entry;
	struct msm_dump_entry dump_entry;
	struct msm_dump_data *l1_inst_data, *l1_data_data, *l2_data;
	int ret, cpu;
	struct {
		unsigned long buf;
		unsigned long size;
	} l1_cache_data;
	u32 l1_size, l2_size;
	unsigned long total_size;
	u32 l1_inst_size, l1_data_size;
	phys_addr_t l1_inst_start, l1_data_start, l2_start;

	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
					   "qcom,l1-dump-size", &l1_size);
		if (ret)
			return ret;

		ret = of_property_read_u32(pdev->dev.of_node,
					   "qcom,l2-dump-size", &l2_size);
		if (ret)
			return ret;
	} else {
		l1_size = d->l1_size;
		l2_size = d->l2_size;
	};

	total_size = l1_size + l2_size;
	msm_cache_dump_vaddr = (void *) dma_alloc_coherent(&pdev->dev,
					total_size, &msm_cache_dump_addr,
					GFP_KERNEL);

	if (!msm_cache_dump_vaddr) {
		pr_err("%s: Could not get memory for cache dumping\n",
			__func__);
		return -ENOMEM;
	}

	memset(msm_cache_dump_vaddr, 0xFF, total_size);
	/* Clean caches before sending buffer to TZ */
	dmac_clean_range(msm_cache_dump_vaddr,
				msm_cache_dump_vaddr + total_size);

	l1_cache_data.buf = msm_cache_dump_addr;
	l1_cache_data.size = l1_size;

	ret = scm_call(L1C_SERVICE_ID, L1C_BUFFER_SET_COMMAND_ID,
			&l1_cache_data, sizeof(l1_cache_data), NULL, 0);

	if (ret)
		pr_err("%s: could not register L1 buffer ret = %d.\n",
			__func__, ret);

	l1_dump = (struct l1_cache_dump *)(uint32_t)msm_cache_dump_addr;
	l2_dump = (struct l2_cache_dump *)(uint32_t)(msm_cache_dump_addr
								+ l1_size);

#if defined(CONFIG_MSM_CACHE_DUMP_ON_PANIC)
	l1_cache_data.buf = msm_cache_dump_addr + l1_size;
	l1_cache_data.size = l2_size;

	ret = scm_call(L1C_SERVICE_ID, L2C_BUFFER_SET_COMMAND_ID,
			&l1_cache_data, sizeof(l1_cache_data), NULL, 0);

	if (ret)
		pr_err("%s: could not register L2 buffer ret = %d.\n",
			__func__, ret);
#endif

	if (MSM_DUMP_MAJOR(msm_dump_table_version()) == 1) {
		l1_dump_entry.id = MSM_L1_CACHE;
		l1_dump_entry.start_addr = msm_cache_dump_addr;
		l1_dump_entry.end_addr = l1_dump_entry.start_addr + l1_size - 1;

		l2_dump_entry.id = MSM_L2_CACHE;
		l2_dump_entry.start_addr = msm_cache_dump_addr + l1_size;
		l2_dump_entry.end_addr = l2_dump_entry.start_addr + l2_size - 1;

		ret = msm_dump_tbl_register(&l1_dump_entry);
		if (ret)
			pr_err("Could not register L1 dump area: %d\n", ret);

		ret = msm_dump_tbl_register(&l2_dump_entry);
		if (ret)
			pr_err("Could not register L2 dump area: %d\n", ret);
	} else {
		l1_inst_data = kzalloc(sizeof(struct msm_dump_data) *
				       num_present_cpus(), GFP_KERNEL);
		if (!l1_inst_data) {
			pr_err("l1 inst data structure allocation failed\n");
			ret = -ENOMEM;
			goto err0;
		}

		l1_data_data = kzalloc(sizeof(struct msm_dump_data) *
				       num_present_cpus(), GFP_KERNEL);
		if (!l1_data_data) {
			pr_err("l1 data data structure allocation failed\n");
			ret = -ENOMEM;
			goto err1;
		}

		l1_inst_start = msm_cache_dump_addr;
		l1_data_start = msm_cache_dump_addr + (l1_size / 2);
		l1_inst_size = l1_size / (num_present_cpus() * 2);
		l1_data_size = l1_inst_size;

		for_each_cpu(cpu, cpu_present_mask) {
			l1_inst_data[cpu].addr = l1_inst_start +
							cpu * l1_inst_size;
			l1_inst_data[cpu].len = l1_inst_size;
			dump_entry.id = MSM_DUMP_DATA_L1_INST_CACHE + cpu;
			dump_entry.addr = virt_to_phys(&l1_inst_data[cpu]);
			ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS,
						     &dump_entry);
			/*
			 * Don't free the buffers in case of error since
			 * registration may have succeeded for some cpus.
			 */
			if (ret)
				pr_err("cpu %d l1 inst dump setup failed\n",
					cpu);

			l1_data_data[cpu].addr = l1_data_start +
							cpu * l1_data_size;
			l1_data_data[cpu].len = l1_data_size;
			dump_entry.id = MSM_DUMP_DATA_L1_DATA_CACHE + cpu;
			dump_entry.addr = virt_to_phys(&l1_data_data[cpu]);
			ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS,
						     &dump_entry);
			/*
			 * Don't free the buffers in case of error since
			 * registration may have succeeded for some cpus.
			 */
			if (ret)
				pr_err("cpu %d l1 data dump setup failed\n",
					cpu);
		}

		l2_data = kzalloc(sizeof(struct msm_dump_data) *
				  num_present_cpus(), GFP_KERNEL);
		if (!l2_data) {
			pr_err("l2 data structure allocation failed\n");
			ret = -ENOMEM;
			goto err2;
		}

		l2_start = msm_cache_dump_addr + l1_size;

		l2_data->addr = l2_start;
		l2_data->len = l2_size;
		dump_entry.id = MSM_DUMP_DATA_L2_CACHE;
		dump_entry.addr = virt_to_phys(l2_data);
		ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS,
					     &dump_entry);
		if (ret)
			pr_err("l2 dump setup failed\n");
	}

	atomic_notifier_chain_register(&panic_notifier_list,
						&msm_cache_dump_blk);
	return 0;
err2:
	kfree(l1_data_data);
err1:
	kfree(l1_inst_data);
err0:
	dma_free_coherent(&pdev->dev, total_size, msm_cache_dump_vaddr,
			  msm_cache_dump_addr);
	return ret;
}

static int msm_cache_dump_remove(struct platform_device *pdev)
{
	atomic_notifier_chain_unregister(&panic_notifier_list,
					&msm_cache_dump_blk);
	return 0;
}

static struct of_device_id cache_dump_match_table[] = {
	{	.compatible = "qcom,cache_dump",	},
	{}
};
EXPORT_COMPAT("qcom,cache_dump");

static struct platform_driver msm_cache_dump_driver = {
	.remove		= msm_cache_dump_remove,
	.driver         = {
		.name = "msm_cache_dump",
		.owner = THIS_MODULE,
		.of_match_table = cache_dump_match_table,
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
