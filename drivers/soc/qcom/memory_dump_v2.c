// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2017, 2019, The Linux Foundation. All rights reserved.
 */

#include <asm/cacheflush.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <soc/qcom/minidump.h>
#include <soc/qcom/memory_dump.h>
#include <soc/qcom/qtee_shmbridge.h>
#include <soc/qcom/secure_buffer.h>
#include <soc/qcom/scm.h>
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

static int register_dump_table_entry(enum msm_dump_table_ids id,
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
	return 0;
}

/**
 * msm_dump_data_register - register to dump data and minidump framework
 * @id: ID of the dump table.
 * @entry: dump entry to be registered
 * This api will register the entry passed to dump table and minidump table
 */
int msm_dump_data_register(enum msm_dump_table_ids id,
			   struct msm_dump_entry *entry)
{
	int ret;

	ret = register_dump_table_entry(id, entry);
	if (!ret)
		if (msm_dump_data_add_minidump(entry))
			pr_err("Failed to add entry in Minidump table\n");

	return ret;
}
EXPORT_SYMBOL(msm_dump_data_register);

/**
 * msm_dump_data_register_nominidump - register to dump data framework
 * @id: ID of the dump table.
 * @entry: dump entry to be registered
 * This api will register the entry passed to dump table only
 */
int msm_dump_data_register_nominidump(enum msm_dump_table_ids id,
			   struct msm_dump_entry *entry)
{
	return register_dump_table_entry(id, entry);
}
EXPORT_SYMBOL(msm_dump_data_register_nominidump);

#define MSM_DUMP_TOTAL_SIZE_OFFSET	0x724
static int init_memory_dump(void *dump_vaddr, phys_addr_t phys_addr,
					size_t size)
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

	memdump.table = dump_vaddr;
	memdump.table->version = MSM_DUMP_TABLE_VERSION;
	memdump.table_phys = phys_addr;
	memcpy_toio(imem_base, &memdump.table_phys,
			sizeof(memdump.table_phys));
	memcpy_toio(imem_base + MSM_DUMP_TOTAL_SIZE_OFFSET,
			&size, sizeof(size_t));

	/* Ensure write to imem_base is complete before unmapping */
	mb();
	pr_info("MSM Memory Dump base table set up\n");

	iounmap(imem_base);
	dump_vaddr +=  sizeof(*table);
	phys_addr += sizeof(*table);
	table = dump_vaddr;
	table->version = MSM_DUMP_TABLE_VERSION;
	entry.id = MSM_DUMP_TABLE_APPS;
	entry.addr = phys_addr;
	ret = msm_dump_table_register(&entry);
	if (ret) {
		pr_err("mem dump apps data table register failed\n");
		return ret;
	}
	pr_info("MSM Memory Dump apps data table set up\n");

	return 0;
}

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

#define MSM_DUMP_DATA_SIZE sizeof(struct msm_dump_data)
static int mem_dump_alloc(struct platform_device *pdev)
{
	struct device_node *child_node;
	const struct device_node *node = pdev->dev.of_node;
	struct msm_dump_data *dump_data;
	struct msm_dump_entry dump_entry;
	struct md_region md_entry;
	size_t total_size;
	u32 size, id;
	int ret, no_of_nodes;
	dma_addr_t dma_handle;
	phys_addr_t phys_addr;
	struct sg_table mem_dump_sgt;
	void *dump_vaddr;
	uint32_t ns_vmids[] = {VMID_HLOS};
	uint32_t ns_vm_perms[] = {PERM_READ | PERM_WRITE};
	u64 shm_bridge_handle;

	total_size = size = ret = no_of_nodes = 0;
	/* For dump table registration with IMEM */
	total_size = sizeof(struct msm_dump_table) * 2;
	for_each_available_child_of_node(node, child_node) {
		ret = of_property_read_u32(child_node, "qcom,dump-size", &size);
		if (ret) {
			dev_err(&pdev->dev, "Unable to find size for %s\n",
					child_node->name);
			continue;
		}

		total_size += size;
		no_of_nodes++;
	}

	total_size += (MSM_DUMP_DATA_SIZE * no_of_nodes);
	total_size = ALIGN(total_size, SZ_4K);
	dump_vaddr = dma_alloc_coherent(&pdev->dev, total_size,
						&dma_handle, GFP_KERNEL);
	if (!dump_vaddr) {
		dev_err(&pdev->dev, "Couldn't get memory for dump entries\n");
		return -ENOMEM;
	}

	dma_get_sgtable(&pdev->dev, &mem_dump_sgt, dump_vaddr,
						dma_handle, total_size);
	phys_addr = page_to_phys(sg_page(mem_dump_sgt.sgl));
	sg_free_table(&mem_dump_sgt);

	ret = qtee_shmbridge_register(phys_addr, total_size, ns_vmids,
		ns_vm_perms, 1, PERM_READ|PERM_WRITE, &shm_bridge_handle);

	if (ret) {
		dev_err(&pdev->dev, "Failed to create shm bridge.ret=%d\n",
						ret);
		dma_free_coherent(&pdev->dev, total_size,
						dump_vaddr, dma_handle);
		return ret;
	}

	memset(dump_vaddr, 0x0, total_size);

	ret = init_memory_dump(dump_vaddr, phys_addr, total_size);
	if (ret) {
		dev_err(&pdev->dev, "Memory Dump table set up is failed\n");
		qtee_shmbridge_deregister(shm_bridge_handle);
		dma_free_coherent(&pdev->dev, total_size,
						dump_vaddr, dma_handle);
		return ret;
	}

	dump_vaddr += (sizeof(struct msm_dump_table) * 2);
	phys_addr += (sizeof(struct msm_dump_table) * 2);
	for_each_available_child_of_node(node, child_node) {
		ret = of_property_read_u32(child_node, "qcom,dump-size", &size);
		if (ret)
			continue;

		ret = of_property_read_u32(child_node, "qcom,dump-id", &id);
		if (ret) {
			dev_err(&pdev->dev, "Unable to find id for %s\n",
					child_node->name);
			continue;
		}

		dump_data = dump_vaddr;
		dump_data->addr = phys_addr + MSM_DUMP_DATA_SIZE;
		dump_data->len = size;
		dump_entry.id = id;
		strlcpy(dump_data->name, child_node->name,
					sizeof(dump_data->name));
		dump_entry.addr = phys_addr;
		ret = msm_dump_data_register_nominidump(MSM_DUMP_TABLE_APPS,
					&dump_entry);
		if (ret)
			dev_err(&pdev->dev, "Data dump setup failed, id = %d\n",
				id);

		md_entry.phys_addr = dump_data->addr;
		md_entry.virt_addr = (uintptr_t)dump_vaddr + MSM_DUMP_DATA_SIZE;
		md_entry.size = size;
		md_entry.id = id;
		strlcpy(md_entry.name, child_node->name, sizeof(md_entry.name));
		if (msm_minidump_add_region(&md_entry))
			dev_err(&pdev->dev, "Mini dump entry failed id = %d\n",
				id);

		dump_vaddr += (size + MSM_DUMP_DATA_SIZE);
		phys_addr += (size  + MSM_DUMP_DATA_SIZE);
	}

	return ret;
}

static int mem_dump_probe(struct platform_device *pdev)
{
	int ret;

	ret = mem_dump_alloc(pdev);
	return ret;
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
