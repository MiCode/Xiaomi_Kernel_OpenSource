#include <linux/slab.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/err.h>
#include <linux/of.h>

#include "lx_fg_class.h"

static struct class *fuel_gauge_class;

int fuel_gauge_get_avg_ibat(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_avg_ibat == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_avg_ibat(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_avg_ibat);

int fuel_gauge_get_soc_decimal(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_soc_decimal == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_soc_decimal(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_soc_decimal);

int fuel_gauge_get_soc_decimal_rate(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_soc_decimal_rate == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_soc_decimal_rate(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_soc_decimal_rate);

bool fuel_gauge_get_chip_ok(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_chip_ok == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_chip_ok(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_chip_ok);

int fuel_gauge_get_resistance_id(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_resistance_id == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_resistance_id(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_resistance_id);

int fuel_gauge_get_battery_id(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_fg_battery_id == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_fg_battery_id(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_battery_id);

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
int fuel_gauge_check_i2c_function(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->check_i2c_function == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->check_i2c_function(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_check_i2c_function);
#endif

int fuel_gauge_set_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge, bool en)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->set_fastcharge_mode == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->set_fastcharge_mode(fuel_gauge, en);
}
EXPORT_SYMBOL(fuel_gauge_set_fastcharge_mode);

int fuel_gauge_set_charger_to_full(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->fg_set_charger_to_full == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->fg_set_charger_to_full(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_set_charger_to_full);

int fuel_guage_mac_write_block(struct fuel_gauge_dev *fuel_gauge, u16 cmd, u8 *data, u8 len)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->fg_write_block == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->fg_write_block(fuel_gauge, cmd,	data, len);
}
EXPORT_SYMBOL(fuel_guage_mac_write_block);

int fuel_guage_mac_read_block(struct fuel_gauge_dev *fuel_gauge, u16 cmd, u8 *data, u8 len)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->fg_read_block == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->fg_read_block(fuel_gauge, cmd,	data, len);
}
EXPORT_SYMBOL(fuel_guage_mac_read_block);

int fuel_guage_write_reg_block(struct fuel_gauge_dev *fuel_gauge, u8 reg, u8 *data, u8 len)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->fg_reg_write_block == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->fg_reg_write_block(fuel_gauge, reg, data, len);
}
EXPORT_SYMBOL(fuel_guage_write_reg_block);

int fuel_guage_read_reg_block(struct fuel_gauge_dev *fuel_gauge, u8 reg, u8 *data, u8 len)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->fg_reg_read_block == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->fg_reg_read_block(fuel_gauge, reg, data, len);
}
EXPORT_SYMBOL(fuel_guage_read_reg_block);

int charger_partition1_get_prop(struct fuel_gauge_dev *fuel_gauge, int type, int *val)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->fg_get_partition_prop == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->fg_get_partition_prop(fuel_gauge, type, val);
}
EXPORT_SYMBOL(charger_partition1_get_prop);

int charger_partition1_set_prop(struct fuel_gauge_dev *fuel_gauge, int type, int val)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->fg_set_partition_prop == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->fg_set_partition_prop(fuel_gauge, type, val);
}
EXPORT_SYMBOL(charger_partition1_set_prop);

int fuel_gauge_get_input_suspend(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_input_suspend == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_input_suspend(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_input_suspend);

int fuel_gauge_get_lpd_charging(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_lpd_charging == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_lpd_charging(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_lpd_charging);

int fuel_gauge_get_mtbf_current(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_mtbf_current == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_mtbf_current(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_mtbf_current);

int fuel_gauge_get_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_fastcharge_mode == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_fastcharge_mode(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_fastcharge_mode);

void fuel_gauge_status_update(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return;
	if (fuel_gauge->ops->fg_update == NULL)
		return;
	fuel_gauge->ops->fg_update(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_status_update);

static int fuel_gauge_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct fuel_gauge_dev *fuel_gauge = dev_get_drvdata(dev);

	if (!name || !fuel_gauge)
		return -EINVAL;

	if (!fuel_gauge->name)
		return -EINVAL;

	return strcmp(fuel_gauge->name, name) == 0;
}

struct fuel_gauge_dev *fuel_gauge_find_dev_by_name(const char *name)
{
	struct fuel_gauge_dev *fuel_gauge = NULL;
	struct device *dev = class_find_device(fuel_gauge_class, NULL, name,
					fuel_gauge_match_device_by_name);

	if (dev) {
		fuel_gauge = dev_get_drvdata(dev);
	}

	return fuel_gauge;
}
EXPORT_SYMBOL(fuel_gauge_find_dev_by_name);


struct fuel_gauge_dev *fuel_gauge_register(char *name, struct device *parent,
							struct fuel_gauge_ops *ops, void *private)
{
	struct fuel_gauge_dev *fuel_gauge;
	struct device *dev;
	int ret;

	if (!parent)
		pr_warn("%s: Expected proper parent device\n", __func__);

	if (!ops || !name)
		return ERR_PTR(-EINVAL);

	fuel_gauge = kzalloc(sizeof(*fuel_gauge), GFP_KERNEL);
	if (!fuel_gauge)
		return ERR_PTR(-ENOMEM);

	dev = &(fuel_gauge->dev);

	device_initialize(dev);

	dev->class = fuel_gauge_class;
	dev->parent = parent;
	dev_set_drvdata(dev, fuel_gauge);

	fuel_gauge->private = private;

	ret = dev_set_name(dev, "%s", name);
	if (ret)
		goto dev_set_name_failed;

	ret = device_add(dev);
	if (ret)
		goto device_add_failed;

	fuel_gauge->name = name;
	fuel_gauge->ops = ops;

	return fuel_gauge;

device_add_failed:
dev_set_name_failed:
	put_device(dev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(fuel_gauge_register);


void *fuel_gauge_get_private(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge)
		return ERR_PTR(-EINVAL);
	return fuel_gauge->private;
}
EXPORT_SYMBOL(fuel_gauge_get_private);

int fuel_gauge_unregister(struct fuel_gauge_dev *fuel_gauge)
{
	device_unregister(&fuel_gauge->dev);
	kfree(fuel_gauge);
	return 0;
}

static int __init fuel_gauge_class_init(void)
{
	fuel_gauge_class = class_create(THIS_MODULE, "fuel_gauge_class");
	if (IS_ERR(fuel_gauge_class)) {
		return PTR_ERR(fuel_gauge_class);
	}

	fuel_gauge_class->dev_uevent = NULL;

	return 0;
}

static void __exit fuel_gauge_class_exit(void)
{
	class_destroy(fuel_gauge_class);
}

subsys_initcall(fuel_gauge_class_init);
module_exit(fuel_gauge_class_exit);

MODULE_DESCRIPTION("LC Fuel Gauge Class Core");
MODULE_LICENSE("GPL v2");
