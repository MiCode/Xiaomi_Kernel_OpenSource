/*
 * Copyright (C) 2010-2011 NVIDIA Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irqflags.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/platform_data/tegra_bpc_mgmt.h>

#include <mach/edp.h>

static irqreturn_t tegra_bpc_mgmt_bh(int irq, void *data)
{
	int ret = -1;
	int gpio_val = 0;
	struct tegra_bpc_mgmt_platform_data *bpc_platform_data;
	bpc_platform_data = (struct tegra_bpc_mgmt_platform_data *)data;

	/**
	 * Keep on checking whether event has passed or not.
	 */
	while (!gpio_val) {
		if (ret)
			ret = tegra_system_edp_alarm(true);

		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(
			bpc_platform_data->bpc_mgmt_timeout));

		gpio_val = gpio_get_value(bpc_platform_data->gpio_trigger);
	}

	tegra_system_edp_alarm(false);

	return IRQ_HANDLED;
}

static irqreturn_t tegra_bpc_mgmt_isr(int irq, void *data)
{
	tegra_edp_throttle_cpu_now(2);
	return IRQ_WAKE_THREAD;
}

static int tegra_bpc_mgmt_probe(struct platform_device *pdev)
{
	u32 ret;
	struct task_struct *bh_thread;
	struct irq_desc *bat_desc;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };
	struct tegra_bpc_mgmt_platform_data *bpc_platform_data;

	bpc_platform_data = pdev->dev.platform_data;
	if (!bpc_platform_data)
		return -ENODEV;

	if (gpio_is_valid(bpc_platform_data->gpio_trigger)) {
		ret = gpio_request(bpc_platform_data->gpio_trigger,
							"tegra-bpc-mgmt");

		if (ret < 0) {
			pr_err("BPC: GPIO request failed");
			return -ENODEV;
		}
	} else {
		pr_err("BPC: GPIO check failed, gpio %d",
					bpc_platform_data->gpio_trigger);
		return -ENODEV;
	}

	gpio_direction_input(bpc_platform_data->gpio_trigger);

	ret = request_threaded_irq(
		  gpio_to_irq(bpc_platform_data->gpio_trigger),
		  tegra_bpc_mgmt_isr,
		  tegra_bpc_mgmt_bh, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
		  "tegra-bpc-mgmt", bpc_platform_data);
	if (ret < 0) {
		pr_err("BPC:IRQ Installation failed\n");
		return -ENODEV;
	}
	bat_desc = irq_to_desc(
		gpio_to_irq(bpc_platform_data->gpio_trigger));

	if (bat_desc) {
		bh_thread = bat_desc->action->thread;
		if (bh_thread)
			sched_setscheduler_nocheck(bh_thread,
						   SCHED_FIFO, &param);
	}

	return 0;
}

static int tegra_bpc_mgmt_remove(struct platform_device *pdev)
{
	struct tegra_bpc_mgmt_platform_data *bpc_platform_data;
	bpc_platform_data = pdev->dev.platform_data;
	free_irq(gpio_to_irq(bpc_platform_data->gpio_trigger), NULL);
	return 0;
}

static struct platform_driver tegra_bpc_mgmt_driver = {
	.probe = tegra_bpc_mgmt_probe,
	.remove = tegra_bpc_mgmt_remove,
	.driver = {
		.name = "tegra-bpc-mgmt",
		.owner = THIS_MODULE,
	},
};

static int __init tegra_bpc_mgmt_init(void)
{
	return platform_driver_register(&tegra_bpc_mgmt_driver);
}

static void __exit tegra_bpc_mgmt_exit(void)
{
	platform_driver_unregister(&tegra_bpc_mgmt_driver);
}

module_init(tegra_bpc_mgmt_init);
module_exit(tegra_bpc_mgmt_exit);

MODULE_DESCRIPTION("TEGRA Battery Peak current Management");
MODULE_AUTHOR("NVIDIA");
MODULE_LICENSE("GPL");
