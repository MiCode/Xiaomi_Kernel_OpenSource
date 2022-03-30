// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

/**
 * @file    gpueb_reserved_mem.c
 * @brief   Reserved memory info init for GPUEB
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/seq_file.h>
#include <linux/pm_runtime.h>
#include <mboot_params.h>
#include <linux/of_reserved_mem.h>

#include "gpueb_helper.h"
#include "gpueb_reserved_mem.h"

phys_addr_t gpueb_mem_base_phys;
phys_addr_t gpueb_mem_base_virt;
phys_addr_t gpueb_mem_size;
unsigned int gpueb_mem_num = 0;

struct gpueb_reserve_mblock *gpueb_reserve_mblock_ary;
const char *gpueb_reserve_mblock_ary_name[20];

int gpueb_reserve_mem_of_init(struct reserved_mem *rmem)
{
	gpueb_pr_debug("@%s: %pa %pa\n", __func__, &rmem->base, &rmem->size);
	gpueb_mem_base_phys = (phys_addr_t) rmem->base;
	gpueb_mem_size = (phys_addr_t) rmem->size;

	return 0;
}
RESERVEDMEM_OF_DECLARE(gpueb_reserve_mem_init,
		GPUEB_MEM_RESERVED_KEY, gpueb_reserve_mem_of_init);

phys_addr_t gpueb_get_reserve_mem_phys(unsigned int id)
{
	if (id >= gpueb_mem_num) {
		gpueb_pr_debug("@%s: no reserve memory for %d", __func__, id);
		return 0;
	} else
		return gpueb_reserve_mblock_ary[id].start_phys;
}
EXPORT_SYMBOL_GPL(gpueb_get_reserve_mem_phys);

phys_addr_t gpueb_get_reserve_mem_virt(unsigned int id)
{
	if (id >= gpueb_mem_num) {
		gpueb_pr_debug("@%s: no reserve memory for %d", __func__, id);
		return 0;
	} else
		return gpueb_reserve_mblock_ary[id].start_virt;
}
EXPORT_SYMBOL_GPL(gpueb_get_reserve_mem_virt);

phys_addr_t gpueb_get_reserve_mem_size(unsigned int id)
{
	if (id >= gpueb_mem_num) {
		gpueb_pr_debug("@%s: no reserve memory for %d", __func__, id);
		return 0;
	} else
		return gpueb_reserve_mblock_ary[id].size;
}
EXPORT_SYMBOL_GPL(gpueb_get_reserve_mem_size);

phys_addr_t gpueb_get_reserve_mem_phys_by_name(char *mem_id_name)
{
	int id = -1;
	int i;

	for (i = 0; i < gpueb_mem_num; i++) {
		if (!strcmp(gpueb_reserve_mblock_ary_name[i], mem_id_name))
			id = i;
	}

	if (id < 0 || id >= gpueb_mem_num) {
		gpueb_pr_debug("@%s: no reserve memory for %d (%s)", __func__, id, mem_id_name);
		return 0;
	} else
		return gpueb_reserve_mblock_ary[id].start_phys;
}
EXPORT_SYMBOL_GPL(gpueb_get_reserve_mem_phys_by_name);

phys_addr_t gpueb_get_reserve_mem_virt_by_name(char *mem_id_name)
{
	int id = -1;
	int i;

	for (i = 0; i < gpueb_mem_num; i++) {
		if (!strcmp(gpueb_reserve_mblock_ary_name[i], mem_id_name))
			id = i;
	}

	if (id < 0 || id >= gpueb_mem_num) {
		gpueb_pr_debug("@%s: no reserve memory for %d (%s)", __func__, id, mem_id_name);
		return 0;
	} else
		return gpueb_reserve_mblock_ary[id].start_virt;
}
EXPORT_SYMBOL_GPL(gpueb_get_reserve_mem_virt_by_name);

phys_addr_t gpueb_get_reserve_mem_size_by_name(char *mem_id_name)
{
	int id = -1;
	int i;

	for (i = 0; i < gpueb_mem_num; i++) {
		if (!strcmp(gpueb_reserve_mblock_ary_name[i], mem_id_name))
			id = i;
	}

	if (id < 0 || id >= gpueb_mem_num) {
		gpueb_pr_debug("@%s: no reserve memory for %d (%s)", __func__, id, mem_id_name);
		return 0;
	} else
		return gpueb_reserve_mblock_ary[id].size;
}
EXPORT_SYMBOL_GPL(gpueb_get_reserve_mem_size_by_name);

int gpueb_reserved_mem_init(struct platform_device *pdev)
{
	struct device_node *of_gpueb = pdev->dev.of_node;
	unsigned int i, m_idx, m_size;
	phys_addr_t accumlate_memory_size = 0;
	int ret;

	of_property_read_u64(of_gpueb, "gpueb_mem_addr", &gpueb_mem_base_phys);
	of_property_read_u64(of_gpueb, "gpueb_mem_size", &gpueb_mem_size);

	if (!gpueb_mem_base_phys || !gpueb_mem_size) {
		gpueb_pr_debug("@%s: invalid gpueb_mem_base_phys (0x%llx), gpueb_mem_size (%llx)\n",
			__func__, gpueb_mem_base_phys, gpueb_mem_size);
		return -EINVAL;
	}

	gpueb_pr_debug("@%s: base_phys = 0x%llx, size = 0x%llx",
		__func__, gpueb_mem_base_phys, gpueb_mem_size);

	if ((gpueb_mem_base_phys >= 0x800000000ULL) || (gpueb_mem_base_phys < 0x40000000ULL)) {
		/*
		 * The gpueb remapped region is fixed, only
		 * 0x4000_0000 ~ 0x7_FFFF_FFFF is accessible.
		 */
		gpueb_pr_debug("@%s: Error: Wrong Address (0x%llx)\n",
			__func__, gpueb_mem_base_phys);
		BUG_ON(1);
		return -1;
	}

	// Set reserved memory table
	gpueb_mem_num = of_property_count_u32_elems(
			pdev->dev.of_node,
			"gpueb_mem_table")
			/ MEMORY_TBL_ELEM_NUM;
	if (gpueb_mem_num <= 0) {
		gpueb_pr_debug("@%s: gpueb_mem_table not found\n",
			__func__);
		gpueb_mem_num = 0;
	}

	// Get reserved mblock name
	ret = of_property_read_string_array(pdev->dev.of_node,
			"gpueb_mem_name_table",
			gpueb_reserve_mblock_ary_name,
			gpueb_mem_num);
	if (ret < 0) {
		gpueb_pr_debug("@%s: gpueb_mem_name_table not found\n", __func__);
		return -1;
	}

	for (i = 0; i < gpueb_mem_num; i++) {
		gpueb_pr_debug("gpueb_reserve_mblock_ary_name[%d] = %s\n",
			i, gpueb_reserve_mblock_ary_name[i]);
	}

	gpueb_reserve_mblock_ary = vzalloc(sizeof(struct gpueb_reserve_mblock) * gpueb_mem_num);

	for (i = 0; i < gpueb_mem_num; i++) {
		// Get reserved block's ID
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"gpueb_mem_table",
				i * MEMORY_TBL_ELEM_NUM,
				&m_idx);
		if (ret) {
			gpueb_pr_debug("@%s: Cannot get memory index(%d)\n", __func__, i);
			return -1;
		}
		gpueb_reserve_mblock_ary[m_idx].num = m_idx;

		// Get reserved block's size
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"gpueb_mem_table",
				(i * MEMORY_TBL_ELEM_NUM) + 1,
				&m_size);
		if (ret) {
			gpueb_pr_debug("@%s: Cannot get memory size(%d)\n", __func__, i);
			return -1;
		}

		if (m_idx >= gpueb_mem_num) {
			gpueb_pr_debug("@%s: Skip unexpected index, %d\n", __func__, m_idx);
			continue;
		}

		gpueb_reserve_mblock_ary[m_idx].size = m_size;
		gpueb_pr_debug("@%s: Reserved block <%d  %d>\n", __func__, m_idx, m_size);
	}

	// Transfer physical address to virtual address
	gpueb_mem_base_virt = (phys_addr_t)(size_t)ioremap_wc(
			gpueb_mem_base_phys, gpueb_mem_size);
	gpueb_pr_debug("@%s: Reserved phy_base = 0x%llx, len:0x%llx, Reserved virt_base = 0x%llx\n",
		__func__, gpueb_mem_base_phys, gpueb_mem_size, gpueb_mem_base_virt);

	// Init the access address for each block
	for (i = 0; i < gpueb_mem_num; i++) {
		gpueb_reserve_mblock_ary[i].start_phys = gpueb_mem_base_phys +
			accumlate_memory_size;
		gpueb_reserve_mblock_ary[i].start_virt = gpueb_mem_base_virt +
			accumlate_memory_size;
		accumlate_memory_size += gpueb_reserve_mblock_ary[i].size;
		gpueb_pr_debug("@%s: Reserved block[%d] phys:0x%llx, virt:0x%llx, len:0x%llx\n",
			__func__, i, gpueb_reserve_mblock_ary[i].start_phys,
			gpueb_reserve_mblock_ary[i].start_virt, gpueb_reserve_mblock_ary[i].size);
	}

	if (accumlate_memory_size > gpueb_mem_size)
		gpueb_pr_debug("@%s: Total memory in memory table is more than reserved",
			__func__);

	return 0;
}
