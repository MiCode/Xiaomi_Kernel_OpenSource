// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>       /* min() */
#include <linux/memblock.h>
#include <linux/uaccess.h>      /* copy_to_user() */
#include <linux/sched.h>        /* TASK_INTERRUPTIBLE/signal_pending/schedule */
#include <linux/sched/clock.h>
#include <uapi/linux/sched/types.h>
#include <linux/poll.h>
#include <linux/io.h>           /* ioremap() */
#include <linux/cma.h>
#include <linux/suspend.h>
#include <asm/cacheflush.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/seq_file.h>
#include <asm/setup.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <asm/page.h>
#include <linux/atomic.h>
#include <linux/page_owner.h>
#include <linux/irq.h>
#include <linux/syscore_ops.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <asm/pgtable.h>
#include <linux/vmalloc.h>
#include <linux/page-flags.h>
#include <linux/mmzone.h>
#include <linux/kthread.h>
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <linux/spinlock.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>


#include <linux/kernel.h>
#include <linux/delay.h>
#include "memory-amms.h"

#define AMMS_PENDING_DRDI_FREE_BIT (1<<0)

#ifdef CONFIG_OF
static const struct of_device_id amms_of_ids[] = {
	{ .compatible = "mediatek,amms", },
	{}
};
#endif

static bool amms_static_free;

static int free_reserved_memory(phys_addr_t start_phys,
				phys_addr_t end_phys)
{

	phys_addr_t pos;
	unsigned long pages = 0;

	if (end_phys <= start_phys) {

		pr_notice("%s end_phys is smaller than start_phys start_phys:0x%pa end_phys:0x%pa\n"
			, __func__, &start_phys, &end_phys);
		return -1;
	}

	for (pos = start_phys; pos < end_phys; pos += PAGE_SIZE, pages++)
		free_reserved_page(phys_to_page(pos));

	if (pages)
		pr_info("Freeing modem memory: %ldK from phys %llx\n",
			pages << (PAGE_SHIFT - 10),
			(unsigned long long)start_phys);

	return 0;
}

static irqreturn_t amms_legacy_handler(int irq, void *data)
{
	phys_addr_t addr = 0, length = 0;
	struct arm_smccc_res res;

	if (!amms_static_free) {
		arm_smccc_smc(MTK_SIP_KERNEL_AMMS_GET_FREE_ADDR,
		0, 0, 0, 0, 0, 0, 0, &res);
		addr = res.a0;
		arm_smccc_smc(
		MTK_SIP_KERNEL_AMMS_GET_FREE_LENGTH,
		0, 0, 0, 0, 0, 0, 0, &res);
		length = res.a0;
		if (pfn_valid(__phys_to_pfn(addr))
			&& pfn_valid(__phys_to_pfn(
			addr + length - 1))) {
			pr_info("%s:addr=%pa length=%pa\n", __func__,
			&addr, &length);
			free_reserved_memory(addr, addr+length);
			amms_static_free = true;
		} else {
			pr_info("AMMS: error addr and length is not set properly\n");
			pr_info("can not free_reserved_memory\n");
		}
	} else {
		pr_info("amms: static memory already free\n");
	}

	return IRQ_HANDLED;
}

static irqreturn_t amms_handler(int irq, void *data)
{

	phys_addr_t addr = 0, length = 0;
	unsigned long long pending;
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_AMMS_GET_PENDING,
			0, 0, 0, 0, 0, 0, 0, &res);
	pending = res.a0;
	pr_info("%s:pending = 0x%llx\n", __func__, pending);
	pr_info("%s:pending = %lld\n", __func__, (long long)pending);

	if (((long long)pending) != AMMS_PENDING_DRDI_FREE_BIT) {
		pr_info("%s:Not support pending\n", __func__);
		return amms_legacy_handler(irq, data);
	}

	if (pending & AMMS_PENDING_DRDI_FREE_BIT) {
		pr_info("%s:Support pending\n", __func__);
		/*below part is for staic memory free */
		if (!amms_static_free) {
			arm_smccc_smc(MTK_SIP_KERNEL_AMMS_GET_FREE_ADDR,
			0, 0, 0, 0, 0, 0, 0, &res);
			addr = res.a0;
			arm_smccc_smc(
			MTK_SIP_KERNEL_AMMS_GET_FREE_LENGTH,
			0, 0, 0, 0, 0, 0, 0, &res);
			length = res.a0;
			if (pfn_valid(__phys_to_pfn(addr))
				&& pfn_valid(__phys_to_pfn(
				addr + length - 1))) {
				pr_info("%s:addr=%pa length=%pa\n",
				__func__, &addr, &length);
				free_reserved_memory(addr, addr+length);
				amms_static_free = true;
				arm_smccc_smc(MTK_SIP_KERNEL_AMMS_ACK_PENDING,
					AMMS_PENDING_DRDI_FREE_BIT, 0,
					0, 0, 0, 0, 0, &res);
			} else {
				pr_info("AMMS: error addr and length is not set properly\n");
				pr_info("can not free_reserved_memory\n");
			}
		} else {
			arm_smccc_smc(MTK_SIP_KERNEL_AMMS_ACK_PENDING,
				AMMS_PENDING_DRDI_FREE_BIT,
				0, 0, 0, 0, 0, 0, &res);
			pr_info("amms: static memory already free, should not happened\n");
		}
	}

	if (pending & (~(AMMS_PENDING_DRDI_FREE_BIT)))
		pr_info("amms:unknown pending interrupt\n");

	return IRQ_HANDLED;
}


static int amms_probe(struct platform_device *pdev)
{

	struct device_node *node;
	int amms_irq_num;
	struct device *amms_dev;

	amms_irq_num = platform_get_irq(pdev, 0);

	if (amms_irq_num < 0) {
		pr_info("Fail to get amms irq number from device tree\n");
		WARN_ON(amms_irq_num == -ENXIO);
		return -EINVAL;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,amms");
	if (!node) {
		pr_info("%s, amms not exist\n", __func__);
		return -EINVAL;
	}

	amms_dev = &pdev->dev;

	if (devm_request_threaded_irq(
	&pdev->dev, amms_irq_num, NULL, amms_handler,
	IRQF_ONESHOT|IRQF_TRIGGER_NONE, "amms_irq", NULL) != 0) {
		pr_info("Fail to request amms_irq interrupt!\n");
		return -EBUSY;
	}

	return 0;
}

static struct platform_driver amms_driver = {
	.probe = amms_probe,
	.driver = {
		.name = "amms",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = amms_of_ids,
#endif
	},
};

module_platform_driver(amms_driver);

MODULE_DESCRIPTION("MEDIATEK Module AMMS Driver");
MODULE_AUTHOR("<johnson.lin@mediatek.com>");
MODULE_LICENSE("GPL");
