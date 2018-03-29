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
static int __init gcpu_init(void)
{
	struct device_node *node = NULL;
	unsigned int gcpu_irq = 0;
	int ret = 0;
	/* register for GCPU */
	node = of_find_compatible_node(NULL, NULL, "mediatek,gcpu");

	if (node) {
		gcpu_irq = irq_of_parse_and_map(node, 0);
		pr_debug("[GCPU] irq_no: (%d)\n", gcpu_irq);
		ret = request_irq(gcpu_irq, (irq_handler_t)gcpu_irq_handler, IRQF_TRIGGER_LOW , "gcpu", NULL);
		if (ret != 0)
			pr_err("[GCPU] Failed to request irq! (%d) irq no: (%d)\n", ret, gcpu_irq);
		else
			pr_debug("[GCPU] request irq (%d) succeed!!\n", gcpu_irq);

	} else{
		pr_err("[GCPU] DT find compatible node failed!!\n");
		ret = -1;
	}

	return ret;
}


/**************************************************************************
 *  GCPU DRIVER EXIT
 **************************************************************************/
static void __exit gcpu_exit(void)
{
	;
}

module_init(gcpu_init);
module_exit(gcpu_exit);

/**************************************************************************
 *  EXPORT FUNCTION
 **************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("Mediatek GCPU Module");
