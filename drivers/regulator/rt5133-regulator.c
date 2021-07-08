// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 Mediatek Inc.

#include <linux/crc8.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/extdev_io_class.h>

#define RT5133_DRV_VERSION		"1.0.2_MTK"

#define RT5133_REG_CHIP_INFO		0x00
#define RT5133_REG_RST_CTRL		0x06
#define RT5133_REG_BASE_CTRL		0x09
#define RT5133_REG_GPIO_CTRL		0x0B
#define RT5133_REG_BASE_EVT		0x10
#define RT5133_REG_BASE_MASK		0x16
#define RT5133_REG_LDO_SHDN		0x19
#define RT5133_REG_LDO1_CTRL1		0x20
#define RT5133_REG_LDO1_CTRL2		0x21
#define RT5133_REG_LDO1_CTRL3		0x22
#define RT5133_REG_LDO2_CTRL1		0x24
#define RT5133_REG_LDO2_CTRL2		0x25
#define RT5133_REG_LDO2_CTRL3		0x26
#define RT5133_REG_LDO3_CTRL1		0x28
#define RT5133_REG_LDO3_CTRL2		0x29
#define RT5133_REG_LDO3_CTRL3		0x2A
#define RT5133_REG_LDO4_CTRL1		0x2C
#define RT5133_REG_LDO4_CTRL2		0x2D
#define RT5133_REG_LDO4_CTRL3		0x2E
#define RT5133_REG_LDO5_CTRL1		0x30
#define RT5133_REG_LDO5_CTRL2		0x31
#define RT5133_REG_LDO5_CTRL3		0x32
#define RT5133_REG_LDO6_CTRL1		0x34
#define RT5133_REG_LDO6_CTRL2		0x35
#define RT5133_REG_LDO6_CTRL3		0x36
#define RT5133_REG_LDO7_CTRL1		0x38
#define RT5133_REG_LDO7_CTRL2		0x39
#define RT5133_REG_LDO7_CTRL3		0x3A
#define RT5133_REG_LDO8_CTRL1		0x3C
#define RT5133_REG_LDO8_CTRL2		0x3D
#define RT5133_REG_LDO8_CTRL3		0x3E
#define RT5133_REG_LDO8_CTRL4		0x3F

#define RT5133_LDO_REG_BASE(_id)	(0x20 + ((_id) - 1) * 4)

#define RT5133_VENDOR_ID_MASK		GENMASK(7, 4)
#define RT5133_VENDOR_ID		0x70
#define RT5133_RESET_CODE		0xB1

#define RT5133_FOFF_BASE_MASK		BIT(1)
#define RT5133_OCSHDN_ALL_MASK		BIT(7)
#define RT5133_PGBSHDN_ALL_MASK		BIT(6)

#define RT5133_OCPTSEL_MASK		BIT(5)
#define RT5133_PGBPTSEL_MASK		BIT(4)
#define RT5133_STBTDSEL_MASK		GENMASK(1, 0)

#define RT5133_LDO_ENABLE_MASK		BIT(7)
#define RT5133_LDO_VSEL_MASK		GENMASK(7, 5)
#define RT5133_LDO_AD_MASK		BIT(2)

#define RT5133_GPIO_NR			3
#define RT5133_GPIOEN_MASK(_id)		(BIT(7 - (_id)) | BIT(3 - (_id)))

#define RT5133_LDO_PGB_EVT_MASK		GENMASK(23, 16)
#define RT5133_LDO_PGB_EVT_SHIFT	16
#define RT5133_LDO_OC_EVT_MASK		GENMASK(15, 8)
#define RT5133_LDO_OC_EVT_SHIFT		8
#define RT5133_VREF_EVT_MASK		BIT(6)
#define RT5133_BASE_EVT_MASK		GENMASK(7, 0)
#define RT5133_INTR_CLR_MASK		GENMASK(23, 0)
#define RT5133_INTR_BYTE_NR		3

#define RT5133_MAX_I2C_BLOCK_SIZE	1

#define RT5133_CRC8_POLYNOMIAL		0x7

#define RT5133_I2C_ADDR_LEN		1
#define RT5133_PREDATA_LEN		2
#define RT5133_I2C_CRC_LEN		1
#define RT5133_REG_ADDR_LEN		1
#define RT5133_I2C_DUMMY_LEN		1

#define I2C_ADDR_XLATE_8BIT(_addr, _rw)	((((_addr) & 0x7F) << 1) | (_rw))

enum {
	RT5133_REGULATOR_BASE = 0,
	RT5133_REGULATOR_LDO1,
	RT5133_REGULATOR_LDO2,
	RT5133_REGULATOR_LDO3,
	RT5133_REGULATOR_LDO4,
	RT5133_REGULATOR_LDO5,
	RT5133_REGULATOR_LDO6,
	RT5133_REGULATOR_LDO7,
	RT5133_REGULATOR_LDO8,
	RT5133_REGULATOR_MAX
};

struct rt5133_priv {
	struct device *dev;
	struct regmap *regmap;
	struct gpio_desc *enable_gpio;
	struct regulator_dev *rdev[RT5133_REGULATOR_MAX];
	struct gpio_chip gc;
	struct extdev_desc extdev_desc;
	struct extdev_io_device *extdev;
	unsigned int gpio_output_flag;
	u8 crc8_tbls[CRC8_TABLE_SIZE];
};

static const unsigned int vout_type1_tables[] = {
	1800000, 2500000, 2700000, 2800000, 2900000, 3000000, 3100000, 3200000
};

static const unsigned int vout_type2_tables[] = {
	1700000, 1800000, 1900000, 2500000, 2700000, 2800000, 2900000, 3000000
};

static const unsigned int vout_type3_tables[] = {
	900000, 950000, 1000000, 1050000, 1100000, 1150000, 1200000, 1800000
};

static const struct regulator_ops rt5133_regulator_ops = {
	.list_voltage = regulator_list_voltage_table,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_active_discharge = regulator_set_active_discharge_regmap,
};

static const struct regulator_ops rt5133_base_regulator_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
};

static int rt5133_of_parse_cb(struct device_node *node,
			      const struct regulator_desc *desc,
			      struct regulator_config *config)
{
	struct rt5133_priv *priv = config->driver_data;
	unsigned int base_addr = 0;

	const struct {
		const char *prop_name;
		unsigned int min;
		unsigned int max;
		unsigned int def_val;
		unsigned int addr_offset;
		unsigned int mask;
	} base_props[] = {
		{ "oc_shutdown_all", 0, 1, 0,
			RT5133_REG_LDO_SHDN, RT5133_OCSHDN_ALL_MASK },
		{ "pgb_shutdown_all", 0, 1, 0,
			RT5133_REG_LDO_SHDN, RT5133_PGBSHDN_ALL_MASK }
	}, ldo_props[] = {
		{ "oc_ptsel", 0, 1, 0, 0, RT5133_OCPTSEL_MASK },
		{ "pgb_ptsel", 0, 1, 0, 0, RT5133_PGBPTSEL_MASK },
		{ "soft_start_time_sel", 0, 3, 1, 0, RT5133_STBTDSEL_MASK }
	}, *props;
	int i, props_size;

	if (desc->id == RT5133_REGULATOR_BASE) {
		props = base_props;
		props_size = ARRAY_SIZE(base_props);
	} else {
		props = ldo_props;
		props_size = ARRAY_SIZE(ldo_props);
		base_addr = RT5133_LDO_REG_BASE(desc->id);
	}

	for (i = 0; i < props_size; i++) {
		int shift = ffs(props[i].mask) - 1, ret;
		unsigned int val;

		ret = of_property_read_u32(node, props[i].prop_name, &val);
		if (ret)
			val = props[i].def_val;

		if (val > props[i].max)
			val = props[i].max;

		ret = regmap_update_bits(priv->regmap,
					 base_addr + props[i].addr_offset,
					 props[i].mask, val << shift);
		if (ret)
			return ret;
	}

	return 0;
}

#define RT5133_REGULATOR_DESC(_name, vtables, _supply) \
{\
	.name = #_name,\
	.id = RT5133_REGULATOR_##_name,\
	.of_match = of_match_ptr(#_name),\
	.regulators_node = of_match_ptr("regulators"),\
	.supply_name = _supply,\
	.of_parse_cb = rt5133_of_parse_cb,\
	.type = REGULATOR_VOLTAGE,\
	.owner = THIS_MODULE,\
	.ops = &rt5133_regulator_ops,\
	.n_voltages = ARRAY_SIZE(vtables),\
	.volt_table = vtables,\
	.enable_reg = RT5133_REG_##_name##_CTRL1,\
	.enable_mask = RT5133_LDO_ENABLE_MASK,\
	.vsel_reg = RT5133_REG_##_name##_CTRL2,\
	.vsel_mask = RT5133_LDO_VSEL_MASK,\
	.active_discharge_reg = RT5133_REG_##_name##_CTRL3,\
	.active_discharge_mask = RT5133_LDO_AD_MASK,\
}

static const struct regulator_desc rt5133_regulators[] = {
	/* For digital part, base current control */
	{
		.name = "rt5133,base",
		.id = RT5133_REGULATOR_BASE,
		.of_match = of_match_ptr("BASE"),
		.regulators_node = of_match_ptr("regulators"),
		.of_parse_cb = rt5133_of_parse_cb,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
		.ops = &rt5133_base_regulator_ops,
		.enable_reg = RT5133_REG_BASE_CTRL,
		.enable_mask = RT5133_FOFF_BASE_MASK,
		.enable_is_inverted = true,
	},
	RT5133_REGULATOR_DESC(LDO1, vout_type1_tables, "rt5133,base"),
	RT5133_REGULATOR_DESC(LDO2, vout_type1_tables, "rt5133,base"),
	RT5133_REGULATOR_DESC(LDO3, vout_type2_tables, "rt5133,base"),
	RT5133_REGULATOR_DESC(LDO4, vout_type2_tables, "rt5133,base"),
	RT5133_REGULATOR_DESC(LDO5, vout_type2_tables, "rt5133,base"),
	RT5133_REGULATOR_DESC(LDO6, vout_type2_tables, "rt5133,base"),
	RT5133_REGULATOR_DESC(LDO7, vout_type3_tables, "rt5133-ldo1"),
	RT5133_REGULATOR_DESC(LDO8, vout_type3_tables, "rt5133-ldo1"),
};

static int rt5133_gpio_direction_output(struct gpio_chip *gpio,
					unsigned int offset, int value)
{
	struct rt5133_priv *priv = gpiochip_get_data(gpio);

	return regmap_update_bits(priv->regmap, RT5133_REG_GPIO_CTRL,
				  BIT(7-offset)|BIT(3-offset),
				  value ? BIT(7-offset)|BIT(3-offset) : 0);
}

static int rt5133_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	struct rt5133_priv *priv = gpiochip_get_data(chip);

	return !!(priv->gpio_output_flag & BIT(offset));
}

static void rt5133_gpio_set(struct gpio_chip *chip, unsigned int offset,
			    int set_val)
{
	struct rt5133_priv *priv = gpiochip_get_data(chip);
	unsigned int mask = RT5133_GPIOEN_MASK(offset);
	unsigned int val = set_val ? mask : 0;
	unsigned int next_flag = priv->gpio_output_flag;
	int ret;

	if (set_val)
		next_flag |= BIT(offset);
	else
		next_flag &= ~BIT(offset);

	ret = regmap_update_bits(priv->regmap, RT5133_REG_GPIO_CTRL, mask, val);
	if (ret) {
		dev_err(priv->dev, "Failed to set gpio [%d] val %d\n", offset,
			set_val);
		return;
	}

	priv->gpio_output_flag = next_flag;
}

static irqreturn_t rt5133_intr_handler(int irq_number, void *data)
{
	struct rt5133_priv *priv = data;
	u32 intr_evts = 0, handle_evts;
	int i, ret;

	ret = regmap_bulk_read(priv->regmap, RT5133_REG_BASE_EVT, &intr_evts,
			       RT5133_INTR_BYTE_NR);
	if (ret)
		goto out_intr_handler;

	handle_evts = intr_evts & RT5133_BASE_EVT_MASK;
	/*
	 * VREF_EVT is a special case, if base off
	 * this event will also be trigger. Skip it
	 */
	if (handle_evts & ~RT5133_VREF_EVT_MASK)
		dev_info(priv->dev, "base event occurred [0x%02x]\n",
			 handle_evts);

	handle_evts = (intr_evts & RT5133_LDO_OC_EVT_MASK) >>
		RT5133_LDO_OC_EVT_SHIFT;
	for (i = RT5133_REGULATOR_LDO1; i < RT5133_REGULATOR_MAX && handle_evts; i++) {
		if (!(handle_evts & BIT(i - 1)))
			continue;
		regulator_notifier_call_chain(priv->rdev[i],
					      REGULATOR_EVENT_OVER_CURRENT,
					      &i);
	}

	handle_evts = (intr_evts & RT5133_LDO_PGB_EVT_MASK) >>
		RT5133_LDO_PGB_EVT_SHIFT;
	for (i = RT5133_REGULATOR_LDO1; i < RT5133_REGULATOR_MAX && handle_evts; i++) {
		if (!(handle_evts & BIT(i - 1)))
			continue;
		regulator_notifier_call_chain(priv->rdev[i],
					      REGULATOR_EVENT_FAIL, &i);
	}

	ret = regmap_bulk_write(priv->regmap, RT5133_REG_BASE_EVT, &intr_evts,
				RT5133_INTR_BYTE_NR);
	if (ret)
		goto out_intr_handler;

	return IRQ_HANDLED;

out_intr_handler:
	return IRQ_NONE;
}

static int rt5133_enable_interrupts(int irq_no, struct rt5133_priv *priv)
{
	u32 mask = RT5133_INTR_CLR_MASK;
	int ret;

	/* Force to write clear all events */
	ret = regmap_bulk_write(priv->regmap, RT5133_REG_BASE_EVT, &mask,
				RT5133_INTR_BYTE_NR);
	if (ret) {
		dev_err(priv->dev, "Failed to clear all interrupts\n");
		return ret;
	}

	/* Unmask all interrupts */
	mask = 0;
	ret = regmap_bulk_write(priv->regmap, RT5133_REG_BASE_MASK, &mask,
				RT5133_INTR_BYTE_NR);
	if (ret) {
		dev_err(priv->dev, "Failed to unmask all interrupts\n");
		return ret;
	}

	return devm_request_threaded_irq(priv->dev, irq_no, NULL,
					 rt5133_intr_handler, IRQF_ONESHOT,
					 dev_name(priv->dev), priv);
}

static int rt5133_dbg_io_read(void *drvdata, u16 reg, void *val, u16 size)
{
	return regmap_bulk_read((struct regmap *)drvdata, reg, val, size);
}

static int rt5133_dbg_io_write(void *drvdata, u16 reg,
			       const void *val, u16 size)
{
	return regmap_bulk_write((struct regmap *)drvdata, reg, val, size);
}

static int rt5133_regmap_hw_read(void *context, const void *reg_buf,
				 size_t reg_size, void *val_buf,
				 size_t val_size)
{
	struct rt5133_priv *priv = context;
	struct i2c_client *client = to_i2c_client(priv->dev);
	u8 reg = *(u8 *)reg_buf, crc;
	u8 *buf;
	int buf_len = RT5133_PREDATA_LEN + val_size + RT5133_I2C_CRC_LEN;
	int read_len, ret;

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf[0] = I2C_ADDR_XLATE_8BIT(client->addr, I2C_SMBUS_READ);
	buf[1] = reg;

	read_len = val_size + RT5133_I2C_CRC_LEN;
	ret = i2c_smbus_read_i2c_block_data(client, reg, read_len,
					    buf + RT5133_PREDATA_LEN);

	if (ret < 0)
		goto out_read_err;
	else if (ret != read_len) {
		ret = -EIO;
		goto out_read_err;
	}

	crc = crc8(priv->crc8_tbls, buf, RT5133_PREDATA_LEN + val_size, 0);
	if (crc != buf[RT5133_PREDATA_LEN + val_size]) {
		ret = -EIO;
		goto out_read_err;
	}

	memcpy(val_buf, buf + RT5133_PREDATA_LEN, val_size);

out_read_err:
	kfree(buf);
	return (ret < 0) ? ret : 0;
}

static int rt5133_regmap_hw_write(void *context, const void *data, size_t count)
{
	struct rt5133_priv *priv = context;
	struct i2c_client *client = to_i2c_client(priv->dev);
	u8 reg = *(u8 *)data, crc;
	u8 *buf;
	int buf_len = RT5133_I2C_ADDR_LEN + count + RT5133_I2C_CRC_LEN +
		RT5133_I2C_DUMMY_LEN;
	int write_len, ret;

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf[0] = I2C_ADDR_XLATE_8BIT(client->addr, I2C_SMBUS_WRITE);
	buf[1] = reg;
	memcpy(buf + RT5133_PREDATA_LEN, data + RT5133_REG_ADDR_LEN,
	       count - RT5133_REG_ADDR_LEN);

	crc = crc8(priv->crc8_tbls, buf, RT5133_I2C_ADDR_LEN + count, 0);
	buf[RT5133_I2C_ADDR_LEN + count] = crc;

	write_len = count - RT5133_REG_ADDR_LEN + RT5133_I2C_CRC_LEN +
		RT5133_I2C_DUMMY_LEN;
	ret = i2c_smbus_write_i2c_block_data(client, reg, write_len,
					     buf + RT5133_PREDATA_LEN);

	kfree(buf);
	return ret;
}

static const struct regmap_bus rt5133_regmap_bus = {
	.read = rt5133_regmap_hw_read,
	.write = rt5133_regmap_hw_write,
	/* Due to crc, the block read/write length has the limit */
	.max_raw_read = RT5133_MAX_I2C_BLOCK_SIZE,
	.max_raw_write = RT5133_MAX_I2C_BLOCK_SIZE,
};

static const struct regmap_config rt5133_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = RT5133_REG_LDO8_CTRL4,
};

static int rt5133_chip_reset(struct rt5133_priv *priv)
{
	int ret;

	ret = regmap_write(priv->regmap, RT5133_REG_RST_CTRL,
			   RT5133_RESET_CODE);
	if (ret)
		return ret;

	/* Wait for register reset to take effect */
	udelay(2);

	return 0;
}

static int rt5133_validate_vendor_info(struct rt5133_priv *priv)
{
	unsigned int val = 0;
	int ret;

	ret = regmap_read(priv->regmap, RT5133_REG_CHIP_INFO, &val);
	if (ret)
		return ret;

	if ((val & RT5133_VENDOR_ID_MASK) != RT5133_VENDOR_ID)
		return -ENODEV;

	return 0;
}

static int rt5133_probe(struct i2c_client *i2c)
{
	struct rt5133_priv *priv;
	struct regulator_config config = {0};
	int i, ret;

	dev_info(&i2c->dev, "%s start(%s)\n", __func__, RT5133_DRV_VERSION);
	priv = devm_kzalloc(&i2c->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &i2c->dev;
	crc8_populate_msb(priv->crc8_tbls, RT5133_CRC8_POLYNOMIAL);

	priv->enable_gpio = devm_gpiod_get_optional(&i2c->dev, "enable",
						    GPIOD_OUT_HIGH);
	if (IS_ERR(priv->enable_gpio)) {
		dev_err(&i2c->dev, "Failed to request HWEN gpio\n");
		return PTR_ERR(priv->enable_gpio);
	}

	priv->regmap = devm_regmap_init(&i2c->dev, &rt5133_regmap_bus, priv,
					&rt5133_regmap_config);
	if (IS_ERR(priv->regmap)) {
		dev_err(&i2c->dev, "Failed to register regmap\n");
		return PTR_ERR(priv->regmap);
	}

	priv->extdev_desc.dirname = devm_kasprintf(&i2c->dev,
						   GFP_KERNEL, "RT5133.%s",
						   dev_name(&i2c->dev));
	priv->extdev_desc.devname = dev_name(&i2c->dev);
	priv->extdev_desc.typestr = devm_kasprintf(&i2c->dev, GFP_KERNEL,
						   "I2C,RT5133");
	priv->extdev_desc.rmap = priv->regmap;
	priv->extdev_desc.io_read = rt5133_dbg_io_read;
	priv->extdev_desc.io_write = rt5133_dbg_io_write;
	priv->extdev = devm_extdev_io_device_register(&i2c->dev,
						      &priv->extdev_desc);
	if (IS_ERR(priv->extdev)) {
		dev_err(&i2c->dev, "failed to register extdev_io device\n",
			__func__);
		return PTR_ERR(priv->extdev);
	}

	ret = rt5133_validate_vendor_info(priv);
	if (ret) {
		dev_err(&i2c->dev, "Failed to check vendor info [%d]\n", ret);
		return ret;
	}

	ret = rt5133_chip_reset(priv);
	if (ret) {
		dev_err(&i2c->dev, "Failed to execute sw reset\n");
		return ret;
	}

	config.dev = &i2c->dev;
	config.driver_data = priv;
	config.regmap = priv->regmap;

	for (i = 0; i < RT5133_REGULATOR_MAX; i++) {
		priv->rdev[i] = devm_regulator_register(&i2c->dev,
							rt5133_regulators + i,
							&config);
		if (IS_ERR(priv->rdev[i])) {
			dev_err(&i2c->dev,
				"Failed to register [%d] regulator\n", i);
			return PTR_ERR(priv->rdev[i]);
		}
	}

	priv->gc.label = dev_name(&i2c->dev);
	priv->gc.parent = &i2c->dev;
	priv->gc.base = -1;
	priv->gc.ngpio = RT5133_GPIO_NR;
	priv->gc.set = rt5133_gpio_set;
	priv->gc.get = rt5133_gpio_get;
	priv->gc.direction_output = rt5133_gpio_direction_output;

	ret = devm_gpiochip_add_data(&i2c->dev, &priv->gc, priv);
	if (ret)
		return ret;

	ret = rt5133_enable_interrupts(i2c->irq, priv);
	if (ret) {
		dev_err(&i2c->dev, "enable interrupt failed\n");
		return ret;
	}

	i2c_set_clientdata(i2c, priv);

	dev_info(&i2c->dev, "%s done.\n", __func__);
	return ret;
}

static int rt5133_remove(struct i2c_client *i2c)
{
	struct rt5133_priv *priv = i2c_get_clientdata(i2c);

	extdev_io_device_unregister(priv->extdev);
	return 0;
}

static const struct of_device_id __maybe_unused rt5133_ofid_tbls[] = {
	{ .compatible = "richtek,rt5133", },
	{ }
};
MODULE_DEVICE_TABLE(of, rt5133_ofid_tbls);

static struct i2c_driver rt5133_driver = {
	.driver = {
		.name = "rt5133",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rt5133_ofid_tbls),
	},
	.probe_new = rt5133_probe,
	.remove = rt5133_remove,
};
module_i2c_driver(rt5133_driver);

MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_DESCRIPTION("RT5133 Regulator Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(RT5133_DRV_VERSION);
