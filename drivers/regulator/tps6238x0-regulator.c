/*
 * tps6238x0-regulator.c -- TI tps623850/tps623860/tps623870
 *
 * Driver for processor core supply tps623850, tps623860 and tps623870
 *
 * Copyright (c) 2012, NVIDIA Corporation.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/tps6238x0-regulator.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/regmap.h>

/* Register definitions */
#define REG_VSET0		0
#define REG_VSET1		1
#define REG_MODE0		2
#define REG_MODE1		3
#define REG_CONTROL		4
#define REG_EXCEPTION		5
#define REG_RAMPCTRL		6
#define REG_IOUT		7
#define REG_CHIPID		8

#define TPS6238X0_BASE_VOLTAGE	500000
#define TPS6238X0_N_VOLTAGES	128
#define TPS6238X0_MAX_VSET	2
#define TPS6238X0_VOUT_MASK	0x7F

/* tps 6238x0 chip information */
struct tps6238x0_chip {
	const char *name;
	struct device *dev;
	struct regulator_desc desc;
	struct regulator_dev *rdev;
	struct regmap *regmap;
	int vsel_gpio;
	int change_uv_per_us;
	bool en_internal_pulldn;
	bool valid_gpios;
	int lru_index[TPS6238X0_MAX_VSET];
	int curr_vset_vsel[TPS6238X0_MAX_VSET];
	int curr_vset_id;
};

/*
 * find_voltage_set_register: Find new voltage configuration register
 * (VSET) id.
 * The finding of the new VSET register will be based on the LRU mechanism.
 * Each VSET register will have different voltage configured . This
 * Function will look if any of the VSET register have requested voltage set
 * or not.
 *     - If it is already there then it will make that register as most
 *       recently used and return as found so that caller need not to set
 *       the VSET register but need to set the proper gpios to select this
 *       VSET register.
 *     - If requested voltage is not found then it will use the least
 *       recently mechanism to get new VSET register for new configuration
 *       and will return not_found so that caller need to set new VSET
 *       register and then gpios (both).
 */
static bool find_voltage_set_register(struct tps6238x0_chip *tps,
		int req_vsel, int *vset_reg_id)
{
	int i;
	bool found = false;
	int new_vset_reg = tps->lru_index[1];
	int found_index = 1;
	for (i = 0; i < TPS6238X0_MAX_VSET; ++i) {
		if (tps->curr_vset_vsel[tps->lru_index[i]] == req_vsel) {
			new_vset_reg = tps->lru_index[i];
			found_index = i;
			found = true;
			goto update_lru_index;
		}
	}

update_lru_index:
	for (i = found_index; i > 0; i--)
		tps->lru_index[i] = tps->lru_index[i - 1];

	tps->lru_index[0] = new_vset_reg;
	*vset_reg_id = new_vset_reg;
	return found;
}

static int tps6238x0_get_voltage_sel(struct regulator_dev *dev)
{
	struct tps6238x0_chip *tps = rdev_get_drvdata(dev);
	unsigned int data;
	int ret;

	ret = regmap_read(tps->regmap, REG_VSET0 + tps->curr_vset_id, &data);
	if (ret < 0) {
		dev_err(tps->dev, "%s: Error in reading register %d\n",
			__func__, REG_VSET0 + tps->curr_vset_id);
		return ret;
	}
	return data & TPS6238X0_VOUT_MASK;
}

static int tps6238x0_set_voltage(struct regulator_dev *dev,
	     int min_uV, int max_uV, unsigned *selector)
{
	struct tps6238x0_chip *tps = rdev_get_drvdata(dev);
	int vsel;
	int ret;
	bool found = false;
	int new_vset_id = tps->curr_vset_id;

	if (min_uV >
		((TPS6238X0_BASE_VOLTAGE + (TPS6238X0_N_VOLTAGES - 1) * 10000)))
		return -EINVAL;

	if ((max_uV < min_uV) || (max_uV < TPS6238X0_BASE_VOLTAGE))
		return -EINVAL;

	vsel = DIV_ROUND_UP(min_uV - TPS6238X0_BASE_VOLTAGE, 10000);
	if (selector)
		*selector = (vsel & TPS6238X0_VOUT_MASK);

	/*
	 * If gpios are available to select the VSET register then least
	 * recently used register for new configuration.
	 */
	if (tps->valid_gpios)
		found = find_voltage_set_register(tps, vsel, &new_vset_id);

	if (!found) {
		ret = regmap_update_bits(tps->regmap, REG_VSET0 + new_vset_id,
				TPS6238X0_VOUT_MASK, vsel);
		if (ret < 0) {
			dev_err(tps->dev, "%s: Error in updating register %d\n",
				 __func__, REG_VSET0 + new_vset_id);
			return ret;
		}
		tps->curr_vset_id = new_vset_id;
		tps->curr_vset_vsel[new_vset_id] = vsel;
	}

	/* Select proper VSET register vio gpios */
	if (tps->valid_gpios)
		gpio_set_value_cansleep(tps->vsel_gpio, new_vset_id & 0x1);
	return 0;
}

static int tps6238x0_list_voltage(struct regulator_dev *dev,
			unsigned selector)
{
	struct tps6238x0_chip *tps = rdev_get_drvdata(dev);

	if ((selector < 0) || (selector >= tps->desc.n_voltages))
		return -EINVAL;

	return TPS6238X0_BASE_VOLTAGE + selector * 10000;
}

static int tps6238x0_regulator_enable_time(struct regulator_dev *rdev)
{
	return 300;
}

static int tps6238x0_set_voltage_time_sel(struct regulator_dev *rdev,
		unsigned int old_selector, unsigned int new_selector)
{
	struct tps6238x0_chip *tps = rdev_get_drvdata(rdev);
	int old_uV, new_uV;
	old_uV = tps6238x0_list_voltage(rdev, old_selector);

	if (old_uV < 0)
		return old_uV;

	new_uV = tps6238x0_list_voltage(rdev, new_selector);
	if (new_uV < 0)
		return new_uV;

	return DIV_ROUND_UP(abs(old_uV - new_uV),
				tps->change_uv_per_us);
}

static int tps6238x0_is_enable(struct regulator_dev *rdev)
{
	struct tps6238x0_chip *tps = rdev_get_drvdata(rdev);
	unsigned int data;
	int ret;

	ret = regmap_read(tps->regmap, REG_VSET0 + tps->curr_vset_id, &data);
	if (ret < 0) {
		dev_err(tps->dev, "%s: Error in reading register %d\n",
			__func__, REG_VSET0 + tps->curr_vset_id);
		return ret;
	}
	return !!(data & BIT(7));
}

static int tps6238x0_enable(struct regulator_dev *rdev)
{
	struct tps6238x0_chip *tps = rdev_get_drvdata(rdev);
	int ret;
	int i;

	/* Enable required VSET configuration */
	for (i = 0; i < TPS6238X0_MAX_VSET; ++i) {
		unsigned int en = 0;
		if (tps->valid_gpios || (i == tps->curr_vset_id))
			en = BIT(7);

		ret = regmap_update_bits(tps->regmap, REG_VSET0 + i,
				BIT(7), en);
		if (ret < 0) {
			dev_err(tps->dev, "%s() fails in updating reg %d\n",
					__func__, REG_VSET0 + i);
			return ret;
		}
	}
	return ret;
}

static int tps6238x0_disable(struct regulator_dev *rdev)
{
	struct tps6238x0_chip *tps = rdev_get_drvdata(rdev);
	int ret;
	int i;

	/* Disable required VSET configuration */
	for (i = 0; i < TPS6238X0_MAX_VSET; ++i) {
		ret = regmap_update_bits(tps->regmap, REG_VSET0 + i,
				BIT(7), 0);
		if (ret < 0) {
			dev_err(tps->dev, "%s() fails in updating reg %d\n",
					__func__, REG_VSET0 + i);
			return ret;
		}
	}
	return ret;
}

static struct regulator_ops tps6238x0_ops = {
	.is_enabled		= tps6238x0_is_enable,
	.enable			= tps6238x0_enable,
	.disable		= tps6238x0_disable,
	.enable_time		= tps6238x0_regulator_enable_time,
	.set_voltage_time_sel	= tps6238x0_set_voltage_time_sel,
	.get_voltage_sel	= tps6238x0_get_voltage_sel,
	.set_voltage		= tps6238x0_set_voltage,
	.list_voltage		= tps6238x0_list_voltage,
};

static int tps6238x0_configure(struct tps6238x0_chip *tps,
		struct tps6238x0_regulator_platform_data *pdata)
{
	int ret;
	int i;

	/* Initailize internal pull up/down control */
	if (tps->en_internal_pulldn)
		ret = regmap_write(tps->regmap, REG_CONTROL, 0xC0);
	else
		ret = regmap_write(tps->regmap, REG_CONTROL, 0x0);
	if (ret < 0) {
		dev_err(tps->dev, "%s() fails in writing reg %d\n",
			__func__, REG_CONTROL);
		return ret;
	}

	/* Enable required VSET configuration */
	for (i = 0; i < TPS6238X0_MAX_VSET; ++i) {
		unsigned int en = 0;
		if (tps->valid_gpios || (i == tps->curr_vset_id))
			en = BIT(7);

		ret = regmap_update_bits(tps->regmap, REG_VSET0 + i,
				BIT(7), en);
		if (ret < 0) {
			dev_err(tps->dev, "%s() fails in updating reg %d\n",
					__func__, REG_VSET0 + i);
			return ret;
		}
	}

	/* Enable output discharge path to have faster discharge */
	ret = regmap_update_bits(tps->regmap, REG_RAMPCTRL, BIT(2), BIT(2));
	if (ret < 0)
		dev_err(tps->dev, "%s() fails in updating reg %d\n",
			__func__, REG_RAMPCTRL);
	tps->change_uv_per_us = 312;
	return ret;
}

static const struct regmap_config tps6238x0_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REG_CHIPID,
	.num_reg_defaults_raw = REG_CHIPID + 1,
	.cache_type = REGCACHE_RBTREE,
};

static int tps6238x0_probe(struct i2c_client *client,
				     const struct i2c_device_id *id)
{
	struct tps6238x0_regulator_platform_data *pdata;
	struct regulator_dev *rdev;
	struct tps6238x0_chip *tps;
	struct regulator_config config = { };
	int ret;
	int i;

	pdata = client->dev.platform_data;
	if (!pdata) {
		dev_err(&client->dev, "%s() Err: Platform data not found\n",
						__func__);
		return -EIO;
	}

	tps = devm_kzalloc(&client->dev, sizeof(*tps), GFP_KERNEL);
	if (!tps) {
		dev_err(&client->dev, "%s() Err: Memory allocation fails\n",
						__func__);
		return -ENOMEM;
	}

	tps->en_internal_pulldn = pdata->en_internal_pulldn;
	tps->vsel_gpio = pdata->vsel_gpio;
	tps->dev = &client->dev;

	tps->desc.name = id->name;
	tps->desc.id = 0;
	tps->desc.n_voltages = TPS6238X0_N_VOLTAGES;
	tps->desc.ops = &tps6238x0_ops;
	tps->desc.type = REGULATOR_VOLTAGE;
	tps->desc.owner = THIS_MODULE;
	tps->regmap = devm_regmap_init_i2c(client, &tps6238x0_regmap_config);
	if (IS_ERR(tps->regmap)) {
		ret = PTR_ERR(tps->regmap);
		dev_err(&client->dev, "%s() Err: Failed to allocate register"
			"map: %d\n", __func__, ret);
		return ret;
	}
	i2c_set_clientdata(client, tps);

	tps->curr_vset_id = (pdata->vsel_def_state & 1);
	tps->lru_index[0] = tps->curr_vset_id;
	tps->valid_gpios = false;

	if (gpio_is_valid(tps->vsel_gpio)) {
		int gpio_flag;
		gpio_flag = (tps->curr_vset_id) ?
				GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
		ret = gpio_request_one(tps->vsel_gpio,
					gpio_flag, "tps6238x0-vsel0");
		if (ret) {
			dev_err(&client->dev,
				"Err: Could not obtain vsel GPIO %d: %d\n",
						tps->vsel_gpio, ret);
			return ret;
		}
		tps->valid_gpios = true;

		/*
		 * Initialize the lru index with vset_reg id
		 * The index 0 will be most recently used and
		 * set with the tps->curr_vset_id */
		for (i = 0; i < TPS6238X0_MAX_VSET; ++i)
			tps->lru_index[i] = i;
		tps->lru_index[0] = tps->curr_vset_id;
		tps->lru_index[tps->curr_vset_id] = 0;
	}

	ret = tps6238x0_configure(tps, pdata);
	if (ret < 0) {
		dev_err(tps->dev, "%s() Err: Init fails with = %d\n",
				__func__, ret);
		goto err_init;
	}

	config.dev = &client->dev;
	config.init_data = pdata->init_data;
	config.driver_data = tps;

	/* Register the regulators */
	rdev = regulator_register(&tps->desc, &config);
	if (IS_ERR(rdev)) {
		dev_err(tps->dev, "%s() Err: Failed to register %s\n",
				__func__, id->name);
		ret = PTR_ERR(rdev);
		goto err_init;
	}

	tps->rdev = rdev;
	return 0;

err_init:
	if (gpio_is_valid(tps->vsel_gpio))
		gpio_free(tps->vsel_gpio);

	return ret;
}

/**
 * tps6238x0_remove - tps62360 driver i2c remove handler
 * @client: i2c driver client device structure
 *
 * Unregister TPS driver as an i2c client device driver
 */
static int tps6238x0_remove(struct i2c_client *client)
{
	struct tps6238x0_chip *tps = i2c_get_clientdata(client);

	if (gpio_is_valid(tps->vsel_gpio))
		gpio_free(tps->vsel_gpio);

	regulator_unregister(tps->rdev);
	return 0;
}

static const struct i2c_device_id tps6238x0_id[] = {
	{.name = "tps623850", },
	{.name = "tps623860", },
	{.name = "tps623870", },
	{},
};

MODULE_DEVICE_TABLE(i2c, tps6238x0_id);

static struct i2c_driver tps6238x0_i2c_driver = {
	.driver = {
		.name = "tps6238x0",
		.owner = THIS_MODULE,
	},
	.probe = tps6238x0_probe,
	.remove = tps6238x0_remove,
	.id_table = tps6238x0_id,
};

static int __init tps6238x0_init(void)
{
	return i2c_add_driver(&tps6238x0_i2c_driver);
}
subsys_initcall(tps6238x0_init);

static void __exit tps6238x0_cleanup(void)
{
	i2c_del_driver(&tps6238x0_i2c_driver);
}
module_exit(tps6238x0_cleanup);

MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_DESCRIPTION("TPS623850/TPS623860/TPS623870 voltage regulator driver");
MODULE_LICENSE("GPL v2");
