/*
* Simple driver for Texas Instruments LM3644 LED Flash driver chip
* Copyright (C) 2017 Xiaomi Corp.
 * Copyright (C) 2018 XiaoMi, Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
*/
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>
#include <linux/qpnp/pwm.h>
#include <linux/leds-lm3644.h>


#define REG_ENABLE			(0x1)
#define REG_IVFM_MODE			(0x2)
#define REG_LED1_FLASH_BRIGHTNESS	(0x3)
#define REG_LED2_FLASH_BRIGHTNESS	(0x4)
#define REG_LED1_TORCH_BRIGHTNESS	(0x5)
#define REG_LED2_TORCH_BRIGHTNESS	(0x6)
#define REG_BOOST_CONF			(0x7)
#define REG_TIMING_CONF			(0x8)
#define REG_TEMP			(0x9)
#define REG_FLAG1			(0xA)
#define REG_FLAG2			(0xB)
#define REG_DEVICE_ID			(0xC)
#define REG_LAST_FLASH			(0xD)
#define REG_MAX				(0xD)

#define LM3644_ID			(0x02)
#define LM3644TT_ID			(0x04)

/* REG_ENABLE (0x01 */
#define TX_PIN_ENABLE_SHIFT		(7)
#define STROBE_TYPE_SHIFT		(6)
#define STROBE_EN_SHIFT			(5)
#define TORCH_PIN_ENABLE_SHIFT		(4)
#define MODE_BITS_SHIFT			(2)
#define LED2_ENABLE_SHIFT		(1)
#define LED1_ENABLE_SHIFT		(0)

#define STROBE_TYPE_LEVEL_TRIGGER	(0)
#define STROBE_TYPE_EDGE_TRIGGER	(1)

/* REG_LED1_BRIGHTNESS */
#define LED2_CURRENT_EQUAL		(0x80)

#define LM3644_DEFAULT_PERIOD_US	2500000
#define LM3644_DEFAULT_DUTY_US		2500

#define NSECS_PER_USEC			1000UL

#define LM3644_MAX_BRIGHTNESS_VALUE	0x7F

enum lm3644_mode {
	MODES_STANDBY = 0,
	MODES_IR,
	MODES_TORCH,
	MODES_FLASH
};

enum lm3644_pinctrl_state {
	STATE_ACTIVE = 0,
	STATE_ACTIVE_WITH_PWM,
	STATE_SUSPEND
};

struct lm3644_chip_data {
	struct device *dev;

	struct led_classdev cdev_torch;
	struct led_classdev cdev_ir;

	u8 br_torch;
	u8 br_ir;

	struct lm3644_platform_data *pdata;
	struct regmap *regmap;
	struct mutex lock;
	struct timer_list ir_stop_timer;
	struct work_struct ir_stop_work;

	unsigned int chip_id;
	unsigned int last_flag1;
	unsigned int last_flag2;

	struct pwm_device *pwm;

	/* pinctrl */
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_active_pwm; /* use simulative PWM */
	struct pinctrl_state *gpio_state_suspend;
};

/* chip initialize */
static int lm3644_chip_init(struct lm3644_chip_data *chip)
{
	unsigned int chip_id;
	int ret;

	/* read device id */
	ret = regmap_read(chip->regmap, REG_DEVICE_ID, &chip_id);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read REG_DEVICE_ID register\n");
		return ret;
	}

	if (chip_id != LM3644_ID && chip_id != LM3644TT_ID) {
		dev_err(chip->dev, "Invalid device id 0x%02X\n", chip_id);
		return -ENODEV;
	}

	chip->chip_id = chip_id;
	return ret;
}

/* chip control */
static int lm3644_control(struct lm3644_chip_data *chip,
			  u8 brightness, enum lm3644_mode opmode)
{
	int ret;
	int val;

	ret = regmap_read(chip->regmap, REG_FLAG1, &chip->last_flag1);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read REG_FLAG1 Register\n");
		goto out;
	}

	ret = regmap_read(chip->regmap, REG_FLAG2, &chip->last_flag2);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read REG_FLAG2 Register\n");
		goto out;
	}

	if (chip->last_flag1 || chip->last_flag2)
		dev_info(chip->dev, "Last FLAG1 is 0x%02X, FLAG2 is 0x%02X\n",
			chip->last_flag1, chip->last_flag2);

	dev_dbg(chip->dev, "[%s]: brightness = %u, opmode = %d\n",
		__func__, brightness, opmode);

	/* brightness 0 means off-state */
	if (!brightness)
		opmode = MODES_STANDBY;

	if (opmode == MODES_FLASH) {
		dev_err(chip->dev, "Flash mode not supported\n");
		opmode = MODES_STANDBY;
	}

	if (opmode != MODES_IR) {
		if (chip->pdata->use_simulative_pwm &&
				chip->pwm != NULL) {
			pwm_disable(chip->pwm);
			dev_dbg(chip->dev, "Simulative PWM disabled\n");
		}

		cancel_work(&chip->ir_stop_work);
		del_timer(&chip->ir_stop_timer);
	}

	if (opmode != MODES_STANDBY)
		val = (opmode << MODE_BITS_SHIFT) | (1 << LED2_ENABLE_SHIFT) |
			(1 << LED1_ENABLE_SHIFT);

	switch (opmode) {
	case MODES_TORCH:
		if (gpio_is_valid(chip->pdata->torch_gpio))
			val |= 1 << TORCH_PIN_ENABLE_SHIFT;
		ret = regmap_write(chip->regmap, REG_ENABLE, val);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to write REG_ENABLE register\n");
			goto out;
		}

		ret = regmap_write(chip->regmap, REG_LED1_TORCH_BRIGHTNESS,
				brightness | LED2_CURRENT_EQUAL);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to write REG_LED1_TORCH_BRIGHTNESS register\n");
			goto out;
		}

		ret = regmap_write(chip->regmap, REG_LED2_TORCH_BRIGHTNESS,
				brightness);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to write REG_LED2_TORCH_BRIGHTNESS register\n");
			goto out;
		}
		break;

	case MODES_IR:
		/* Enable STORBE_EN bit */
		val |= 1 << STROBE_EN_SHIFT;

		ret = regmap_write(chip->regmap, REG_ENABLE, val);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to write REG_ENABLE register\n");
			goto out;
		}

		ret = regmap_write(chip->regmap, REG_LED1_FLASH_BRIGHTNESS,
				brightness | LED2_CURRENT_EQUAL);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to write REG_LED1_TORCH_BRIGHTNESS register\n");
			goto out;
		}

		ret = regmap_write(chip->regmap, REG_LED2_FLASH_BRIGHTNESS,
				brightness);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to write REG_LED2_TORCH_BRIGHTNESS register\n");
			goto out;
		}

		if (chip->pdata->use_simulative_pwm && chip->pwm != NULL) {
			ret = pwm_enable(chip->pwm);
			if (ret < 0) {
				dev_err(chip->dev, "Failed to enable PWM device\n");
				goto out;

			} else
				dev_err(chip->dev, "Simulative PWM enabled\n");
		}

		if (chip->pdata->ir_prot_time > 0)
			mod_timer(&chip->ir_stop_timer,
				  jiffies + msecs_to_jiffies(chip->pdata->ir_prot_time));
		break;

	case MODES_STANDBY:
		ret = regmap_write(chip->regmap, REG_ENABLE, 0);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to write REG_ENABLE register\n");
			goto out;
		}
		break;

	default:
		return ret;
	}

out:
	return ret;
}

/* torch mode */
static int lm3644_torch_brightness_set(struct led_classdev *cdev,
					enum led_brightness brightness)
{
	struct lm3644_chip_data *chip =
	    container_of(cdev, struct lm3644_chip_data, cdev_torch);
	int ret;

	mutex_lock(&chip->lock);
	chip->br_torch = brightness;
	ret = lm3644_control(chip, chip->br_torch, MODES_TORCH);
	mutex_unlock(&chip->lock);
	return ret;
}

/* ir mode */
static int lm3644_ir_brightness_set(struct led_classdev *cdev,
					enum led_brightness brightness)
{
	struct lm3644_chip_data *chip =
	    container_of(cdev, struct lm3644_chip_data, cdev_ir);
	int ret;

	mutex_lock(&chip->lock);
	chip->br_ir = brightness;
	ret = lm3644_control(chip, chip->br_ir, MODES_IR);
	mutex_unlock(&chip->lock);
	return ret;
}

static enum led_brightness lm3644_ir_brightness_get(struct led_classdev *cdev)
{
	struct lm3644_chip_data *chip =
	    container_of(cdev, struct lm3644_chip_data, cdev_ir);

	return chip->br_ir;
}

static int lm3644_init_pinctrl(struct lm3644_chip_data *chip)
{
	struct device *dev = chip->dev;

	chip->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(chip->pinctrl)) {
		dev_err(dev, "Unable to acquire pinctrl\n");
		chip->pinctrl = NULL;
		return 0;
	}

	chip->gpio_state_active =
		pinctrl_lookup_state(chip->pinctrl, "lm3644_led_active");
	if (IS_ERR_OR_NULL(chip->gpio_state_active)) {
		dev_err(dev, "Cannot lookup LED active state\n");
		devm_pinctrl_put(chip->pinctrl);
		chip->pinctrl = NULL;
		return PTR_ERR(chip->gpio_state_active);
	}

	chip->gpio_state_active_pwm =
		pinctrl_lookup_state(chip->pinctrl, "lm3644_led_active_pwm");
	if (IS_ERR_OR_NULL(chip->gpio_state_active_pwm)) {
		dev_err(dev, "Cannot lookup LED active with simulative PWM state\n");
		devm_pinctrl_put(chip->pinctrl);
		chip->pinctrl = NULL;
		return PTR_ERR(chip->gpio_state_active_pwm);
	}

	chip->gpio_state_suspend =
		pinctrl_lookup_state(chip->pinctrl, "lm3644_led_suspend");
	if (IS_ERR_OR_NULL(chip->gpio_state_suspend)) {
		dev_err(dev, "Cannot lookup LED suspend state\n");
		devm_pinctrl_put(chip->pinctrl);
		chip->pinctrl = NULL;
		return PTR_ERR(chip->gpio_state_suspend);
	}

	return 0;
}

static int lm3644_pinctrl_select(struct lm3644_chip_data *chip,
					enum lm3644_pinctrl_state state)
{
	int ret = 0;
	struct pinctrl_state *pins_state;
	struct device *dev = chip->dev;

	switch (state) {
	case STATE_ACTIVE:
		pins_state = chip->gpio_state_active;
		break;
	case STATE_ACTIVE_WITH_PWM:
		pins_state = chip->gpio_state_active_pwm;
		break;
	case STATE_SUSPEND:
		pins_state = chip->gpio_state_suspend;
		break;
	default:
		dev_err(chip->dev, "Invalid pinctrl state %d\n", state);
		return -ENODEV;
	}

	ret = pinctrl_select_state(chip->pinctrl, pins_state);
	if (ret < 0)
		dev_err(dev, "Failed to select pins state %d\n", state);

	return ret;
}

static ssize_t lm3644_pwm_period_store(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3644_chip_data *chip =
		container_of(led_cdev, struct lm3644_chip_data, cdev_ir);
	int ret;
	unsigned int val;

	ret = kstrtou32(buff, 10, &val);
	if (ret < 0)
		return ret;

	chip->pdata->pwm_period_us = val;

	mutex_lock(&chip->lock);
	ret = pwm_config(chip->pwm, chip->pdata->pwm_duty_us * NSECS_PER_USEC,
		chip->pdata->pwm_period_us * NSECS_PER_USEC);
	if (ret < 0) {
		dev_err(chip->dev, "PWM config failed: %d\n", ret);
		mutex_unlock(&chip->lock);
		return ret;
	}
	mutex_unlock(&chip->lock);

	return count;
}

static ssize_t lm3644_pwm_period_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3644_chip_data *chip =
		container_of(led_cdev, struct lm3644_chip_data, cdev_ir);

	return snprintf(buf, PAGE_SIZE, "%u\n", chip->pdata->pwm_period_us);
}

static ssize_t lm3644_pwm_duty_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buff, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3644_chip_data *chip =
		container_of(led_cdev, struct lm3644_chip_data, cdev_ir);
	int ret;
	unsigned int val;

	ret = kstrtou32(buff, 10, &val);
	if (ret < 0)
		return ret;

	chip->pdata->pwm_duty_us = val;

	mutex_lock(&chip->lock);
	ret = pwm_config(chip->pwm, chip->pdata->pwm_duty_us * NSECS_PER_USEC,
		chip->pdata->pwm_period_us * NSECS_PER_USEC);
	if (ret < 0) {
		dev_err(chip->dev, "PWM config failed: %d\n", ret);
		mutex_unlock(&chip->lock);
		return ret;
	}
	mutex_unlock(&chip->lock);

	return count;
}

static ssize_t lm3644_pwm_duty_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3644_chip_data *chip =
		container_of(led_cdev, struct lm3644_chip_data, cdev_ir);

	return snprintf(buf, PAGE_SIZE, "%u\n", chip->pdata->pwm_duty_us);
}

static ssize_t lm3644_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3644_chip_data *chip =
		container_of(led_cdev, struct lm3644_chip_data, cdev_ir);

	return snprintf(buf, PAGE_SIZE, "%s\n",
		(chip->chip_id == LM3644_ID) ? "LM3644" : "LM3644TT");
}


static DEVICE_ATTR(pwm_period, S_IRUGO | S_IWUSR,
	lm3644_pwm_period_show, lm3644_pwm_period_store);
static DEVICE_ATTR(pwm_duty, S_IRUGO | S_IWUSR,
	lm3644_pwm_duty_show, lm3644_pwm_duty_store);
static DEVICE_ATTR(id, S_IRUGO,
	lm3644_id_show, NULL);

static struct attribute *lm3644_ir_attrs[] = {
	&dev_attr_pwm_period.attr,
	&dev_attr_pwm_duty.attr,
	&dev_attr_id.attr,
	NULL
};
ATTRIBUTE_GROUPS(lm3644_ir);

#ifdef CONFIG_OF
static struct lm3644_platform_data *lm3644_parse_dt(struct i2c_client *client)
{
	struct lm3644_platform_data *pdata;
	struct device_node *np = client->dev.of_node;
	int ret = 0;

	if (!np)
		return ERR_PTR(-ENOENT);

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->hwen_gpio = of_get_named_gpio(np, "lm3644,hwen-gpio", 0);
	pdata->torch_gpio = of_get_named_gpio(np, "lm3644,torch-gpio", 0);
	pdata->tx_gpio = of_get_named_gpio(np, "lm3644,tx-gpio", 0);

	pdata->use_simulative_pwm = of_property_read_bool(np,
			"lm3644,use-simulative-pwm");

	ret = of_property_read_s32(np, "lm3644,ir-prot-time",
		&pdata->ir_prot_time);
	if (ret < 0) {
		dev_info(&client->dev, "No protect time specified for IR mode\n");
		pdata->ir_prot_time = -1;
	}

	if (pdata->use_simulative_pwm) {
		ret = of_property_read_u32(np,
			"lm3644,period-us", &pdata->pwm_period_us);
		if (ret < 0) {
			dev_err(&client->dev, "Could not find PWM period, use default value\n");
			pdata->pwm_period_us = LM3644_DEFAULT_PERIOD_US;
		}

		ret = of_property_read_u32(np,
			"lm3644,duty-us", &pdata->pwm_duty_us);
		if (ret < 0) {
			dev_err(&client->dev, "Could not find PWM duty, use default value\n");
			pdata->pwm_period_us = LM3644_DEFAULT_DUTY_US;
		}
	}

	return pdata;
}
#endif

static void lm3644_ir_stop_work(struct work_struct *work)
{
	struct lm3644_chip_data *chip =
		container_of(work, struct lm3644_chip_data, ir_stop_work);

	lm3644_ir_brightness_set(&chip->cdev_ir, LED_OFF);
}

static void lm3644_ir_stop_timer(unsigned long data)
{
	struct lm3644_chip_data *chip = (struct lm3644_chip_data *)data;

	dev_err(chip->dev, "Force shutdown IR LED after %d msecs\n",
		chip->pdata->ir_prot_time);
	schedule_work(&chip->ir_stop_work);
}

static const struct regmap_config lm3644_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REG_MAX,
};

static int lm3644_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct lm3644_platform_data *pdata;
	struct lm3644_chip_data *chip;
	enum lm3644_pinctrl_state pin_state = STATE_ACTIVE;

	int err;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c functionality check fail.\n");
		return -EOPNOTSUPP;
	}

	chip = devm_kzalloc(&client->dev,
			sizeof(struct lm3644_chip_data), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

#ifdef CONFIG_OF
	if (client->dev.of_node) {
		pdata = lm3644_parse_dt(client);
		if (IS_ERR(pdata)) {
			dev_err(&client->dev, "Failed to parse devicetree\n");
			return -ENODEV;
		}
	} else
		pdata = dev_get_platdata(&client->dev);
#else
	pdata = dev_get_platdata(&client->dev);
#endif

	if (pdata == NULL) {
		dev_err(&client->dev, "needs platform Data.\n");
		return -ENODATA;
	}

	chip->dev = &client->dev;
	chip->pdata = pdata;

	chip->regmap = devm_regmap_init_i2c(client, &lm3644_regmap);
	if (IS_ERR(chip->regmap)) {
		err = PTR_ERR(chip->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			err);
		return err;
	}

#ifdef CONFIG_OF
	/* Simulative PWM output */
	if (pdata->use_simulative_pwm) {
		chip->pwm = of_pwm_get(client->dev.of_node, NULL);
		if (IS_ERR(chip->pwm)) {
			err = PTR_ERR(chip->pwm);
			dev_err(&client->dev, "Failed to get PWM device: %d\n",
				err);
			chip->pwm = NULL;
		}

		err = pwm_config(chip->pwm, pdata->pwm_duty_us * NSECS_PER_USEC,
			pdata->pwm_period_us * NSECS_PER_USEC);
		if (err < 0) {
			dev_err(&client->dev, "PWM config failed: %d\n", err);
			goto err_free_pwm;
		}

		pin_state = STATE_ACTIVE_WITH_PWM;
	}
#endif

	err = lm3644_init_pinctrl(chip);
	if (err) {
		dev_err(&client->dev, "Failed to initialize pinctrl\n");
		goto err_free_pwm;
	} else {
		if (chip->pinctrl) {
			/* Target support pinctrl */
			err = lm3644_pinctrl_select(chip, pin_state);
			if (err) {
				dev_err(&client->dev, "Failed to select pinctrl\n");
				goto err_free_pwm;
			}
		}
	}

	if (gpio_is_valid(pdata->hwen_gpio)) {
		err = gpio_request(pdata->hwen_gpio, "lm3644_hwen");
		if (err) {
			dev_err(&client->dev, "Unable to request gpio[%d]\n",
					pdata->hwen_gpio);
			goto err_pinctrl_sleep;
		}

		err = gpio_direction_output(pdata->hwen_gpio, 1);
		if (err) {
			dev_err(&client->dev, "Unable to set hwen to output\n");
			goto err_pinctrl_sleep;
		}
		msleep(10);
	}

	if (gpio_is_valid(pdata->tx_gpio)) {
		err = gpio_request(pdata->tx_gpio, "lm3644_tx");
		if (err) {
			dev_err(&client->dev, "Unable to request gpio[%d]\n",
					pdata->tx_gpio);
			goto err_free_hwen_gpio;
		}

		err = gpio_direction_output(pdata->tx_gpio, 0);
		if (err) {
			dev_err(&client->dev, "Unable to set tx_gpio to output\n");
			goto err_free_hwen_gpio;
		}
	}

	if (gpio_is_valid(pdata->torch_gpio)) {
		err = gpio_request(pdata->torch_gpio, "lm3644_torch");
		if (err) {
			dev_err(&client->dev, "Unable to request gpio[%d]\n",
				pdata->torch_gpio);
			goto err_free_tx_gpio;
		}

		err = gpio_direction_output(pdata->torch_gpio, 0);
		if (err) {
			dev_err(&client->dev, "Unable to set torch_gpio to output\n");
			goto err_free_hwen_gpio;
		}
	}

	mutex_init(&chip->lock);
	i2c_set_clientdata(client, chip);

	INIT_WORK(&chip->ir_stop_work, lm3644_ir_stop_work);
	setup_timer(&chip->ir_stop_timer, lm3644_ir_stop_timer,
		(unsigned long)chip);

	err = lm3644_chip_init(chip);
	if (err < 0)
		goto err_free_torch_gpio;

	/* torch mode */
	chip->cdev_torch.name = "ir_torch";
	chip->cdev_torch.max_brightness = LM3644_MAX_BRIGHTNESS_VALUE;
	chip->cdev_torch.brightness_set_blocking = lm3644_torch_brightness_set;
	err = led_classdev_register((struct device *)
		&client->dev, &chip->cdev_torch);
	if (err < 0) {
		dev_err(chip->dev, "Failed to register ir torch\n");
		goto err_free_torch_gpio;
	}

	/* ir mode */
	chip->cdev_ir.name = "ir";
	chip->cdev_ir.max_brightness = LM3644_MAX_BRIGHTNESS_VALUE;
	chip->cdev_ir.brightness_set_blocking = lm3644_ir_brightness_set;
	chip->cdev_ir.brightness_get = lm3644_ir_brightness_get;
	if (pdata->use_simulative_pwm)
		chip->cdev_ir.groups = lm3644_ir_groups;
	err = led_classdev_register((struct device *)
		&client->dev, &chip->cdev_ir);
	if (err < 0) {
		dev_err(chip->dev, "Failed to register ir\n");
		goto err_free_torch_classdev;
	}

	dev_info(&client->dev, "LM3644 is initialized\n");
	return 0;

err_free_pwm:
	pwm_put(chip->pwm);
err_free_torch_classdev:
	led_classdev_unregister(&chip->cdev_torch);
err_free_torch_gpio:
	if (gpio_is_valid(pdata->torch_gpio))
		gpio_free(pdata->torch_gpio);
err_free_tx_gpio:
	if (gpio_is_valid(pdata->tx_gpio))
		gpio_free(pdata->tx_gpio);
err_free_hwen_gpio:
	if (gpio_is_valid(pdata->hwen_gpio)) {
		/* Pull HWEN to ground to reduce power */
		gpio_set_value(pdata->hwen_gpio, 0);
		gpio_free(pdata->hwen_gpio);
	}
err_pinctrl_sleep:
	if (chip->pinctrl) {
		if (lm3644_pinctrl_select(chip, STATE_SUSPEND) < 0)
			dev_err(&client->dev, "Failed to select suspend pinstate\n");
		devm_pinctrl_put(chip->pinctrl);
	}

	return err;
}

static int lm3644_remove(struct i2c_client *client)
{
	struct lm3644_chip_data *chip = i2c_get_clientdata(client);

	cancel_work(&chip->ir_stop_work);
	del_timer(&chip->ir_stop_timer);
	pwm_put(chip->pwm);
	led_classdev_unregister(&chip->cdev_torch);
	led_classdev_unregister(&chip->cdev_ir);
	regmap_write(chip->regmap, REG_ENABLE, 0);
	if (gpio_is_valid(chip->pdata->hwen_gpio)) {
		gpio_set_value(chip->pdata->hwen_gpio, 0);
		gpio_free(chip->pdata->hwen_gpio);
	}
	if (gpio_is_valid(chip->pdata->tx_gpio))
		gpio_free(chip->pdata->tx_gpio);
	if (gpio_is_valid(chip->pdata->torch_gpio))
		gpio_free(chip->pdata->torch_gpio);
	lm3644_pinctrl_select(chip, STATE_SUSPEND);
	return 0;
}

static const struct i2c_device_id lm3644_id[] = {
	{LM3644_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, lm3644_id);

static struct i2c_driver lm3644_i2c_driver = {
	.driver = {
		.name = LM3644_NAME,
	},
	.probe = lm3644_probe,
	.remove = lm3644_remove,
	.id_table = lm3644_id,
};

module_i2c_driver(lm3644_i2c_driver);

MODULE_DESCRIPTION("Texas Instruments Flash Lighting driver for LM3644");
MODULE_AUTHOR("Tao, Jun <taojun@xiaomi.com>");
MODULE_LICENSE("GPL v2");
