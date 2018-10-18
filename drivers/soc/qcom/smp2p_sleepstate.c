/* Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
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
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/smem_state.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/pm_wakeup.h>

#define PROC_AWAKE_ID 12 /* 12th bit */
#define AWAKE_BIT BIT(PROC_AWAKE_ID)
static struct qcom_smem_state *state;
struct wakeup_source notify_ws;

/**
 * sleepstate_pm_notifier() - PM notifier callback function.
 * @nb:		Pointer to the notifier block.
 * @event:	Suspend state event from PM module.
 * @unused:	Null pointer from PM module.
 *
 * This function is register as callback function to get notifications
 * from the PM module on the system suspend state.
 */
static int sleepstate_pm_notifier(struct notifier_block *nb,
				  unsigned long event, void *unused)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		qcom_smem_state_update_bits(state, AWAKE_BIT, 0);
		break;

	case PM_POST_SUSPEND:
		qcom_smem_state_update_bits(state, AWAKE_BIT, AWAKE_BIT);
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block sleepstate_pm_nb = {
	.notifier_call = sleepstate_pm_notifier,
	.priority = INT_MAX,
};

static irqreturn_t smp2p_sleepstate_handler(int irq, void *ctxt)
{
	__pm_wakeup_event(&notify_ws, 200);
	return IRQ_HANDLED;
}

static int smp2p_sleepstate_probe(struct platform_device *pdev)
{
	int ret;
	int irq = -1;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;

	state = qcom_smem_state_get(&pdev->dev, 0, &ret);
	if (IS_ERR(state))
		return PTR_ERR(state);
	qcom_smem_state_update_bits(state, AWAKE_BIT, AWAKE_BIT);

	ret = register_pm_notifier(&sleepstate_pm_nb);
	if (ret)
		dev_err(&pdev->dev, "%s: power state notif error %d\n",
							__func__, ret);

	wakeup_source_init(&notify_ws, "smp2p-sleepstate");

	irq = of_irq_get_byname(node, "smp2p-sleepstate-in");
	if (irq <= 0) {
		dev_err(&pdev->dev,
			"failed for irq getbyname for smp2p_sleep_state\n");
		ret = -EPROBE_DEFER;
		goto err;
	}
	dev_info(&pdev->dev, "got smp2p-sleepstate-in irq %d\n", irq);
	ret = devm_request_threaded_irq(dev, irq, NULL,
		(irq_handler_t)smp2p_sleepstate_handler,
		IRQF_TRIGGER_RISING, "smp2p_sleepstate", dev);
	if (ret) {
		dev_err(&pdev->dev, "fail to register smp2p threaded_irq=%d\n",
									irq);
		goto err;
	}
	return 0;
err:
	wakeup_source_trash(&notify_ws);
	unregister_pm_notifier(&sleepstate_pm_nb);
	return ret;
}

static const struct of_device_id smp2p_slst_match_table[] = {
	{.compatible = "qcom,smp2p-sleepstate"},
	{},
};

static struct platform_driver smp2p_sleepstate_driver = {
	.probe = smp2p_sleepstate_probe,
	.driver = {
		.name = "smp2p_sleepstate",
		.owner = THIS_MODULE,
		.of_match_table = smp2p_slst_match_table,
	},
};

static int __init smp2p_sleepstate_init(void)
{
	int ret;

	ret = platform_driver_register(&smp2p_sleepstate_driver);
	if (ret) {
		pr_err("%s: register failed %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

module_init(smp2p_sleepstate_init);
MODULE_DESCRIPTION("SMP2P SLEEP STATE");
MODULE_LICENSE("GPL v2");
