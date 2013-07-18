/*  Copyright (c) 2011  Bosch Sensortec GmbH
    Copyright (c) 2011  Unixphere

    Based on:
    BMP085 driver, bmp085.c
    Copyright (c) 2010  Christoph Mair <christoph.mair@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include "bmp18x.h"

struct sensor_regulator {
	struct regulator *vreg;
	const char *name;
	u32	min_uV;
	u32	max_uV;
};

struct sensor_regulator bmp_vreg[] = {
	{NULL, "vdd", 2850000, 2850000},
	{NULL, "vddio", 1800000, 1800000},
};


static int bmp18x_config_regulator(struct i2c_client *client, bool on)
{
	int rc = 0, i;
	int num_vreg = ARRAY_SIZE(bmp_vreg);

	if (on) {
		for (i = 0; i < num_vreg; i++) {
			bmp_vreg[i].vreg = regulator_get(&client->dev,
					bmp_vreg[i].name);
			if (IS_ERR(bmp_vreg[i].vreg)) {
				rc = PTR_ERR(bmp_vreg[i].vreg);
				dev_err(&client->dev, "%s:regulator get failed rc=%d\n",
						__func__, rc);
				bmp_vreg[i].vreg = NULL;
				goto error_vdd;
			}
			if (regulator_count_voltages(bmp_vreg[i].vreg) > 0) {
				rc = regulator_set_voltage(bmp_vreg[i].vreg,
					bmp_vreg[i].min_uV, bmp_vreg[i].max_uV);
				if (rc) {
					dev_err(&client->dev, "%s:set_voltage failed rc=%d\n",
							__func__, rc);
					regulator_put(bmp_vreg[i].vreg);
					bmp_vreg[i].vreg = NULL;
					goto error_vdd;
				}
			}
			rc = regulator_enable(bmp_vreg[i].vreg);
			if (rc) {
				dev_err(&client->dev, "%s: regulator_enable failed rc =%d\n",
						__func__, rc);
				if (regulator_count_voltages(bmp_vreg[i].vreg)
						> 0) {
					regulator_set_voltage(bmp_vreg[i].vreg,
							0, bmp_vreg[i].max_uV);
				}
				regulator_put(bmp_vreg[i].vreg);
				bmp_vreg[i].vreg = NULL;
				goto error_vdd;
			}
		}
		return rc;
	} else {
		i = num_vreg;
	}
error_vdd:
	while (--i >= 0) {
		if (!IS_ERR_OR_NULL(bmp_vreg[i].vreg)) {
			if (regulator_count_voltages(
				bmp_vreg[i].vreg) > 0) {
				regulator_set_voltage(bmp_vreg[i].vreg, 0,
						bmp_vreg[i].max_uV);
			}
			regulator_disable(bmp_vreg[i].vreg);
			regulator_put(bmp_vreg[i].vreg);
			bmp_vreg[i].vreg = NULL;
		}
	}
	return rc;
}

static int bmp18x_init_hw(struct bmp18x_data_bus *data_bus)
{
	if (data_bus->client)
		return bmp18x_config_regulator(data_bus->client, 1);
	return 0;
}

static void bmp18x_deinit_hw(struct bmp18x_data_bus *data_bus)
{
	if (data_bus->client)
		bmp18x_config_regulator(data_bus->client, 0);
}

#ifdef CONFIG_OF
static int bmp18x_parse_dt(struct device *dev,
			struct bmp18x_platform_data *pdata)
{
	int ret = 0;
	u32 val;

	ret = of_property_read_u32(dev->of_node, "bosch,chip-id", &val);
	if (ret) {
		dev_err(dev, "no chip_id from dt\n");
		return ret;
	}
	pdata->chip_id = (u8)val;

	ret = of_property_read_u32(dev->of_node, "bosch,oversample", &val);
	if (ret) {
		dev_err(dev, "no default_oversampling from dt\n");
		return ret;
	}
	pdata->default_oversampling = (u8)val;

	ret = of_property_read_u32(dev->of_node, "bosch,period",
				&pdata->temp_measurement_period);
	if (ret) {
		dev_err(dev, "no temp_measurement_period from dt\n");
		return ret;
	}

	pdata->default_sw_oversampling = of_property_read_bool(dev->of_node,
			"bosch,sw-oversample");
	return 0;
}
#else
static int bmp18x_parse_dt(struct device *dev,
			struct bmp18x_platform_data *pdata)
{
	return -EINVAL;
}
#endif

static int bmp18x_i2c_read_block(void *client, u8 reg, int len, char *buf)
{
	return i2c_smbus_read_i2c_block_data(client, reg, len, buf);
}

static int bmp18x_i2c_read_byte(void *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

static int bmp18x_i2c_write_byte(void *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

static const struct bmp18x_bus_ops bmp18x_i2c_bus_ops = {
	.read_block	= bmp18x_i2c_read_block,
	.read_byte	= bmp18x_i2c_read_byte,
	.write_byte	= bmp18x_i2c_write_byte
};

static int __devinit bmp18x_i2c_probe(struct i2c_client *client,
				      const struct i2c_device_id *id)
{
	struct bmp18x_data_bus data_bus = {
		.bops = &bmp18x_i2c_bus_ops,
		.client = client
	};
	struct bmp18x_platform_data *pdata;
	int ret;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct bmp18x_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		ret =  bmp18x_parse_dt(&client->dev, pdata);
		if (ret) {
			dev_err(&client->dev, "Failed to parse device tree\n");
			return ret;
		}
		pdata->init_hw = bmp18x_init_hw;
		pdata->deinit_hw = bmp18x_deinit_hw;
		client->dev.platform_data = pdata;
	}
	return bmp18x_probe(&client->dev, &data_bus);
}

static void bmp18x_i2c_shutdown(struct i2c_client *client)
{
	bmp18x_disable(&client->dev);
}

static int bmp18x_i2c_remove(struct i2c_client *client)
{
	return bmp18x_remove(&client->dev);
}

#ifdef CONFIG_PM
static int bmp18x_i2c_suspend(struct device *dev)
{
	return bmp18x_disable(dev);
}

static int bmp18x_i2c_resume(struct device *dev)
{
	return bmp18x_enable(dev);
}

static const struct dev_pm_ops bmp18x_i2c_pm_ops = {
	.suspend	= bmp18x_i2c_suspend,
	.resume		= bmp18x_i2c_resume
};
#endif

static const struct i2c_device_id bmp18x_id[] = {
	{ BMP18X_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bmp18x_id);

static const struct of_device_id bmp18x_of_match[] = {
	{ .compatible = "bosch,bmp180", },
	{ },
};

static struct i2c_driver bmp18x_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= BMP18X_NAME,
#ifdef CONFIG_PM
		.pm	= &bmp18x_i2c_pm_ops,
#endif
		.of_match_table = bmp18x_of_match,
	},
	.id_table	= bmp18x_id,
	.probe		= bmp18x_i2c_probe,
	.shutdown	= bmp18x_i2c_shutdown,
	.remove		= __devexit_p(bmp18x_i2c_remove)
};

static int __init bmp18x_i2c_init(void)
{
	return i2c_add_driver(&bmp18x_i2c_driver);
}

static void __exit bmp18x_i2c_exit(void)
{
	i2c_del_driver(&bmp18x_i2c_driver);
}


MODULE_AUTHOR("Eric Andersson <eric.andersson@unixphere.com>");
MODULE_DESCRIPTION("BMP18X I2C bus driver");
MODULE_LICENSE("GPL");

module_init(bmp18x_i2c_init);
module_exit(bmp18x_i2c_exit);
