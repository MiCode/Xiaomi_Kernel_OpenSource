/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>       /* min() */
#include <linux/uaccess.h>      /* copy_to_user() */
#include <linux/sched.h>        /* TASK_INTERRUPTIBLE/signal_pending/schedule */
#include <linux/poll.h>
#include <linux/io.h>           /* ioremap() */
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/seq_file.h>
#include <asm/setup.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/atomic.h>
#include <linux/irq.h>
#include <linux/syscore_ops.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <mt-plat/mtk_secure_api.h>

struct work_struct *amms_work;
bool amms_static_free;
static const struct of_device_id amms_of_ids[] = {
	{ .compatible = "mediatek,amms", },
	{}
};

static irqreturn_t amms_irq_handler(int irq, void *dev_id)
{
	if (amms_work)
		schedule_work(amms_work);
	else
		pr_notice("%s:amms_work is null\n", __func__);
	return IRQ_HANDLED;
}


static void amms_work_handler(struct work_struct *work)
{

	phys_addr_t addr = 0, length = 0;

	/*below part is for staic memory free */
	if (!amms_static_free) {
		addr = mt_secure_call(MTK_SIP_KERNEL_AMMS_GET_FREE_ADDR,
			0, 0, 0, 0);
		length = mt_secure_call(MTK_SIP_KERNEL_AMMS_GET_FREE_LENGTH,
			0, 0, 0, 0);
		if (pfn_valid(__phys_to_pfn(addr)) &&
			pfn_valid(__phys_to_pfn(addr + length - 1))) {
			pr_info("%s:addr = 0x%pa length=0x%pa\n",
				__func__, &addr, &length);
			free_reserved_memory(addr, addr+length);
			amms_static_free = true;
		} else {
			pr_notice("AMMS: error addr and length is not set properly\n");
			pr_notice("can not free_reserved_memory\n");
		}
	} else {
		pr_notice("amms: static memory already free, should not happened\n");
	}
}

static int __init amms_probe(struct platform_device *pdev)
{
	int irq_num;

	irq_num = platform_get_irq(pdev, 0);
	if (irq_num == -ENXIO) {
		pr_err("Fail to get amms irq number from device tree\n");
		WARN_ON(irq_num == -ENXIO);
		return -EINVAL;
	}

	pr_info("amms irq num %d.\n", irq_num);

	if (request_irq(irq_num, (irq_handler_t)amms_irq_handler,
			   IRQF_TRIGGER_NONE,
			"amms_irq", NULL) != 0) {
		pr_crit("Fail to request amms_irq interrupt!\n");
		return -EBUSY;
	}

	amms_work = kmalloc(sizeof(struct work_struct), GFP_KERNEL);

	if (!amms_work)
		return -ENOMEM;

	INIT_WORK(amms_work, amms_work_handler);

	return 0;
}

static void __exit amms_exit(void)
{
	kfree(amms_work);
	pr_notice("amms: exited");
}
static int amms_remove(struct platform_device *dev)
{
	return 0;
}

/* variable with __init* or __refdata (see linux/init.h) or */
/* name the variable *_template, *_timer, *_sht, *_ops, *_probe, */
/* *_probe_one, *_console */
static struct platform_driver amms_driver_probe = {
	.probe = amms_probe,
	.remove = amms_remove,
	.driver = {
		.name = "amms",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = amms_of_ids,
#endif
	},
};

static int __init amms_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&amms_driver_probe);
	if (ret)
		pr_err("amms init FAIL, ret 0x%x!!!\n", ret);

	return ret;
}


module_init(amms_init);
module_exit(amms_exit);

MODULE_DESCRIPTION("MEDIATEK Module AMMS Driver");
MODULE_AUTHOR("<Johnson.Lin@mediatek.com>");
