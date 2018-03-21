/**
 * Copyright (C) 2017.9.1 Wingtech
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * Songmuchun Create <songmuchun@wingtech.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#define pr_fmt(fmt) "[radio_frequence]: " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pm_wakeup.h>

/*
 * Debug messages level
 */
static int debug;
module_param(debug, int, 0644);

/* debug code */
#define radio_fre_dbg(msg...) do {    \
	if (debug > 0) {                  \
		pr_info(msg);                 \
	}                                 \
} while (0)

/**
 * struct radio_freq_platform_data - The radio_freq_platform_data structure
 * @irq:                irq number(virtual).
 * @type:               type of the event (EV_KEY, EV_REL, etc...).
 * @code:               event code.
 * @det_gpio:           gpio number.
 * @debounce_interval:  how many msecs to debounce from device tree.
 * @timer_debounce:     software time debounce when gpiolib doesn't provide debounce.
 * @work:               delayed work.
 * @det_gpio_flags:     gpio flags from device tree.
 * @input_dev:          input device that is registered.
 */
struct radio_freq_platform_data {
	int irq;
	unsigned int type;
	unsigned int code[2];
	int det_gpio;
	int debounce_interval;
	int timer_debounce;
	struct delayed_work work;
	enum of_gpio_flags det_gpio_flags;
	struct input_dev *input_dev;
};

static void radio_freq__work_func(struct work_struct *work)
{
	struct radio_freq_platform_data *pdata =
		container_of(work, struct radio_freq_platform_data, work.work);
	int value;
	unsigned int code;

	value = gpio_get_value(pdata->det_gpio);
	radio_fre_dbg("gpio input value = %d\n", value);

	code = value ? pdata->code[0] : pdata->code[1];
	input_event(pdata->input_dev, pdata->type, code, 1);
	input_sync(pdata->input_dev);
	input_event(pdata->input_dev, pdata->type, code, 0);
	input_sync(pdata->input_dev);
}

static irqreturn_t radio_freq_interrupt(int irq, void *dev_id)
{
	struct radio_freq_platform_data *pdata = dev_id;

	radio_fre_dbg("irq enter\n");
	BUG_ON(irq != pdata->irq);

	mod_delayed_work(system_wq,
			&pdata->work,
			msecs_to_jiffies(pdata->timer_debounce));
	radio_fre_dbg("irq exit\n");

	return IRQ_HANDLED;
}

static int radio_freq_input_dev_init(struct device *dev)
{
	int i;
	struct radio_freq_platform_data *pdata = dev_get_drvdata(dev);
	struct input_dev *input_dev = pdata->input_dev;

	/* Init and register input device */
	input_dev->name = dev->driver->name;
	input_dev->id.bustype = BUS_HOST;
	input_dev->dev.parent = dev;

	input_set_drvdata(input_dev, pdata);
	for (i = 0; i < sizeof(pdata->code) / sizeof(pdata->code[0]); i++)
		input_set_capability(input_dev, pdata->type, pdata->code[i]);

	return input_register_device(input_dev);
}

/**
 * radio_freq_prase_dt - prase device tree.
 * @dev: pointer to the device structure
 *
 * @return: 0 if success, otherwise negative number will be return.
 */
static int radio_freq_prase_dt(struct device *dev)
{
	struct device_node *dev_node = dev->of_node;
	struct platform_device *pdev = to_platform_device(dev);
	struct radio_freq_platform_data *pdata = dev_get_drvdata(dev);

	if (of_property_read_u32(dev_node, "debounce-interval", &pdata->debounce_interval))
		pdata->debounce_interval = 10;

	pdata->det_gpio = of_get_gpio_flags(dev_node, 0, &pdata->det_gpio_flags);
	if (unlikely(!gpio_is_valid(pdata->det_gpio))) {
		pr_err("failed to prase gpios property!\n");
		return -EPERM;
	}

	if (of_property_read_u32_array(dev_node, "linux,code", pdata->code,
				sizeof(pdata->code) / sizeof(pdata->code[0]))) {
		pr_err("without keycode: 0x%x\n", pdata->det_gpio);
		return -EPERM;
	}

	if (of_property_read_u32(dev_node, "linux,input-type", &pdata->type))
		pdata->type = EV_KEY;

	pdata->irq = platform_get_irq(pdev, 0);
	if (unlikely(pdata->irq < 0)) {
		pr_err("failed to prase irq property!\n");
		return pdata->irq;
	}

	return 0;
}

static int radio_freq_suspend(struct device *dev)
{
	struct radio_freq_platform_data *pdata = dev_get_drvdata(dev);

	radio_fre_dbg("suspend\n");
	enable_irq_wake(pdata->irq);

	return 0;
}

static int radio_freq_resume(struct device *dev)
{
	struct radio_freq_platform_data *pdata = dev_get_drvdata(dev);

	radio_fre_dbg("resume\n");
	disable_irq_wake(pdata->irq);

	return 0;
}

/**
 * radio_freq_quiesce - prase device tree.
 * @data: pointer to the private data
 *
 * This function will cancel the delayed work which is registered.When the driver probe
 * is failure or driver is removed, the driver model will invoke this function to free
 * device resource.
 */
static void radio_freq_quiesce(void *data)
{
	struct radio_freq_platform_data *pdata = data;

	cancel_delayed_work(&pdata->work);
}

static int radio_freq_probe(struct platform_device *pdev)
{
	int ret;
	struct radio_freq_platform_data *pdata;
	struct device *dev = &pdev->dev;
	const char *devname = dev_name(dev);

	pr_info("radio frequence detection probe start\n");

	if (likely(pdev->dev.of_node)) {
		pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			pr_err("failed to allocate memory!\n");
			return -ENOMEM;
		}
		platform_set_drvdata(pdev, pdata);

		ret = radio_freq_prase_dt(dev);
		if (ret < 0) {
			pr_err("failed to prase device tree!\n");
			return ret;
		}
	} else {
		pr_err("device node is not exist!\n");
		return -EPERM;
	}

	/**
	* Allocate and register input device.
	*/
	pdata->input_dev = devm_input_allocate_device(dev);
	if (!pdata->input_dev) {
		pr_err("failed to allocate input device!\n");
		return -EPERM;
	}
	ret = radio_freq_input_dev_init(dev);
	if (unlikely(ret)) {
		pr_err("failed to register input device!\n");
		return ret;
	}

	/**
	* After prase dt, we can do some usingfull thing.
	*/
	ret = gpio_set_debounce(pdata->det_gpio, pdata->debounce_interval * 1000);
	/* use timer if gpiolib doesn't provide debounce */
	if (ret < 0)
		pdata->timer_debounce = pdata->debounce_interval;

	INIT_DELAYED_WORK(&pdata->work, radio_freq__work_func);
	ret = devm_add_action(dev, radio_freq_quiesce, pdata);
	if (unlikely(ret)) {
		pr_err("failed to register quiesce action, error: %d\n", ret);
		return ret;
	}

	ret = devm_gpio_request_one(dev, pdata->det_gpio,
			GPIOF_IN | GPIOF_EXPORT, "radio_frequence_det_gpio");
	if (unlikely(ret)) {
		pr_err("failed to request gpio %d\n", pdata->det_gpio);
		return ret;
	}

	ret = devm_request_any_context_irq(dev, pdata->irq, radio_freq_interrupt,
			irq_get_trigger_type(pdata->irq),
			devname ?: dev->driver->name, pdata);
	if (unlikely(ret)) {
		pr_err("failed to request irq %d\n", pdata->irq);
		return ret;
	}

	device_init_wakeup(dev, true);

	pr_info("radio frequence detection probe end\n");

	return 0;
}

static int radio_freq_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id radio_freq_of_match[] = {
	{.compatible = "wingtech,radio_frequence_detection",},
	{/* sentinel */}
};

MODULE_DEVICE_TABLE(of, radio_freq_of_match);

static SIMPLE_DEV_PM_OPS(radio_freq_pm_ops, radio_freq_suspend, radio_freq_resume);

static struct platform_driver radio_freq_driver = {
	.driver = {
		.owner          = THIS_MODULE,
		.name           = "radio_frequence",
		.pm             = &radio_freq_pm_ops,
		.of_match_table = of_match_ptr(radio_freq_of_match),
	},
	.probe	= radio_freq_probe,
	.remove	= radio_freq_remove,
};

module_platform_driver(radio_freq_driver);

MODULE_DESCRIPTION("radio frequence gpio check driver");
MODULE_AUTHOR("Songmuchun <songmuchun@wingtech.com>");
MODULE_LICENSE("GPL");
