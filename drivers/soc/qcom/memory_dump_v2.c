/* Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <soc/qcom/memory_dump.h>
#include <soc/qcom/minidump.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/minidump.h>
#include <linux/of_device.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>

#define MSM_DUMP_TABLE_VERSION		MSM_DUMP_MAKE_VERSION(2, 0)

#define SCM_CMD_DEBUG_LAR_UNLOCK	0x4

struct msm_dump_table {
	uint32_t version;
	uint32_t num_entries;
	struct msm_dump_entry entries[MAX_NUM_ENTRIES];
};

struct msm_memory_dump {
	uint64_t table_phys;
	struct msm_dump_table *table;
};


static struct msm_memory_dump memdump;
static struct msm_mem_dump_vaddr_tbl vaddr_tbl;

uint32_t msm_dump_table_version(void)
{
	return MSM_DUMP_TABLE_VERSION;
}
EXPORT_SYMBOL(msm_dump_table_version);

static int msm_dump_table_register(struct msm_dump_entry *entry)
{
	struct msm_dump_entry *e;
	struct msm_dump_table *table = memdump.table;

	if (!table || table->num_entries >= MAX_NUM_ENTRIES)
		return -EINVAL;

	e = &table->entries[table->num_entries];
	e->id = entry->id;
	e->type = MSM_DUMP_TYPE_TABLE;
	e->addr = entry->addr;
	table->num_entries++;

	dmac_flush_range(table, (void *)table + sizeof(struct msm_dump_table));
	return 0;
}

static struct msm_dump_table *msm_dump_get_table(enum msm_dump_table_ids id)
{
	struct msm_dump_table *table = memdump.table;
	int i;

	if (!table) {
		pr_err("mem dump base table does not exist\n");
		return ERR_PTR(-EINVAL);
	}

	for (i = 0; i < MAX_NUM_ENTRIES; i++) {
		if (table->entries[i].id == id)
			break;
	}
	if (i == MAX_NUM_ENTRIES || !table->entries[i].addr) {
		pr_err("mem dump base table entry %d invalid\n", id);
		return ERR_PTR(-EINVAL);
	}

	/* Get the apps table pointer */
	table = phys_to_virt(table->entries[i].addr);

	return table;
}

static int msm_dump_data_add_minidump(struct msm_dump_entry *entry)
{
	struct msm_dump_data *data;
	struct md_region md_entry;

	data = (struct msm_dump_data *)(phys_to_virt(entry->addr));

	if (!data->addr || !data->len)
		return -EINVAL;

	if (!strcmp(data->name, "")) {
		pr_debug("Entry name is NULL, Use ID %d for minidump\n",
			 entry->id);
		snprintf(md_entry.name, sizeof(md_entry.name), "KMDT0x%X",
			 entry->id);
	} else {
		strlcpy(md_entry.name, data->name, sizeof(md_entry.name));
	}

	md_entry.phys_addr = data->addr;
	md_entry.virt_addr = (uintptr_t)phys_to_virt(data->addr);
	md_entry.size = data->len;
	md_entry.id = entry->id;

	return msm_minidump_add_region(&md_entry);
}

int msm_dump_data_register(enum msm_dump_table_ids id,
			   struct msm_dump_entry *entry)
{
	struct msm_dump_entry *e;
	struct msm_dump_table *table;

	table = msm_dump_get_table(id);
	if (IS_ERR(table))
		return PTR_ERR(table);

	if (!table || table->num_entries >= MAX_NUM_ENTRIES)
		return -EINVAL;

	e = &table->entries[table->num_entries];
	e->id = entry->id;
	e->type = MSM_DUMP_TYPE_DATA;
	e->addr = entry->addr;
	table->num_entries++;

	dmac_flush_range(table, (void *)table + sizeof(struct msm_dump_table));

	if (msm_dump_data_add_minidump(entry) < 0)
		pr_err("Failed to add entry in Minidump table\n");

	return 0;
}
EXPORT_SYMBOL(msm_dump_data_register);

struct dump_vaddr_entry *get_msm_dump_ptr(enum msm_dump_data_ids id)
{
	int i;

	if (!vaddr_tbl.entries)
		return NULL;

	if (id > MSM_DUMP_DATA_MAX)
		return NULL;

	for (i = 0; i < vaddr_tbl.num_node; i++) {
		if (vaddr_tbl.entries[i].id == id)
			break;
	}

	if (i == vaddr_tbl.num_node)
		return NULL;

	return &vaddr_tbl.entries[i];
}
EXPORT_SYMBOL(get_msm_dump_ptr);

static int __init init_memory_dump(void)
{
	struct msm_dump_table *table;
	struct msm_dump_entry entry;
	struct device_node *np;
	void __iomem *imem_base;
	int ret;

	np = of_find_compatible_node(NULL, NULL,
				     "qcom,msm-imem-mem_dump_table");
	if (!np) {
		pr_err("mem dump base table DT node does not exist\n");
		return -ENODEV;
	}

	imem_base = of_iomap(np, 0);
	if (!imem_base) {
		pr_err("mem dump base table imem offset mapping failed\n");
		return -ENOMEM;
	}

	memdump.table = kzalloc(sizeof(struct msm_dump_table), GFP_KERNEL);
	if (!memdump.table) {
		ret = -ENOMEM;
		goto err0;
	}
	memdump.table->version = MSM_DUMP_TABLE_VERSION;
	memdump.table_phys = virt_to_phys(memdump.table);
	memcpy_toio(imem_base, &memdump.table_phys, sizeof(memdump.table_phys));
	/* Ensure write to imem_base is complete before unmapping */
	mb();
	pr_info("MSM Memory Dump base table set up\n");

	iounmap(imem_base);

	table = kzalloc(sizeof(struct msm_dump_table), GFP_KERNEL);
	if (!table) {
		ret = -ENOMEM;
		goto err1;
	}
	table->version = MSM_DUMP_TABLE_VERSION;

	entry.id = MSM_DUMP_TABLE_APPS;
	entry.addr = virt_to_phys(table);
	ret = msm_dump_table_register(&entry);
	if (ret) {
		pr_info("mem dump apps data table register failed\n");
		goto err2;
	}
	pr_info("MSM Memory Dump apps data table set up\n");

	return 0;
err2:
	kfree(table);
err1:
	kfree(memdump.table);
	return ret;
err0:
	iounmap(imem_base);
	return ret;
}
early_initcall(init_memory_dump);

#ifdef CONFIG_MSM_DEBUG_LAR_UNLOCK
static int __init init_debug_lar_unlock(void)
{
	int ret;
	uint32_t argument = 0;
	struct scm_desc desc = {0};

	if (!is_scm_armv8())
		ret = scm_call(SCM_SVC_TZ, SCM_CMD_DEBUG_LAR_UNLOCK, &argument,
			       sizeof(argument), NULL, 0);
	else
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_TZ,
				SCM_CMD_DEBUG_LAR_UNLOCK), &desc);
	if (ret)
		pr_err("Core Debug Lock unlock failed, ret: %d\n", ret);
	else
		pr_info("Core Debug Lock unlocked\n");

	return ret;
}
early_initcall(init_debug_lar_unlock);
#endif

static int mem_dump_probe(struct platform_device *pdev)
{
	struct device_node *child_node;
	const struct device_node *node = pdev->dev.of_node;
	static dma_addr_t dump_addr;
	static void *dump_vaddr;
	struct msm_dump_data *dump_data;
	struct msm_dump_entry dump_entry;
	int ret;
	u32 size, id;
	int i = 0;

	vaddr_tbl.num_node = of_get_child_count(node);
	vaddr_tbl.entries = devm_kcalloc(&pdev->dev, vaddr_tbl.num_node,
				 sizeof(struct dump_vaddr_entry),
				 GFP_KERNEL);
	if (!vaddr_tbl.entries)
		dev_err(&pdev->dev, "Unable to allocate mem for ptr addr\n");

	for_each_available_child_of_node(node, child_node) {
		ret = of_property_read_u32(child_node, "qcom,dump-size", &size);
		if (ret) {
			dev_err(&pdev->dev, "Unable to find size for %s\n",
					child_node->name);
			continue;
		}

		ret = of_property_read_u32(child_node, "qcom,dump-id", &id);
		if (ret) {
			dev_err(&pdev->dev, "Unable to find id for %s\n",
					child_node->name);
			continue;
		}

		dump_vaddr = (void *) dma_alloc_coherent(&pdev->dev, size,
						&dump_addr, GFP_KERNEL);

		if (!dump_vaddr) {
			dev_err(&pdev->dev, "Couldn't get memory for dumping\n");
			continue;
		}

		memset(dump_vaddr, 0x0, size);

		dump_data = devm_kzalloc(&pdev->dev,
				sizeof(struct msm_dump_data), GFP_KERNEL);
		if (!dump_data) {
			dma_free_coherent(&pdev->dev, size, dump_vaddr,
					dump_addr);
			continue;
		}

		dump_data->addr = dump_addr;
		dump_data->len = size;
		strlcpy(dump_data->name, child_node->name,
			strlen(child_node->name) + 1);

		dump_entry.id = id;
		dump_entry.addr = virt_to_phys(dump_data);
		ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS, &dump_entry);
		if (ret) {
			dev_err(&pdev->dev, "Data dump setup failed, id = %d\n",
				id);
			dma_free_coherent(&pdev->dev, size, dump_vaddr,
					dump_addr);
			devm_kfree(&pdev->dev, dump_data);
		} else if (vaddr_tbl.entries) {
			vaddr_tbl.entries[i].id = id;
			vaddr_tbl.entries[i].dump_vaddr = dump_vaddr;
			vaddr_tbl.entries[i].dump_data_vaddr = dump_data;
			i++;
		}
	}
	return 0;
}

static const struct of_device_id mem_dump_match_table[] = {
	{.compatible = "qcom,mem-dump",},
	{}
};

static struct platform_driver mem_dump_driver = {
	.probe = mem_dump_probe,
	.driver = {
		.name = "msm_mem_dump",
		.owner = THIS_MODULE,
		.of_match_table = mem_dump_match_table,
	},
};

static int __init mem_dump_init(void)
{
	return platform_driver_register(&mem_dump_driver);
}

pure_initcall(mem_dump_init);
