// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include "sspm_define.h"
#include "sspm_common.h"
#include "sspm_reservedmem.h"


#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>

#define SSPM_MEM_RESERVED_KEY "mediatek,reserve-memory-sspm_share"
#endif

static phys_addr_t sspm_mem_base_phys;
static phys_addr_t sspm_mem_base_virt;
static phys_addr_t sspm_mem_size;

struct sspm_reserve_mblock *sspm_reserve_mblock;

#if defined(MODULE)
static void sspm_reserve_memory_ioremap(struct platform_device *pdev)
{
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

	sspm_mem_base_phys = (phys_addr_t) rmem->base;
	sspm_mem_size = (phys_addr_t) rmem->size;
}

#elif defined(CONFIG_OF_RESERVED_MEM)
static int __init sspm_reserve_mem_of_init(struct reserved_mem *rmem)
{
	sspm_mem_base_phys = rmem->base;
	sspm_mem_size      = rmem->size;

#ifdef DEBUG
	pr_debug("[SSPM] phys:0x%llx - 0x%llx (0x%llx)\n",
		(unsigned long long)rmem->base,
		(unsigned long long)(rmem->base + rmem->size),
		(unsigned long long)rmem->size);
#endif

	return 0;
}
RESERVEDMEM_OF_DECLARE(sspm_reservedmem, SSPM_MEM_RESERVED_KEY,
	sspm_reserve_mem_of_init);
#endif

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

int sspm_reserve_memory_init(void)
{
	unsigned int id;
	phys_addr_t accumlate_memory_size;

	if (NUMS_MEM_ID == 0)
		return 0;

#if defined(MODULE)
	sspm_reserve_memory_ioremap(sspm_pdev);
#endif

	if (!sspm_mem_base_phys)
		return -1;

    /* Phy memory */
	accumlate_memory_size = 0;
	for (id = 0; id < NUMS_MEM_ID; id++) {
		sspm_reserve_mblock[id].start_phys = sspm_mem_base_phys +
							accumlate_memory_size;

		accumlate_memory_size += sspm_reserve_mblock[id].size;
	}

    /* Virt memory */
	accumlate_memory_size = 0;
	sspm_mem_base_virt = (phys_addr_t)(uintptr_t)
			ioremap_wc(sspm_mem_base_phys, sspm_mem_size);

#ifdef DEBUG
	pr_info("[SSPM]reserve mem: virt:0x%llx - 0x%p (0x%llx)\n",
			sspm_mem_base_virt,
			sspm_mem_base_virt + sspm_mem_size,
			sspm_mem_size);
#endif

	for (id = 0; id < NUMS_MEM_ID; id++) {
		sspm_reserve_mblock[id].start_virt = sspm_mem_base_virt +
							accumlate_memory_size;
		accumlate_memory_size += sspm_reserve_mblock[id].size;
	}
	/* the reserved memory should be larger then expected memory
	 * or sspm_reserve_mblock does not match dts
	 */

	BUG_ON(accumlate_memory_size > sspm_mem_size);

#ifdef DEBUG
	for (id = 0; id < NUMS_MEM_ID; id++) {
		pr_info("[SSPM][mem_reserve-%d] ", id);
		pr_info("phys:0x%llx, virt:0x%llx,size:0x%llx\n",
			(unsigned long long)sspm_reserve_mem_get_phys(id),
			(unsigned long long)sspm_reserve_mem_get_virt(id),
			(unsigned long long)sspm_reserve_mem_get_size(id));
	}
#endif

	return 0;
}

void sspm_lock_emi_mpu(unsigned int region)
{
#ifdef CONFIG_MTK_EMI
	if (sspm_mem_size > 0)
		sspm_set_emi_mpu(region, sspm_mem_base_phys, sspm_mem_size);
#endif
}

#ifdef SSPM_SHARE_BUFFER_SUPPORT
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

int sspm_sbuf_init(void)
{
	struct device *dev = &sspm_pdev->dev;
	struct resource *res;

	if (sspm_pdev) {
		res = platform_get_resource_byname(sspm_pdev,
			IORESOURCE_MEM, "sspm_base");
		if (!res)
			return -1;

		sspm_base = devm_ioremap_resource(dev, res);
		if (IS_ERR((void const *) sspm_base))
			return -1;
	}
	return 0;
}
#endif
