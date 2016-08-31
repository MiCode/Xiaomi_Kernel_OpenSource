/*
 * Base driver for Maxim MAX8831
 *
 * Copyright (c) 2008-2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/mfd/max8831.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/err.h>
#include <linux/mfd/core.h>

enum chips { max8831 };

static const struct i2c_device_id max8831_id[] = {
	{ "max8831", max8831 },
	{},
};
MODULE_DEVICE_TABLE(i2c, max8831_id);

static const struct regmap_config max8831_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x3A,
};

static struct mfd_cell max8831_mfd_cells[] = {
	[MAX8831_ID_LED1] = {
		.name = "max8831_led_bl",
		.id = MAX8831_ID_LED1,
	},
	[MAX8831_ID_LED2] = {
		.name = "max8831_led_bl",
		.id = MAX8831_ID_LED2,
	},
	[MAX8831_ID_LED3] = {
		.name = "max8831_led_bl",
		.id = MAX8831_ID_LED3,
	},
	[MAX8831_ID_LED4] = {
		.name = "max8831_led_bl",
		.id = MAX8831_ID_LED4,
	},
	[MAX8831_ID_LED5] = {
		.name = "max8831_led_bl",
		.id = MAX8831_ID_LED5,
	},
	[MAX8831_BL_LEDS] = {
		.name = "max8831_display_bl",
		.id = MAX8831_BL_LEDS,
	},
};

static int max8831_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct max8831_platform_data *pdata = client->dev.platform_data;
	struct max8831_chip *chip;
	struct max8831_subdev_info *subdev;
	int ret;
	int i;

	chip = devm_kzalloc(&client->dev,
				sizeof(struct max8831_chip), GFP_KERNEL);
	if (chip == NULL)
		return -ENOMEM;

	chip->client = client;
	chip->dev = &client->dev;

	i2c_set_clientdata(client, chip);

	chip->regmap = devm_regmap_init_i2c(client, &max8831_regmap_config);
	if (IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		dev_err(&client->dev, "regmap init failed: %d\n", ret);
		return ret;
	}

	for (i = 0; i < pdata->num_subdevs; i++) {
		subdev = &pdata->subdevs[i];
		max8831_mfd_cells[subdev->id].platform_data =
							subdev->platform_data;
		max8831_mfd_cells[subdev->id].pdata_size = subdev->pdata_size;
	}

	ret = mfd_add_devices(chip->dev, 0,
		      max8831_mfd_cells, ARRAY_SIZE(max8831_mfd_cells),
		      NULL, 0, NULL);
	if (ret < 0)
		goto failed;

	return 0;

failed:
	mfd_remove_devices(chip->dev);
	return ret;
}

static int max8831_remove(struct i2c_client *client)
{
	struct max8831_chip *chip = i2c_get_clientdata(client);
	mfd_remove_devices(chip->dev);
	return 0;
}

static struct i2c_driver max8831_driver = {
	.driver = {
		.name	= "max8831",
		.owner	= THIS_MODULE,
	},
	.probe	= max8831_probe,
	.remove	= max8831_remove,
	.id_table = max8831_id,
};
module_i2c_driver(max8831_driver);

MODULE_AUTHOR("Chaitanya Bandi<bandik@nvidia.com>");
MODULE_DESCRIPTION("MAX8831 MFD driver");
MODULE_LICENSE("GPL v2");
