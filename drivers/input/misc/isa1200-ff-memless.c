/*
 * Copyright (C) 2009 Samsung Electronics
 * Kyungmin Park <kyungmin.park@samsung.com>
 *
 * Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/pwm.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/i2c/isa1200.h>

#define ISA1200_HCTRL0			0x30
#define HCTRL0_MODE_CTRL_BIT		(3)
#define HCTRL0_OVERDRIVE_HIGH_BIT	(5)
#define HCTRL0_OVERDRIVE_EN_BIT		(6)
#define HCTRL0_HAP_EN			(7)
#define HCTRL0_RESET			0x01
#define HCTRL1_RESET			0x4B

#define ISA1200_HCTRL1			0x31
#define HCTRL1_SMART_ENABLE_BIT		(3)
#define HCTRL1_ERM_BIT			(5)
#define HCTRL1_EXT_CLK_ENABLE_BIT	(7)

#define ISA1200_HCTRL5			0x35
#define HCTRL5_VIB_STRT			0xD5
#define HCTRL5_VIB_STOP			0x6B

#define DIVIDER_128			(128)
#define DIVIDER_1024			(1024)
#define DIVIDE_SHIFTER_128		(7)

#define FREQ_22400			(22400)
#define FREQ_172600			(172600)

#define POR_DELAY_USEC			250

struct isa1200_chip {
	const struct isa1200_platform_data *pdata;
	struct i2c_client *client;
	struct input_dev *input_device;
	struct pwm_device *pwm;
	unsigned int period_ns;
	unsigned int state;
	struct work_struct work;
};

static void isa1200_vib_set(struct isa1200_chip *haptic, int enable)
{
	int rc;

	if (enable) {
		if (haptic->pdata->mode_ctrl == PWM_INPUT_MODE) {
			int period_us = haptic->period_ns / NSEC_PER_USEC;
			rc = pwm_config(haptic->pwm,
				(period_us * haptic->pdata->duty) / 100,
				period_us);
			if (rc < 0)
				pr_err("pwm_config fail\n");
			rc = pwm_enable(haptic->pwm);
			if (rc < 0)
				pr_err("pwm_enable fail\n");
		} else if (haptic->pdata->mode_ctrl == PWM_GEN_MODE) {
			rc = i2c_smbus_write_byte_data(haptic->client,
						ISA1200_HCTRL5,
						HCTRL5_VIB_STRT);
			if (rc < 0)
				pr_err("start vibration fail\n");
		}
	} else {
		if (haptic->pdata->mode_ctrl == PWM_INPUT_MODE)
			pwm_disable(haptic->pwm);
		else if (haptic->pdata->mode_ctrl == PWM_GEN_MODE) {
			rc = i2c_smbus_write_byte_data(haptic->client,
						ISA1200_HCTRL5,
						HCTRL5_VIB_STOP);
			if (rc < 0)
				pr_err("stop vibration fail\n");
		}
	}
}

static int isa1200_setup(struct i2c_client *client)
{
	struct isa1200_chip *haptic = i2c_get_clientdata(client);
	int value, temp, rc;

	gpio_set_value_cansleep(haptic->pdata->hap_en_gpio, 0);
	udelay(POR_DELAY_USEC);
	gpio_set_value_cansleep(haptic->pdata->hap_en_gpio, 1);

	value =	(haptic->pdata->smart_en << HCTRL1_SMART_ENABLE_BIT) |
		(haptic->pdata->is_erm << HCTRL1_ERM_BIT) |
		(haptic->pdata->ext_clk_en << HCTRL1_EXT_CLK_ENABLE_BIT);

	rc = i2c_smbus_write_byte_data(client, ISA1200_HCTRL1, value);
	if (rc < 0) {
		pr_err("i2c write failure\n");
		return rc;
	}

	if (haptic->pdata->mode_ctrl == PWM_GEN_MODE) {
		temp = haptic->pdata->pwm_fd.pwm_div;
		if (temp < DIVIDER_128 || temp > DIVIDER_1024 ||
					temp % DIVIDER_128) {
			pr_err("Invalid divider\n");
			rc = -EINVAL;
			goto reset_hctrl1;
		}
		value = ((temp >> DIVIDE_SHIFTER_128) - 1);
	} else if (haptic->pdata->mode_ctrl == PWM_INPUT_MODE) {
		temp = haptic->pdata->pwm_fd.pwm_freq;
		if (temp < FREQ_22400 || temp > FREQ_172600 ||
					temp % FREQ_22400) {
			pr_err("Invalid frequency\n");
			rc = -EINVAL;
			goto reset_hctrl1;
		}
		value = ((temp / FREQ_22400) - 1);
		haptic->period_ns = NSEC_PER_SEC / temp;
	}
	value |= (haptic->pdata->mode_ctrl << HCTRL0_MODE_CTRL_BIT) |
		(haptic->pdata->overdrive_high << HCTRL0_OVERDRIVE_HIGH_BIT) |
		(haptic->pdata->overdrive_en << HCTRL0_OVERDRIVE_EN_BIT) |
		(haptic->pdata->chip_en << HCTRL0_HAP_EN);

	rc = i2c_smbus_write_byte_data(client, ISA1200_HCTRL0, value);
	if (rc < 0) {
		pr_err("i2c write failure\n");
		goto reset_hctrl1;
	}

	return 0;

reset_hctrl1:
	i2c_smbus_write_byte_data(client, ISA1200_HCTRL1,
				HCTRL1_RESET);
	return rc;
}

static void isa1200_worker(struct work_struct *work)
{
	struct isa1200_chip *haptic;

	haptic = container_of(work, struct isa1200_chip, work);
	isa1200_vib_set(haptic, !!haptic->state);
}

static int isa1200_play_effect(struct input_dev *dev, void *data,
				struct ff_effect *effect)
{
	struct isa1200_chip *haptic = input_get_drvdata(dev);

	/* support basic vibration */
	haptic->state = effect->u.rumble.strong_magnitude >> 8;
	if (!haptic->state)
		haptic->state = effect->u.rumble.weak_magnitude >> 9;

	schedule_work(&haptic->work);

	return 0;
}

#ifdef CONFIG_PM
static int isa1200_suspend(struct device *dev)
{
	struct isa1200_chip *haptic = dev_get_drvdata(dev);
	int rc;

	cancel_work_sync(&haptic->work);
	/* turn-off current vibration */
	isa1200_vib_set(haptic, 0);

	if (haptic->pdata->power_on) {
		rc = haptic->pdata->power_on(0);
		if (rc) {
			pr_err("power-down failed\n");
			return rc;
		}
	}

	return 0;
}

static int isa1200_resume(struct device *dev)
{
	struct isa1200_chip *haptic = dev_get_drvdata(dev);
	int rc;

	if (haptic->pdata->power_on) {
		rc = haptic->pdata->power_on(1);
		if (rc) {
			pr_err("power-up failed\n");
			return rc;
		}
	}

	isa1200_setup(haptic->client);
	return 0;
}
#else
#define isa1200_suspend		NULL
#define isa1200_resume		NULL
#endif

static int isa1200_open(struct input_dev *dev)
{
	struct isa1200_chip *haptic = input_get_drvdata(dev);
	int rc;

	/* device setup */
	if (haptic->pdata->dev_setup) {
		rc = haptic->pdata->dev_setup(true);
		if (rc < 0) {
			pr_err("setup failed!\n");
			return rc;
		}
	}

	/* power on */
	if (haptic->pdata->power_on) {
		rc = haptic->pdata->power_on(true);
		if (rc < 0) {
			pr_err("power failed\n");
			goto err_setup;
		}
	}

	/* request gpio */
	rc = gpio_is_valid(haptic->pdata->hap_en_gpio);
	if (rc) {
		rc = gpio_request(haptic->pdata->hap_en_gpio, "haptic_gpio");
		if (rc) {
			pr_err("gpio %d request failed\n",
					haptic->pdata->hap_en_gpio);
			goto err_power_on;
		}
	} else {
		pr_err("Invalid gpio %d\n",
					haptic->pdata->hap_en_gpio);
		goto err_power_on;
	}

	rc = gpio_direction_output(haptic->pdata->hap_en_gpio, 0);
	if (rc) {
		pr_err("gpio %d set direction failed\n",
					haptic->pdata->hap_en_gpio);
		goto err_gpio_free;
	}

	/* setup registers */
	rc = isa1200_setup(haptic->client);
	if (rc < 0) {
		pr_err("setup fail %d\n", rc);
		goto err_gpio_free;
	}

	if (haptic->pdata->mode_ctrl == PWM_INPUT_MODE) {
		haptic->pwm = pwm_request(haptic->pdata->pwm_ch_id,
				haptic->client->driver->id_table->name);
		if (IS_ERR(haptic->pwm)) {
			pr_err("pwm request failed\n");
			rc = PTR_ERR(haptic->pwm);
			goto err_reset_hctrl0;
		}
	}

	/* init workqeueue */
	INIT_WORK(&haptic->work, isa1200_worker);
	return 0;

err_reset_hctrl0:
	i2c_smbus_write_byte_data(haptic->client, ISA1200_HCTRL0,
					HCTRL0_RESET);
err_gpio_free:
	gpio_free(haptic->pdata->hap_en_gpio);
err_power_on:
	if (haptic->pdata->power_on)
		haptic->pdata->power_on(0);
err_setup:
	if (haptic->pdata->dev_setup)
		haptic->pdata->dev_setup(false);

	return rc;
}

static void isa1200_close(struct input_dev *dev)
{
	struct isa1200_chip *haptic = input_get_drvdata(dev);

	/* turn-off current vibration */
	isa1200_vib_set(haptic, 0);

	if (haptic->pdata->mode_ctrl == PWM_INPUT_MODE)
		pwm_free(haptic->pwm);

	gpio_free(haptic->pdata->hap_en_gpio);

	/* reset hardware registers */
	i2c_smbus_write_byte_data(haptic->client, ISA1200_HCTRL0,
				HCTRL0_RESET);
	i2c_smbus_write_byte_data(haptic->client, ISA1200_HCTRL1,
				HCTRL1_RESET);

	if (haptic->pdata->dev_setup)
		haptic->pdata->dev_setup(false);

	/* power-off the chip */
	if (haptic->pdata->power_on)
		haptic->pdata->power_on(0);
}

static int __devinit isa1200_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct isa1200_chip *haptic;
	int rc;

	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("i2c is not supported\n");
		return -EIO;
	}

	if (!client->dev.platform_data) {
		pr_err("pdata is not avaiable\n");
		return -EINVAL;
	}

	haptic = kzalloc(sizeof(struct isa1200_chip), GFP_KERNEL);
	if (!haptic) {
		pr_err("no memory\n");
		return -ENOMEM;
	}

	haptic->pdata = client->dev.platform_data;
	haptic->client = client;

	i2c_set_clientdata(client, haptic);

	haptic->input_device = input_allocate_device();
	if (!haptic->input_device) {
		pr_err("input device alloc failed\n");
		rc = -ENOMEM;
		goto err_mem_alloc;
	}

	input_set_drvdata(haptic->input_device, haptic);
	haptic->input_device->name = haptic->pdata->name ? :
					"isa1200-ff-memless";

	haptic->input_device->dev.parent = &client->dev;

	input_set_capability(haptic->input_device, EV_FF, FF_RUMBLE);

	haptic->input_device->open = isa1200_open;
	haptic->input_device->close = isa1200_close;

	rc = input_ff_create_memless(haptic->input_device, NULL,
					isa1200_play_effect);
	if (rc < 0) {
		pr_err("unable to register with ff\n");
		goto err_free_dev;
	}

	rc = input_register_device(haptic->input_device);
	if (rc < 0) {
		pr_err("unable to register input device\n");
		goto err_ff_destroy;
	}

	return 0;

err_ff_destroy:
	input_ff_destroy(haptic->input_device);
err_free_dev:
	input_free_device(haptic->input_device);
err_mem_alloc:
	kfree(haptic);
	return rc;
}

static int __devexit isa1200_remove(struct i2c_client *client)
{
	struct isa1200_chip *haptic = i2c_get_clientdata(client);

	input_unregister_device(haptic->input_device);
	kfree(haptic);

	return 0;
}

static const struct i2c_device_id isa1200_id_table[] = {
	{"isa1200_1", 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, isa1200_id_table);

static const struct dev_pm_ops isa1200_pm_ops = {
	.suspend = isa1200_suspend,
	.resume = isa1200_resume,
};

static struct i2c_driver isa1200_driver = {
	.driver = {
		.name = "isa1200-ff-memless",
		.owner = THIS_MODULE,
		.pm = &isa1200_pm_ops,
	},
	.probe = isa1200_probe,
	.remove = __devexit_p(isa1200_remove),
	.id_table = isa1200_id_table,
};

static int __init isa1200_init(void)
{
	return i2c_add_driver(&isa1200_driver);
}
module_init(isa1200_init);

static void __exit isa1200_exit(void)
{
	i2c_del_driver(&isa1200_driver);
}
module_exit(isa1200_exit);

MODULE_DESCRIPTION("isa1200 based vibrator chip driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Kyungmin Park <kyungmin.park@samsung.com>");
