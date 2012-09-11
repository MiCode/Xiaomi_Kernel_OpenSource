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
 */
#include <asm/cacheflush.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/export.h>
#include <mach/msm_iomap.h>
#include <mach/msm_memory_dump.h>


/*TODO: Needs to be set to correct value */
#define DUMP_TABLE_OFFSET	0x14
#define MSM_DUMP_TABLE_VERSION	MK_TABLE(1, 0)

static struct msm_memory_dump mem_dump_data;

static int msm_memory_dump_panic(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	writel_relaxed(0, MSM_IMEM_BASE + DUMP_TABLE_OFFSET);
	return 0;
}

static struct notifier_block msm_memory_dump_blk = {
	.notifier_call  = msm_memory_dump_panic,
};

int msm_dump_table_register(struct msm_client_dump *client_entry)
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
	dmac_flush_range(table, table + sizeof(struct msm_dump_table));
	return 0;
}
EXPORT_SYMBOL(msm_dump_table_register);

static int __init init_memory_dump(void)
{
	struct msm_dump_table *table;

	mem_dump_data.dump_table_ptr = kzalloc(sizeof(struct msm_dump_table),
						GFP_KERNEL);
	if (!mem_dump_data.dump_table_ptr) {
		printk(KERN_ERR "unable to allocate memory for dump table\n");
		return -ENOMEM;
	}
	table = mem_dump_data.dump_table_ptr;
	table->version = MSM_DUMP_TABLE_VERSION;
	mem_dump_data.dump_table_phys = virt_to_phys(table);
	writel_relaxed(mem_dump_data.dump_table_phys,
				MSM_IMEM_BASE + DUMP_TABLE_OFFSET);
	/* TODO: Write to Debug image IMEM.
	 * Once IMEM issues are resolved MSM_IMEM_BASE
	 * will have actual mapping.
	 */
	writel_relaxed(mem_dump_data.dump_table_phys,
				MSM_DBG_IMEM_BASE + DUMP_TABLE_OFFSET);
	atomic_notifier_chain_register(&panic_notifier_list,
						&msm_memory_dump_blk);
	printk(KERN_INFO "MSM Memory Dump table set up\n");
	return 0;
}

early_initcall(init_memory_dump);

