/*
 * tlv320aic325x-i2c.c  -- driver for TLV320AIC3XXX
 *
 * Author:	Mukund Navada <navada@ti.com>
 *		Mehar Bajwa <mehar.bajwa@ti.com>
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

#include <linux/mfd/tlv320aic325x-core.h>

struct regmap_config aic325x_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

static __devinit int aic325x_i2c_probe(struct i2c_client *i2c,
					  const struct i2c_device_id *id)
{
	struct aic325x *aic325x;
	const struct regmap_config *regmap_config;
	int ret;

	switch (id->driver_data) {
#ifdef CONFIG_AIC3262_CORE
	case TLV320AIC3262:
		regmap_config = &aic325x_i2c_regmap;
		break;
#endif
	case TLV320AIC3256:
		regmap_config = &aic325x_i2c_regmap;
		break;

	default:
		dev_err(&i2c->dev, "Unknown device type %ld\n",
			id->driver_data);
		return -EINVAL;
	}

	aic325x = devm_kzalloc(&i2c->dev, sizeof(*aic325x), GFP_KERNEL);
	if (aic325x == NULL)
		return -ENOMEM;

	aic325x->regmap = devm_regmap_init_i2c(i2c, regmap_config);

	if (IS_ERR(aic325x->regmap)) {
		ret = PTR_ERR(aic325x->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	aic325x->type = id->driver_data;
	aic325x->dev = &i2c->dev;
	aic325x->irq = i2c->irq;

	return aic325x_device_init(aic325x);
}

static int __devexit aic325x_i2c_remove(struct i2c_client *i2c)
{
	struct aic325x *aic325x = dev_get_drvdata(&i2c->dev);

	aic325x_device_exit(aic325x);
	return 0;
}

static const struct i2c_device_id aic325x_i2c_id[] = {
	{"tlv320aic3262", TLV320AIC3262},
	{"tlv320aic325x", TLV320AIC3256},
	{ }
};
MODULE_DEVICE_TABLE(i2c, aic325x_i2c_id);

static struct i2c_driver aic325x_i2c_driver = {
	.driver = {
		.name	= "tlv320aic325x",
		.owner	= THIS_MODULE,
	},
	.probe		= aic325x_i2c_probe,
	.remove		= __devexit_p(aic325x_i2c_remove),
	.id_table	= aic325x_i2c_id,
};

module_i2c_driver(aic325x_i2c_driver);

MODULE_DESCRIPTION("TLV320AIC3XXX I2C bus interface");
MODULE_AUTHOR("Mukund Navada <navada@ti.com>");
MODULE_LICENSE("GPL");
