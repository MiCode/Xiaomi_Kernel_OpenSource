/*
 * tlv320aic3xxx-i2c.c  -- driver for TLV320AIC3XXX TODO
 *
 * Author:	Mukund Navada <navada@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include <linux/mfd/tlv320aic3xxx-core.h>

struct regmap_config aicxxx_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

#ifdef CONFIG_PM
static int aic3xxx_suspend(struct device *dev)
{
	struct aic3xxx *aic3xxx = dev_get_drvdata(dev);

	aic3xxx->suspended = true;

	return 0;
}

static int aic3xxx_resume(struct device *dev)
{
	struct aic3xxx *aic3xxx = dev_get_drvdata(dev);

	aic3xxx->suspended = false;

	return 0;
}
#endif

static __devinit int aic3xxx_i2c_probe(struct i2c_client *i2c,
					  const struct i2c_device_id *id)
{
	struct aic3xxx *aicxxx;
	const struct regmap_config *regmap_config;
	int ret;

	switch (id->driver_data) {
	case TLV320AIC3262:
		regmap_config = &aicxxx_i2c_regmap;
		break;
#ifdef CONFIG_MFD_AIC3285
	case TLV320AIC3285:
		regmap_config = &aicxxx_i2c_regmap;
		break;
#endif
	default:
		dev_err(&i2c->dev, "Unknown device type %ld\n",
			id->driver_data);
		return -EINVAL;
	}

	aicxxx = devm_kzalloc(&i2c->dev, sizeof(*aicxxx), GFP_KERNEL);
	if (aicxxx == NULL)
		return -ENOMEM;

	aicxxx->regmap = devm_regmap_init_i2c(i2c, regmap_config);

	if (IS_ERR(aicxxx->regmap)) {
		ret = PTR_ERR(aicxxx->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	aicxxx->type = id->driver_data;
	aicxxx->dev = &i2c->dev;
	aicxxx->irq = i2c->irq;

	return aic3xxx_device_init(aicxxx, aicxxx->irq);
}

static int __devexit aic3xxx_i2c_remove(struct i2c_client *i2c)
{
	struct aic3xxx *aicxxx = dev_get_drvdata(&i2c->dev);
	aic3xxx_device_exit(aicxxx);
	return 0;
}

static void aic3xxx_i2c_shutdown(struct i2c_client *i2c)
{
	struct aic3xxx *aic3xxx = dev_get_drvdata(&i2c->dev);
	mutex_lock(&aic3xxx->io_lock);
	aic3xxx->shutdown_complete = 1;
	mutex_unlock(&aic3xxx->io_lock);
}

static const struct i2c_device_id aic3xxx_i2c_id[] = {
	{"tlv320aic3262", TLV320AIC3262},
	{"tlv320aic3285", TLV320AIC3285},
	{ }
};
MODULE_DEVICE_TABLE(i2c, aic3xxx_i2c_id);

static UNIVERSAL_DEV_PM_OPS(aic3xxx_pm_ops, aic3xxx_suspend, aic3xxx_resume,
				NULL);

static struct i2c_driver aic3xxx_i2c_driver = {
	.driver = {
		.name	= "tlv320aic3xxx",
		.owner	= THIS_MODULE,
		.pm	= &aic3xxx_pm_ops,
	},
	.probe		= aic3xxx_i2c_probe,
	.remove		= __devexit_p(aic3xxx_i2c_remove),
	.id_table	= aic3xxx_i2c_id,
	.shutdown	= aic3xxx_i2c_shutdown,
};

module_i2c_driver(aic3xxx_i2c_driver);

MODULE_DESCRIPTION("TLV320AIC3XXX I2C bus interface");
MODULE_AUTHOR("Mukund Navada <navada@ti.com>");
MODULE_LICENSE("GPL");
