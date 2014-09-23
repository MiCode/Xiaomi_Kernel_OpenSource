/*
 * FAN53555 Fairchild Digitally Programmable TinyBuck Regulator Driver.
 *
 * Supported Part Numbers:
 * FAN53555UC00X/01X/03X/04X/05X
 *
 * Copyright (c) 2012 Marvell Technology Ltd.
 * Yunfan Zhang <yfzhang@marvell.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/param.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/regulator/fan53555.h>

/* Voltage setting */
#define FAN53555_VSEL0		0x00
#define FAN53555_VSEL1		0x01
/* Control register */
#define FAN53555_CONTROL	0x02
/* IC Type */
#define FAN53555_ID1		0x03
/* IC mask version */
#define FAN53555_ID2		0x04
/* Monitor register */
#define FAN53555_MONITOR	0x05

/* VSEL bit definitions */
#define VSEL_BUCK_EN	(1 << 7)
#define VSEL_MODE		(1 << 6)
#define VSEL_NSEL_MASK	0x3F
#define VSEL_FULL_MASK	0xFF
/* Chip ID and Verison */
#define DIE_ID		0x0F	/* ID1 */
#define DIE_REV		0x0F	/* ID2 */
#define DIE_13_REV	0x0F	/* DIE Revsion ID of 13 option */
#define DIE_23_REV	0x0C	/* DIE Revision ID of 23 option */

/* Control bit definitions */
#define CTL_OUTPUT_DISCHG	(1 << 7)
#define CTL_SLEW_MASK		(0x7 << 4)
#define CTL_SLEW_SHIFT		4
#define CTL_RESET			(1 << 2)

#define FAN53555_NVOLTAGES	64	/* Numbers of voltages */

/* IC Type */
enum {
	FAN53555_CHIP_ID_00 = 0,
	FAN53555_CHIP_ID_01,
	FAN53555_CHIP_ID_02,
	FAN53555_CHIP_ID_03,
	FAN53555_CHIP_ID_04,
	FAN53555_CHIP_ID_05,
};

static const int slew_rate_plan[] = {
	64000,
	32000,
	16000,
	8000,
	4000,
	2000,
	1000,
	500
};

struct fan53555_device_info {
	struct regmap *regmap;
	struct device *dev;
	struct regulator_desc desc;
	struct regulator_dev *rdev;
	struct regulator_init_data *regulator;
	/* IC Type and Rev */
	int chip_id;
	int chip_rev;
	/* Voltage setting register */
	unsigned int vol_reg;
	unsigned int sleep_reg;
	/* Voltage range and step(linear) */
	unsigned int vsel_min;
	unsigned int vsel_step;
	/* Voltage slew rate limiting */
	unsigned int slew_rate;
	/* Sleep voltage cache */
	unsigned int sleep_vol_cache;
	unsigned int peek_poke_address;

	struct dentry *debug_root;

	bool disable_suspend;
};

static int delay_array[] = {10, 20, 30, 40, 50};

static int fan53555_read(struct fan53555_device_info *di, unsigned int reg,
						unsigned int *val)
{
	int i = 0, rc = 0;

	rc = regmap_read(di->regmap, reg, val);
	for (i = 0; rc && i < ARRAY_SIZE(delay_array); i++) {
		pr_debug("Failed reading reg=%u - retry(%d)\n", reg, i);
		msleep(delay_array[i]);
		rc = regmap_read(di->regmap, reg, val);
	}

	if (rc)
		pr_err("Failed reading reg=%u rc=%d\n", reg, rc);

	return rc;
}

static int fan53555_write(struct fan53555_device_info *di, unsigned int reg,
						unsigned int val)
{
	int i = 0, rc = 0;

	rc = regmap_write(di->regmap, reg, val);
	for (i = 0; rc && i < ARRAY_SIZE(delay_array); i++) {
		pr_debug("Failed writing reg=%u - retry(%d)\n", reg, i);
		msleep(delay_array[i]);
		rc = regmap_write(di->regmap, reg, val);
	}

	if (rc)
		pr_err("Failed writing reg=%u rc=%d\n", reg, rc);

	return rc;
}

static int fan53555_update_bits(struct fan53555_device_info *di,
			unsigned int reg, unsigned int mask, unsigned int val)
{
	int i = 0, rc = 0;

	rc = regmap_update_bits(di->regmap, reg, mask, val);
	for (i = 0; rc && i < ARRAY_SIZE(delay_array); i++) {
		pr_debug("Failed updating reg=%u - retry(%d)\n", reg, i);
		msleep(delay_array[i]);
		rc = regmap_update_bits(di->regmap, reg, mask, val);
	}

	if (rc)
		pr_err("Failed updating reg=%u rc=%d\n", reg, rc);

	return rc;
}

static int fan53555_set_suspend_voltage(struct regulator_dev *rdev, int uV)
{
	struct fan53555_device_info *di = rdev_get_drvdata(rdev);
	int ret;

	if (di->sleep_vol_cache == uV)
		return 0;
	ret = regulator_map_voltage_linear(rdev, uV, uV);
	if (ret < 0)
		return -EINVAL;
	ret = fan53555_update_bits(di, di->sleep_reg,
					VSEL_NSEL_MASK, ret);
	if (ret < 0)
		return -EINVAL;
	/* Cache the sleep voltage setting.
	 * Might not be the real voltage which is rounded */
	di->sleep_vol_cache = uV;

	return 0;
}

static int fan53555_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct fan53555_device_info *di = rdev_get_drvdata(rdev);

	switch (mode) {
	case REGULATOR_MODE_FAST:
		fan53555_update_bits(di, di->vol_reg,
				VSEL_MODE, VSEL_MODE);
		break;
	case REGULATOR_MODE_NORMAL:
		fan53555_update_bits(di, di->vol_reg, VSEL_MODE, 0);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static unsigned int fan53555_get_mode(struct regulator_dev *rdev)
{
	struct fan53555_device_info *di = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret = 0;

	ret = fan53555_read(di, di->vol_reg, &val);
	if (ret < 0)
		return ret;
	if (val & VSEL_MODE)
		return REGULATOR_MODE_FAST;
	else
		return REGULATOR_MODE_NORMAL;
}

static struct regulator_ops fan53555_regulator_ops = {
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.map_voltage = regulator_map_voltage_linear,
	.list_voltage = regulator_list_voltage_linear,
	.set_suspend_voltage = fan53555_set_suspend_voltage,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = fan53555_set_mode,
	.get_mode = fan53555_get_mode,
};

static struct regulator_ops fan53555_regulator_disable_suspend_ops = {
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.map_voltage = regulator_map_voltage_linear,
	.list_voltage = regulator_list_voltage_linear,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = fan53555_set_mode,
	.get_mode = fan53555_get_mode,
};

/*
 * The formula for calculating the actual slew rate is:
 * actual_slew_rate = 10mv_based_slew_rate * scaling_step_size_mv / 10mV
 */
#define FAN53555_DVS_DEFAULT_STEP_SIZE_UV	10000
static u32 fan53555_get_slew_rate_reg_value(struct fan53555_device_info *di,
							u32 slew_rate)
{
	u32 index;
	int scaled_slew_rate = slew_rate * FAN53555_DVS_DEFAULT_STEP_SIZE_UV /
						di->vsel_step;

	for (index = 0; index < ARRAY_SIZE(slew_rate_plan); index++)
		if (scaled_slew_rate >= slew_rate_plan[index])
			break;

	if (index == ARRAY_SIZE(slew_rate_plan)) {
		dev_err(di->dev, "invalid slew rate.\n");
		index = FAN53555_SLEW_RATE_8MV;
	}

	return index;
}

/* For 00,01,03,05 options:
 * VOUT = 0.60V + NSELx * 10mV, from 0.60 to 1.23V.
 * For 04 option:
 * VOUT = 0.603V + NSELx * 12.826mV, from 0.603 to 1.411V.
 * For 13 option:
 * 13 option, its DIE ID is 0x00 and DIE_REV is 0x0F.
 * VOUT = 0.80V + NSELx * 10mV, from 0.80 to 1.43V.
 * For 23 option:
 * 23 option, its DIE ID is 0x00 and DIE_REV is 0x0C.
 * VOUT = 0.60V + NSELx * 12.5mV, from 0.60 to 1.3875V.
 * */
static int fan53555_device_setup(struct fan53555_device_info *di,
				struct fan53555_platform_data *pdata)
{
	unsigned int reg, data, mask;

	/* Setup voltage control register */
	switch (pdata->sleep_vsel_id) {
	case FAN53555_VSEL_ID_0:
		di->sleep_reg = FAN53555_VSEL0;
		di->vol_reg = FAN53555_VSEL1;
		break;
	case FAN53555_VSEL_ID_1:
		di->sleep_reg = FAN53555_VSEL1;
		di->vol_reg = FAN53555_VSEL0;
		break;
	default:
		dev_err(di->dev, "Invalid VSEL ID!\n");
		return -EINVAL;
	}
	/* Init voltage range and step */
	switch (di->chip_id) {
	case FAN53555_CHIP_ID_00:
		if (di->chip_rev == DIE_13_REV) {
			di->vsel_min = 800000;
			di->vsel_step = 10000;
			break;
		} else if (di->chip_rev == DIE_23_REV) {
			di->vsel_min = 600000;
			di->vsel_step = 12500;
			break;
		}
	case FAN53555_CHIP_ID_01:
	case FAN53555_CHIP_ID_03:
	case FAN53555_CHIP_ID_05:
		di->vsel_min = 600000;
		di->vsel_step = 10000;
		break;
	case FAN53555_CHIP_ID_04:
		di->vsel_min = 603000;
		di->vsel_step = 12826;
		break;
	default:
		dev_err(di->dev,
			"Chip ID[%d]\n not supported!\n", di->chip_id);
		return -EINVAL;
	}
	/* Init slew rate */
	if (di->dev->of_node)
		pdata->slew_rate = fan53555_get_slew_rate_reg_value(di,
			pdata->regulator->constraints.ramp_delay);
	else if (pdata->slew_rate & 0x7)
		di->slew_rate = pdata->slew_rate;
	else
		di->slew_rate = FAN53555_SLEW_RATE_64MV;

	reg = FAN53555_CONTROL;
	data = di->slew_rate << CTL_SLEW_SHIFT;
	mask = CTL_SLEW_MASK;
	return fan53555_update_bits(di, reg, mask, data);
}

static int fan53555_regulator_register(struct fan53555_device_info *di,
			struct regulator_config *config)
{
	struct regulator_desc *rdesc = &di->desc;

	rdesc->name = "fan53555-reg";
	if (di->disable_suspend)
		rdesc->ops = &fan53555_regulator_disable_suspend_ops;
	else
		rdesc->ops = &fan53555_regulator_ops;
	rdesc->type = REGULATOR_VOLTAGE;
	rdesc->n_voltages = FAN53555_NVOLTAGES;
	rdesc->enable_reg = di->vol_reg;
	rdesc->enable_mask = VSEL_BUCK_EN;
	rdesc->min_uV = di->vsel_min;
	rdesc->uV_step = di->vsel_step;
	rdesc->vsel_reg = di->vol_reg;
	rdesc->vsel_mask = VSEL_NSEL_MASK;
	rdesc->owner = THIS_MODULE;

	di->rdev = regulator_register(&di->desc, config);
	return PTR_RET(di->rdev);

}

static struct regmap_config fan53555_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int fan53555_parse_backup_reg(struct i2c_client *client, u32 *sleep_sel)
{
	int rc = -EINVAL;

	rc = of_property_read_u32(client->dev.of_node, "fairchild,backup-vsel",
				sleep_sel);
	if (rc) {
		dev_err(&client->dev, "fairchild,backup-vsel property missing\n");
	} else {
		switch (*sleep_sel) {
		case FAN53555_VSEL_ID_0:
		case FAN53555_VSEL_ID_1:
			break;
		default:
			dev_err(&client->dev, "Invalid VSEL ID!\n");
			rc = -EINVAL;
		}
	}

	return rc;
}


static struct fan53555_platform_data *
	fan53555_get_of_platform_data(struct i2c_client *client)
{
	struct fan53555_platform_data *pdata = NULL;
	struct regulator_init_data *init_data;
	u32 sleep_sel;

	init_data = of_get_regulator_init_data(&client->dev,
			client->dev.of_node);
	if (!init_data) {
		dev_err(&client->dev, "regulator init data is missing\n");
		return pdata;
	}

	if (fan53555_parse_backup_reg(client, &sleep_sel))
		return pdata;

	pdata = devm_kzalloc(&client->dev,
			sizeof(struct fan53555_platform_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev,
			"fan53555_platform_data allocation failed.\n");
		return pdata;
	}

	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask |=
		REGULATOR_CHANGE_STATUS	| REGULATOR_CHANGE_VOLTAGE |
		REGULATOR_CHANGE_MODE;
	init_data->constraints.valid_modes_mask =
				REGULATOR_MODE_NORMAL |
				REGULATOR_MODE_FAST;
	init_data->constraints.initial_mode = REGULATOR_MODE_NORMAL;

	pdata->regulator = init_data;
	pdata->sleep_vsel_id = sleep_sel;

	return pdata;
}

static int fan53555_restore_working_reg(struct device_node *node,
			struct fan53555_device_info *di)
{
	int ret;
	u32 val;

	/* Restore register from back up register */
	ret = fan53555_read(di, di->sleep_reg, &val);
	if (ret < 0) {
		dev_err(di->dev,
			"Failed to get backup data from reg %d, ret = %d\n",
			di->sleep_reg, ret);
		return ret;
	}

	ret = fan53555_update_bits(di,
		di->vol_reg, VSEL_FULL_MASK, val);
	if (ret < 0) {
		dev_err(di->dev,
			"Failed to update working reg %d, ret = %d\n",
			di->vol_reg, ret);
		return ret;
	}

	return ret;
}

static int fan53555_of_init(struct device_node *node,
			struct fan53555_device_info *di)
{
	int ret, gpio;
	enum of_gpio_flags flags;

	if (of_property_read_bool(node, "fairchild,restore-reg")) {
		ret = fan53555_restore_working_reg(node, di);
		if (ret)
			return ret;
	}

	if (of_find_property(node, "fairchild,vsel-gpio", NULL)) {
		gpio = of_get_named_gpio_flags(node, "fairchild,vsel-gpio", 0,
						&flags);

		if (!gpio_is_valid(gpio)) {
			if (gpio != -EPROBE_DEFER)
				dev_err(di->dev, "Could not get vsel, ret = %d\n",
					gpio);
			return gpio;
		}

		ret = devm_gpio_request(di->dev, gpio, "fan53555_vsel");
		if (ret) {
			dev_err(di->dev, "Failed to obtain gpio %d ret = %d\n",
				gpio, ret);
			return ret;
		}

		ret = gpio_direction_output(gpio, flags & OF_GPIO_ACTIVE_LOW ?
							0 : 1);
		if (ret) {
			dev_err(di->dev,
				"Failed to set GPIO %d to: %s, ret = %d",
				gpio, flags & OF_GPIO_ACTIVE_LOW ?
				"GPIO_LOW" : "GPIO_HIGH", ret);
			return ret;
		}
	}

	di->disable_suspend = of_property_read_bool(node,
				"fairchild,disable-suspend");

	return 0;
}

static int get_reg(void *data, u64 *val)
{
	struct fan53555_device_info *di = data;
	int rc;
	unsigned int temp = 0;

	rc = fan53555_read(di, di->peek_poke_address, &temp);
	if (rc < 0)
		dev_err(di->dev, "Couldn't read reg %x rc = %d\n",
				di->peek_poke_address, rc);
	else
		*val = temp;

	return rc;
}

static int set_reg(void *data, u64 val)
{
	struct fan53555_device_info *di = data;
	int rc;
	unsigned int temp = 0;

	temp = (unsigned int) val;
	rc = fan53555_write(di, di->peek_poke_address, temp);
	if (rc < 0)
		dev_err(di->dev, "Couldn't write 0x%02x to 0x%02x rc= %d\n",
			di->peek_poke_address, temp, rc);

	return rc;
}
DEFINE_SIMPLE_ATTRIBUTE(poke_poke_debug_ops, get_reg, set_reg, "0x%02llx\n");

static int fan53555_regulator_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct fan53555_device_info *di;
	struct fan53555_platform_data *pdata;
	struct regulator_config config = { };
	unsigned int val;
	int ret;

	if (client->dev.of_node)
		pdata = fan53555_get_of_platform_data(client);
	else
		pdata = client->dev.platform_data;

	if (!pdata || !pdata->regulator) {
		dev_err(&client->dev, "Platform data not found!\n");
		return -ENODEV;
	}

	di = devm_kzalloc(&client->dev, sizeof(struct fan53555_device_info),
					GFP_KERNEL);
	if (!di) {
		dev_err(&client->dev, "Failed to allocate device info data!\n");
		return -ENOMEM;
	}
	di->regmap = devm_regmap_init_i2c(client, &fan53555_regmap_config);
	if (IS_ERR(di->regmap)) {
		dev_err(&client->dev, "Failed to allocate regmap!\n");
		return PTR_ERR(di->regmap);
	}
	di->dev = &client->dev;
	di->regulator = pdata->regulator;
	i2c_set_clientdata(client, di);
	/* Get chip ID */
	ret = fan53555_read(di, FAN53555_ID1, &val);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to get chip ID!\n");
		return -ENODEV;
	}
	di->chip_id = val & DIE_ID;
	/* Get chip revision */
	ret = fan53555_read(di, FAN53555_ID2, &val);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to get chip Rev!\n");
		return -ENODEV;
	}
	di->chip_rev = val & DIE_REV;
	dev_info(&client->dev, "FAN53555 Option[%d] Rev[%d] Detected!\n",
				di->chip_id, di->chip_rev);
	/* Device init */
	ret = fan53555_device_setup(di, pdata);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to setup device!\n");
		return ret;
	}

	/* Set up from device tree */
	if (client->dev.of_node) {
		ret = fan53555_of_init(client->dev.of_node, di);
		if (ret)
			return ret;
	}
	/* Register regulator */
	config.dev = di->dev;
	config.init_data = di->regulator;
	config.regmap = di->regmap;
	config.driver_data = di;
	config.of_node = client->dev.of_node;

	ret = fan53555_regulator_register(di, &config);
	if (ret < 0)
		dev_err(&client->dev, "Failed to register regulator!\n");

	di->debug_root = debugfs_create_dir("fan53555", NULL);
	if (!di->debug_root)
		dev_err(&client->dev, "Couldn't create debug dir\n");

	if (di->debug_root) {
		struct dentry *ent;

		ent = debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO,
					  di->debug_root,
					  &(di->peek_poke_address));
		if (!ent)
			dev_err(&client->dev, "Couldn't create address debug file rc = %d\n",
									ret);

		ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
					  di->debug_root, di,
					  &poke_poke_debug_ops);
		if (!ent)
			dev_err(&client->dev, "Couldn't create data debug file rc = %d\n",
									ret);
	}

	return ret;

}

static int fan53555_regulator_remove(struct i2c_client *client)
{
	struct fan53555_device_info *di = i2c_get_clientdata(client);

	regulator_unregister(di->rdev);

	debugfs_remove_recursive(di->debug_root);

	return 0;
}

static struct of_device_id fan53555_match_table[] = {
	{ .compatible = "fairchild,fan53555-regulator",},
	{},
};
MODULE_DEVICE_TABLE(of, fan53555_match_table);

static const struct i2c_device_id fan53555_id[] = {
	{"fan53555", -1},
	{ },
};

static struct i2c_driver fan53555_regulator_driver = {
	.driver = {
		.name = "fan53555-regulator",
		.owner = THIS_MODULE,
		.of_match_table = fan53555_match_table,
	},
	.probe = fan53555_regulator_probe,
	.remove = fan53555_regulator_remove,
	.id_table = fan53555_id,
};

/**
 * fan53555_regulator_init() - initialized fan53555 regulator driver
 * This function registers the fan53555 regulator platform driver.
 *
 * Returns 0 on success or errno on failure.
 */
int __init fan53555_regulator_init(void)
{
	static bool initialized;

	if (initialized)
		return 0;
	else
		initialized = true;

	return i2c_add_driver(&fan53555_regulator_driver);
}
EXPORT_SYMBOL(fan53555_regulator_init);
arch_initcall(fan53555_regulator_init);

static void __exit fan53555_regulator_exit(void)
{
	i2c_del_driver(&fan53555_regulator_driver);
}
module_exit(fan53555_regulator_exit);

MODULE_AUTHOR("Yunfan Zhang <yfzhang@marvell.com>");
MODULE_DESCRIPTION("FAN53555 regulator driver");
MODULE_LICENSE("GPL v2");
