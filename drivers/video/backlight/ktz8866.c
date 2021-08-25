/*
 * KTZ Semiconductor KTZ8866 LED Driver
 *
 * Copyright (C) 2013 Ideas on board SPRL
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * Contact: Zhang Teng <zhangteng3@xiaomi.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/platform_data/ktz8866.h>
#include <linux/slab.h>

#define u8	unsigned char

struct ktz8866 {
	struct i2c_client *client;
	struct backlight_device *backlight;
	struct ktz8866_platform_data *pdata;
};

struct ktz8866 *bd;

static struct ktz8866_led g_ktz8866_led;

int ktz8866_read(u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(bd->client, reg);
	if (ret < 0) {
		dev_err(&bd->client->dev, "failed reading at 0x%02x\n", reg);
		return ret;
	}

	*data = (uint8_t)ret;

	return 0;
}
EXPORT_SYMBOL(ktz8866_read);

int ktz8866_write(u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(bd->client, reg, data);
}
EXPORT_SYMBOL(ktz8866_write);

static int ktz8866_backlight_update_status(struct backlight_device *backlight)
{
	//struct ktz8866 *bd = bl_get_data(backlight);
	int exponential_bl = backlight->props.brightness;
	int brightness = 0;
	u8 v[2];
	brightness = bl_level_remap[exponential_bl];
	//brightness = exponential_bl;

	if (brightness < 0 || brightness > BL_LEVEL_MAX || brightness == g_ktz8866_led.level)
		return 0;

	mutex_lock(&g_ktz8866_led.lock);

	dev_warn(&bd->client->dev,
		"ktz8866 backlight 0x%02x ,exponential brightness %d \n", brightness, exponential_bl);
	if (!g_ktz8866_led.ktz8866_status && brightness > 0) {
		ktz8866_write(KTZ8866_DISP_BL_ENABLE, 0x5f);
		g_ktz8866_led.ktz8866_status = 1;
		dev_warn(&bd->client->dev, "ktz8866 backlight enable,dimming close");
	} else if (brightness == 0) {
		ktz8866_write(KTZ8866_DISP_BL_ENABLE, 0x1f);
		g_ktz8866_led.ktz8866_status = 0;
		usleep_range((10 * 1000),(10 * 1000) + 10);        
		dev_warn(&bd->client->dev, "ktz8866 backlight disable,dimming close");
	}
	v[0] = (brightness >> 3) & 0xff;
	v[1] = (brightness - (brightness >> 3) * 8) & 0x7;

	//v[0] = brightness & 0xff;
	//v[1] = (brightness >> 8) & 0x7;

	ktz8866_write(KTZ8866_DISP_BB_LSB, v[1]);
	ktz8866_write(KTZ8866_DISP_BB_MSB, v[0]);

	if (!g_ktz8866_led.dimming_status ) {
		usleep_range((5 * 1000),(5 * 1000) + 10);
		ktz8866_write(KTZ8866_DISP_BC2, 0xbd);
		g_ktz8866_led.dimming_status = 1;
		dev_warn(&bd->client->dev, "ktz8866 backlight dimming 128ms");
	}
	g_ktz8866_led.level = brightness;
	mutex_unlock(&g_ktz8866_led.lock);
	return 0;
}

static int ktz8866_backlight_get_brightness(struct backlight_device *backlight)
{
	//struct ktz8866 *bd = bl_get_data(backlight);
	int brightness = backlight->props.brightness;
	u8 v[2];

	ktz8866_read(0x5, &v[0]);
	ktz8866_read(0x4, &v[1]);

	brightness = (v[1] << 8)+v[0];

	return brightness;
}

static const struct backlight_ops ktz8866_backlight_ops = {
	.options	= BL_CORE_SUSPENDRESUME,
	.update_status	= ktz8866_backlight_update_status,
	.get_brightness	= ktz8866_backlight_get_brightness,
};

static int ktz8866_backlight_conf(struct ktz8866 *bd)
{
	int ret, i, reg_count;
	u8 read;

	dev_warn(&bd->client->dev,
		"K81 ktz8866_backlight_conf \n");

	reg_count = ARRAY_SIZE(ktz8866_regs_conf);
	for (i = 0; i < reg_count; i++){
		ret = ktz8866_write(ktz8866_regs_conf[i].reg, ktz8866_regs_conf[i].value);
		ktz8866_read(ktz8866_regs_conf[i].reg, &read);
		dev_err(&bd->client->dev, "K81 reading 0x%02x is 0x%02x\n", ktz8866_regs_conf[i].reg, read);
	}

	return ret;
}

static int parse_dt(struct device *dev, struct ktz8866_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	pdata->hw_en_gpio = of_get_named_gpio_flags(np, "ktz8866,hwen-gpio", 0, NULL);

	pdata->enp_gpio = of_get_named_gpio_flags(np, "ktz8866,enp-gpio", 0, NULL);

	pdata->enn_gpio = of_get_named_gpio_flags(np, "ktz8866,enn-gpio", 0, NULL);

	return 0;
}

static int ktz8866_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	//struct ktz8866_platform_data *pdata = dev_get_platdata(&client->dev);
	struct backlight_device *backlight;
	struct backlight_properties props;
	int ret;
	u8 read;

	dev_warn(&client->dev,
		"ktz8866_probe I2C adapter support I2C_FUNC_SMBUS_BYTE\n");

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_warn(&client->dev,
			 "ktz8866 I2C adapter doesn't support I2C_FUNC_SMBUS_BYTE\n");
		return -EIO;
	}
	dev_warn(&client->dev,
		"ktz8866 i2c_check_functionality I2C adapter support I2C_FUNC_SMBUS_BYTE\n");

	bd = devm_kzalloc(&client->dev, sizeof(*bd), GFP_KERNEL);
	if (!bd)
		return -ENOMEM;
	dev_warn(&client->dev,
		"ktz8866 bd = devm_kzalloc \n");

	bd->client = client;

	bd->pdata = devm_kzalloc(&client->dev, sizeof(struct ktz8866_platform_data), GFP_KERNEL);
	if (!bd->pdata)
		return -ENOMEM;
	dev_warn(&client->dev,
		"bd->pdata = devm_kzalloc \n");

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = 2047;
	props.brightness = clamp_t(unsigned int, 253, 16,
				   props.max_brightness);
	dev_warn(&client->dev,
		"ktz8866 devm_backlight_device_register \n");
	backlight = devm_backlight_device_register(&client->dev,
					      dev_name(&client->dev),
					      &bd->client->dev, bd,
					      &ktz8866_backlight_ops, &props);
	if (IS_ERR(backlight)) {
		dev_err(&client->dev, "K81 failed to register backlight\n");
		return PTR_ERR(backlight);
	}
	dev_warn(&client->dev,
		"ktz8866 backlight_update_status \n");
	backlight_update_status(backlight);
	dev_warn(&client->dev,
		"ktz8866 i2c_set_clientdata \n");
	i2c_set_clientdata(client, backlight);

	parse_dt(&client->dev, bd->pdata);
	dev_warn(&client->dev,
		"ktz8866 parse_dt \n");

	dev_warn(&client->dev,
		"ktz8866 ktz8866_probe KTZ8866_LCD_DRV_HW_EN\n");
	ret = devm_gpio_request_one(&client->dev, bd->pdata->hw_en_gpio,
				    GPIOF_DIR_OUT | GPIOF_INIT_HIGH, "HW_EN");
	if (ret < 0) {
		dev_err(&client->dev, "unable to request K81 HW_EN GPIO\n");
		return ret;
	}

	dev_warn(&client->dev, "K81 ktz8866_backlight_conf \n");
	ktz8866_backlight_conf(bd);

	ktz8866_read(KTZ8866_DISP_FLAGS, &read);
	dev_err(&bd->client->dev, "ktz8866 reading 0x%02x is 0x%02x\n", KTZ8866_DISP_FLAGS, read);

	return ret;
}

static int ktz8866_remove(struct i2c_client *client)
{
	struct backlight_device *backlight = i2c_get_clientdata(client);

	backlight->props.brightness = 0;
	backlight_update_status(backlight);

	return 0;
}

static const struct i2c_device_id ktz8866_ids[] = {
	{ "ktz8866", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ktz8866_ids);

static struct of_device_id ktz8866_match_table[] = {
	{ .compatible = "ktz,ktz8866",},
	{ },
};

static struct i2c_driver ktz8866_driver = {
	.driver = {
		.name = "ktz8866",
		.owner = THIS_MODULE,
		.of_match_table = ktz8866_match_table,
	},
	.probe = ktz8866_probe,
	.remove = ktz8866_remove,
	.id_table = ktz8866_ids,
};

module_i2c_driver(ktz8866_driver);

MODULE_DESCRIPTION("zhangteng3 ktz8866 Backlight Driver");
MODULE_AUTHOR("Laurent Pinchart <zhangteng3@xiaomi.com>");
MODULE_LICENSE("GPL");

