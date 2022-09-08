// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 Mediatek Inc.

#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

#define fan53870_PRODUCT_ID	0x00
#define fan53870_SILICON_REV	0x01
#define fan53870_IOUT		0x02
#define fan53870_LDO_EN		0x03
#define fan53870_LDO1_VOUT	0x04
#define fan53870_LDO2_VOUT	0x05
#define fan53870_LDO3_VOUT	0x06
#define fan53870_LDO4_VOUT	0x07
#define fan53870_LDO5_VOUT	0x08
#define fan53870_LDO6_VOUT	0x09
#define fan53870_LDO7_VOUT	0x0A
#define fan53870_LDO1_LDO2_SEQ	0x0b
#define fan53870_LDO3_LDO4_SEQ	0x0c
#define fan53870_LDO5_LDO6_SEQ	0x0d
#define fan53870_LDO7_SEQ		0x0e
#define fan53870_SEQ_STATUS		0x0f
#define fan53870_DISCHARGE_ENABLE	0x10


/* fan53870_LDO1_VSEL ~ fan53870_LDO7_VSEL =
 * 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A
 */
#define  fan53870_LDO1_VSEL                      fan53870_LDO1_VOUT
#define  fan53870_LDO2_VSEL                      fan53870_LDO2_VOUT
#define  fan53870_LDO3_VSEL                      fan53870_LDO3_VOUT
#define  fan53870_LDO4_VSEL                      fan53870_LDO4_VOUT
#define  fan53870_LDO5_VSEL                      fan53870_LDO5_VOUT
#define  fan53870_LDO6_VSEL                      fan53870_LDO6_VOUT
#define  fan53870_LDO7_VSEL                      fan53870_LDO7_VOUT

#define  fan53870_VSEL_MASK                      (0xff << 0)

enum fan53870_regulators {
	fan53870_REGULATOR_LDO1 = 0,
	fan53870_REGULATOR_LDO2,
	fan53870_REGULATOR_LDO3,
	fan53870_REGULATOR_LDO4,
	fan53870_REGULATOR_LDO5,
	fan53870_REGULATOR_LDO6,
	fan53870_REGULATOR_LDO7,
	fan53870_MAX_REGULATORS,
};

struct fan53870 {
	struct device *dev;
	struct regmap *regmap;
	struct regulator_desc *rdesc[fan53870_MAX_REGULATORS];
	struct regulator_dev *rdev[fan53870_MAX_REGULATORS];
};

struct fan53870_evt_sta {
	unsigned int sreg;
};

static const struct fan53870_evt_sta fan53870_status_reg = { fan53870_LDO_EN };

static const struct regmap_range fan53870_writeable_ranges[] = {
	regmap_reg_range(fan53870_IOUT, fan53870_DISCHARGE_ENABLE),
};

static const struct regmap_range fan53870_readable_ranges[] = {
	regmap_reg_range(fan53870_PRODUCT_ID, fan53870_DISCHARGE_ENABLE),
};

static const struct regmap_range fan53870_volatile_ranges[] = {
	regmap_reg_range(fan53870_IOUT, fan53870_DISCHARGE_ENABLE),
};

static const struct regmap_access_table fan53870_writeable_table = {
	.yes_ranges	= fan53870_writeable_ranges,
	.n_yes_ranges	= ARRAY_SIZE(fan53870_writeable_ranges),
};

static const struct regmap_access_table fan53870_readable_table = {
	.yes_ranges	= fan53870_readable_ranges,
	.n_yes_ranges	= ARRAY_SIZE(fan53870_readable_ranges),
};

static const struct regmap_access_table fan53870_volatile_table = {
	.yes_ranges	= fan53870_volatile_ranges,
	.n_yes_ranges	= ARRAY_SIZE(fan53870_volatile_ranges),
};

static const struct regmap_config fan53870_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x10,
	.wr_table = &fan53870_writeable_table,
	.rd_table = &fan53870_readable_table,
	.volatile_table = &fan53870_volatile_table,
};

static int fan53870_get_status(struct regulator_dev *rdev)
{
	struct fan53870 *chip = rdev_get_drvdata(rdev);
	int ret, id = rdev_get_id(rdev);
	unsigned int status = 0;

	ret = regulator_is_enabled_regmap(rdev);
	if (ret < 0) {
		dev_info(chip->dev, "Failed to read enable register(%d)\n",
			ret);
		return ret;
	}

	if (!ret)
		return REGULATOR_STATUS_OFF;

	ret = regmap_read(chip->regmap, fan53870_status_reg.sreg, &status);
	if (ret < 0) {
		dev_info(chip->dev, "Failed to read status register(%d)\n",
			ret);
		return ret;
	}

	if (status & (0x01ul << id))
		return REGULATOR_STATUS_ON;
	else
		return REGULATOR_STATUS_OFF;
}

static const struct regulator_ops fan53870_regl_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_status = fan53870_get_status,
};

static int fan53870_of_parse_cb(struct device_node *np,
				const struct regulator_desc *desc,
				struct regulator_config *config)
{
	return 0;
}

static const struct linear_range fan53870_range1[] = {
	REGULATOR_LINEAR_RANGE(800000, 0x63, 0xbb, 8000),
};

static const struct linear_range fan53870_range2[] = {
	REGULATOR_LINEAR_RANGE(1500000, 0x10, 0xff, 8000),
};

#define fan53870_LDO1_VOTAGES		(0xbc)
#define fan53870_LDO2_VOTAGES		(0xbc)
#define fan53870_LDO3_VOTAGES		(0x100)
#define fan53870_LDO4_VOTAGES		(0x100)
#define fan53870_LDO5_VOTAGES		(0x100)
#define fan53870_LDO6_VOTAGES		(0x100)
#define fan53870_LDO7_VOTAGES		(0x100)

#define fan53870_REGL_DESC(_id, _name, _linear_ranges, _min_sel, _min, _step)       \
	[fan53870_REGULATOR_##_id] = {                             \
		.name = #_name,                                    \
		.id = fan53870_REGULATOR_##_id,                    \
		.of_match = of_match_ptr(#_name),                  \
		.of_parse_cb = fan53870_of_parse_cb,               \
		.ops = &fan53870_regl_ops,                         \
		.regulators_node = of_match_ptr("regulators"),     \
		.linear_ranges = _linear_ranges,                   \
		.n_voltages = fan53870_##_id##_VOTAGES,            \
		.n_linear_ranges = ARRAY_SIZE(_linear_ranges),     \
		.min_uV = _min,                                    \
		.uV_step = _step,                              \
		.linear_min_sel = _min_sel,  \
		.vsel_mask = fan53870_VSEL_MASK,                   \
		.vsel_reg = fan53870_##_id##_VSEL,                 \
		.enable_reg = fan53870_LDO_EN,                     \
		.enable_mask = BIT(fan53870_REGULATOR_##_id),      \
		.type = REGULATOR_VOLTAGE,                         \
		.owner = THIS_MODULE,                              \
	}

static struct regulator_desc fan53870_regls_desc[fan53870_MAX_REGULATORS] = {
	fan53870_REGL_DESC(LDO1, fan53870_ldo1, fan53870_range1, 0x63, 800000, 8000),
	fan53870_REGL_DESC(LDO2, fan53870_ldo2, fan53870_range1, 0x63, 800000, 8000),
	fan53870_REGL_DESC(LDO3, fan53870_ldo3, fan53870_range2, 0x10, 1500000, 8000),
	fan53870_REGL_DESC(LDO4, fan53870_ldo4, fan53870_range2, 0x10, 1500000, 8000),
	fan53870_REGL_DESC(LDO5, fan53870_ldo5, fan53870_range2, 0x10, 1500000, 8000),
	fan53870_REGL_DESC(LDO6, fan53870_ldo6, fan53870_range2, 0x10, 1500000, 8000),
	fan53870_REGL_DESC(LDO7, fan53870_ldo7, fan53870_range2, 0x10, 1500000, 8000),
};

static int fan53870_regulator_init(struct fan53870 *chip)
{
	struct regulator_config config = { };
	struct regulator_desc *rdesc;
	int id, ret = 0;

	for (id = 0; id < fan53870_MAX_REGULATORS; id++) {
		chip->rdesc[id] = &fan53870_regls_desc[id];
		rdesc = chip->rdesc[id];
		config.regmap = chip->regmap;
		config.dev = chip->dev;
		config.driver_data = chip;

		chip->rdev[id] = devm_regulator_register(chip->dev, rdesc,
							 &config);
		if (IS_ERR(chip->rdev[id])) {
			ret = PTR_ERR(chip->rdev[id]);
			dev_info(chip->dev,
				"Failed to register regulator(%s):%d\n",
					chip->rdesc[id]->name, ret);
			return ret;
		}
	}

	return 0;
}

static int fan53870_i2c_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct fan53870 *chip;
	int error, ret, i;

	const unsigned int initial_register[6][2] = {
		{fan53870_DISCHARGE_ENABLE, 0x7F},
		{fan53870_LDO1_LDO2_SEQ, 0x00},
		{fan53870_LDO3_LDO4_SEQ, 0x00},
		{fan53870_LDO5_LDO6_SEQ, 0x00},
		{fan53870_LDO7_SEQ, 0x00},
		{fan53870_SEQ_STATUS, 0x00},
	};
	chip = devm_kzalloc(dev, sizeof(struct fan53870), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	dev_info(chip->dev, "%s Enter...\n", __func__);

	i2c_set_clientdata(client, chip);
	chip->dev = dev;
	chip->regmap = devm_regmap_init_i2c(client, &fan53870_regmap_config);
	if (IS_ERR(chip->regmap)) {
		error = PTR_ERR(chip->regmap);
		dev_info(dev, "Failed to allocate register map: %d\n",
			error);
		return error;
	}

	for (i = 0; i < 6; i++) {
		ret = regmap_write(chip->regmap, initial_register[i][0], initial_register[i][1]);
		if (ret < 0)
			dev_info(chip->dev, "Failed to write register\n");
	}

	ret = fan53870_regulator_init(chip);
	if (ret < 0) {
		dev_info(chip->dev, "Failed to init regulator(%d)\n", ret);
		return ret;
	}

	dev_info(chip->dev, "%s Exit...\n", __func__);
	return ret;
}

static int fan53870_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id fan53870_i2c_id[] = {
	{"fan53870", 0},
	{},
};


MODULE_DEVICE_TABLE(i2c, fan53870_i2c_id);

static struct i2c_driver fan53870_regulator_driver = {
	.driver = {
		.name = "fan53870",
	},
	.probe = fan53870_i2c_probe,
	.remove = fan53870_i2c_remove,
	.id_table = fan53870_i2c_id,
};

module_i2c_driver(fan53870_regulator_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Lu Tang <Lu.Tang@mediatek.com>");
MODULE_DESCRIPTION("Regulator driver for fan53870");
