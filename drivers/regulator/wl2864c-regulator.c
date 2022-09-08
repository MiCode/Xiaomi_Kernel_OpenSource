// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2022 Mediatek Inc.

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

#define WL2864C_REG_NUM (WL2864C_SEQ_STATUS-WL2864C_CHIP_REV+1)

#define WL2864C_CHIP_REV 0x00
#define WL2864C_CURRENT_LIMITSEL 0x01
#define WL2864C_DISCHARGE_RESISTORS 0x02
#define WL2864C_LDO1_VOUT 0x03
#define WL2864C_LDO2_VOUT 0x04
#define WL2864C_LDO3_VOUT 0x05
#define WL2864C_LDO4_VOUT 0x06
#define WL2864C_LDO5_VOUT 0x07
#define WL2864C_LDO6_VOUT 0x08
#define WL2864C_LDO7_VOUT 0x09
#define WL2864C_LDO1_LDO2_SEQ 0x0a
#define WL2864C_LDO3_LDO4_SEQ 0x0b
#define WL2864C_LDO5_LDO6_SEQ 0x0c
#define WL2864C_LDO7_SEQ 0x0d
#define WL2864C_LDO_EN 0x0e
#define WL2864C_SEQ_STATUS 0x0f


/* WL2864C_LDO1_VSEL ~ WL2864C_LDO7_VSEL =
 * 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09
 */
#define  WL2864C_LDO1_VSEL                      WL2864C_LDO1_VOUT
#define  WL2864C_LDO2_VSEL                      WL2864C_LDO2_VOUT
#define  WL2864C_LDO3_VSEL                      WL2864C_LDO3_VOUT
#define  WL2864C_LDO4_VSEL                      WL2864C_LDO4_VOUT
#define  WL2864C_LDO5_VSEL                      WL2864C_LDO5_VOUT
#define  WL2864C_LDO6_VSEL                      WL2864C_LDO6_VOUT
#define  WL2864C_LDO7_VSEL                      WL2864C_LDO7_VOUT

#define  WL2864C_VSEL_SHIFT                     0
#define  WL2864C_VSEL_MASK                      (0xff << 0)

enum WL2864C_regulators {
	WL2864C_REGULATOR_LDO1 = 0,
	WL2864C_REGULATOR_LDO2,
	WL2864C_REGULATOR_LDO3,
	WL2864C_REGULATOR_LDO4,
	WL2864C_REGULATOR_LDO5,
	WL2864C_REGULATOR_LDO6,
	WL2864C_REGULATOR_LDO7,
	WL2864C_MAX_REGULATORS,
};

struct wl2864c {
	struct device *dev;
	struct regmap *regmap;
	struct regulator_desc *rdesc[WL2864C_MAX_REGULATORS];
	struct regulator_dev *rdev[WL2864C_MAX_REGULATORS];
	int chip_cs_pin;
	int vin1_enable_gpio;
};

struct wl2864c_evt_sta {
	unsigned int sreg;
};

static const struct wl2864c_evt_sta wl2864c_status_reg = { WL2864C_LDO_EN };

static const struct regmap_range wl2864c_writeable_ranges[] = {
	regmap_reg_range(WL2864C_CURRENT_LIMITSEL, WL2864C_SEQ_STATUS),
};

static const struct regmap_range wl2864c_readable_ranges[] = {
	regmap_reg_range(WL2864C_CHIP_REV, WL2864C_SEQ_STATUS),
};

static const struct regmap_range wl2864c_volatile_ranges[] = {
	regmap_reg_range(WL2864C_CURRENT_LIMITSEL, WL2864C_SEQ_STATUS),
};

static const struct regmap_access_table wl2864c_writeable_table = {
	.yes_ranges	= wl2864c_writeable_ranges,
	.n_yes_ranges	= ARRAY_SIZE(wl2864c_writeable_ranges),
};

static const struct regmap_access_table wl2864c_readable_table = {
	.yes_ranges	= wl2864c_readable_ranges,
	.n_yes_ranges	= ARRAY_SIZE(wl2864c_readable_ranges),
};

static const struct regmap_access_table wl2864c_volatile_table = {
	.yes_ranges	= wl2864c_volatile_ranges,
	.n_yes_ranges	= ARRAY_SIZE(wl2864c_volatile_ranges),
};

static const struct regmap_config wl2864c_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x0F,
	.wr_table = &wl2864c_writeable_table,
	.rd_table = &wl2864c_readable_table,
	.volatile_table = &wl2864c_volatile_table,
};

static int wl2864c_get_current_limit(struct regulator_dev *rdev)
{
	struct wl2864c *chip = rdev_get_drvdata(rdev);
	uint8_t reg_dump[WL2864C_REG_NUM];
	uint8_t reg_idx;
	unsigned int val = 0;

	dev_info(chip->dev, "************ start dump wl2864c register ************\n");
	dev_info(chip->dev, "regulator name = %s\n", rdev->desc->name);
	dev_info(chip->dev, "register 0x00:      chip version\n");
	dev_info(chip->dev, "register 0x01:      LDO CL\n");
	dev_info(chip->dev, "register 0x03~0x09: LDO1~LDO7 OUT Voltage\n");
	dev_info(chip->dev, "register 0x0e:      Bit[6:0] LDO7~LDO1 EN\n");

	for (reg_idx = 0; reg_idx < WL2864C_REG_NUM; reg_idx++) {
		regmap_read(chip->regmap, reg_idx, &val);
		reg_dump[reg_idx] = val;
		dev_info(chip->dev, "Reg[0x%02x] = 0x%x", reg_idx, reg_dump[reg_idx]);
	}
	dev_info(chip->dev, "************ end dump wl2864c register ************\n");

	return 0;
}

static int wl2864c_get_status(struct regulator_dev *rdev)
{
	struct wl2864c *chip = rdev_get_drvdata(rdev);
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

	wl2864c_get_current_limit(rdev);

	ret = regmap_read(chip->regmap, wl2864c_status_reg.sreg, &status);
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

static const struct regulator_ops wl2864c_regl_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_status = wl2864c_get_status,
	.get_current_limit = wl2864c_get_current_limit,
};

static int wl2864c_of_parse_cb(struct device_node *np,
				const struct regulator_desc *desc,
				struct regulator_config *config)
{
	int ena_gpio;

	ena_gpio = of_get_named_gpio(np, "enable-gpios", 0);
	if (gpio_is_valid(ena_gpio))
		config->ena_gpiod = gpio_to_desc(ena_gpio);

	return 0;
}

#define WL2864C_REGL_DESC(_id, _name, _s_name, _min, _step)       \
	[WL2864C_REGULATOR_##_id] = {                             \
		.name = #_name,                                    \
		.supply_name = _s_name,                            \
		.id = WL2864C_REGULATOR_##_id,                    \
		.of_match = of_match_ptr(#_name),                  \
		.of_parse_cb = wl2864c_of_parse_cb,               \
		.ops = &wl2864c_regl_ops,                         \
		.regulators_node = of_match_ptr("regulators"),     \
		.n_voltages = 256,                                 \
		.min_uV = _min,                                    \
		.uV_step = _step,                                  \
		.linear_min_sel = 0,                               \
		.vsel_mask = WL2864C_VSEL_MASK,                   \
		.vsel_reg = WL2864C_##_id##_VSEL,                 \
		.enable_reg = WL2864C_LDO_EN,       \
		.enable_mask = BIT(WL2864C_REGULATOR_##_id),      \
		.type = REGULATOR_VOLTAGE,                         \
		.owner = THIS_MODULE,                              \
	}

static struct regulator_desc wl2864c_regls_desc[WL2864C_MAX_REGULATORS] = {
	WL2864C_REGL_DESC(LDO1, camldo1, "vin3", 600000,  12500),
	WL2864C_REGL_DESC(LDO2, camldo2, "vin3", 600000,  12500),
	WL2864C_REGL_DESC(LDO3, ldo3, "vin2", 1200000, 12500),
	WL2864C_REGL_DESC(LDO4, ldo4, "vin2", 1200000, 12500),
	WL2864C_REGL_DESC(LDO5, ldo5, "vin2", 1200000,  12500),
	WL2864C_REGL_DESC(LDO6, ldo6, "vin2", 1200000,  12500),
	WL2864C_REGL_DESC(LDO7, ldo7, "vin2", 1200000, 12500),
};

static int wl2864c_regulator_init(struct wl2864c *chip)
{
	struct regulator_config config = { };
	struct regulator_desc *rdesc;
	u8 vsel_range[1];
	int id, ret = 0;
	const unsigned int ldo_regs[WL2864C_MAX_REGULATORS] = {
		WL2864C_LDO1_VOUT,
		WL2864C_LDO2_VOUT,
		WL2864C_LDO3_VOUT,
		WL2864C_LDO4_VOUT,
		WL2864C_LDO5_VOUT,
		WL2864C_LDO6_VOUT,
		WL2864C_LDO7_VOUT,
	};

	const unsigned int initial_voltage[WL2864C_MAX_REGULATORS] = {
		0x24,//LDO1 1.05V
		0x30,//LDO2 1.2V
		0x80,//LDO3 2.8V
		0x80,//LDO4 2.8V
		0x80,//LDO5 2.8V
		0x80,//LDO6 2.8V
		0x30,//LDO7 1.8V
	};

	/*Disable all ldo output by default*/
	ret = regmap_write(chip->regmap, WL2864C_LDO_EN, 0);
	if (ret < 0) {
		dev_info(chip->dev,
			"Disable all LDO output failed!!!\n");
		return ret;
	}

	for (id = 0; id < WL2864C_MAX_REGULATORS; id++) {
		chip->rdesc[id] = &wl2864c_regls_desc[id];
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
				"Failed to write initial voltage register\n");
			return ret;
		}
		dev_info(chip->dev, "%s: LDO%d, initial value:0x%x",
			__func__, (id+1), initial_voltage[id]);

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

static int wl2864c_i2c_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct wl2864c *chip;
	int error, vin1_enable_gpio, cs_gpio, ret, i;

	/*
	 * Set all register to initial value when probe driver to avoid register value was modified.
	 */
	const unsigned int initial_register[7][2] = {
		{WL2864C_CURRENT_LIMITSEL,		0x40},
		{WL2864C_DISCHARGE_RESISTORS,	0x00},
		{WL2864C_LDO1_LDO2_SEQ,			0x00},
		{WL2864C_LDO3_LDO4_SEQ,			0x00},
		{WL2864C_LDO5_LDO6_SEQ,			0x00},
		{WL2864C_LDO7_SEQ,				0x00},
		{WL2864C_SEQ_STATUS,			0x00},
	};
	chip = devm_kzalloc(dev, sizeof(struct wl2864c), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	dev_info(chip->dev, "%s Enter...\n", __func__);

	cs_gpio = of_get_named_gpio(dev->of_node, "semi,cs-gpios", 0);
	if (cs_gpio > 0) {
		if (!gpio_is_valid(cs_gpio)) {
			dev_info(dev, "Invalid chip select pin\n");
			return -EPERM;
		}

		ret = devm_gpio_request_one(dev, cs_gpio, GPIOF_OUT_INIT_LOW,
					    "wl2864c_cs_pin");
		if (ret) {
			dev_info(dev, "GPIO(%d) request failed(%d)\n",
				cs_gpio, ret);
			return ret;
		}

		chip->chip_cs_pin = cs_gpio;
	}

	vin1_enable_gpio = of_get_named_gpio(dev->of_node, "vin1-supply", 0);
	if (vin1_enable_gpio > 0) {
		if (!gpio_is_valid(vin1_enable_gpio)) {
			dev_info(dev, "Invalid chip select pin\n");
			return -EPERM;
		}

		ret = devm_gpio_request_one(dev, vin1_enable_gpio, GPIOF_OUT_INIT_HIGH,
					    "vin1_enable_gpio");
		if (ret) {
			dev_info(dev, "GPIO(%d) request failed(%d)\n",
				vin1_enable_gpio, ret);
			return ret;
		}

		chip->vin1_enable_gpio = vin1_enable_gpio;
	}

	dev_info(chip->dev, "%s vin1_enable_gpio:%d...\n", __func__, vin1_enable_gpio);

	mdelay(10);

	i2c_set_clientdata(client, chip);
	chip->dev = dev;
	chip->regmap = devm_regmap_init_i2c(client, &wl2864c_regmap_config);
	if (IS_ERR(chip->regmap)) {
		error = PTR_ERR(chip->regmap);
		dev_info(dev, "Failed to allocate register map: %d\n",
			error);
		return error;
	}

	for (i = 0; i < 7; i++) {
		ret = regmap_write(chip->regmap, initial_register[i][0], initial_register[i][1]);
		if (ret < 0) {
			dev_info(chip->dev, "Failed to write register: 0x%x, value: 0x%x\n",
				initial_register[i][0], initial_register[i][1]);
		}

		dev_info(chip->dev, "Success to write register: 0x%x, value: 0x%x\n",
			initial_register[i][0], initial_register[i][1]);
	}

	ret = wl2864c_regulator_init(chip);
	if (ret < 0) {
		dev_info(chip->dev, "Failed to init regulator(%d)\n", ret);
		return ret;
	}

	wl2864c_get_current_limit(chip->rdev[0]);

	dev_info(chip->dev, "%s Exit...\n", __func__);
	return ret;
}

static int wl2864c_i2c_remove(struct i2c_client *client)
{
	struct wl2864c *chip = i2c_get_clientdata(client);
	struct gpio_desc *desc;
	int ret = 0;

	if (chip->vin1_enable_gpio > 0) {
		desc = gpio_to_desc(chip->vin1_enable_gpio);
		ret = gpiod_direction_output_raw(desc, GPIOF_INIT_LOW);
	}
	if (chip->chip_cs_pin > 0) {
		desc = gpio_to_desc(chip->chip_cs_pin);
		ret = gpiod_direction_output_raw(desc, GPIOF_INIT_LOW);
	}
	return ret;
}

static const struct i2c_device_id wl2864c_i2c_id[] = {
	{"wl2864c", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, wl2864c_i2c_id);

static struct i2c_driver wl2864c_regulator_driver = {
	.driver = {
		.name = "wl2864c",
	},
	.probe = wl2864c_i2c_probe,
	.remove = wl2864c_i2c_remove,
	.id_table = wl2864c_i2c_id,
};

module_i2c_driver(wl2864c_regulator_driver);

MODULE_AUTHOR("Lu Tang <Lu Tang@mediatek.com>");
MODULE_DESCRIPTION("WL2864C regulator driver");
MODULE_LICENSE("GPL");
