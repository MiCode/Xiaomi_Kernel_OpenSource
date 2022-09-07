// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/qti-regmap-debugfs.h>
#include <misc/isl97900_led.h>

#define ISL97900_DRIVER_NAME		"isl97900-driver"
#define ISL97900_MAX_REG		0x29
#define ISL97900_LED_STATUS		0x01
#define ISL97900_ENABLE_CONTROL		0x02
#define ISL97900_LED_R_LSB		0x13
#define ISL97900_LED_G_LSB		0x14
#define ISL97900_LED_B_LSB		0x15
#define ISL97900_LED_RGB_MSB		0x17

struct isl97900_priv {
	struct regmap *regmap;
	struct device *dev;
	u32 cali_red;
	u32 cali_green;
	u32 cali_blue;
};

static const struct regmap_config isl97900_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = ISL97900_MAX_REG,
};


static int isl97900_led_set_value(struct i2c_client *client,
			 enum isl_function event,
			 u32 level)
{
	struct isl97900_priv *priv;
	u32 level_lsb, level_msb, level_msb_cur;
	u32 red_level, green_level, blue_level;

	if (!client)
		return -EINVAL;

	priv = (struct isl97900_priv *)i2c_get_clientdata(client);
	if (!priv || !priv->regmap)
		return -EINVAL;

	if (level > 1023)
		level = 1023;

	level_lsb = level & 0xFF;
	level_msb = (level >> 8) & 0x03;
	level_msb_cur = 0;

	switch (event) {
	case ISL_LED_BRIGHTNESS_RGB_LEVEL:
		red_level = priv->cali_red * (level + 1) / 1024;
		green_level = priv->cali_green * (level + 1) / 1024;
		blue_level = priv->cali_blue * (level + 1) / 1024;

		level_msb = ((red_level >> 8) & 0x03) << 6;
		level_msb |= ((green_level >> 8) & 0x03) << 4;
		level_msb |= ((blue_level >> 8) & 0x03) << 2;

		regmap_write(priv->regmap, ISL97900_ENABLE_CONTROL, 0x04);
		regmap_write(priv->regmap, ISL97900_LED_R_LSB, red_level & 0xFF);
		regmap_write(priv->regmap, ISL97900_LED_G_LSB, green_level & 0xFF);
		regmap_write(priv->regmap, ISL97900_LED_B_LSB, blue_level & 0xFF);
		regmap_write(priv->regmap, ISL97900_LED_RGB_MSB, level_msb);
		break;
	case ISL_LED_BRIGHTNESS_RED_LEVEL:
		regmap_read(priv->regmap, ISL97900_LED_RGB_MSB, &level_msb_cur);
		level_msb = (level_msb << 6) | (level_msb_cur & 0x3C);

		regmap_write(priv->regmap, ISL97900_ENABLE_CONTROL, 0x04);
		regmap_write(priv->regmap, ISL97900_LED_R_LSB, level_lsb);
		regmap_write(priv->regmap, ISL97900_LED_RGB_MSB, level_msb);
		break;
	case ISL_LED_BRIGHTNESS_GREEN_LEVEL:
		regmap_read(priv->regmap, ISL97900_LED_RGB_MSB, &level_msb_cur);
		level_msb = (level_msb << 4) | (level_msb_cur & 0xCC);

		regmap_write(priv->regmap, ISL97900_ENABLE_CONTROL, 0x04);
		regmap_write(priv->regmap, ISL97900_LED_G_LSB, level_lsb);
		regmap_write(priv->regmap, ISL97900_LED_RGB_MSB, level_msb);
		break;
	case ISL_LED_BRIGHTNESS_BLUE_LEVEL:
		regmap_read(priv->regmap, ISL97900_LED_RGB_MSB, &level_msb_cur);
		level_msb = (level_msb << 2) | (level_msb_cur & 0xF0);

		regmap_write(priv->regmap, ISL97900_ENABLE_CONTROL, 0x04);
		regmap_write(priv->regmap, ISL97900_LED_B_LSB, level_lsb);
		regmap_write(priv->regmap, ISL97900_LED_RGB_MSB, level_msb);
		break;
	default:
		break;
	}

	return 0;
}


static int isl97900_led_get_value(struct i2c_client *client,
			 enum isl_function event,
			 u32 *level)
{
	struct isl97900_priv *priv;
	u32 level_lsb, level_msb;

	if (!client || !level)
		return -EINVAL;

	priv = (struct isl97900_priv *)i2c_get_clientdata(client);

	if (!priv || !priv->regmap)
		return -EINVAL;

	level_lsb = 0;
	level_msb = 0;

	switch (event) {
	case ISL_LED_BRIGHTNESS_RED_LEVEL:
		regmap_read(priv->regmap, ISL97900_LED_R_LSB, &level_lsb);
		regmap_read(priv->regmap, ISL97900_LED_RGB_MSB, &level_msb);
		level_msb = (level_msb >> 6) & 0x03;
		*level = (level_lsb & 0xFF) + (level_msb << 8);
		break;
	case ISL_LED_BRIGHTNESS_GREEN_LEVEL:
		regmap_read(priv->regmap, ISL97900_LED_G_LSB, &level_lsb);
		regmap_read(priv->regmap, ISL97900_LED_RGB_MSB, &level_msb);
		level_msb = (level_msb >> 4) & 0x03;
		*level = (level_lsb & 0xFF) + (level_msb << 8);
		break;
	case ISL_LED_BRIGHTNESS_BLUE_LEVEL:
		regmap_read(priv->regmap, ISL97900_LED_B_LSB, &level_lsb);
		regmap_read(priv->regmap, ISL97900_LED_RGB_MSB, &level_msb);
		level_msb = (level_msb >> 2) & 0x03;
		*level = (level_lsb & 0xFF) + (level_msb << 8);
		break;
	case ISL_LED_BRIGHTNESS_RGB_LEVEL:
	default:
		*level = 0;
		break;
	}

	return 0;
}


static ssize_t red_led_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	u32 level;
	int rc;

	rc = isl97900_led_get_value(client, ISL_LED_BRIGHTNESS_RED_LEVEL, &level);
	if (rc)
		return rc;

	return scnprintf(buf, 10, "%d\n", level);
}

static ssize_t red_led_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	u32 level;
	int rc;

	rc = kstrtou32(buf, 10, &level);
	if (rc)
		return rc;

	rc = isl97900_led_set_value(client, ISL_LED_BRIGHTNESS_RED_LEVEL, level);
	if (rc)
		return rc;

	return count;
}


static DEVICE_ATTR_RW(red_led);

static ssize_t green_led_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	u32 level;
	int rc;

	rc = isl97900_led_get_value(client, ISL_LED_BRIGHTNESS_GREEN_LEVEL, &level);
	if (rc)
		return rc;

	return scnprintf(buf, 10, "%d\n", level);
}

static ssize_t green_led_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	u32 level;
	int rc;

	rc = kstrtou32(buf, 10, &level);
	if (rc)
		return rc;

	rc = isl97900_led_set_value(client, ISL_LED_BRIGHTNESS_GREEN_LEVEL, level);
	if (rc)
		return rc;

	return count;
}


static DEVICE_ATTR_RW(green_led);

static ssize_t blue_led_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	u32 level;
	int rc;

	rc = isl97900_led_get_value(client, ISL_LED_BRIGHTNESS_BLUE_LEVEL, &level);
	if (rc)
		return rc;

	return scnprintf(buf, 10, "%d\n", level);
}

static ssize_t blue_led_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	u32 level;
	int rc;

	rc = kstrtou32(buf, 10, &level);
	if (rc)
		return rc;

	rc = isl97900_led_set_value(client, ISL_LED_BRIGHTNESS_BLUE_LEVEL, level);
	if (rc)
		return rc;

	return count;
}


static DEVICE_ATTR_RW(blue_led);

static ssize_t led_status_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int rc;
	u32 led_status;
	struct isl97900_priv *priv;

	priv = (struct isl97900_priv *)i2c_get_clientdata(client);

	if (!priv || !priv->regmap)
		return -EINVAL;

	rc = regmap_read(priv->regmap, ISL97900_LED_STATUS, &led_status);
	if (rc)
		led_status = 0;

	return scnprintf(buf, 10, "%d\n", (led_status & 0x01));
}

static DEVICE_ATTR_RO(led_status);

static struct attribute *isl97900_attrs[] = {
	&dev_attr_red_led.attr,
	&dev_attr_green_led.attr,
	&dev_attr_blue_led.attr,
	&dev_attr_led_status.attr,
	NULL
};


static const struct attribute_group isl97900_attr_group = {
	.attrs = isl97900_attrs,
};


static int isl97900_led_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct isl97900_priv *priv;
	int rc = 0;

	priv = devm_kzalloc(&client->dev, sizeof(*priv),
				GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &client->dev;

	priv->regmap = devm_regmap_init_i2c(client, &isl97900_regmap_config);
	if (IS_ERR_OR_NULL(priv->regmap)) {
		pr_err("isl97900: failed to init regmap(%d)!\n", rc);
		return -EINVAL;
	}

	devm_regmap_qti_debugfs_register(priv->dev, priv->regmap);

	rc = sysfs_create_group(&client->dev.kobj, &isl97900_attr_group);
	if (rc)
		pr_err("isl97900: failed to create sysfs group(%d)!\n", rc);

	i2c_set_clientdata(client, priv);

	priv->cali_red = 0xFF;
	priv->cali_green = 0xFF;
	priv->cali_blue = 0xFF;

	return 0;
}

static int isl97900_led_remove(struct i2c_client *client)
{
	struct isl97900_priv *priv =
			(struct isl97900_priv *)i2c_get_clientdata(client);

	if (!priv)
		return -EINVAL;

	dev_set_drvdata(&client->dev, NULL);

	sysfs_remove_group(&client->dev.kobj, &isl97900_attr_group);

	return 0;
}

int isl97900_led_cali_data_update(struct device_node *node,
			u32 red_level,
			u32 green_level,
			u32 blue_level)
{
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct isl97900_priv *priv;

	if (!client)
		return -EINVAL;

	priv = (struct isl97900_priv *)i2c_get_clientdata(client);
	if (!priv)
		return -EINVAL;

	if (!red_level || !green_level || !blue_level)
		return -EINVAL;

	priv->cali_red = red_level;
	priv->cali_green = green_level;
	priv->cali_blue = blue_level;

	return 0;
}
EXPORT_SYMBOL(isl97900_led_cali_data_update);


int isl97900_led_event(struct device_node *node,
			enum isl_function event,
			u32 level)
{
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	int rc;

	rc = isl97900_led_set_value(client, event, level);

	return rc;
}
EXPORT_SYMBOL(isl97900_led_event);


static const struct of_device_id isl97900_of_match[] = {
	{ .compatible = "qcom,isl97900-led" },
	{ }
};

static struct i2c_driver isl97900_led_driver = {
	.probe = isl97900_led_probe,
	.remove = isl97900_led_remove,
	.driver = {
		.name = ISL97900_DRIVER_NAME,
		.of_match_table = isl97900_of_match,
	},
};

static int __init isl97900_led_init(void)
{
	int rc;

	rc = i2c_add_driver(&isl97900_led_driver);
	if (rc)
		pr_err("isl97900:: failed to add i2c driver(%d)!\n", rc);

	return rc;
}
module_init(isl97900_led_init);

static void __exit isl97900_led_exit(void)
{
	i2c_del_driver(&isl97900_led_driver);
}
module_exit(isl97900_led_exit);

MODULE_DESCRIPTION("ISL97900 LED driver");
MODULE_LICENSE("GPL v2");
