// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by kmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <mt-plat/sync_write.h>
#include "sspm_define.h"
#include "sspm_helper.h"
#include "sspm_excep.h"
#include "sspm_reservedmem.h"
#define _SSPM_INTERNAL_
#include "sspm_reservedmem_define.h"
#define MEMORY_TBL_ELEM_NUM (2)

#if IS_ENABLED(CONFIG_OF_RESERVED_MEM)
#include <linux/of_reserved_mem.h>

#define SSPM_MEM_RESERVED_KEY "mediatek,reserve-memory-sspm_share"
#endif

static phys_addr_t sspm_mem_base_phys;
static phys_addr_t sspm_mem_base_virt;
static phys_addr_t sspm_mem_size;

static void sspm_reserve_memory_ioremap(struct platform_device *pdev)
{
	unsigned int id;
	phys_addr_t accumlate_memory_size = 0;

	struct device_node *rmem_node;
	struct reserved_mem *rmem;

	/* Get reserved memory */
	rmem_node = of_find_compatible_node(NULL, NULL, SSPM_MEM_RESERVED_KEY);
	if (!rmem_node) {
		pr_err("[SSPM] no node for reserved memory\n");
		return;
	}

	rmem = of_reserved_mem_lookup(rmem_node);
	if (!rmem) {
		pr_err("[SSPM] cannot lookup reserved memory\n");
		return;
	}

	sspm_mem_base_phys = rmem->base;
	sspm_mem_size = rmem->size;

	pr_debug("[SSPM] phys:0x%llx - 0x%llx (0x%llx)\n",
		(unsigned long long)rmem->base,
		(unsigned long long)rmem->base + (unsigned long long)rmem->size,
		(unsigned long long)rmem->size);
	accumlate_memory_size = 0;
	for (id = 0; id < NUMS_MEM_ID; id++) {
		sspm_reserve_mblock[id].start_phys = sspm_mem_base_phys +
							accumlate_memory_size;
		accumlate_memory_size += sspm_reserve_mblock[id].size;

		pr_debug("[SSPM][reserve_mem:%d]: ", id);
		pr_debug("phys:0x%llx - 0x%llx (0x%llx)\n",
			sspm_reserve_mblock[id].start_phys,
			sspm_reserve_mblock[id].start_phys +
				sspm_reserve_mblock[id].size,
			sspm_reserve_mblock[id].size);
	}
}

phys_addr_t sspm_reserve_mem_get_phys(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_err("[SSPM] no reserve memory for 0x%x", id);
		return 0;
	} else
		return sspm_reserve_mblock[id].start_phys;
}
EXPORT_SYMBOL_GPL(sspm_reserve_mem_get_phys);

phys_addr_t sspm_reserve_mem_get_virt(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_err("[SSPM] no reserve memory for 0x%x", id);
		return 0;
	} else
		return sspm_reserve_mblock[id].start_virt;
}
EXPORT_SYMBOL_GPL(sspm_reserve_mem_get_virt);

phys_addr_t sspm_reserve_mem_get_size(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_err("[SSPM] no reserve memory for 0x%x", id);
		return 0;
	} else
		return sspm_reserve_mblock[id].size;
}
EXPORT_SYMBOL_GPL(sspm_reserve_mem_get_size);

void __iomem *sspm_base;
phys_addr_t sspm_sbuf_get(unsigned int offset)
{
	if (!is_sspm_ready()) {
		pr_notice("[SSPM] device resource is not ready\n");
		return 0;
	}

	if (offset < SSPM_SHARE_REGION_BASE ||
		offset > SSPM_SHARE_REGION_BASE + SSPM_SHARE_REGION_SIZE) {
		pr_notice("[SSPM] illegal sbuf request: 0x%x\n", offset);
		return 0;
	} else {
		return (phys_addr_t)(sspm_base + offset);
	}
}
EXPORT_SYMBOL_GPL(sspm_sbuf_get);

int sspm_reserve_memory_init(struct platform_device *pdev)
{
	unsigned int id;
	unsigned int sspm_mem_num = 0;
	unsigned int i, m_idx, m_size;
	int ret;
	const char *mem_key;
	phys_addr_t accumlate_memory_size;

	if (NUMS_MEM_ID == 0)
		return 0;

	sspm_reserve_memory_ioremap(sspm_pdev);

	if (sspm_mem_base_phys == 0)
		return -1;
	/* Get reserved memory */
	ret = of_property_read_string(pdev->dev.of_node, "sspm_mem_key",
			&mem_key);
	if (ret) {
		pr_info("[SSPM] cannot find property\n");
		return -EINVAL;
	}

	/* Set reserved memory table */
	sspm_mem_num = of_property_count_u32_elems(
				pdev->dev.of_node,
				"sspm_mem_tbl")
				/ MEMORY_TBL_ELEM_NUM;
	if (sspm_mem_num <= 0) {
		pr_info("[SSPM] SSPM_mem_tbl not found\n");
		sspm_mem_num = 0;
	}
	for (i = 0; i < sspm_mem_num; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"sspm_mem_tbl",
				i * MEMORY_TBL_ELEM_NUM,
				&m_idx);
		if (ret) {
			pr_info("Cannot get memory index(%d)\n", i);
			return -1;
		}
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"sspm_mem_tbl",
				(i * MEMORY_TBL_ELEM_NUM) + 1,
				&m_size);
		if (ret) {
			pr_info("Cannot get memory size(%d)\n", i);
			return -1;
		}
		if (m_idx >= NUMS_MEM_ID) {
			pr_notice("[SSPM] skip unexpected index, %d\n", m_idx);
			continue;
		}
	}
	accumlate_memory_size = 0;
	sspm_mem_base_virt = (phys_addr_t)(uintptr_t)
			ioremap_wc(sspm_mem_base_phys, sspm_mem_size);

	pr_debug("[SSPM]reserve mem: virt:0x%llx - 0x%llx (0x%llx)\n",
			(unsigned long long)sspm_mem_base_virt,
			(unsigned long long)sspm_mem_base_virt +
				(unsigned long long)sspm_mem_size,
			(unsigned long long)sspm_mem_size);

	for (id = 0; id < NUMS_MEM_ID; id++) {
		sspm_reserve_mblock[id].start_virt = sspm_mem_base_virt +
							accumlate_memory_size;
		accumlate_memory_size += sspm_reserve_mblock[id].size;
	}
	/* the reserved memory should be larger then expected memory
	 * or sspm_reserve_mblock does not match dts
	 */

	WARN_ON(accumlate_memory_size > sspm_mem_size);
#ifdef DEBUG
	for (id = 0; id < NUMS_MEM_ID; id++) {
		pr_debug("[SSPM][mem_reserve-%d] ", id);
		pr_debug("phys:0x%llx,virt:0x%llx,size:0x%llx\n",
			(unsigned long long)sspm_reserve_mem_get_phys(id),
			(unsigned long long)sspm_reserve_mem_get_virt(id),
			(unsigned long long)sspm_reserve_mem_get_size(id));
	}
#endif

	return 0;
}

void sspm_lock_emi_mpu(void)
{
#if SSPM_EMI_PROTECTION_SUPPORT
	if (sspm_mem_size > 0)
		sspm_set_emi_mpu(sspm_mem_base_phys, sspm_mem_size);
#endif
}


int sspm_sbuf_init(void)
{
	struct device *dev = &sspm_pdev->dev;
	struct resource *res;

	if (sspm_pdev) {
		res = platform_get_resource_byname(sspm_pdev,
			IORESOURCE_MEM, "sspm_base");
		sspm_base = devm_ioremap_resource(dev, res);

		if (IS_ERR((void const *) sspm_base))
			return -1;
	}
	return 0;
}
