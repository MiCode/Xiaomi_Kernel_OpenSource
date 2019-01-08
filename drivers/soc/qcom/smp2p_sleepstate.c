/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#include <linux/ipc_router.h>
#include "smp2p_private.h"

#define SET_DELAY (2 * HZ)
#define PROC_AWAKE_ID 12 /* 12th bit */
#if defined(CONFIG_KERNEL_CUSTOM_WAYNE) || defined(CONFIG_KERNEL_CUSTOM_WHYRED)
int slst_gpio_base_id;
#else
static int slst_gpio_base_id;
#endif

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
#if defined(CONFIG_KERNEL_CUSTOM_WAYNE) || defined(CONFIG_KERNEL_CUSTOM_WHYRED)
#else
		gpio_set_value(slst_gpio_base_id + PROC_AWAKE_ID, 0);
#endif
		msleep(25); /* To be tuned based on SMP2P latencies */
		msm_ipc_router_set_ws_allowed(true);
		break;

	case PM_POST_SUSPEND:
#if defined(CONFIG_KERNEL_CUSTOM_WAYNE) || defined(CONFIG_KERNEL_CUSTOM_WHYRED)
#else
		gpio_set_value(slst_gpio_base_id + PROC_AWAKE_ID, 1);
#endif
		msleep(25); /* To be tuned based on SMP2P latencies */
		msm_ipc_router_set_ws_allowed(false);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block sleepstate_pm_nb = {
	.notifier_call = sleepstate_pm_notifier,
	.priority = INT_MAX,
};

static int smp2p_sleepstate_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *node = pdev->dev.of_node;

	slst_gpio_base_id = of_get_gpio(node, 0);
	if (slst_gpio_base_id == -EPROBE_DEFER) {
		return slst_gpio_base_id;
	} else if (slst_gpio_base_id < 0) {
		SMP2P_ERR("%s: Error to get gpio %d\n",
				__func__, slst_gpio_base_id);
		return slst_gpio_base_id;
	}


	gpio_set_value(slst_gpio_base_id + PROC_AWAKE_ID, 1);

	ret = register_pm_notifier(&sleepstate_pm_nb);
	if (ret)
		SMP2P_ERR("%s: power state notif error %d\n", __func__, ret);

	return 0;
}

static struct of_device_id msm_smp2p_slst_match_table[] = {
	{.compatible = "qcom,smp2pgpio_sleepstate_3_out"},
	{.compatible = "qcom,smp2pgpio-sleepstate-out"},
	{},
};

static struct platform_driver smp2p_sleepstate_driver = {
	.probe = smp2p_sleepstate_probe,
	.driver = {
		.name = "smp2p_sleepstate",
		.owner = THIS_MODULE,
		.of_match_table = msm_smp2p_slst_match_table,
	},
};

static int __init smp2p_sleepstate_init(void)
{
	int ret;
	ret = platform_driver_register(&smp2p_sleepstate_driver);
	if (ret) {
		SMP2P_ERR("%s: smp2p_sleepstate_driver register failed %d\n",
			 __func__, ret);
		return ret;
	}

	return 0;
}

module_init(smp2p_sleepstate_init);
MODULE_DESCRIPTION("SMP2P SLEEP STATE");
MODULE_LICENSE("GPL v2");
