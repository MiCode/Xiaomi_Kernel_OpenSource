// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>

#include "mcupm_driver.h"
#include "mcupm_plt.h"
#include "mcupm_ipi_id.h"
#include "mcupm_ipi_table.h"


#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#define MCUPM_MEM_RESERVED_KEY "mediatek,reserve-memory-mcupm_share"
#endif

/* Todo implement mcupm driver pdata*/
struct platform_device *mcupm_pdev;
spinlock_t mcupm_mbox_lock[MCUPM_MBOX_TOTAL];

int mcupm_plt_ackdata;

static int mtk_ipi_init(struct platform_device *pdev)
{
	int i = 0;
	int ret;
	struct device *dev = &pdev->dev;
	void __iomem *base;
	struct resource *res;
	char name[32];

	mcupm_mbox_table[i].mbdev = &mcupm_mboxdev;
	ret = mtk_mbox_probe(pdev, mcupm_mbox_table[i].mbdev, i);
	if (ret) {
		pr_debug("[MCUPM] mbox(0) probe fail on mbox-0, ret %d\n", ret);
		return ret;
	}

	for (i = 1; i < MCUPM_MBOX_TOTAL; i++) {
		mcupm_mbox_table[i].mbdev = &mcupm_mboxdev;
		snprintf(name, sizeof(name), "mbox%d_base", i);
		res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, name);
		base = devm_ioremap_resource(dev, res);
		if (IS_ERR((void const *) base)) {
			ret = PTR_ERR(base);
			pr_debug("mbox-%d can't remap base\n", i);
			return ret;
		}
		ret = mtk_smem_init(pdev, mcupm_mbox_table[i].mbdev, i, base,
		mcupm_mbox_table[0].mbdev->info_table[0].set_irq_reg,
		mcupm_mbox_table[0].mbdev->info_table[0].clr_irq_reg,
		mcupm_mbox_table[0].mbdev->info_table[0].send_status_reg,
		mcupm_mbox_table[0].mbdev->info_table[0].recv_status_reg);
		if (ret) {
			pr_debug("[MCUPM] mbox probe fail on mbox-%d, ret %d\n",
				i, ret);
			return ret;
		}
	}

	pr_debug("[MCUPM] ipi register\n");
	ret = mtk_ipi_device_register(&mcupm_ipidev, pdev, &mcupm_mboxdev,
				      MCUPM_IPI_COUNT);
	if (ret) {
		pr_debug("[MCUPM] ipi_dev_register fail, ret %d\n", ret);
		return ret;
	}

    /* Initialize mcupm ipi driver. Move to struct mtk_mcupm */
	ret = mtk_ipi_register(&mcupm_ipidev, CH_S_PLATFORM, NULL, NULL,
			       (void *) &mcupm_plt_ackdata);
	if (ret) {
		pr_debug("[MCUPM] ipi_register fail, ret %d\n", ret);
		return ret;
	}
	return 0;
}

/* MCUPM RESERVED MEM */
static phys_addr_t mcupm_mem_base_phys;
static phys_addr_t mcupm_mem_base_virt;
static phys_addr_t mcupm_mem_size;

#ifdef CONFIG_OF_RESERVED_MEM
static struct mcupm_reserve_mblock mcupm_reserve_mblock[NUMS_MCUPM_MEM_ID] = {
	{
		.num = MCUPM_MEM_ID,
		.size = 0x100 + MCUPM_PLT_LOGGER_BUF_LEN,
		/* logger header + 1M log buffer */
	},
#if !defined(CONFIG_MTK_GMO_RAM_OPTIMIZE) && !defined(CONFIG_MTK_MET_MEM_ALLOC)
	{
		.num = MCUPM_MET_ID,
		.size = MCUPM_MET_LOGGER_BUF_LEN,
	},
#endif
	{
		.num = MCUPM_EEMSN_MEM_ID,
		.size = MCUPM_PLT_EEMSN_BUF_LEN,
	},
	{
		.num = MCUPM_BRISKET_ID,
		.size = MCUPM_BRISKET_BUF_LEN,
	},
};

phys_addr_t mcupm_reserve_mem_get_phys(unsigned int id)
{
	if (id >= NUMS_MCUPM_MEM_ID) {
		pr_info("[MCUPM] no reserve memory for 0x%x", id);
		return 0;
	} else
		return mcupm_reserve_mblock[id].start_phys;
}
EXPORT_SYMBOL_GPL(mcupm_reserve_mem_get_phys);

phys_addr_t mcupm_reserve_mem_get_virt(unsigned int id)
{
	if (id >= NUMS_MCUPM_MEM_ID) {
		pr_info("[MCUPM] no reserve memory for 0x%x", id);
		return 0;
	} else
		return (phys_addr_t)mcupm_reserve_mblock[id].start_virt;
}
EXPORT_SYMBOL_GPL(mcupm_reserve_mem_get_virt);

phys_addr_t mcupm_reserve_mem_get_size(unsigned int id)
{
	if (id >= NUMS_MCUPM_MEM_ID) {
		pr_info("[MCUPM] no reserve memory for 0x%x", id);
		return 0;
	} else
		return mcupm_reserve_mblock[id].size;
}
EXPORT_SYMBOL_GPL(mcupm_reserve_mem_get_size);

#if defined(MODULE)
//Todo add struct platform_device *pdev in parameter
static int mcupm_map_memory_region(void)
{
	struct device_node *rmem_node;
	struct reserved_mem *rmem;

	/* Get reserved memory */
	rmem_node = of_find_compatible_node(NULL, NULL, MCUPM_MEM_RESERVED_KEY);
	if (!rmem_node) {
		pr_info("[MCUPM] no node for reserved memory\n");
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(rmem_node);
	if (!rmem) {
		pr_info("[MCUPM] cannot lookup reserved memory\n");
		return -EINVAL;
	}

	mcupm_mem_base_phys = (phys_addr_t) rmem->base;
	mcupm_mem_size = (phys_addr_t) rmem->size;

	WARN_ON(!(mcupm_mem_base_phys && mcupm_mem_size));

    /* Mapping the MCUPM's SRAM address /
     * DMEM (Data Extended Memory) memory address /
     * Working buffer memory address to
     * kernel virtual address.
     */
	mcupm_mem_base_virt = (phys_addr_t)(uintptr_t)
		ioremap_wc(mcupm_mem_base_phys, mcupm_mem_size);

	if (!mcupm_mem_base_virt)
		return -ENOMEM;

	pr_info("[MCUPM]reserve mem: virt:0x%llx - 0x%llx (0x%llx)\n",
		 (unsigned long long)mcupm_mem_base_virt,
		 (unsigned long long)mcupm_mem_base_virt +
		 (unsigned long long)mcupm_mem_size,
		 (unsigned long long)mcupm_mem_size);

	return 0;
}
static int mcupm_assign_memory_block(void)
{
	int ret = 0;
	unsigned int id;
	phys_addr_t accumlate_memory_size = 0;

    //Todo consider remove global variable
	WARN_ON(!(mcupm_mem_base_phys && mcupm_mem_size));

	for (id = 0; id < NUMS_MCUPM_MEM_ID; id++) {
		mcupm_reserve_mblock[id].start_phys = mcupm_mem_base_phys +
			accumlate_memory_size;
		accumlate_memory_size += mcupm_reserve_mblock[id].size;

		pr_debug("[MCUPM][reserve_mem:%d]: ", id);
		pr_debug("phys:0x%llx - 0x%llx (0x%llx)\n",
			 mcupm_reserve_mblock[id].start_phys,
			 mcupm_reserve_mblock[id].start_phys +
			 mcupm_reserve_mblock[id].size,
			 mcupm_reserve_mblock[id].size);
	}

	for (id = 0; id < NUMS_MCUPM_MEM_ID; id++) {
		mcupm_reserve_mblock[id].start_virt = mcupm_mem_base_virt +
			accumlate_memory_size;
		accumlate_memory_size += mcupm_reserve_mblock[id].size;
	}

#ifdef MCUPM_RESERVED_DEBUG
	for (id = 0; id < NUMS_MCUPM_MEM_ID; id++) {
		pr_debug("[MCUPM][mem_reserve-%d] ", id);
		pr_debug("phys:0x%llx,virt:0x%llx,size:0x%llx\n",
			 (unsigned long long)mcupm_reserve_mem_get_phys(id),
			 (unsigned long long)mcupm_reserve_mem_get_virt(id),
			 (unsigned long long)mcupm_reserve_mem_get_size(id));
	}
#endif

	return ret;
}
#else
static int __init mcupm_reserve_mem_of_init(struct reserved_mem *rmem)
{
	unsigned int id;
	phys_addr_t accumlate_memory_size = 0;

	mcupm_mem_base_phys = (phys_addr_t) rmem->base;
	mcupm_mem_size = (phys_addr_t) rmem->size;

	WARN_ON(!(mcupm_mem_base_phys && mcupm_mem_size));

	pr_debug("[MCUPM] phys:0x%llx - 0x%llx (0x%llx)\n",
		 (unsigned long long)rmem->base,
		 (unsigned long long)rmem->base +
		 (unsigned long long)rmem->size,
		 (unsigned long long)rmem->size);

	for (id = 0; id < NUMS_MCUPM_MEM_ID; id++) {
		mcupm_reserve_mblock[id].start_phys = mcupm_mem_base_phys +
			accumlate_memory_size;
		accumlate_memory_size += mcupm_reserve_mblock[id].size;

		pr_debug("[MCUPM][reserve_mem:%d]: ", id);
		pr_debug("phys:0x%llx - 0x%llx (0x%llx)\n",
			 mcupm_reserve_mblock[id].start_phys,
			 mcupm_reserve_mblock[id].start_phys +
			 mcupm_reserve_mblock[id].size,
			 mcupm_reserve_mblock[id].size);
	}

    //Todo: combin mcupm_reserve_mblock start_phys, start_virt
    //    and size in same loop
	accumlate_memory_size = 0;
	for (id = 0; id < NUMS_MCUPM_MEM_ID; id++) {
		mcupm_reserve_mblock[id].start_virt = mcupm_mem_base_virt +
			accumlate_memory_size;
		accumlate_memory_size += mcupm_reserve_mblock[id].size;
	}

	WARN_ON(accumlate_memory_size > mcupm_mem_size);
#ifdef MCUPM_RESERVED_DEBUG
	for (id = 0; id < NUMS_MCUPM_MEM_ID; id++) {
		pr_debug("[MCUPM][mem_reserve-%d] ", id);
		pr_debug("phys:0x%llx,virt:0x%llx,size:0x%llx\n",
			 (unsigned long long)mcupm_reserve_mem_get_phys(id),
			 (unsigned long long)mcupm_reserve_mem_get_virt(id),
			 (unsigned long long)mcupm_reserve_mem_get_size(id));
	}
#endif
	return 0;
}
RESERVEDMEM_OF_DECLARE(mcupm_reservedmem, MCUPM_MEM_RESERVED_KEY,
	mcupm_reserve_mem_of_init);

static int mcupm_reserve_memory_init(void)
{
	unsigned int id;
	phys_addr_t accumlate_memory_size;

	if (NUMS_MCUPM_MEM_ID == 0)
		return 0;

	if (mcupm_mem_base_phys == 0)
		return -ENOMEM;

	accumlate_memory_size = 0;
	mcupm_mem_base_virt = (phys_addr_t)(uintptr_t)
		ioremap_wc(mcupm_mem_base_phys, mcupm_mem_size);

	if (!mcupm_mem_base_virt)
		return -ENOMEM;

	pr_debug("[MCUPM]reserve mem: virt:0x%llx - 0x%llx (0x%llx)\n",
		 (unsigned long long)mcupm_mem_base_virt,
		 (unsigned long long)mcupm_mem_base_virt +
		 (unsigned long long)mcupm_mem_size,
		 (unsigned long long)mcupm_mem_size);

	for (id = 0; id < NUMS_MCUPM_MEM_ID; id++) {
		mcupm_reserve_mblock[id].start_virt = mcupm_mem_base_virt +
			accumlate_memory_size;
		accumlate_memory_size += mcupm_reserve_mblock[id].size;
	}

	WARN_ON(accumlate_memory_size > mcupm_mem_size);
#ifdef MCUPM_RESERVED_DEBUG
	for (id = 0; id < NUMS_MCUPM_MEM_ID; id++) {
		pr_debug("[MCUPM][mem_reserve-%d] ", id);
		pr_debug("phys:0x%llx,virt:0x%llx,size:0x%llx\n",
			 (unsigned long long)mcupm_reserve_mem_get_phys(id),
			 (unsigned long long)mcupm_reserve_mem_get_virt(id),
			 (unsigned long long)mcupm_reserve_mem_get_size(id));
	}
#endif

	return 0;
}

#endif
#endif


/* MCUPM HELPER. User is apmcu_sspm_mailbox_read/write*/
int mcupm_mbox_read(unsigned int mbox, unsigned int slot, void *buf,
			unsigned int len)
{
	if (WARN_ON(len > (mcupm_mboxdev.pin_send_table[mbox]).msg_size)) {
		pr_debug("mbox:%u warning\n", mbox);
		return -EINVAL;
	}

	return mtk_mbox_read(&mcupm_mboxdev, mbox, slot,
				buf, len * MBOX_SLOT_SIZE);
}
EXPORT_SYMBOL_GPL(mcupm_mbox_read);

int mcupm_mbox_write(unsigned int mbox, unsigned int slot, void *buf,
			unsigned int len)
{
	unsigned long flags;
	unsigned int status;
	int ret;

	if (WARN_ON(len > (mcupm_mboxdev.pin_send_table[mbox]).msg_size) ||
		WARN_ON(!buf)) {
		pr_debug("mbox:%u warning\n", mbox);
		return -EINVAL;
	}

	spin_lock_irqsave(&mcupm_mbox_lock[mbox], flags);
	status = mtk_mbox_check_send_irq(&mcupm_mboxdev, mbox,
				(mcupm_mboxdev.pin_send_table[mbox]).pin_index);
	if (status != 0) {
		spin_unlock_irqrestore(&mcupm_mbox_lock[mbox], flags);
		return MBOX_PIN_BUSY;
	}
	spin_unlock_irqrestore(&mcupm_mbox_lock[mbox], flags);

	ret = mtk_mbox_write(&mcupm_mboxdev, mbox, slot
				, buf, len * MBOX_SLOT_SIZE);
	if (ret != MBOX_DONE)
		return ret;
	/* Ensure that all writes to SRAM are committed */
	mb();

	return 0;
}
EXPORT_SYMBOL_GPL(mcupm_mbox_write);

void *get_mcupm_ipidev(void)
{
	return &mcupm_ipidev;
}
EXPORT_SYMBOL_GPL(get_mcupm_ipidev);

static int mcupm_device_probe(struct platform_device *pdev)
{
	int i, ret;

	mcupm_pdev = pdev;

	pr_debug("[MCUPM] mbox probe\n");

#ifdef CONFIG_OF_RESERVED_MEM
#if defined(MODULE)
	if (mcupm_map_memory_region()) {
		pr_info("[MCUPM] Reserved Memory Failed\n");
		return -ENOMEM;
	}
	if (mcupm_assign_memory_block()) {
		pr_info("[MCUPM] assign phys, virt address and size Failed\n");
		return -ENOMEM;
	}
#else
	if (mcupm_reserve_memory_init()) {
		pr_info("[MCUPM] Reserved Memory Failed\n");
		return -ENOMEM;
	}
#endif
#endif

	ret = mtk_ipi_init(pdev);
	if (ret) {
		pr_debug("[MCUPM] ipi interface init fail, ret %d\n", ret);
		return ret;
	}

	/* Initialize spin_lock for MCUPM HELPER for internal use. */
	for (i = 0; i < MCUPM_MBOX_TOTAL; i++)
		spin_lock_init(&mcupm_mbox_lock[i]);

	pr_info("MCUPM is ready to service IPI\n");

	ret = mcupm_plt_module_init();
	if (ret) {
		pr_debug("[MCUPM] plt module init fail, ret %d\n", ret);
		return ret;
	}

	return 0;
}
static int mcupm_device_remove(struct platform_device *pdev)
{
	//Todo implement remove ipi interface and memory
	mcupm_plt_module_exit();
	return 0;
}

static const struct of_device_id mcupm_of_match[] = {
	{ .compatible = "mediatek,mcupm", },
	{},
};

static const struct platform_device_id mcupm_id_table[] = {
	{ "mcupm", 0},
	{ },
};

static struct platform_driver mtk_mcupm_driver = {
	.shutdown = NULL,
	.suspend = NULL,
	.resume = NULL,
	.probe = mcupm_device_probe,
	.remove = mcupm_device_remove,
	.driver = {
		.name = "mcupm",
		.owner = THIS_MODULE,
		.of_match_table = mcupm_of_match,
	},
	.id_table = mcupm_id_table,
};

/*
 * driver initialization entry point
 */
static int __init mcupm_module_init(void)
{
	int ret = 0;

	pr_info("[MCUPM] mcupm module init.\n");

	ret = platform_driver_register(&mtk_mcupm_driver);

	return ret;
}

static void __exit mcupm_module_exit(void)
{
    //Todo release resource
	pr_info("[MCUPM] mcupm module exit.\n");
}

MODULE_DESCRIPTION("MEDIATEK Module MCUPM driver");
MODULE_LICENSE("GPL v2");

module_init(mcupm_module_init);
module_exit(mcupm_module_exit);
