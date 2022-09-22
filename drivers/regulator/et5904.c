// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/driver.h>

//TODO
#define LDO1_I2C_ID 1
#define LDO3_I2C_ID 3
#define LDO6_I2C_ID 6
#define LDO9_I2C_ID 9

enum et5904_regulator_ids {
	ET5904_LDO1,
	ET5904_LDO2,
	ET5904_LDO3,
	ET5904_LDO4,
};

enum et5904_registers {
	ET5904_PRODUCT_ID = 0x00,
	ET5904_ILIMIT,
	ET5904_RDIS,
	ET5904_LDO1VOUT,
	ET5904_LDO2VOUT,
	ET5904_LDO3VOUT,
	ET5904_LDO4VOUT,
	ET5904_SEQ1 = 0x0A,
	ET5904_SEQ2,
	ET5904_ENABLE = 0x0E,
};

#define ET5904_ID	0x00

static int my_regulator_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned int sel)
{
	int ret;

	pr_info("wyj %s  %s sel=%d\n", __func__,  rdev->desc->name, sel);

	ret = regulator_set_voltage_sel_regmap(rdev, sel);

	return ret;
}

static const struct regulator_ops et5904_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = my_regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
};

#define ET5904_LDOD(_i2cid, _num, _supply)				\
	[ET5904_LDO ## _num] = {					\
		.name =		   "ET5904LDO"#_num#_i2cid,				\
		.of_match =	   of_match_ptr("et5904ldo"#_num#_i2cid),		\
		.regulators_node = of_match_ptr("regulators"),		\
		.type =		   REGULATOR_VOLTAGE,			\
		.owner =	   THIS_MODULE,				\
		.linear_ranges =   (struct linear_range[]) {		\
		      REGULATOR_LINEAR_RANGE(600000, 0x0, 0xff, 6000),	\
		},							\
		.n_linear_ranges = 1,					\
		.n_voltages =	   0xff,				\
		.vsel_reg =	   ET5904_LDO ## _num ## VOUT,	\
		.vsel_mask =	   0xff,				\
		.enable_reg =	   ET5904_ENABLE,			\
		.enable_mask =	   BIT(_num - 1),			\
		.enable_time =	   150,					\
		.supply_name =	   _supply,				\
		.ops =		   &et5904_ops,			\
	}

#define ET5904_LDOA(_i2cid, _num, _supply)				\
	[ET5904_LDO ## _num] = {					\
		.name =		   "ET5904LDO"#_num#_i2cid,				\
		.of_match =	   of_match_ptr("et5904ldo"#_num#_i2cid),		\
		.regulators_node = of_match_ptr("regulators"),		\
		.type =		   REGULATOR_VOLTAGE,			\
		.owner =	   THIS_MODULE,				\
		.linear_ranges =   (struct linear_range[]) {		\
		      REGULATOR_LINEAR_RANGE(1200000, 0x0, 0xff, 12500),	\
		},							\
		.n_linear_ranges = 1,					\
		.n_voltages =	   0xff,				\
		.vsel_reg =	   ET5904_LDO ## _num ## VOUT,	\
		.vsel_mask =	   0xff,				\
		.enable_reg =	   ET5904_ENABLE,			\
		.enable_mask =	   BIT(_num - 1),			\
		.enable_time =	   150,					\
		.supply_name =	   _supply,				\
		.ops =		   &et5904_ops,			\
	}

static const struct regulator_desc et5904_regulators1[] = {
	ET5904_LDOD(1, 1, "VIND"),
	ET5904_LDOD(1, 2, "VIND"),
	ET5904_LDOA(1, 3, "VINA"),
	ET5904_LDOA(1, 4, "VINA"),
};

static const struct regulator_desc et5904_regulators3[] = {
	ET5904_LDOD(3, 1, "VIND"),
	ET5904_LDOD(3, 2, "VIND"),
	ET5904_LDOA(3, 3, "VINA"),
	ET5904_LDOA(3, 4, "VINA"),
};

static const struct regulator_desc et5904_regulators6[] = {
	ET5904_LDOD(6, 1, "VIND"),
	ET5904_LDOD(6, 2, "VIND"),
	ET5904_LDOA(6, 3, "VINA"),
	ET5904_LDOA(6, 4, "VINA"),
};

static const struct regulator_desc et5904_regulators9[] = {
	ET5904_LDOD(9, 1, "VIND"),
	ET5904_LDOD(9, 2, "VIND"),
	ET5904_LDOA(9, 3, "VINA"),
	ET5904_LDOA(9, 4, "VINA"),
};

static const struct regmap_config et5904_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = ET5904_ENABLE,
};

static int et5904_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct regulator_config config = { };
	struct regulator_dev *rdev;
	struct regmap *regmap;
	const struct regulator_desc *preg_desc;
	int i, ret, i2cid;
	unsigned int data;

	pr_info("%s-%d, enter\n", __func__, __LINE__);
	regmap = devm_regmap_init_i2c(i2c, &et5904_regmap);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_info(&i2c->dev, "Failed to create regmap: %d\n", ret);
		return ret;
	}

	ret = regmap_read(regmap, ET5904_PRODUCT_ID, &data);
	if (ret < 0) {
		dev_info(&i2c->dev, "Failed to read PRODUCT_ID: %d\n", ret);
		return ret;
	}
	if (data != ET5904_ID) {
		dev_info(&i2c->dev, "Unsupported device id: 0x%x.\n", data);
		//return -ENODEV;
	}

	i2cid = i2c_adapter_id(i2c->adapter);

	switch (i2cid) {
	case LDO1_I2C_ID:
		preg_desc = et5904_regulators1;
		break;
	case LDO3_I2C_ID:
		preg_desc = et5904_regulators3;
		break;
	case LDO6_I2C_ID:
		preg_desc = et5904_regulators6;
		break;
	case LDO9_I2C_ID:
		preg_desc = et5904_regulators9;
		break;
	default:
		dev_info(&i2c->dev, "UnSupported I2C-[%d]\n", i2cid);
		preg_desc = NULL;
		break;
	}
	pr_info("%s-%d\n", __func__, __LINE__);
	config.dev = &i2c->dev;
	config.init_data = NULL;

	if (preg_desc == NULL) {
		dev_info(&i2c->dev, "ET5904 Regulator description init failed\n");
		return -1;
	}
	for (i = 0; i < ARRAY_SIZE(et5904_regulators1); i++) {
		rdev = devm_regulator_register(&i2c->dev, &preg_desc[i], &config);
		if (IS_ERR(rdev)) {
			ret = PTR_ERR(rdev);
			dev_info(&i2c->dev, "Failed to register %s: %d\n",
				preg_desc[i].name, ret);
			return ret;
		}
	}
	pr_info("%s-%d, end\n", __func__, __LINE__);
	return 0;
}

static const struct of_device_id et5904_dt_ids[] = {
	{ .compatible = "mediatek,et5904", },
	{}
};
MODULE_DEVICE_TABLE(of, et5904_dt_ids);

static const struct i2c_device_id et5904_i2c_id[] = {
	{ "et5904", },
	{}
};
MODULE_DEVICE_TABLE(i2c, et5904_i2c_id);

static struct i2c_driver et5904_regulator_driver = {
	.driver = {
		.name = "et5904",
		.of_match_table = of_match_ptr(et5904_dt_ids),
	},
	.probe = et5904_i2c_probe,
	.id_table = et5904_i2c_id,
};
module_i2c_driver(et5904_regulator_driver);

MODULE_DESCRIPTION("ET5904 PMIC voltage regulator driver");
MODULE_AUTHOR("Forrest He <Forrest.He@mediatek.com>");
MODULE_LICENSE("GPL");
