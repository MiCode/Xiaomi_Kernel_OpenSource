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


#define WL2868C_CHIP_REV 0x01
#define WL2868C_DISCHARGE_RESISTORS 0x02
#define WL2868C_LDO1_VOUT 0x03
#define WL2868C_LDO2_VOUT 0x04
#define WL2868C_LDO3_VOUT 0x05
#define WL2868C_LDO4_VOUT 0x06
#define WL2868C_LDO5_VOUT 0x07
#define WL2868C_LDO6_VOUT 0x08
#define WL2868C_LDO7_VOUT 0x09
#define WL2868C_LDO1_LDO2_SEQ 0x0a
#define WL2868C_LDO3_LDO4_SEQ 0x0b
#define WL2868C_LDO5_LDO6_SEQ 0x0c
#define WL2868C_LDO7_SEQ 0x0d
#define WL2868C_LDO_EN 0x0e
#define WL2868C_SEQ_STATUS 0x0f


/* WL2868C_LDO1_VSEL ~ WL2868C_LDO7_VSEL =
 * 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09
 */
#define  WL2868C_LDO1_VSEL                      WL2868C_LDO1_VOUT
#define  WL2868C_LDO2_VSEL                      WL2868C_LDO2_VOUT
#define  WL2868C_LDO3_VSEL                      WL2868C_LDO3_VOUT
#define  WL2868C_LDO4_VSEL                      WL2868C_LDO4_VOUT
#define  WL2868C_LDO5_VSEL                      WL2868C_LDO5_VOUT
#define  WL2868C_LDO6_VSEL                      WL2868C_LDO6_VOUT
#define  WL2868C_LDO7_VSEL                      WL2868C_LDO7_VOUT

#define  WL2868C_VSEL_MASK                      (0xff << 0)

enum slg51000_regulators {
	WL2868C_REGULATOR_LDO1 = 0,
	WL2868C_REGULATOR_LDO2,
	WL2868C_REGULATOR_LDO3,
	WL2868C_REGULATOR_LDO4,
	WL2868C_REGULATOR_LDO5,
	WL2868C_REGULATOR_LDO6,
	WL2868C_REGULATOR_LDO7,
	WL2868C_MAX_REGULATORS,
};

struct wl2868c {
	struct device *dev;
	struct regmap *regmap;
	struct regulator_desc *rdesc[WL2868C_MAX_REGULATORS];
	struct regulator_dev *rdev[WL2868C_MAX_REGULATORS];
};

struct wl2868c_evt_sta {
	unsigned int sreg;
};

static const struct wl2868c_evt_sta wl2868c_status_reg = { WL2868C_LDO_EN };

static const struct regmap_range wl2868c_writeable_ranges[] = {
	regmap_reg_range(WL2868C_DISCHARGE_RESISTORS, WL2868C_SEQ_STATUS),
};

static const struct regmap_range wl2868c_readable_ranges[] = {
	regmap_reg_range(WL2868C_CHIP_REV, WL2868C_SEQ_STATUS),
};

static const struct regmap_range wl2868c_volatile_ranges[] = {
	regmap_reg_range(WL2868C_DISCHARGE_RESISTORS, WL2868C_SEQ_STATUS),
};

static const struct regmap_access_table wl2868c_writeable_table = {
	.yes_ranges	= wl2868c_writeable_ranges,
	.n_yes_ranges	= ARRAY_SIZE(wl2868c_writeable_ranges),
};

static const struct regmap_access_table wl2868c_readable_table = {
	.yes_ranges	= wl2868c_readable_ranges,
	.n_yes_ranges	= ARRAY_SIZE(wl2868c_readable_ranges),
};

static const struct regmap_access_table wl2868c_volatile_table = {
	.yes_ranges	= wl2868c_volatile_ranges,
	.n_yes_ranges	= ARRAY_SIZE(wl2868c_volatile_ranges),
};

static const struct regmap_config wl2868c_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x0F,
	.wr_table = &wl2868c_writeable_table,
	.rd_table = &wl2868c_readable_table,
	.volatile_table = &wl2868c_volatile_table,
};

static int wl2868c_get_status(struct regulator_dev *rdev)
{
	struct wl2868c *chip = rdev_get_drvdata(rdev);
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

	ret = regmap_read(chip->regmap, wl2868c_status_reg.sreg, &status);
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

static const struct regulator_ops wl2868c_regl_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_status = wl2868c_get_status,
};

static int wl2868c_of_parse_cb(struct device_node *np,
				const struct regulator_desc *desc,
				struct regulator_config *config)
{
	return 0;
}

#define WL2868C_REGL_DESC(_id, _name, _min, _step)       \
	[WL2868C_REGULATOR_##_id] = {                             \
		.name = #_name,                                    \
		.id = WL2868C_REGULATOR_##_id,                    \
		.of_match = of_match_ptr(#_name),                  \
		.of_parse_cb = wl2868c_of_parse_cb,               \
		.ops = &wl2868c_regl_ops,                         \
		.regulators_node = of_match_ptr("regulators"),     \
		.n_voltages = 256,                                 \
		.min_uV = _min,                                    \
		.uV_step = _step,                                  \
		.linear_min_sel = 0,                               \
		.vsel_mask = WL2868C_VSEL_MASK,                   \
		.vsel_reg = WL2868C_##_id##_VSEL,                 \
		.enable_reg = WL2868C_LDO_EN,       \
		.enable_mask = BIT(WL2868C_REGULATOR_##_id),      \
		.type = REGULATOR_VOLTAGE,                         \
		.owner = THIS_MODULE,                              \
	}

static struct regulator_desc wl2868c_regls_desc[WL2868C_MAX_REGULATORS] = {
	WL2868C_REGL_DESC(LDO1, wl2868c_ldo1, 496000, 8000),
	WL2868C_REGL_DESC(LDO2, wl2868c_ldo2, 496000, 8000),
	WL2868C_REGL_DESC(LDO3, wl2868c_ldo3, 1504000, 8000),
	WL2868C_REGL_DESC(LDO4, wl2868c_ldo4, 1504000, 8000),
	WL2868C_REGL_DESC(LDO5, wl2868c_ldo5, 1504000, 8000),
	WL2868C_REGL_DESC(LDO6, wl2868c_ldo6, 1504000, 8000),
	WL2868C_REGL_DESC(LDO7, wl2868c_ldo7, 1504000, 8000),
};

static int wl2868c_regulator_init(struct wl2868c *chip)
{
	struct regulator_config config = { };
	struct regulator_desc *rdesc;
	u8 vsel_range[1] = {0};
	int id, ret = 0;
	const unsigned int ldo_regs[WL2868C_MAX_REGULATORS] = {
		WL2868C_LDO1_VOUT,
		WL2868C_LDO2_VOUT,
		WL2868C_LDO3_VOUT,
		WL2868C_LDO4_VOUT,
		WL2868C_LDO5_VOUT,
		WL2868C_LDO6_VOUT,
		WL2868C_LDO7_VOUT,
	};

	const unsigned int initial_voltage[WL2868C_MAX_REGULATORS] = {
		0x00,//LDO1 1.2V main DVDD
		0x00,//LDO2 1.2V sub  DVDD2
		0x00,//LDO3 2.8V main AFVDD
		0x00,//LDO4 2.8V sub AVDD
		0x00,//LDO5 1.8V DOVDD
		0x00,//LDO6 2.8V main AVDD
		0xFF,//LDO7 3.3V p_sensor
	};

	/*Disable all ldo output by default*/
	ret = regmap_write(chip->regmap, WL2868C_LDO_EN, 0);
	if (ret < 0) {
		dev_info(chip->dev,
			"Disable all LDO output failed!!!\n");
		return ret;
	}

	for (id = 0; id < WL2868C_MAX_REGULATORS; id++) {
		chip->rdesc[id] = &wl2868c_regls_desc[id];
		rdesc = chip->rdesc[id];
		config.regmap = chip->regmap;
		config.dev = chip->dev;
		config.driver_data = chip;

		ret = regmap_bulk_read(chip->regmap, ldo_regs[id],
				       vsel_range, 1);

		if (ret < 0) {
			dev_info(chip->dev,
				"Failed to read the ldo register\n");
			return ret;
		}

		ret = regmap_write(chip->regmap, ldo_regs[id], initial_voltage[id]);
		if (ret < 0) {
			dev_info(chip->dev,
			"Failed to write init voltage register\n");
			return ret;
		}


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

	ret = regmap_write(chip->regmap, ldo_regs[WL2868C_MAX_REGULATORS - 1],
			initial_voltage[WL2868C_MAX_REGULATORS - 1]);
	if (ret < 0) {
		dev_info(chip->dev,
				"Failed to write init voltage register\n");
		return ret;
	}

	ret = regmap_bulk_read(chip->regmap,
				ldo_regs[WL2868C_MAX_REGULATORS - 1],
				       vsel_range, 1);
	if (ret < 0) {
		dev_info(chip->dev,
				"Failed to read register\n");
		return ret;
	}

	ret = regmap_write(chip->regmap, WL2868C_LDO_EN, 0x80);
	if (ret < 0) {
		dev_info(chip->dev,
				"Failed to write init voltage register\n");
		return ret;
	}
	ret = regmap_bulk_read(chip->regmap, WL2868C_LDO_EN,
				       vsel_range, 1);
	if (ret < 0) {
		dev_info(chip->dev,
				"Failed to read register\n");
		return ret;
	}

	if (vsel_range[0] == 0x80)
		pr_info("%s p sensor 3.3v power on success\n", __func__);

	return 0;
}

static int wl2868c_i2c_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct wl2868c *chip;
	int error, ret, i;

	const unsigned int initial_register[6][2] = {
		{WL2868C_DISCHARGE_RESISTORS, 0x00},
		{WL2868C_LDO1_LDO2_SEQ, 0x00},
		{WL2868C_LDO3_LDO4_SEQ, 0x00},
		{WL2868C_LDO5_LDO6_SEQ, 0x00},
		{WL2868C_LDO7_SEQ, 0x00},
		{WL2868C_SEQ_STATUS, 0x00},
	};
	chip = devm_kzalloc(dev, sizeof(struct wl2868c), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	dev_info(chip->dev, "wl2868c probe Enter...\n");

	i2c_set_clientdata(client, chip);
	chip->dev = dev;
	chip->regmap = devm_regmap_init_i2c(client, &wl2868c_regmap_config);
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

	ret = wl2868c_regulator_init(chip);
	if (ret < 0) {
		dev_info(chip->dev, "Failed to init regulator(%d)\n", ret);
		return ret;
	}

	dev_info(chip->dev, "wl2868c probe Exit...\n");
	return ret;
}

static int wl2868c_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id wl2868c_i2c_id[] = {
	{"wl2868c", 0},
	{},
};


MODULE_DEVICE_TABLE(i2c, wl2868c_i2c_id);

static struct i2c_driver wl2868c_regulator_driver = {
	.driver = {
		.name = "wl2868c",
	},
	.probe = wl2868c_i2c_probe,
	.remove = wl2868c_i2c_remove,
	.id_table = wl2868c_i2c_id,
};

module_i2c_driver(wl2868c_regulator_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Lu Tang <Lu.Tang@mediatek.com>");
MODULE_DESCRIPTION("Regulator driver for wl2868c");
