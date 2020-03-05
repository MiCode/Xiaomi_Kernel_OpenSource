/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>


enum subsys_policies {
	SUBSYS_PANIC = 0,
	SUBSYS_NOP,
};

static const char * const policies[] = {
	[SUBSYS_PANIC] = "PANIC",
	[SUBSYS_NOP] = "NOP",
};

enum gpios {
	AP2MDM_STATUS = 0,
	MDM2AP_STATUS,
	MDM2AP_STATUS2,
	NUM_GPIOS,
};

static const char * const gpio_map[] = {
	[AP2MDM_STATUS] = "qcom,ap2mdm-status-gpio",
	[MDM2AP_STATUS] = "qcom,mdm2ap-status-gpio",
	[MDM2AP_STATUS2] = "qcom,mdm2ap-status2-gpio",
};

struct gpio_cntrl {
	unsigned int gpios[NUM_GPIOS];
	int status_irq;
	int policy;
	struct device *dev;
	struct mutex policy_lock;
	struct mutex e911_lock;
	struct notifier_block panic_blk;
};

static ssize_t policy_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	int ret;
	struct gpio_cntrl *mdm = dev_get_drvdata(dev);

	mutex_lock(&mdm->policy_lock);
	ret = scnprintf(buf, strlen(policies[mdm->policy]) + 1,
						 policies[mdm->policy]);
	mutex_unlock(&mdm->policy_lock);

	return ret;
}

static ssize_t policy_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct gpio_cntrl *mdm = dev_get_drvdata(dev);
	const char *p;
	int i, orig_count = count;

	p = memchr(buf, '\n', count);
	if (p)
		count = p - buf;

	for (i = 0; i < ARRAY_SIZE(policies); i++)
		if (!strncasecmp(buf, policies[i], count)) {
			mutex_lock(&mdm->policy_lock);
			mdm->policy = i;
			mutex_unlock(&mdm->policy_lock);
			return orig_count;
		}
	return -EPERM;
}
static DEVICE_ATTR_RW(policy);

static ssize_t e911_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	int ret, state;
	struct gpio_cntrl *mdm = dev_get_drvdata(dev);

	mutex_lock(&mdm->e911_lock);
	state = gpio_get_value(mdm->gpios[MDM2AP_STATUS2]);
	ret = scnprintf(buf, 2, "%d\n", state);
	mutex_unlock(&mdm->e911_lock);

	return ret;
}

static ssize_t e911_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct gpio_cntrl *mdm = dev_get_drvdata(dev);
	int e911;

	if (kstrtoint(buf, 0, &e911))
		return -EINVAL;

	mutex_lock(&mdm->e911_lock);
	if (e911)
		gpio_set_value(mdm->gpios[MDM2AP_STATUS2], 1);
	else
		gpio_set_value(mdm->gpios[MDM2AP_STATUS2], 0);
	mutex_unlock(&mdm->e911_lock);

	return count;
}
static DEVICE_ATTR_RW(e911);

static irqreturn_t ap_status_change(int irq, void *dev_id)
{
	struct gpio_cntrl *mdm = dev_id;
	int state;
	struct gpio_desc *gp_status = gpio_to_desc(mdm->gpios[AP2MDM_STATUS]);
	int active_low = 0;

	if (gp_status)
		active_low = gpiod_is_active_low(gp_status);

	state = gpio_get_value(mdm->gpios[AP2MDM_STATUS]);
	if ((!active_low && !state) || (active_low && state)) {
		if (mdm->policy)
			dev_info(mdm->dev, "Host undergoing SSR, leaving SDX as it is\n");
		else
			panic("Host undergoing SSR, panicking SDX\n");
	} else
		dev_info(mdm->dev, "HOST booted\n");

	return IRQ_HANDLED;
}

static void remove_ipc(struct gpio_cntrl *mdm)
{
	int i;

	for (i = 0; i < NUM_GPIOS; ++i) {
		if (gpio_is_valid(mdm->gpios[i]))
			gpio_free(mdm->gpios[i]);
	}
}

static int setup_ipc(struct gpio_cntrl *mdm)
{
	int i, val, ret, irq;
	struct device_node *node;

	node = mdm->dev->of_node;
	for (i = 0; i < ARRAY_SIZE(gpio_map); i++) {
		val = of_get_named_gpio(node, gpio_map[i], 0);
		if (val >= 0)
			mdm->gpios[i] = val;
	}

	ret = gpio_request(mdm->gpios[AP2MDM_STATUS], "AP2MDM_STATUS");
	if (ret) {
		dev_err(mdm->dev, "Failed to configure AP2MDM_STATUS gpio\n");
		return ret;
	}
	gpio_direction_input(mdm->gpios[AP2MDM_STATUS]);

	ret = gpio_request(mdm->gpios[MDM2AP_STATUS], "MDM2AP_STATUS");
	if (ret) {
		dev_err(mdm->dev, "Failed to configure MDM2AP_STATUS gpio\n");
		return ret;
	}
	gpio_direction_output(mdm->gpios[MDM2AP_STATUS], 1);

	ret = gpio_request(mdm->gpios[MDM2AP_STATUS2], "MDM2AP_STATUS2");
	if (ret) {
		dev_err(mdm->dev, "Failed to configure MDM2AP_STATUS2 gpio\n");
		return ret;
	}
	gpio_direction_output(mdm->gpios[MDM2AP_STATUS2], 0);

	irq = gpio_to_irq(mdm->gpios[AP2MDM_STATUS]);
	if (irq < 0) {
		dev_err(mdm->dev, "bad AP2MDM_STATUS IRQ resource\n");
		return irq;
	}
	mdm->status_irq = irq;

	return 0;
}

static int sdx_ext_ipc_panic(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	struct gpio_cntrl *mdm = container_of(this,
					struct gpio_cntrl, panic_blk);

	gpio_set_value(mdm->gpios[MDM2AP_STATUS], 0);

	return NOTIFY_DONE;
}

static int sdx_ext_ipc_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *node;
	struct gpio_cntrl *mdm;

	node = pdev->dev.of_node;
	mdm = devm_kzalloc(&pdev->dev, sizeof(*mdm), GFP_KERNEL);
	if (!mdm)
		return -ENOMEM;

	mdm->dev = &pdev->dev;
	ret = setup_ipc(mdm);
	if (ret) {
		dev_err(mdm->dev, "Error setting up gpios\n");
		devm_kfree(&pdev->dev, mdm);
		return ret;
	}

	mdm->panic_blk.notifier_call = sdx_ext_ipc_panic;
	atomic_notifier_chain_register(&panic_notifier_list, &mdm->panic_blk);

	mutex_init(&mdm->policy_lock);
	mutex_init(&mdm->e911_lock);
	mdm->policy = SUBSYS_NOP;

	ret = device_create_file(mdm->dev, &dev_attr_policy);
	if (ret) {
		dev_err(mdm->dev, "cannot create sysfs attribute\n");
		goto sys_fail;
	}

	ret = device_create_file(mdm->dev, &dev_attr_e911);
	if (ret) {
		dev_err(mdm->dev, "cannot create sysfs attribute\n");
		goto sys_fail1;
	}

	platform_set_drvdata(pdev, mdm);

	ret = devm_request_irq(mdm->dev, mdm->status_irq, ap_status_change,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
							"ap status", mdm);
	if (ret < 0) {
		dev_err(mdm->dev, "%s: AP2MDM_STATUS IRQ#%d request failed,\n",
					__func__, mdm->status_irq);
		goto irq_fail;
	}
	irq_set_irq_wake(mdm->status_irq, 1);
	return 0;

irq_fail:
	device_remove_file(mdm->dev, &dev_attr_policy);
sys_fail1:
	device_remove_file(mdm->dev, &dev_attr_e911);
sys_fail:
	atomic_notifier_chain_unregister(&panic_notifier_list, &mdm->panic_blk);
	remove_ipc(mdm);
	devm_kfree(&pdev->dev, mdm);
	return ret;
}

static int sdx_ext_ipc_remove(struct platform_device *pdev)
{
	struct gpio_cntrl *mdm;

	mdm = dev_get_drvdata(&pdev->dev);
	disable_irq_wake(mdm->status_irq);
	atomic_notifier_chain_unregister(&panic_notifier_list, &mdm->panic_blk);
	remove_ipc(mdm);
	device_remove_file(mdm->dev, &dev_attr_policy);
	return 0;
}

static const struct of_device_id sdx_ext_ipc_of_match[] = {
	{ .compatible = "qcom,sdx-ext-ipc"},
	{},
};

static struct platform_driver sdx_ext_ipc_driver = {
	.probe		= sdx_ext_ipc_probe,
	.remove		= sdx_ext_ipc_remove,
	.driver = {
		.name	= "sdx-ext-ipc",
		.owner	= THIS_MODULE,
		.of_match_table = sdx_ext_ipc_of_match,
	},
};

static int __init sdx_ext_ipc_register(void)
{
	return platform_driver_register(&sdx_ext_ipc_driver);
}
subsys_initcall(sdx_ext_ipc_register);

static void __exit sdx_ext_ipc_unregister(void)
{
	platform_driver_unregister(&sdx_ext_ipc_driver);
}
module_exit(sdx_ext_ipc_unregister);
MODULE_LICENSE("GPL v2");
