#include <linux/device.h>
#include <linux/init.h>
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

struct dpdt_data {
	struct device *dev;
	int status_gpio;
};

static ssize_t dpdt_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dpdt_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", gpio_get_value(data->status_gpio));
}
static DEVICE_ATTR(status, 0444, dpdt_status_show, NULL);

static int dpdt_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct dpdt_data *data;

	pr_info("%s enter\n", __func__);

	data = devm_kzalloc(dev, sizeof(struct dpdt_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->status_gpio = of_get_named_gpio(np, "status-gpio", 0);
	if (data->status_gpio < 0)
		return -EINVAL;

	ret = sysfs_create_file(&dev->kobj, &dev_attr_status.attr);
	if (ret < 0) {
		dev_err(dev, "Failed to create sysfs node.\n");
		return -EINVAL;
	}

	data->dev = dev;
	platform_set_drvdata(pdev, data);

	return ret;
}

static int dpdt_remove(struct platform_device *pdev)
{
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_status.attr);
	return 0;
}

static const struct of_device_id dpdt_of_match[] = {
	{ .compatible = "xiaomi,dpdt-status", },
	{},
};

static struct platform_driver dpdt_status_driver = {
	.driver = {
		.name = "dpdt-status",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(dpdt_of_match),
	},
	.probe = dpdt_probe,
	.remove = dpdt_remove,
};

module_platform_driver(dpdt_status_driver);
MODULE_AUTHOR("Tao Jun<taojun@xiaomi.com>");
MODULE_DESCRIPTION("Xiaomi dpdt status");
MODULE_LICENSE("GPL");



