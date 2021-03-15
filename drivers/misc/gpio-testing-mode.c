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

struct testing_mode_data {
	struct device *dev;
	int debounce_time;
	int status_gpio;
	int gpio_status;
	int irq;
	struct delayed_work debounce_work;
};

static ssize_t gpio_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct testing_mode_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", gpio_get_value(data->status_gpio));
}
static DEVICE_ATTR(status, S_IRUGO, gpio_status_show, NULL);

#define MAX_MSG_LENGTH 20
static void gpio_debounce_work(struct work_struct *work)
{
	struct testing_mode_data *data =
			container_of(work, struct testing_mode_data, debounce_work.work);
	struct device *dev = data->dev;
	char status_env[MAX_MSG_LENGTH];
	char *envp[] = { status_env, NULL };
	int gpio_status = gpio_get_value(data->status_gpio);

	if (gpio_status == data->gpio_status) {
		snprintf(status_env, MAX_MSG_LENGTH, "STATUS=%d", gpio_status);
		dev_info(dev, "Update testing mode status: %d\n", gpio_status);
		kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
	}
	return;
}
static irqreturn_t testing_threaded_irq_handler(int irq, void *irq_data)
{
	struct testing_mode_data *data = irq_data;
	struct device *dev = data->dev;

	data->gpio_status = gpio_get_value(data->status_gpio);
	dev_info(dev, "testing_threaded_irq triggered\n");
	mod_delayed_work(system_wq, &data->debounce_work, msecs_to_jiffies(data->debounce_time));

	return IRQ_HANDLED;
}

static int testing_mode_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct testing_mode_data *data;

	pr_info("%s enter\n", __func__);

	data = devm_kzalloc(dev, sizeof(struct testing_mode_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->status_gpio = of_get_named_gpio(np, "status-gpio", 0);
	if (data->status_gpio < 0)
		return -EINVAL;

	ret = of_property_read_u32(np, "debounce-time", &data->debounce_time);
	if (ret) {
		dev_info(dev, "Failed to get debounce-time, use default.\n");
		data->debounce_time = 5;
	}

	data->irq = gpio_to_irq(data->status_gpio);
	ret = devm_request_threaded_irq(dev, data->irq, NULL,
		testing_threaded_irq_handler, IRQF_ONESHOT | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "testing_mode", data);
	if (ret < 0) {
		dev_err(dev, "Failed to request irq.\n");
		return -EINVAL;
	}

	device_init_wakeup(dev, true);
	dev_pm_set_wake_irq(dev, data->irq);
	INIT_DELAYED_WORK(&data->debounce_work, gpio_debounce_work);
	ret = sysfs_create_file(&dev->kobj, &dev_attr_status.attr);
	if (ret < 0) {
		dev_err(dev, "Failed to create sysfs node.\n");
		return -EINVAL;
	}

	data->dev = dev;
	platform_set_drvdata(pdev, data);

	return ret;
}

static int testing_mode_remove(struct platform_device *pdev)
{
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_status.attr);
	return 0;
}

static const struct of_device_id testing_mode_of_match[] = {
	{ .compatible = "xiaomi,testing-mode", },
	{},
};

static struct platform_driver testing_mode_driver = {
	.driver = {
		.name = "testing-mode",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(testing_mode_of_match),
	},
	.probe = testing_mode_probe,
	.remove = testing_mode_remove,
};

module_platform_driver(testing_mode_driver);
MODULE_AUTHOR("Tao Jun<taojun@xiaomi.com>");
MODULE_DESCRIPTION("A simple driver for GPIO testing mode");
MODULE_LICENSE("GPL");
