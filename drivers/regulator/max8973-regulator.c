/*
 * max8973-regulator.c -- Maxim max8973
 *
 * Regulator driver for MAXIM 8973 DC-DC step-down switching regulator.
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
#include <linux/regulator/max8973-regulator.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/regmap.h>

/* Register definitions */
#define MAX8973_VOUT			0x0
#define MAX8973_VOUT_DVS		0x1
#define MAX8973_CONTROL1		0x2
#define MAX8973_CONTROL2		0x3
#define MAX8973_CHIPID1			0x4
#define MAX8973_CHIPID2			0x5

#define MAX8973_MAX_VOUT_REG		2

/* MAX8973_VOUT */
#define MAX8973_VOUT_ENABLE		BIT(7)
#define MAX8973_VOUT_MASK		0x7F

/* MAX8973_VOUT_DVS */
#define MAX8973_DVS_VOUT_MASK		0x7F

/* MAX8973_CONTROL1 */
#define MAX8973_SNS_ENABLE		BIT(7)
#define MAX8973_FPWM_EN_M		BIT(6)
#define MAX8973_NFSR_ENABLE		BIT(5)
#define MAX8973_AD_ENABLE		BIT(4)
#define MAX8973_BIAS_ENABLE		BIT(3)
#define MAX8973_FREQSHIFT_9PER		BIT(2)

#define MAX8973_RAMP_12mV_PER_US	0x0
#define MAX8973_RAMP_25mV_PER_US	0x1
#define MAX8973_RAMP_50mV_PER_US	0x2
#define MAX8973_RAMP_200mV_PER_US	0x3

/* MAX8973_CONTROL2 */
#define MAX8973_WDTMR_ENABLE		BIT(6)
#define MAX8973_DISCH_ENBABLE		BIT(5)
#define MAX8973_FT_ENABLE		BIT(4)

#define MAX8973_CKKADV_TRIP_DISABLE			0xC
#define MAX8973_CKKADV_TRIP_75mV_PER_US			0x0
#define MAX8973_CKKADV_TRIP_150mV_PER_US		0x4
#define MAX8973_CKKADV_TRIP_75mV_PER_US_HIST_DIS	0x8

#define MAX8973_INDUCTOR_MIN_30_PER	0x0
#define MAX8973_INDUCTOR_NOMINAL	0x1
#define MAX8973_INDUCTOR_PLUS_30_PER	0x2
#define MAX8973_INDUCTOR_PLUS_60_PER	0x3

#define MAX8973_MIN_VOLATGE		606250
#define MAX8973_MAX_VOLATGE		1400000
#define MAX8973_VOLATGE_STEP		6250
#define MAX8973_BUCK_N_VOLTAGE	\
	(((MAX8973_MAX_VOLATGE - MAX8973_MIN_VOLATGE) / MAX8973_VOLATGE_STEP) \
			+ 1)

/* Maxim 8973 chip information */
struct max8973_chip {
	struct device *dev;
	struct regulator_desc desc;
	struct regulator_dev *rdev;
	struct regmap *regmap;
	bool enable_external_control;
	int dvs_gpio;
	int lru_index[MAX8973_MAX_VOUT_REG];
	int curr_vout_val[MAX8973_MAX_VOUT_REG];
	int curr_vout_reg;
	int curr_gpio_val;
	int change_uv_per_us;
	bool valid_dvs_gpio;
};

/*
 * find_voltage_set_register: Find new voltage configuration register (VOUT).
 * The finding of the new VOUT register will be based on the LRU mechanism.
 * Each VOUT register will have different voltage configured . This
 * Function will look if any of the VOUT register have requested voltage set
 * or not.
 *     - If it is already there then it will make that register as most
 *       recently used and return as found so that caller need not to set
 *       the VOUT register but need to set the proper gpios to select this
 *       VOUT register.
 *     - If requested voltage is not found then it will use the least
 *       recently mechanism to get new VOUT register for new configuration
 *       and will return not_found so that caller need to set new VOUT
 *       register and then gpios (both).
 */
static bool find_voltage_set_register(struct max8973_chip *tps,
		int req_vsel, int *vout_reg, int *gpio_val)
{
	int i;
	bool found = false;
	int new_vout_reg = tps->lru_index[MAX8973_MAX_VOUT_REG - 1];
	int found_index = MAX8973_MAX_VOUT_REG - 1;

	for (i = 0; i < MAX8973_MAX_VOUT_REG; ++i) {
		if (tps->curr_vout_val[tps->lru_index[i]] == req_vsel) {
			new_vout_reg = tps->lru_index[i];
			found_index = i;
			found = true;
			goto update_lru_index;
		}
	}

update_lru_index:
	for (i = found_index; i > 0; i--)
		tps->lru_index[i] = tps->lru_index[i - 1];

	tps->lru_index[0] = new_vout_reg;
	*gpio_val = new_vout_reg;
	*vout_reg = MAX8973_VOUT + new_vout_reg;
	return found;
}

static int max8973_dcdc_get_voltage_sel(struct regulator_dev *rdev)
{
	struct max8973_chip *max = rdev_get_drvdata(rdev);
	unsigned int data;
	int ret;

	ret = regmap_read(max->regmap, max->curr_vout_reg, &data);
	if (ret < 0) {
		dev_err(max->dev, "%s(): register %d read failed with err %d\n",
			__func__, max->curr_vout_reg, ret);
		return ret;
	}
	return data & MAX8973_VOUT_MASK;
}

static int max8973_dcdc_set_voltage(struct regulator_dev *rdev,
	     int min_uV, int max_uV, unsigned *selector)
{
	struct max8973_chip *max = rdev_get_drvdata(rdev);
	int vsel;
	int ret;
	bool found = false;
	int vout_reg = max->curr_vout_reg;
	int gpio_val = max->curr_gpio_val;

	if ((max_uV < min_uV) || (max_uV < MAX8973_MIN_VOLATGE) ||
			(min_uV > MAX8973_MAX_VOLATGE))
		return -EINVAL;

	vsel = DIV_ROUND_UP(min_uV - MAX8973_MIN_VOLATGE, MAX8973_VOLATGE_STEP);
	if (selector)
		*selector = (vsel & MAX8973_VOUT_MASK);

	/*
	 * If gpios are available to select the VOUT register then least
	 * recently used register for new configuration.
	 */
	if (max->valid_dvs_gpio)
		found = find_voltage_set_register(max, vsel,
					&vout_reg, &gpio_val);

	if (!found) {
		ret = regmap_update_bits(max->regmap, vout_reg,
						MAX8973_VOUT_MASK, vsel);
		if (ret < 0) {
			dev_err(max->dev,
				"%s(): register %d update failed with err %d\n",
				 __func__, vout_reg, ret);
			return ret;
		}
		max->curr_vout_reg = vout_reg;
		max->curr_vout_val[gpio_val] = vsel;
	}

	/* Select proper VOUT register vio gpios */
	if (max->valid_dvs_gpio) {
		gpio_set_value_cansleep(max->dvs_gpio, gpio_val & 0x1);
		max->curr_gpio_val = gpio_val;
	}
	return 0;
}

static int max8973_dcdc_list_voltage(struct regulator_dev *rdev,
					unsigned selector)
{
	if (selector >= MAX8973_BUCK_N_VOLTAGE)
		return -EINVAL;

	return MAX8973_MIN_VOLATGE + selector * MAX8973_VOLATGE_STEP;
}

static int max8973_dcdc_set_voltage_time_sel(struct regulator_dev *rdev,
		unsigned int old_selector, unsigned int new_selector)
{
	struct max8973_chip *max = rdev_get_drvdata(rdev);
	int old_uV, new_uV;

	old_uV = max8973_dcdc_list_voltage(rdev, old_selector);
	if (old_uV < 0)
		return old_uV;

	new_uV = max8973_dcdc_list_voltage(rdev, new_selector);
	if (new_uV < 0)
		return new_uV;

	return DIV_ROUND_UP(abs(old_uV - new_uV), max->change_uv_per_us);
}
static int max8973_dcdc_enable(struct regulator_dev *rdev)
{
	struct max8973_chip *max = rdev_get_drvdata(rdev);
	int ret;

	if (max->enable_external_control)
		return 0;

	ret = regmap_update_bits(max->regmap, MAX8973_VOUT,
			MAX8973_VOUT_ENABLE, MAX8973_VOUT_ENABLE);
	if (ret < 0)
		dev_err(max->dev, "%s(): register %d update failed with err %d",
			 __func__, MAX8973_VOUT, ret);
	return ret;
}

static int max8973_dcdc_disable(struct regulator_dev *rdev)
{
	struct max8973_chip *max = rdev_get_drvdata(rdev);
	int ret = 0;

	if (max->enable_external_control)
		return 0;

	ret = regmap_update_bits(max->regmap, MAX8973_VOUT,
					MAX8973_VOUT_ENABLE, 0);
	if (ret < 0)
		dev_err(max->dev, "%s(): register %d update failed with err %d",
			 __func__, MAX8973_VOUT, ret);
	return ret;
}

static int max8973_dcdc_is_enabled(struct regulator_dev *rdev)
{
	struct max8973_chip *max = rdev_get_drvdata(rdev);
	int ret;
	unsigned int data;

	if (max->enable_external_control)
		return 1;

	ret = regmap_read(max->regmap, MAX8973_VOUT, &data);
	if (ret < 0) {
		dev_err(max->dev, "%s(): register %d read failed with err %d",
			__func__, max->curr_vout_reg, ret);
		return ret;
	}

	return !!(data & MAX8973_VOUT_ENABLE);
}

static int max8973_dcdc_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct max8973_chip *max = rdev_get_drvdata(rdev);
	int ret;
	int pwm;

	/* Enable force PWM mode in FAST mode only. */
	switch (mode) {
	case REGULATOR_MODE_FAST:
		pwm = MAX8973_FPWM_EN_M;
		break;

	case REGULATOR_MODE_NORMAL:
		pwm = 0;
		break;

	default:
		return -EINVAL;
	}

	ret = regmap_update_bits(max->regmap, MAX8973_CONTROL1,
				MAX8973_FPWM_EN_M, pwm);
	if (ret < 0)
		dev_err(max->dev,
			"%s(): register %d update failed with err %d\n",
			__func__, MAX8973_CONTROL1, ret);
	return ret;
}

static unsigned int max8973_dcdc_get_mode(struct regulator_dev *rdev)
{
	struct max8973_chip *max = rdev_get_drvdata(rdev);
	unsigned int data;
	int ret;

	ret = regmap_read(max->regmap, MAX8973_CONTROL1, &data);
	if (ret < 0) {
		dev_err(max->dev, "%s(): register %d read failed with err %d\n",
			__func__, MAX8973_CONTROL1, ret);
		return ret;
	}
	return (data & MAX8973_FPWM_EN_M) ?
		REGULATOR_MODE_FAST : REGULATOR_MODE_NORMAL;
}

static struct regulator_ops max8973_dcdc_ops = {
	.get_voltage_sel	= max8973_dcdc_get_voltage_sel,
	.set_voltage		= max8973_dcdc_set_voltage,
	.list_voltage		= max8973_dcdc_list_voltage,
	.set_voltage_time_sel	= max8973_dcdc_set_voltage_time_sel,
	.enable			= max8973_dcdc_enable,
	.disable		= max8973_dcdc_disable,
	.is_enabled		= max8973_dcdc_is_enabled,
	.set_mode		= max8973_dcdc_set_mode,
	.get_mode		= max8973_dcdc_get_mode,
};

static int __devinit max8973_init_dcdc(struct max8973_chip *max,
		struct max8973_regulator_platform_data *pdata)
{
	int ret;
	uint8_t	control1 = 0;
	uint8_t control2 = 0;

	if (pdata->control_flags & MAX8973_CONTROL_REMOTE_SENSE_ENABLE)
		control1 |= MAX8973_SNS_ENABLE;

	if (!(pdata->control_flags & MAX8973_CONTROL_FALLING_SLEW_RATE_ENABLE))
		control1 |= MAX8973_NFSR_ENABLE;

	if (pdata->control_flags & MAX8973_CONTROL_OUTPUT_ACTIVE_DISCH_ENABLE)
		control1 |= MAX8973_AD_ENABLE;

	if (pdata->control_flags & MAX8973_CONTROL_BIAS_ENABLE)
		control1 |= MAX8973_BIAS_ENABLE;

	if (pdata->control_flags & MAX8973_CONTROL_FREQ_SHIFT_9PER_ENABLE)
		control1 |= MAX8973_FREQSHIFT_9PER;

	switch (pdata->control_flags & MAX8973_CONTROL_SLEW_RATE_200MV_PER_US) {
	case MAX8973_CONTROL_SLEW_RATE_12_5mV_PER_US:
		control1 = MAX8973_RAMP_12mV_PER_US;
		max->change_uv_per_us = 12500;
		break;

	case MAX8973_CONTROL_SLEW_RATE_25mV_PER_US:
		control1 = MAX8973_RAMP_25mV_PER_US;
		max->change_uv_per_us = 25000;
		break;

	case MAX8973_CONTROL_SLEW_RATE_50mV_PER_US:
		control1 = MAX8973_RAMP_50mV_PER_US;
		max->change_uv_per_us = 50000;
		break;

	case MAX8973_CONTROL_SLEW_RATE_200MV_PER_US:
		control1 = MAX8973_RAMP_200mV_PER_US;
		max->change_uv_per_us = 200000;
		break;
	}

	if (!(pdata->control_flags & MAX8973_CONTROL_PULL_DOWN_ENABLE))
		control2 |= MAX8973_DISCH_ENBABLE;

	switch (pdata->control_flags &
				MAX8973_CONTROL_CLKADV_TRIP_75mV_PER_US) {
	case MAX8973_CONTROL_CLKADV_TRIP_DISABLED:
		control2 |= MAX8973_CKKADV_TRIP_DISABLE;
		break;

	case MAX8973_CONTROL_CLKADV_TRIP_75mV_PER_US:
		control2 |= MAX8973_CKKADV_TRIP_75mV_PER_US;
		break;

	case MAX8973_CONTROL_CLKADV_TRIP_150mV_PER_US:
		control2 |= MAX8973_CKKADV_TRIP_150mV_PER_US;
		break;

	case MAX8973_CONTROL_CLKADV_TRIP_75mV_PER_US_HIST_DIS:
		control2 |= MAX8973_CKKADV_TRIP_75mV_PER_US_HIST_DIS;
		break;
	}

	switch (pdata->control_flags &
			MAX8973_CONTROL_INDUCTOR_VALUE_PLUS_60_PER) {
	case MAX8973_CONTROL_INDUCTOR_VALUE_NOMINAL:
		control2 |= MAX8973_INDUCTOR_NOMINAL;
		break;

	case MAX8973_CONTROL_INDUCTOR_VALUE_MINUS_30_PER:
		control2 |= MAX8973_INDUCTOR_MIN_30_PER;
		break;

	case MAX8973_CONTROL_INDUCTOR_VALUE_PLUS_30_PER:
		control2 |= MAX8973_INDUCTOR_PLUS_30_PER;
		break;

	case MAX8973_CONTROL_INDUCTOR_VALUE_PLUS_60_PER:
		control2 |= MAX8973_INDUCTOR_PLUS_60_PER;
		break;
	}

	ret = regmap_write(max->regmap, MAX8973_CONTROL1, control1);
	if (ret < 0) {
		dev_err(max->dev, "%s(): register %d write failed with err %d",
			__func__, MAX8973_CONTROL1, ret);
		return ret;
	}

	ret = regmap_write(max->regmap, MAX8973_CONTROL2, control2);
	if (ret < 0) {
		dev_err(max->dev, "%s(): register %d write failed with err %d",
			__func__, MAX8973_CONTROL2, ret);
		return ret;
	}

	/* If external control is enabled then disable EN bit */
	if (max->enable_external_control) {
		ret = regmap_update_bits(max->regmap, MAX8973_VOUT,
						MAX8973_VOUT_ENABLE, 0);
		if (ret < 0)
			dev_err(max->dev, "%s(): register %d update failed with err %d",
			__func__, MAX8973_VOUT, ret);
	}
	return ret;
}

static const struct regmap_config max8973_regmap_config = {
	.reg_bits		= 8,
	.val_bits		= 8,
	.max_register		= MAX8973_CHIPID2,
	.cache_type		= REGCACHE_RBTREE,
};

static int __devinit max8973_probe(struct i2c_client *client,
				     const struct i2c_device_id *id)
{
	struct max8973_regulator_platform_data *pdata;
	struct regulator_dev *rdev;
	struct max8973_chip *max;
	int ret;

	pdata = client->dev.platform_data;
	if (!pdata) {
		dev_err(&client->dev, "%s(): No Platform data", __func__);
		return -EIO;
	}

	max = devm_kzalloc(&client->dev, sizeof(*max), GFP_KERNEL);
	if (!max) {
		dev_err(&client->dev, "%s(): Memory allocation failed\n",
						__func__);
		return -ENOMEM;
	}

	max->dev = &client->dev;

	max->desc.name = id->name;
	max->desc.id = 0;
	max->desc.ops = &max8973_dcdc_ops;
	max->desc.type = REGULATOR_VOLTAGE;
	max->desc.owner = THIS_MODULE;
	max->regmap = devm_regmap_init_i2c(client, &max8973_regmap_config);
	if (IS_ERR(max->regmap)) {
		ret = PTR_ERR(max->regmap);
		dev_err(&client->dev,
			"%s(): regmap allocation failed with err %d\n",
			__func__, ret);
		return ret;
	}
	i2c_set_clientdata(client, max);

	max->enable_external_control = pdata->enable_ext_control;
	max->dvs_gpio = pdata->dvs_gpio;
	max->curr_gpio_val = pdata->dvs_def_state;
	max->curr_vout_reg = MAX8973_VOUT + pdata->dvs_def_state;
	max->lru_index[0] = max->curr_vout_reg;
	max->valid_dvs_gpio = false;

	if (gpio_is_valid(max->dvs_gpio)) {
		int gpio_flags;
		int i;

		gpio_flags = (pdata->dvs_def_state) ?
				GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
		ret = gpio_request_one(max->dvs_gpio,
				gpio_flags, "max8973-dvs");
		if (ret) {
			dev_err(&client->dev,
				"%s(): Could not obtain dvs GPIO %d: %d\n",
				__func__, max->dvs_gpio, ret);
			return ret;
		}
		max->valid_dvs_gpio = true;

		/*
		 * Initialize the lru index with vout_reg id
		 * The index 0 will be most recently used and
		 * set with the max->curr_vout_reg */
		for (i = 0; i < MAX8973_MAX_VOUT_REG; ++i)
			max->lru_index[i] = i;
		max->lru_index[0] = max->curr_vout_reg;
		max->lru_index[max->curr_vout_reg] = 0;
	}

	ret = max8973_init_dcdc(max, pdata);
	if (ret < 0) {
		dev_err(max->dev, "%s(): Init failed with err = %d\n",
				__func__, ret);
		goto err_init;
	}

	/* Register the regulators */
	rdev = regulator_register(&max->desc, &client->dev,
			pdata->reg_init_data, max, NULL);
	if (IS_ERR(rdev)) {
		dev_err(max->dev,
			"%s(): regulator register failed with err %s\n",
			__func__, id->name);
		ret = PTR_ERR(rdev);
		goto err_init;
	}

	max->rdev = rdev;
	return 0;

err_init:
	if (gpio_is_valid(max->dvs_gpio))
		gpio_free(max->dvs_gpio);
	return ret;
}

/**
 * max8973_remove - max8973 driver i2c remove handler
 * @client: i2c driver client device structure
 *
 * Unregister TPS driver as an i2c client device driver
 */
static int __devexit max8973_remove(struct i2c_client *client)
{
	struct max8973_chip *max = i2c_get_clientdata(client);

	if (gpio_is_valid(max->dvs_gpio))
		gpio_free(max->dvs_gpio);

	regulator_unregister(max->rdev);
	return 0;
}

static const struct i2c_device_id max8973_id[] = {
	{.name = "max8973",},
	{},
};

MODULE_DEVICE_TABLE(i2c, max8973_id);

static struct i2c_driver max8973_i2c_driver = {
	.driver = {
		.name = "max8973",
		.owner = THIS_MODULE,
	},
	.probe = max8973_probe,
	.remove = __devexit_p(max8973_remove),
	.id_table = max8973_id,
};

static int __init max8973_init(void)
{
	return i2c_add_driver(&max8973_i2c_driver);
}
subsys_initcall(max8973_init);

static void __exit max8973_cleanup(void)
{
	i2c_del_driver(&max8973_i2c_driver);
}
module_exit(max8973_cleanup);

MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_DESCRIPTION("MAX8973 voltage regulator driver");
MODULE_LICENSE("GPL v2");
