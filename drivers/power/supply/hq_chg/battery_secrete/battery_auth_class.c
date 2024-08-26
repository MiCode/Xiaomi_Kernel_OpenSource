#include <linux/module.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/gpio/driver.h>
#include <linux/of_gpio.h>

#include "battery_auth_class.h"

static struct class *auth_class;
static struct gpio_desc *gpiod;

static ssize_t name_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct auth_device *auth = to_auth_device(dev);

	return snprintf(buf, 20, "%s\n",
			auth->name ? auth->name : "anonymous");
}

static DEVICE_ATTR_RO(name);

static struct attribute *auth_attrs[] = {
	&dev_attr_name.attr,
	NULL,
};

static const struct attribute_group auth_group = {
	.attrs = auth_attrs,
};

static const struct attribute_group *auth_groups[] = {
	&auth_group,
	NULL,
};

int auth_device_start_auth(struct auth_device *auth_dev)
{
	if (auth_dev != NULL && auth_dev->ops != NULL &&
	    auth_dev->ops->auth_battery != NULL)
		return auth_dev->ops->auth_battery(auth_dev);

	return -EOPNOTSUPP;
}

EXPORT_SYMBOL(auth_device_start_auth);

int auth_device_get_batt_id(struct auth_device *auth_dev, u8 * id)
{
	if (auth_dev != NULL && auth_dev->ops != NULL &&
	    auth_dev->ops->get_battery_id != NULL)
		return auth_dev->ops->get_battery_id(auth_dev, id);

	return -EOPNOTSUPP;
}

EXPORT_SYMBOL(auth_device_get_batt_id);

static void auth_device_release(struct device *dev)
{
	struct auth_device *auth_dev = to_auth_device(dev);

	kfree(auth_dev);
}

struct auth_device *auth_device_register(const char *name,
					 struct device *parent,
					 void *devdata,
					 const struct auth_ops *ops)
{
	struct auth_device *auth_dev;

	int rc;

	pr_debug("%s: name = %s\n", __func__, name);

	auth_dev = kzalloc(sizeof(*auth_dev), GFP_KERNEL);
	if (!auth_dev)
		return ERR_PTR(-ENOMEM);

	raw_spin_lock_init(&auth_dev->io_lock);

	auth_dev->dev.class = auth_class;
	auth_dev->dev.parent = parent;
	auth_dev->dev.release = auth_device_release;
	auth_dev->gpiod = gpiod;
	dev_set_name(&auth_dev->dev, name);
	dev_set_drvdata(&auth_dev->dev, devdata);

	rc = device_register(&auth_dev->dev);
	if (rc) {
		kfree(auth_dev);
		return ERR_PTR(rc);
	}

	auth_dev->ops = ops;

	return auth_dev;
}

EXPORT_SYMBOL(auth_device_register);

void auth_device_unregister(struct auth_device *auth_dev)
{
	if (!auth_dev)
		return;

	auth_dev->ops = NULL;

	device_unregister(&auth_dev->dev);
}

EXPORT_SYMBOL(auth_device_unregister);

static int auth_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;

	return strcmp(dev_name(dev), name) == 0;
}

struct auth_device *get_batt_auth_by_name(const char *name)
{
	struct device *dev;

	if (!name)
		return (struct auth_device *) NULL;

	dev = class_find_device(auth_class, NULL, name,
				auth_match_device_by_name);

	return dev ? to_auth_device(dev) : NULL;
}

EXPORT_SYMBOL(get_batt_auth_by_name);

static int onewire_gpio_probe(struct platform_device *pdev)
{
	enum gpiod_flags gflags = GPIOD_OUT_LOW_OPEN_DRAIN;

	auth_class = class_create(THIS_MODULE, "battery_auth");
	if (IS_ERR(auth_class)) {
		pr_notice("Unable to create auth class(%d)\n",
			  PTR_ERR(auth_class));
		return PTR_ERR(auth_class);
	}

	auth_class->dev_groups = auth_groups;

	gpiod = devm_gpiod_get_index(&(pdev->dev), NULL, 0, gflags);
	if (IS_ERR(gpiod)) {
		pr_err("%s gpio_request (pin) failed\n", __func__);
		return PTR_ERR(gpiod);
	}

	gpiod_direction_output(gpiod, 1);

	return 0;
}

static const struct of_device_id onewire_gpio_dt_match[] = {
	{.compatible = "xiaomi,onewire_gpio"},
	{},
};

static struct platform_driver onewire_gpio_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "onewire_gpio",
		   .of_match_table = onewire_gpio_dt_match,
		   },
	.probe = onewire_gpio_probe,
};

static int __init batt_auth_class_init(void)
{
	return platform_driver_register(&onewire_gpio_driver);
}

static void __exit batt_auth_class_exit(void)
{
	platform_driver_unregister(&onewire_gpio_driver);
}

module_init(batt_auth_class_init);
module_exit(batt_auth_class_exit);

MODULE_LICENSE("GPL");
