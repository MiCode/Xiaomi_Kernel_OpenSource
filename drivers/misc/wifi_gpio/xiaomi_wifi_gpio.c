// SPDX-License-Identifier: GPL-2.0
/*
 * WIFI Gpio Module driver for mi dirver manage sar,
 * which is only used for xiaomi corporation internally.
 *
 * Copyright (c) 2023 xiaomi inc.
 */

/*****************************************************************************
* Included header files
*****************************************************************************/
#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/pm_wakeirq.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/pm_wakeup.h>
#include <net/genetlink.h>
#include <net/net_namespace.h>
#include "hwid.h"

#define SAR_GENL_NAME "wifi_sar"
/*****************************************************************************
* Global variable
*****************************************************************************/
struct ant_gpio_data {
	struct device *dev;
	int debounce_time;
	int irq;
	atomic_t pm_count;
	struct delayed_work debounce_work;
};
/* the netlink family */
static struct genl_family sar_fam;
static uint32_t sar_status = 0;
static int gpio_num;

enum {
	SAR_ATTR_UNSPEC,
	SAR_ATTR_STATUS,
	__SAR_ATTR_AFTER_LAST,
	NUM_SAR_ATTR = __SAR_ATTR_AFTER_LAST,
	SAR_ATTR_MAX = __SAR_ATTR_AFTER_LAST - 1
};

enum sar_commands {
	SAR_CMD_UNSPEC,
	SAR_CMD_WPAS_READY,
	SAR_CMD_GPIO_STATUS
};

/*****************************************************************************
* Netlink functions
*****************************************************************************/
static int sar_send_status(pid_t pid, uint32_t status)
{
	struct sk_buff *msg;
	void *hdr;
	int err = -EMSGSIZE;

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_ATOMIC);
	if (!msg)
		return -ENOMEM;

	hdr = genlmsg_put(msg, pid, 0, &sar_fam, 0, SAR_CMD_GPIO_STATUS);
	if (!hdr)
		goto out_free;

	if (nla_put_u32(msg, SAR_ATTR_STATUS, status))
		goto out_cancel;

	genlmsg_end(msg, hdr);

	if (pid) {
		err = genlmsg_unicast(&init_net, msg, pid);
	} else {
		err = genlmsg_multicast(&sar_fam, msg, 0, 0, GFP_ATOMIC);
	}

	if (err && err != -ESRCH)
		goto failed;
	return 0;

out_cancel:
	genlmsg_cancel(msg, hdr);
out_free:
	nlmsg_free(msg);
failed:
	pr_err("sar send status failed: %d\n", err);
	return err;
}

static int sar_status_set(struct sk_buff *skb, struct genl_info *info)
{
	int ret;
	sar_status = gpio_get_value(gpio_num);
	ret = sar_send_status(0, sar_status);

	return ret;
}

/*****************************************************************************
* Netlink config
*****************************************************************************/
static const struct nla_policy sar_policy[NUM_SAR_ATTR] = {
	[SAR_ATTR_UNSPEC] = { .type = NLA_UNSPEC, },
	[SAR_ATTR_STATUS] = { .type = NLA_U32 },
};

static const struct genl_ops sar_ops[] = {
	{
		.cmd = SAR_CMD_WPAS_READY,
		.validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.doit = sar_status_set,
		.flags = GENL_ADMIN_PERM,
	},
};

/* multicast groups */
enum sar_multicast_groups {
	SAR_MCGRP_STATUS,
};

static const struct genl_multicast_group sar_mcgrps[] = {
	[SAR_MCGRP_STATUS] = { .name = "sar_event" },
};

static struct genl_family sar_fam __ro_after_init = {
	.name = SAR_GENL_NAME,
	.hdrsize = 0,			/* no private header */
	.version = 1,			/* no particular meaning now */
	.maxattr = SAR_ATTR_MAX,
	.policy = sar_policy,
	.module = THIS_MODULE,
	.ops = sar_ops,
	.n_ops = ARRAY_SIZE(sar_ops),
	.resv_start_op = SAR_CMD_GPIO_STATUS + 1,
	.mcgrps = sar_mcgrps,
	.n_mcgrps = ARRAY_SIZE(sar_mcgrps),
};

/*****************************************************************************
* Static functions
*****************************************************************************/
static void wifi_pm_stay_awake(struct ant_gpio_data *ant_data)
{
	if (atomic_inc_return(&ant_data->pm_count) > 1) {
		atomic_set(&ant_data->pm_count, 1);
		return;
	}

	dev_info(ant_data->dev, "PM stay awake, count: %d\n", atomic_read(&ant_data->pm_count));
	pm_stay_awake(ant_data->dev);
}

static void wifi_pm_relax(struct ant_gpio_data *ant_data)
{
	int r = atomic_dec_return(&ant_data->pm_count);

	WARN_ON(r < 0);

	if (r != 0)
		return;

	dev_info(ant_data->dev, "PM relax, count: %d\n", atomic_read(&ant_data->pm_count));
	pm_relax(ant_data->dev);
}

static void gpio_debounce_work(struct work_struct *work)
{
	struct ant_gpio_data *ant_data =
			container_of(work, struct ant_gpio_data, debounce_work.work);

	sar_status = gpio_get_value(gpio_num);
	sar_send_status(0, sar_status);

	dev_info(ant_data->dev, "gpio status: %d\n", sar_status);
	wifi_pm_relax(ant_data);
}

/*****************************************************************************
* Interrupt function
*****************************************************************************/
static irqreturn_t gpio_testing_threaded_irq_handler(int irq, void *irq_data)
{
	struct ant_gpio_data *ant_data = irq_data;
	struct device *dev = ant_data->dev;
	dev_info(dev, "irq [%d] triggered\n", irq);

	wifi_pm_stay_awake(ant_data);

	mod_delayed_work(system_wq, &ant_data->debounce_work,
                         msecs_to_jiffies(ant_data->debounce_time));
	return IRQ_HANDLED;
}

/*****************************************************************************
* Platform Driver function
*****************************************************************************/
static int xiaomi_wifi_gpio_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct ant_gpio_data *ant_data;

	ant_data = devm_kzalloc(dev, sizeof(struct ant_gpio_data), GFP_KERNEL);
	if (!ant_data)
		return -ENOMEM;

	if (of_property_read_u32(np, "debounce-time", &ant_data->debounce_time)) {
		dev_info(dev, "Failed to get debounce-time, use default.\n");
		ant_data->debounce_time = 50;
	}

	gpio_num = of_get_named_gpio(np, "gpio", 0);
	if (gpio_num < 0) {
		dev_err(dev, "Failed to get ant gpio %d\n", gpio_num);
		return gpio_num;
	}

	ret = devm_gpio_request(dev, gpio_num, "gpio");
	if (ret) {
		dev_err(dev, "Request gpio failed %d\n", gpio_num);
		return ret;
	}

	gpio_direction_input(gpio_num);
	ant_data->irq = gpio_to_irq(gpio_num);
	ant_data->dev = dev;
	INIT_DELAYED_WORK(&ant_data->debounce_work, gpio_debounce_work);

	ret = devm_request_threaded_irq(dev, ant_data->irq, NULL,
		gpio_testing_threaded_irq_handler, IRQF_ONESHOT | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
		"xiaomi_wifi_gpio", ant_data);
	if (ret < 0) {
		dev_err(dev, "Failed to request irq.\n");
		return -EBUSY;
	}

	ret = device_init_wakeup(dev, true);
	if(ret) {
		dev_err(dev, "Failed to configure device as wakeup %d\n", ret);
		return ret;
	}

	ret = dev_pm_set_wake_irq(dev, ant_data->irq);
	if(ret) {
		dev_err(dev, "Failed to set wake irq %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, ant_data);
	pr_info("xiaomi_wifi_gpio driver probed successfully.\n");

	return ret;
}

static int xiaomi_wifi_gpio_remove(struct platform_device *pdev)
{
	struct ant_gpio_data *ant_data = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&ant_data->debounce_work);
	dev_pm_clear_wake_irq(ant_data->dev);
	device_init_wakeup(&pdev->dev, false);

	pr_info("%s cn version, exit gpio-testing\n", __func__);
	return 0;
}

static const struct of_device_id gpio_testing_mode_of_match[] = {
	{ .compatible = "xiaomi,xiaomi-wifi", },
	{},
};

static struct platform_driver xiaomi_wifi_gpio_driver = {
	.driver = {
		.name = "xiaomi_wifi_gpio",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(gpio_testing_mode_of_match),
	},
	.probe = xiaomi_wifi_gpio_probe,
	.remove = xiaomi_wifi_gpio_remove,
};

/*****************************************************************************
*  Name: wifi_gpio_driver_init
*****************************************************************************/
static int __init wifi_gpio_driver_init(void)
{
	int err;
	uint32_t hw_country_ver = 10;

	hw_country_ver = get_hw_country_version();
	if(hw_country_ver != (uint32_t)CountryCN) {
		pr_info("%s not cn devices, not init\n", __func__);
		return 0;
	}

	err = genl_register_family(&sar_fam);
	if (err)
		return err;

	err = platform_driver_register(&xiaomi_wifi_gpio_driver);
	if (err)
		goto out;

	return 0;

out:
	genl_unregister_family(&sar_fam);
	return err;
}

/*****************************************************************************
*  Name: wifi_gpio_driver_exit
*****************************************************************************/
static void __exit wifi_gpio_driver_exit(void)
{
	uint32_t hw_country_ver = 0;
	hw_country_ver = get_hw_country_version();

	if ((uint32_t)CountryCN != hw_country_ver) {
		pr_info("%s not cn devices, exit gpio-testing\n", __func__);
		return;
	}
	genl_unregister_family(&sar_fam);
	platform_driver_unregister(&xiaomi_wifi_gpio_driver);
}

module_init(wifi_gpio_driver_init);
module_exit(wifi_gpio_driver_exit);

MODULE_AUTHOR("songgang3@xiaomi.com");
MODULE_DESCRIPTION("Wifi Gpio Testing Driver for Xiaomi Corporation");
MODULE_LICENSE("GPL v2");
