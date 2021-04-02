// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

/**
 * @file    gpueb_common_reserved_mem.c
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

#include "gpueb_common_helper.h"
#include "gpueb_common_reserved_mem.h"

struct gpueb_reserve_mblock gpueb_reserve_mblock_ary[] = {
	{
		.num = GPUEB_LOGGER_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
};

phys_addr_t gpueb_mem_base_phys;
phys_addr_t gpueb_mem_base_virt;
phys_addr_t gpueb_mem_size;

int gpueb_common_reserve_mem_of_init(struct reserved_mem *rmem)
{
    gpueb_pr_debug("@%s: %pa %pa\n", __func__, &rmem->base, &rmem->size);
	gpueb_mem_base_phys = (phys_addr_t) rmem->base;
	gpueb_mem_size = (phys_addr_t) rmem->size;

	return 0;
}
RESERVEDMEM_OF_DECLARE(gpueb_reserve_mem_init,
                GPUEB_MEM_RESERVED_KEY, gpueb_common_reserve_mem_of_init);



phys_addr_t gpueb_common_get_reserve_mem_phys(enum gpueb_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		gpueb_pr_debug("@%s: no reserve memory for %d", __func__, id);
		return 0;
	} else
		return gpueb_reserve_mblock_ary[id].start_phys;
}


phys_addr_t gpueb_common_get_reserve_mem_virt(enum gpueb_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		gpueb_pr_debug("@%s: no reserve memory for %d", __func__, id);
		return 0;
	} else
		return gpueb_reserve_mblock_ary[id].start_virt;
}

phys_addr_t gpueb_common_get_reserve_mem_size(enum gpueb_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		gpueb_pr_debug("@%s: no reserve memory for %d", __func__, id);
		return 0;
	} else
		return gpueb_reserve_mblock_ary[id].size;
}

int gpueb_common_reserved_mem_init(struct platform_device *pdev)
{
    unsigned int gpueb_mem_num = 0;
	struct device_node *rmem_node;
	struct reserved_mem *rmem;
	unsigned int i, m_idx, m_size;
    phys_addr_t accumlate_memory_size = 0;
	int ret;

	rmem_node = of_find_compatible_node(NULL, NULL, GPUEB_MEM_RESERVED_KEY);
	if (!rmem_node) {
		gpueb_pr_debug("@%s: no node for reserved memory\n", __func__);
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(rmem_node);
	if (!rmem) {
		gpueb_pr_debug("@%s: cannot lookup reserved memory\n", __func__);
		return -EINVAL;
	}

	gpueb_mem_base_phys = (phys_addr_t) rmem->base;
	gpueb_mem_size = (phys_addr_t) rmem->size;

	gpueb_pr_debug("@%s: base_phys = 0x%x, size = 0x%x",
		__func__,
		(unsigned int)gpueb_mem_base_phys,
		(unsigned int)gpueb_mem_size);

    
	if ((gpueb_mem_base_phys >= (0x90000000ULL)) ||
			 (gpueb_mem_base_phys <= 0x0)) {
		/* The gpueb remapped region is fixed, only
		 * 0x4000_0000ULL ~ 0x8FFF_FFFFULL is accessible.
		 */
		gpueb_pr_debug("@%s: Error: Wrong Address (0x%llx)\n",
			    __func__, (uint64_t)gpueb_mem_base_phys);
		BUG_ON(1);
		return -1;
	}

    /* Set reserved memory table */
	gpueb_mem_num = of_property_count_u32_elems(
				pdev->dev.of_node,
				"gpueb_mem_tbl")
				/ MEMORY_TBL_ELEM_NUM;
	if (gpueb_mem_num <= 0) {
		gpueb_pr_debug("@%s: gpueb_mem_tbl not found\n",
            __func__);
		gpueb_mem_num = 0;
	}

	for (i = 0; i < gpueb_mem_num; i++) {
        // Get reserved block's ID
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"gpueb_mem_tbl",
				i * MEMORY_TBL_ELEM_NUM,
				&m_idx);
		if (ret) {
			gpueb_pr_debug("@%s: Cannot get memory index(%d)\n", __func__, i);
			return -1;
		}

        // Get reserved block's size
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"gpueb_mem_tbl",
				(i * MEMORY_TBL_ELEM_NUM) + 1,
				&m_size);
		if (ret) {
			gpueb_pr_debug("@%s: Cannot get memory size(%d)\n", __func__, i);
			return -1;
		}

		if (m_idx >= NUMS_MEM_ID) {
			gpueb_pr_debug("@%s: Skip unexpected index, %d\n", __func__, m_idx);
			continue;
		}

		gpueb_reserve_mblock_ary[m_idx].size = m_size;
		gpueb_pr_debug("@%s: Reserved block <%d  %d>\n", __func__, m_idx, m_size);
	}

	gpueb_mem_base_virt = (phys_addr_t)(size_t)ioremap_wc(gpueb_mem_base_phys,
		gpueb_mem_size);
	gpueb_pr_debug("@%s: Reserved phy_base = 0x%llx, len:0x%llx\n, "
                    "Reserved virt_base = 0x%llx",
                    __func__,
		            (uint64_t)gpueb_mem_base_phys,
                    (uint64_t)gpueb_mem_size,
                    (uint64_t)gpueb_mem_base_virt);

	for (i = 0; i < NUMS_MEM_ID; i++) {
		gpueb_reserve_mblock_ary[i].start_phys = gpueb_mem_base_phys +
			accumlate_memory_size;
		gpueb_reserve_mblock_ary[i].start_virt = gpueb_mem_base_virt +
			accumlate_memory_size;
		accumlate_memory_size += gpueb_reserve_mblock_ary[i].size;
		gpueb_pr_debug("@%s: Reserved block[%d] phys:0x%llx, virt:0x%llx, "
                        "len:0x%llx\n",
                        __func__,
                        i, (uint64_t)gpueb_reserve_mblock_ary[i].start_phys,
                        (uint64_t)gpueb_reserve_mblock_ary[i].start_virt,
                        (uint64_t)gpueb_reserve_mblock_ary[i].size);
	}

    if (accumlate_memory_size > gpueb_mem_size)
        gpueb_pr_debug("@%s: Total memory in memory table is more than "
                        "reserved", __func__);

    return 0;
}
