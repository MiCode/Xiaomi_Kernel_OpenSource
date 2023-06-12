// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 */

#include <linux/delay.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <soc/qcom/sb_notification.h>

#define GPIO_BASE 0xf100000
#define GPIO_REG_OFFSET 0x1000
#define GPIO_REG_SIZE 0x300000

#define E911_GPIO_NUMBER 45

#define STATUS_UP 1
#define STATUS_DOWN 0

enum subsys_policies {
	SUBSYS_PANIC = 0,
	SUBSYS_NOP,
};

static const char * const policies[] = {
	[SUBSYS_PANIC] = "PANIC",
	[SUBSYS_NOP] = "NOP",
};

enum gpios {
	STATUS_IN = 0,
	STATUS_OUT,
	STATUS_OUT2,
	WAKEUP_OUT,
	WAKEUP_IN,
	NUM_GPIOS,
};

static const char * const gpio_map[] = {
	[STATUS_IN] = "qcom,status-in-gpio",
	[STATUS_OUT] = "qcom,status-out-gpio",
	[STATUS_OUT2] = "qcom,status-out2-gpio",
	[WAKEUP_OUT] = "qcom,wakeup-gpio-out",
	[WAKEUP_IN] = "qcom,wakeup-gpio-in",
};

struct gpio_cntrl {
	int gpios[NUM_GPIOS];
	int status_irq;
	int wakeup_irq;
	int policy;
	unsigned int ipq807x_attach;
	void __iomem *gpio_base;
	struct device *dev;
	struct mutex policy_lock;
	struct mutex e911_lock;
	struct notifier_block panic_blk;
	struct notifier_block sideband_nb;
};

static ssize_t set_remote_status_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int status;

	if (kstrtoint(buf, 10, &status)) {
		dev_err(dev, "%s: Failed to read status\n", __func__);
		return -EINVAL;
	}

	if (status == STATUS_UP)
		sb_notifier_call_chain(EVENT_REMOTE_STATUS_UP, NULL);
	else if (status == STATUS_DOWN)
		sb_notifier_call_chain(EVENT_REMOTE_STATUS_DOWN, NULL);

	return count;
}
static DEVICE_ATTR_WO(set_remote_status);

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
	int ret;
	u32 state;
	struct gpio_cntrl *mdm = dev_get_drvdata(dev);
	void __iomem *addr = NULL;

	if (mdm->gpios[STATUS_OUT2] < 0)
		return -ENXIO;

	mutex_lock(&mdm->e911_lock);

	if (mdm->ipq807x_attach) {
		addr = mdm->gpio_base + GPIO_REG_OFFSET * E911_GPIO_NUMBER;
		state = readl_relaxed(addr);
		if (state == 0x3c7)
			ret = scnprintf(buf, 2, "1\n");
		else
			ret = scnprintf(buf, 2, "0\n");
	} else {
		state = gpio_get_value(mdm->gpios[STATUS_OUT2]);
		ret = scnprintf(buf, 2, "%d\n", state);
	}
	mutex_unlock(&mdm->e911_lock);

	return ret;
}

static ssize_t e911_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct gpio_cntrl *mdm = dev_get_drvdata(dev);
	void __iomem *addr = NULL;
	int e911;

	if (kstrtoint(buf, 0, &e911))
		return -EINVAL;

	if (mdm->gpios[STATUS_OUT2] < 0)
		return -ENXIO;

	mutex_lock(&mdm->e911_lock);

	if (mdm->ipq807x_attach) {
		addr = mdm->gpio_base + GPIO_REG_OFFSET * E911_GPIO_NUMBER;
		if (e911)
			writel_relaxed(0x3c7, addr);
		else
			writel_relaxed(0x4, addr);
	} else {
		if (e911)
			gpio_set_value(mdm->gpios[STATUS_OUT2], 1);
		else
			gpio_set_value(mdm->gpios[STATUS_OUT2], 0);
	}
	mutex_unlock(&mdm->e911_lock);

	return count;
}
static DEVICE_ATTR_RW(e911);

static int sideband_notify(struct notifier_block *nb,
		unsigned long action, void *dev)
{
	struct gpio_cntrl *mdm = container_of(nb,
					struct gpio_cntrl, sideband_nb);

	switch (action) {

	case EVENT_REQUEST_WAKE_UP:
		gpio_set_value(mdm->gpios[WAKEUP_OUT], 1);
		usleep_range(10000, 20000);
		gpio_set_value(mdm->gpios[WAKEUP_OUT], 0);
		break;
	}

	return NOTIFY_OK;
}

static irqreturn_t ap_status_change(int irq, void *dev_id)
{
	struct gpio_cntrl *mdm = dev_id;
	int state;
	struct gpio_desc *gp_status = gpio_to_desc(mdm->gpios[STATUS_IN]);
	int active_low = 0;

	if (gp_status)
		active_low = gpiod_is_active_low(gp_status);

	state = gpio_get_value(mdm->gpios[STATUS_IN]);
	if ((!active_low && !state) || (active_low && state)) {
		if (mdm->policy) {
			dev_info(mdm->dev, "Host undergoing SSR, leaving SDX as it is\n");
			sb_notifier_call_chain(EVENT_REMOTE_STATUS_DOWN, NULL);
		} else
			panic("Host undergoing SSR, panicking SDX\n");
	} else {
		dev_info(mdm->dev, "HOST booted\n");
		sb_notifier_call_chain(EVENT_REMOTE_STATUS_UP, NULL);
	}

	return IRQ_HANDLED;
}

static irqreturn_t sdx_ext_ipc_wakeup_irq(int irq, void *dev_id)
{
	pr_info("%s: Received\n", __func__);

	sb_notifier_call_chain(EVENT_REMOTE_WOKEN_UP, NULL);
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
		mdm->gpios[i] = (val >= 0) ? val : -1;
	}

	if (mdm->gpios[STATUS_IN] >= 0) {
		ret = gpio_request(mdm->gpios[STATUS_IN], "STATUS_IN");
		if (ret) {
			dev_err(mdm->dev,
				"Failed to configure STATUS_IN gpio\n");
			return ret;
		}
		gpio_direction_input(mdm->gpios[STATUS_IN]);

		irq = gpio_to_irq(mdm->gpios[STATUS_IN]);
		if (irq < 0) {
			dev_err(mdm->dev, "bad STATUS_IN IRQ resource\n");
			return irq;
		}
		mdm->status_irq = irq;
	} else
		dev_info(mdm->dev, "STATUS_IN not used\n");

	if (mdm->gpios[STATUS_OUT] >= 0) {
		ret = gpio_request(mdm->gpios[STATUS_OUT], "STATUS_OUT");
		if (ret) {
			dev_err(mdm->dev, "Failed to configure STATUS_OUT gpio\n");
			return ret;
		}
		gpio_direction_output(mdm->gpios[STATUS_OUT], 1);
	} else
		dev_info(mdm->dev, "STATUS_OUT not used\n");

	if (mdm->gpios[STATUS_OUT2] >= 0) {
		ret = gpio_request(mdm->gpios[STATUS_OUT2],
						"STATUS_OUT2");
		if (ret) {
			dev_err(mdm->dev, "Failed to configure STATUS_OUT2 gpio\n");
			return ret;
		}
		gpio_direction_output(mdm->gpios[STATUS_OUT2], 0);
	} else
		dev_info(mdm->dev, "STATUS_OUT2 not used\n");

	if (mdm->gpios[WAKEUP_OUT] >= 0) {
		ret = gpio_request(mdm->gpios[WAKEUP_OUT], "WAKEUP_OUT");

		if (ret) {
			dev_err(mdm->dev, "Failed to configure WAKEUP_OUT gpio\n");
			return ret;
		}
		gpio_direction_output(mdm->gpios[WAKEUP_OUT], 0);
	} else
		dev_info(mdm->dev, "WAKEUP_OUT not used\n");

	if (mdm->gpios[WAKEUP_IN] >= 0) {
		ret = gpio_request(mdm->gpios[WAKEUP_IN], "WAKEUP_IN");

		if (ret) {
			dev_warn(mdm->dev, "Failed to configure WAKEUP_IN gpio\n");
			return ret;
		}
		gpio_direction_input(mdm->gpios[WAKEUP_IN]);

		irq = gpio_to_irq(mdm->gpios[WAKEUP_IN]);
		if (irq < 0) {
			dev_err(mdm->dev, "bad WAKEUP_IN IRQ resource\n");
			return irq;
		}
		mdm->wakeup_irq = irq;
	} else
		dev_info(mdm->dev, "WAKEUP_IN not used\n");

	return 0;
}

static int sdx_ext_ipc_panic(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	void __iomem *addr = NULL;
	struct gpio_cntrl *mdm = container_of(this,
					struct gpio_cntrl, panic_blk);

	gpio_set_value(mdm->gpios[STATUS_OUT], 0);

	return NOTIFY_DONE;
}

static irqreturn_t hw_irq_handler(int irq, void *p)
{
	pm_system_wakeup();
	return IRQ_WAKE_THREAD;
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

	of_property_read_u32(node, "ipq807x_attach", &mdm->ipq807x_attach);
	if (mdm->ipq807x_attach) {
		dev_info(mdm->dev, "IPQ807x attach detected\n");
		mdm->gpio_base = ioremap(GPIO_BASE, GPIO_REG_SIZE);
	}

	ret = setup_ipc(mdm);
	if (ret) {
		dev_err(mdm->dev, "Error setting up gpios\n");
		return ret;
	}

	if (mdm->gpios[STATUS_OUT] >= 0) {
		mdm->panic_blk.notifier_call = sdx_ext_ipc_panic;
		atomic_notifier_chain_register(&panic_notifier_list,
						&mdm->panic_blk);
	}

	mutex_init(&mdm->policy_lock);
	mutex_init(&mdm->e911_lock);
	if (of_property_read_bool(pdev->dev.of_node, "qcom,default-policy-nop"))
		mdm->policy = SUBSYS_NOP;
	else
		mdm->policy = SUBSYS_PANIC;

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

	ret = device_create_file(mdm->dev, &dev_attr_set_remote_status);
	if (ret) {
		dev_err(mdm->dev, "cannot create sysfs attribute\n");
		goto sys_fail;
	}

	platform_set_drvdata(pdev, mdm);

	if (mdm->gpios[STATUS_IN] >= 0) {
		ret = devm_request_irq(mdm->dev, mdm->status_irq,
				ap_status_change,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				"ap status", mdm);
		if (ret < 0) {
			dev_err(mdm->dev,
				 "%s: STATUS_IN IRQ#%d request failed,\n",
				__func__, mdm->status_irq);
			goto irq_fail;
		}
		irq_set_irq_wake(mdm->status_irq, 1);
	}

	if (mdm->gpios[WAKEUP_IN] >= 0) {
		ret = devm_request_threaded_irq(mdm->dev, mdm->wakeup_irq,
				hw_irq_handler, sdx_ext_ipc_wakeup_irq,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT |
				IRQF_NO_SUSPEND, "sdx_ext_ipc_wakeup", mdm);
		if (ret < 0) {
			dev_err(mdm->dev,
				"%s: WAKEUP_IN IRQ#%d request failed,\n",
				__func__, mdm->wakeup_irq);
			goto irq_fail;
		}
	}

	if (mdm->gpios[WAKEUP_OUT] >= 0) {
		mdm->sideband_nb.notifier_call = sideband_notify;
		ret = sb_register_evt_listener(&mdm->sideband_nb);
		if (ret) {
			dev_err(mdm->dev,
				"%s: sb_register_evt_listener failed!\n",
				__func__);
			goto irq_fail;
		}
	}

	return 0;

irq_fail:
	device_remove_file(mdm->dev, &dev_attr_policy);
sys_fail1:
	device_remove_file(mdm->dev, &dev_attr_e911);
sys_fail:
	if (mdm->gpios[STATUS_OUT] >= 0)
		atomic_notifier_chain_unregister(&panic_notifier_list,
						&mdm->panic_blk);
	remove_ipc(mdm);
	return ret;
}

static int sdx_ext_ipc_remove(struct platform_device *pdev)
{
	struct gpio_cntrl *mdm;

	mdm = dev_get_drvdata(&pdev->dev);
	if (mdm->gpios[STATUS_IN] >= 0)
		disable_irq_wake(mdm->status_irq);
	if (mdm->gpios[STATUS_OUT] >= 0)
		atomic_notifier_chain_unregister(&panic_notifier_list,
						&mdm->panic_blk);
	remove_ipc(mdm);
	device_remove_file(mdm->dev, &dev_attr_policy);
	if (mdm->ipq807x_attach)
		iounmap(mdm->gpio_base);

	return 0;
}

#ifdef CONFIG_PM
static int sdx_ext_ipc_suspend(struct device *dev)
{
	struct gpio_cntrl *mdm = dev_get_drvdata(dev);

	if (mdm->gpios[WAKEUP_IN] >= 0)
		enable_irq_wake(mdm->wakeup_irq);
	return 0;
}

static int sdx_ext_ipc_resume(struct device *dev)
{
	struct gpio_cntrl *mdm = dev_get_drvdata(dev);

	if (mdm->gpios[WAKEUP_IN] >= 0)
		disable_irq_wake(mdm->wakeup_irq);
	return 0;
}

static const struct dev_pm_ops sdx_ext_ipc_pm_ops = {
	.suspend        =    sdx_ext_ipc_suspend,
	.resume         =    sdx_ext_ipc_resume,
};
#endif

static const struct of_device_id sdx_ext_ipc_of_match[] = {
	{ .compatible = "qcom,sdx-ext-ipc"},
	{},
};

static struct platform_driver sdx_ext_ipc_driver = {
	.probe		= sdx_ext_ipc_probe,
	.remove		= sdx_ext_ipc_remove,
	.driver = {
		.name	= "sdx-ext-ipc",
		.of_match_table = sdx_ext_ipc_of_match,
#ifdef CONFIG_PM
		.pm = &sdx_ext_ipc_pm_ops,
#endif
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
