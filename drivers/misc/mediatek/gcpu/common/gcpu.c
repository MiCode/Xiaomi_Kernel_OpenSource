// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/******************************************************************************
 *  INCLUDE LINUX HEADER
 ******************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

/******************************************************************************
 *  INCLUDE LIBRARY
 ******************************************************************************/

/**************************************************************************
 *  EXTERNAL VARIABLE
 **************************************************************************/

/*************************************************************************
 *  GLOBAL VARIABLE
 **************************************************************************/

/**************************************************************************
 *  EXTERNAL FUNCTION
 **************************************************************************/

/* dummy interrupt handler for gcpu */
static irqreturn_t gcpu_irq_handler(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

/**************************************************************************
 *  GCPU DRIVER INIT
 **************************************************************************/
static int gcpu_probe(struct platform_device *pdev)
{
	unsigned int gcpu_irq = 0;
	int ret = 0;

	/* register for GCPU */
	gcpu_irq = platform_get_irq(pdev, 0);
	pr_debug("[GCPU] irq_no: (%d)\n", gcpu_irq);
	ret = request_irq(gcpu_irq,
			(irq_handler_t)gcpu_irq_handler,
			IRQF_TRIGGER_LOW,
			"gcpu",
			NULL);

	if (ret != 0)
		pr_info("[GCPU] request irq %d fail %d\n", gcpu_irq, ret);
	else
		pr_debug("[GCPU] request irq (%d) succeed!!\n", gcpu_irq);

	return ret;
}

static int gcpu_remove(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id gcpu_of_ids[] = {
	{ .compatible = "mediatek,gcpu", },
	{}
};
#endif

static struct platform_driver mtk_gcpu_driver = {
	.probe = gcpu_probe,
	.remove = gcpu_remove,
	.driver = {
		.name = "gcpu",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = gcpu_of_ids,
#endif
		},
};

static int __init gcpu_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mtk_gcpu_driver);
	if (ret)
		pr_info("[GCPU] init FAIL, ret 0x%x!!!\n", ret);

	return ret;
}

module_init(gcpu_init);

/**************************************************************************
 *  EXPORT FUNCTION
 **************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("Mediatek GCPU Module");
