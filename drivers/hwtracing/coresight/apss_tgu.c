/* Copyright (c) 2019,  The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#define CREATE_TRACE_POINTS
#include "trace/events/tgu.h"
#include "apss_tgu.h"

static irqreturn_t tgu_irq_handler(int irq, void *data)
{
	trace_tgu_interrupt(irq);
	return IRQ_HANDLED;
}


int register_interrupt_handler(struct device_node *node)
{
	int irq, ret, i, n;

	n = of_irq_count(node);
	pr_debug("number of irqs == %d\n", n);

	for (i = 0; i < n; i++) {
		irq = of_irq_get(node, i);
		if (irq < 0) {
			pr_err("Invalid IRQ for error fatal %u\n", irq);
			return irq;
		}

		ret = request_irq(irq,  tgu_irq_handler,
				IRQF_TRIGGER_RISING, "apps-tgu", NULL);
		if (ret < 0) {
			pr_err("Unable to register IRQ handler %d", irq);
			continue;
		}

		ret = irq_set_irq_wake(irq, true);
		if (ret < 0)
			pr_err("Unable to set as wakeup irq %d\n", irq);

	}
	return 0;
}
