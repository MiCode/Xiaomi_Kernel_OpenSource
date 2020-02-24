// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <soc/qcom/tgu.h>
#define CREATE_TRACE_POINTS
#include "trace/events/tgu.h"
#include "apss_tgu.h"

struct tgu_test_notifier tgu_notify;
int register_tgu_notifier(struct tgu_test_notifier *tgu_test)
{
	tgu_notify.cb = tgu_test->cb;
	return 0;
}
EXPORT_SYMBOL(register_tgu_notifier);

int unregister_tgu_notifier(struct tgu_test_notifier *tgu_test)
{
	if (tgu_test->cb == tgu_notify.cb)
		tgu_notify.cb = NULL;
	return 0;
}
EXPORT_SYMBOL(unregister_tgu_notifier);

irqreturn_t tgu_irq_thread_handler(int irq, void *dev_id)
{
	if (tgu_notify.cb)
		tgu_notify.cb();
	return IRQ_HANDLED;
}

static irqreturn_t tgu_irq_handler(int irq, void *data)
{
	trace_tgu_interrupt(irq);
	return IRQ_WAKE_THREAD;
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

		ret = request_threaded_irq(irq,  tgu_irq_handler,
				tgu_irq_thread_handler,
				IRQF_TRIGGER_RISING, "apps-tgu", NULL);
		if (ret < 0) {
			pr_err("Unable to register IRQ handler %d\n", irq);
			return ret;
		}

		ret = irq_set_irq_wake(irq, true);
		if (ret < 0) {
			pr_err("Unable to set as wakeup irq %d\n", irq);
			return ret;
		}
	}

	return 0;
}
