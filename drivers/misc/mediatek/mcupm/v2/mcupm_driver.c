// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>

#include "mcupm_driver.h"
#include "mcupm_plt.h"
#include "mcupm_ipi_id.h"
#include "mcupm_ipi_table.h"
#include "mcupm_timesync.h"

#if IS_ENABLED(CONFIG_MTK_EMI)
#include <soc/mediatek/emi.h>
#include "mcupm_emi_mpu.h"
#endif

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#define MCUPM_MEM_RESERVED_KEY "mediatek,reserve-memory-mcupm_share"
bool has_reserved_memory;
bool skip_logger;
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

#if IS_ENABLED(CONFIG_MTK_EMI)
static unsigned long long mcupm_start;
static unsigned long long mcupm_end;
#endif

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

	accumlate_memory_size = 0;
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
static int mcupm_map_memory_region(void)
{
	struct device_node *rmem_node;
	struct reserved_mem *rmem;
	unsigned int id;

	/* Get reserved memory */
	rmem_node = of_find_compatible_node(NULL, NULL, MCUPM_MEM_RESERVED_KEY);
	if (!rmem_node) {
		pr_info("[MCUPM] no node for reserved memory\n");
		has_reserved_memory = false;
		skip_logger = true;
		for (id = 0; id < NUMS_MCUPM_MEM_ID; id++) {
			mcupm_reserve_mblock[id].start_phys = 0x0;
			mcupm_reserve_mblock[id].start_virt = 0x0;
		}
		return 0;
	}

	has_reserved_memory = true;

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

	if (mcupm_assign_memory_block()) {
		pr_info("[MCUPM] assign phys, virt address and size Failed\n");
		return -ENOMEM;
	}

	return 0;
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

static struct task_struct *mcupm_task;
#define UBUS_BASE   0x0C800000
#define CLUSTER_MPAM_BASE   0X10000
#define MPAMCFG_PART_SEL_OFS    0x100
#define MPAMCFG_CPBM_NS_OFS 0x1000
#define MPAMCFG_PART_SEL    (UBUS_BASE+CLUSTER_MPAM_BASE+MPAMCFG_PART_SEL_OFS)
#define MPAMCFG_CPBM    (UBUS_BASE+CLUSTER_MPAM_BASE+MPAMCFG_CPBM_NS_OFS)
#define UBUS_MPAM_SIZE  0x1100
static phys_addr_t mpam_base_virt;

int mcupm_thread(void *data)
{
	int ipi_recv_d = 0, ret = 0, i;
	struct mcupm_ipi_data_s ipi_data;
	unsigned int cmd, partid, cache_por;

	ipi_data.cmd = MCUPM_PLT_SERV_READY;
	mcupm_plt_ackdata = 0;

	ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_PLATFORM, IPI_SEND_POLLING,
			&ipi_data,
			sizeof(struct mcupm_ipi_data_s) / MCUPM_MBOX_SLOT_SIZE,
			2000);

	if (ret) {
		pr_info("MCUPM: plt IPI fail ret=%d, ackdata=%d\n",
				ret, mcupm_plt_ackdata);
	}

	if (mcupm_plt_ackdata < 0) {
		pr_info("MCUPM: plt IPI init fail, ackdata=%d\n",
				mcupm_plt_ackdata);
	}

	//mmap utility bus
	mpam_base_virt =
		(phys_addr_t)(uintptr_t)ioremap(UBUS_BASE+CLUSTER_MPAM_BASE, UBUS_MPAM_SIZE);

	if (!mpam_base_virt)
		return -ENOMEM;

	do {
		mtk_ipi_recv(&mcupm_ipidev, CH_S_PLATFORM);
		ipi_recv_d = mcupm_plt_ackdata;

		cmd = (ipi_recv_d >> 28) & 0xF;
		partid = (ipi_recv_d >> 16) & 0xFFF;
		cache_por = ipi_recv_d & 0xFFFF;

		if (cmd != 0)
			continue;
		//part sel
		iowrite32(partid, (void __iomem *)(mpam_base_virt+MPAMCFG_PART_SEL_OFS));
		//set cpbm
		ret = 0;
		for (i = 0; i < cache_por; i++)
			ret = (ret << 1) + 1;
		iowrite32(ret, (void __iomem *)(mpam_base_virt+MPAMCFG_CPBM_NS_OFS));
	} while (!kthread_should_stop());

	return 0;
}
#if IS_ENABLED(CONFIG_MTK_EMI)
static void mcupm_set_emi_mpu(phys_addr_t base, phys_addr_t size)
{
	mcupm_start = base;
	mcupm_end = base + size - 1;
}

static void mcupm_lock_emi_mpu(void)
{
	if (mcupm_mem_size > 0)
		mcupm_set_emi_mpu(mcupm_mem_base_phys, mcupm_mem_size);
}

static int mcupm_init_emi_mpu(void)
{
	struct emimpu_region_t rg_info;

	mtk_emimpu_init_region(&rg_info, MUCPM_MPU_REGION_ID);

	mtk_emimpu_set_addr(&rg_info, mcupm_start, mcupm_end);

	mtk_emimpu_set_apc(&rg_info, 0, MTK_EMIMPU_NO_PROTECTION);

	mtk_emimpu_set_apc(&rg_info, MUCPM_MPU_DOMAIN_ID,
						MTK_EMIMPU_NO_PROTECTION);

	mtk_emimpu_set_protection(&rg_info);

	mtk_emimpu_free_region(&rg_info);

	return 0;
}
#endif

static int mcupm_device_probe(struct platform_device *pdev)
{
	int i, ret;
	struct device *dev = &pdev->dev;

	mcupm_pdev = pdev;

	pr_debug("[MCUPM] mbox probe\n");

	if (of_property_read_bool(dev->of_node, "skip-logger"))
		skip_logger = true;
	else
		skip_logger = false;

#ifdef CONFIG_OF_RESERVED_MEM
#if defined(MODULE)
	if (mcupm_map_memory_region()) {
		pr_info("[MCUPM] Reserved Memory Failed\n");
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

#if IS_ENABLED(CONFIG_MTK_EMI)
	if (has_reserved_memory == true) {
		mcupm_lock_emi_mpu();
		mcupm_init_emi_mpu();
	} else
		pr_debug("[MCUPM] no need to set emi mpu\n");
#endif

	ret = mcupm_plt_module_init();
	if (ret) {
		pr_info("[MCUPM] plt module init fail, ret %d\n", ret);
		return ret;
	}

	ret = mcupm_timesync_init();
	if (ret) {
		pr_info("MCUPM timesync init fail\n");
		return ret;
	}

	mcupm_task = kthread_run(mcupm_thread, NULL, "mcupm_task");

	return 0;
}
static int mcupm_device_remove(struct platform_device *pdev)
{
	//Todo implement remove ipi interface and memory
	mcupm_plt_module_exit();
	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int mt6779_mcupm_suspend(struct device *dev)
{
	mcupm_timesync_suspend();
	return 0;
}

static int mt6779_mcupm_resume(struct device *dev)
{
	mcupm_timesync_resume();
	return 0;
}

static const struct dev_pm_ops mt6779_mcupm_dev_pm_ops = {
	.suspend = mt6779_mcupm_suspend,
	.resume  = mt6779_mcupm_resume,
};
#endif

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
#if IS_ENABLED(CONFIG_PM)
		.pm = &mt6779_mcupm_dev_pm_ops,
#endif

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
