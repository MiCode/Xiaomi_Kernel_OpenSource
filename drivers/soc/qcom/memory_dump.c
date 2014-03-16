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
 */
#include <asm/cacheflush.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <soc/qcom/memory_dump.h>

#define MSM_DUMP_TABLE_VERSION		MSM_DUMP_MAKE_VERSION(1, 0)

struct msm_dump_table {
	u32 version;
	u32 num_entries;
	struct msm_client_dump client_entries[MAX_NUM_CLIENTS];
};

struct msm_memory_dump {
	unsigned long dump_table_phys;
	struct msm_dump_table *dump_table_ptr;
};

static struct msm_memory_dump mem_dump_data;

uint32_t msm_dump_table_version(void)
{
	return MSM_DUMP_TABLE_VERSION;
}
EXPORT_SYMBOL(msm_dump_table_version);

int msm_dump_tbl_register(struct msm_client_dump *client_entry)
{
	struct msm_client_dump *entry;
	struct msm_dump_table *table = mem_dump_data.dump_table_ptr;

	if (!table || table->num_entries >= MAX_NUM_CLIENTS)
		return -EINVAL;
	entry = &table->client_entries[table->num_entries];
	entry->id = client_entry->id;
	entry->start_addr = client_entry->start_addr;
	entry->end_addr = client_entry->end_addr;
	table->num_entries++;
	/* flush cache */
	dmac_flush_range(table, (void *)table + sizeof(struct msm_dump_table));
	return 0;
}
EXPORT_SYMBOL(msm_dump_tbl_register);

static int __init init_memory_dump(void)
{
	struct msm_dump_table *table;
	struct device_node *np;
	static void __iomem *imem_base;

	np = of_find_compatible_node(NULL, NULL, "qcom,msm-imem-mem_dump_table");
	if (!np) {
		pr_err("unable to find DT imem dump table node\n");
		return -ENODEV;
	}
	imem_base = of_iomap(np, 0);
	if (!imem_base) {
		pr_err("unable to map imem dump table offset\n");
		return -ENOMEM;
	}

	mem_dump_data.dump_table_ptr = kzalloc(sizeof(struct msm_dump_table),
						GFP_KERNEL);
	if (!mem_dump_data.dump_table_ptr) {
		iounmap(imem_base);
		printk(KERN_ERR "unable to allocate memory for dump table\n");
		return -ENOMEM;
	}
	table = mem_dump_data.dump_table_ptr;
	table->version = MSM_DUMP_TABLE_VERSION;
	mem_dump_data.dump_table_phys = virt_to_phys(table);
	writel_relaxed(mem_dump_data.dump_table_phys, imem_base);
	printk(KERN_INFO "MSM Memory Dump table set up\n");
	iounmap(imem_base);

	return 0;
}

early_initcall(init_memory_dump);

