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
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/interrupt.h>
#include <uapi/linux/leds-lm3644.h>


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

/* REG_ENABLE */
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

/* REG_BOOST_CONF */
#define PASS_MODE_SHIFT			(2)

#define DRV_NAME "flood"
#define LM3644_CLASS_NAME "lm3644"

static DECLARE_WAIT_QUEUE_HEAD(lm3644_poll_wait_queue);

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
	struct cdev cdev;
	struct class *chr_class;
	struct device *chr_dev;

	dev_t dev_num;

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
	int ito_irq;

	struct pwm_device *pwm;

	atomic_t ito_exception;

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

static int lm3644_enable_pass_mode(struct lm3644_chip_data *chip)
{
	unsigned int val;
	int ret;

	ret = regmap_read(chip->regmap, REG_BOOST_CONF, &val);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read REG_BOOST_CONF register\n");
		return ret;
	}

	val |= (1 << PASS_MODE_SHIFT);
	ret = regmap_write(chip->regmap, REG_BOOST_CONF, val);
	if (ret < 0)
		dev_err(chip->dev, "Failed to write REG_BOOST_CONF register\n");

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

static int lm3644_ir_release(struct inode *node, struct file *filp)
{
	struct lm3644_chip_data *chip =
		container_of(node->i_cdev, struct lm3644_chip_data, cdev);

	disable_irq_nosync(chip->ito_irq);

	return 0;
}

static void lm3644_ir_stop_work(struct work_struct *work)
{
	struct lm3644_chip_data *chip =
		container_of(work, struct lm3644_chip_data, ir_stop_work);

	lm3644_ir_brightness_set(&chip->cdev_ir, LED_OFF);
}

static void lm3644_ir_stop_timer(unsigned long data)
{
	struct lm3644_chip_data *chip = (struct lm3644_chip_data*)data;

	dev_err(chip->dev, "Force shutdown IR LED after %d msecs\n",
		chip->pdata->ir_prot_time);
	schedule_work(&chip->ir_stop_work);
}

static irqreturn_t lm3644_ito_irq(int ic_irq, void *dev_id)
{
	struct lm3644_chip_data *chip = dev_id;

	dev_err(chip->dev, "ITO EXCEPTION!\n");
	atomic_set(&chip->ito_exception, 1);

	wake_up(&lm3644_poll_wait_queue);
	return IRQ_HANDLED;
}

static int lm3644_ir_open(struct inode *node, struct file *filp)
{
	struct lm3644_chip_data *chip =
		container_of(node->i_cdev, struct lm3644_chip_data, cdev);

	filp->private_data = chip;
	enable_irq(chip->ito_irq);
	return 0;
}

static ssize_t lm3644_ir_write(struct file *filp, const char *buf, size_t len, loff_t *fseek)
{
	int ret = 0;

	return ret;
}

static ssize_t lm3644_ir_read(struct file *filp, char *buf, size_t len, loff_t *fseek)
{
	int ret = 0;
	int data = 0;
	struct lm3644_chip_data *chip = filp->private_data;

	data = atomic_read(&chip->ito_exception);
	if (copy_to_user(buf,&data,sizeof(data)) != 0)
		dev_err(chip->dev, "copy to user failed!\n");

	return ret;
}

static unsigned int lm3644_poll(struct file *filp, poll_table *wait)
{
	int ret = 0;
	unsigned int mask = 0;
	struct lm3644_chip_data *chip = filp->private_data;

	dev_dbg(chip->dev, "Poll enter\n");

	poll_wait(filp, &lm3644_poll_wait_queue, wait);
	if (atomic_read(&chip->ito_exception)) {
		mask = POLLIN | POLLRDNORM;

		mutex_lock(&chip->lock);
		ret = lm3644_control(chip, LED_OFF, MODES_STANDBY);
		mutex_unlock(&chip->lock);

		lm3644_ir_stop_timer((unsigned long)chip);

		if (gpio_is_valid(chip->pdata->hwen_gpio)) {
			ret = gpio_direction_output(chip->pdata->hwen_gpio, 0);
			if (ret)
				dev_err(chip->dev, "Unable to shutdown flood\n");
		}
	}

	return mask;
}

static int lm3644_ir_init(struct lm3644_chip_data *chip)
{
	int ret = 0;

	ret = lm3644_control(chip, LED_OFF, MODES_IR);
	if (ret < 0)
		dev_err(chip->dev, "Init failed, %d\n", ret);

	return ret;
}

static int lm3644_ir_deinit(struct lm3644_chip_data *chip)
{
	int ret = 0;

	ret = lm3644_control(chip, LED_OFF, MODES_STANDBY);
	if (ret < 0)
		dev_err(chip->dev, "Deinit failed, %d\n", ret);

	return ret;
}

static int lm3644_ir_set_data(struct lm3644_chip_data *chip, lm3644_data params)
{
	int ret = 0;
	uint8_t brightness;

	if (SET_BRIGHTNESS_EVENT == params.event) {
		brightness = (uint8_t)params.data;
		if (brightness > 0x3F) {
			dev_err(chip->dev, "brightness %d is higher then the max value 0x3F, set to 0x3F\n", brightness);
			brightness = 0x3F;
		}
		ret = lm3644_control(chip, brightness, MODES_IR);
		if (ret < 0)
			dev_err(chip->dev, "Set brightness failed, %d\n", ret);
	}

	return ret;
}

static int lm3644_ir_get_data(struct lm3644_chip_data *chip, lm3644_data params)
{
	int ret = 0;
	unsigned int data = 0;

	if (GET_CHIP_ID_EVENT == params.event)
		data = chip->chip_id;
	if (GET_BRIGHTNESS_EVENT == params.event)
		data = chip->pdata->brightness;

	if (copy_to_user((void __user *)&params.data, &data, sizeof(data))) {
		ret = -ENODEV;
		dev_err(chip->dev, "Copy data to user space failed\n");
	}

	return ret;
}


static long lm3644_ir_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct lm3644_chip_data *chip = filp->private_data;
	lm3644_data params;

	dev_dbg(chip->dev, "[%s] ioctl_cmd = %u\n", __func__, cmd);

	if (_IOC_TYPE(cmd) != LM3644_IOC_MAGIC)
		return -ENODEV;

	if (_IOC_DIR(cmd) & _IOC_READ)
		ret = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		ret = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (ret)
		return -EFAULT;

	if ((_IOC_DIR(cmd) & _IOC_WRITE) || (_IOC_DIR(cmd) & (_IOC_WRITE|_IOC_READ))) {
		if (copy_from_user(&params, (void __user *)arg, sizeof(lm3644_data))) {
			dev_err(chip->dev, "Copy data from user space failed\n");
			return -EFAULT;
		}
	}

	switch (cmd) {
	case FLOOD_IR_IOC_POWER_UP:
		ret = lm3644_ir_init(chip);
		break;
	case FLOOD_IR_IOC_POWER_DOWN:
		ret = lm3644_ir_deinit(chip);
		break;
	case FLOOD_IR_IOC_WRITE:
		ret = lm3644_ir_set_data(chip, params);
		break;
	case FLOOD_IR_IOC_READ:
		ret = lm3644_ir_get_data(chip, params);
		break;
	case FLOOD_IR_IOC_READ_INFO:
	{
		lm3644_info flood_info;
		unsigned int ret = 0;
		unsigned int val;

		ret += regmap_read(chip->regmap, REG_ENABLE, &val);
		flood_info.flood_enable = (val == 0x27) ? 1 : 0;

		ret += regmap_read(chip->regmap, REG_LED2_FLASH_BRIGHTNESS, &val);
		flood_info.flood_current = (val * 11725 + 10900) * 2 / 1000;

		ret += regmap_read(chip->regmap, REG_FLAG1, &val);
		flood_info.flood_error = val;
		ret += regmap_read(chip->regmap, REG_FLAG2, &val);
		flood_info.flood_error = (flood_info.flood_error<<8) + val;

		if (copy_to_user((void __user *)arg, &flood_info, sizeof(flood_info)) != 0 || ret != 0)
			dev_err(chip->dev, "copy to user failed!\n");

		dev_err(chip->dev, "flood_info:en=%d, current=%d, error=0x%x\n", flood_info.flood_enable, flood_info.flood_current, flood_info.flood_error);
		break;
	}
	default:
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long lm3644_ir_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return lm3644_ir_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

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

static int lm3644_dump_reg(struct device *dev)
{
	unsigned int val;
	int ret = 0;
	int i = 0;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3644_chip_data *chip = container_of(led_cdev, struct lm3644_chip_data, cdev_ir);

	dev_err(dev, "lm3644_dump_reg start:\n");
	for (i = 0; i < 13; i++) {
	    ret += regmap_read(chip->regmap, i+1, &val);
	    dev_err(dev, "lm3644 0x%x:0x%x", i+1, val);
	}

	if (ret != 0)
		dev_err(dev, "lm reg dump fail!\n");
	else
		dev_err(dev, "lm reg dump success!\n");

	return 0;
}

static ssize_t lm3644_reg_opt_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buff, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3644_chip_data *chip =
		container_of(led_cdev, struct lm3644_chip_data, cdev_ir);
	unsigned int val, addr;
	int ret = 0;

	ret = sscanf(buff, "0x%x 0x%x", &addr, &val);
	if (ret == 0)
		dev_err(chip->dev, "lm3644_reg_opt_store, reg=0x%x,val=0x%x.\n", addr, val);

	if (addr > 0x13) {
		dev_err(chip->dev, "lm3644_reg_opt_store, addr invalid:0x%x\n", addr);
		return count;
	}

	ret = regmap_write(chip->regmap, addr, val);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to write reg:0x%x, val:0x%x, ret:%d\n", addr, val, ret);
		return ret;
	}
	regmap_read(chip->regmap, addr, &val);
	dev_err(chip->dev, "lm3644_reg_opt_store, reg:0x%x, val_after_set:0x%x.\n", addr, val);

	return count;
}

static ssize_t lm3644_reg_opt_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3644_chip_data *chip =
		container_of(led_cdev, struct lm3644_chip_data, cdev_ir);

	lm3644_dump_reg(dev);

	dev_err(chip->dev, "lm3644_reg_opt_show\n");
	return snprintf(buf, PAGE_SIZE, "lm3644_reg_opt_show\n");
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
static DEVICE_ATTR(reg_opt, S_IRUGO | S_IWUSR,
	lm3644_reg_opt_show, lm3644_reg_opt_store);

static struct attribute *lm3644_ir_attrs[] = {
	&dev_attr_pwm_period.attr,
	&dev_attr_pwm_duty.attr,
	&dev_attr_id.attr,
	&dev_attr_reg_opt.attr,
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
	pdata->ito_detect_gpio = of_get_named_gpio(np, "lm3644,ito-detect-gpio", 0);

	pdata->pass_mode = of_property_read_bool(np,
			"lm3644,pass-mode");

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

static const struct regmap_config lm3644_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REG_MAX,
};

static const struct file_operations lm3644_ir_fops =
{
	.owner = THIS_MODULE,
	.release = lm3644_ir_release,
	.open = lm3644_ir_open,
	.read = lm3644_ir_read,
	.write = lm3644_ir_write,
	.unlocked_ioctl = lm3644_ir_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = lm3644_ir_compat_ioctl,
#endif
	.poll = lm3644_poll
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

	if (gpio_is_valid(pdata->ito_detect_gpio)) {
		err = gpio_request(pdata->ito_detect_gpio, "lm3644_ito_det");
		if (err) {
			dev_err(&client->dev, "Unable to request gpio[%d]\n",
				pdata->ito_detect_gpio);
			goto err_free_torch_gpio;
		}

		err = gpio_direction_input(pdata->ito_detect_gpio);
		if (err) {
			dev_err(&client->dev, "Unable to set ito_detect to input\n");
			goto err_free_ito_gpio;
		}

		chip->ito_irq = gpio_to_irq(pdata->ito_detect_gpio);
		err = request_threaded_irq(chip->ito_irq, NULL, lm3644_ito_irq,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"lm3644_ito_det", chip);
		if (err < 0) {
			dev_err(&client->dev, "Unable to request irq\n");
			goto err_free_ito_gpio;
		}
	}
	disable_irq_nosync(chip->ito_irq);

	mutex_init(&chip->lock);
	i2c_set_clientdata(client, chip);

	INIT_WORK(&chip->ir_stop_work, lm3644_ir_stop_work);
	setup_timer(&chip->ir_stop_timer, lm3644_ir_stop_timer,
		(unsigned long)chip);

	err = lm3644_chip_init(chip);
	if (err < 0)
		goto err_free_ito_irq;

	if (pdata->pass_mode) {
		err = lm3644_enable_pass_mode(chip);
		if (err < 0)
			goto err_free_ito_irq;
	}

	chip->chr_class = class_create(THIS_MODULE, LM3644_CLASS_NAME);
	if (chip->chr_class == NULL) {
		dev_err(&client->dev, "Failed to create class.\n");
		err = -ENODEV;
		goto err_free_ito_irq;
	}


	err = alloc_chrdev_region(&chip->dev_num, 0, 1, DRV_NAME);
	if (err < 0) {
		dev_err(&client->dev, "Failed to allocate chrdev region\n");
		goto err_destory_class;
	}

	chip->chr_dev = device_create(chip->chr_class, NULL,
						chip->dev_num, chip, DRV_NAME);
	if (IS_ERR(chip->chr_dev)) {
		dev_err(&client->dev, "Failed to create char device\n");
		err = PTR_ERR(chip->chr_dev);
		goto err_unregister_chrdev;
	}

	cdev_init(&(chip->cdev), &lm3644_ir_fops);
	chip->cdev.owner = THIS_MODULE;

	err = cdev_add(&(chip->cdev), chip->dev_num, 1);
	if (err < 0) {
		dev_err(&client->dev, "Failed to add cdev\n");
		goto err_destroy_device;
	}

	/* torch mode */
	chip->cdev_torch.name = "ir_torch";
	chip->cdev_torch.max_brightness = LM3644_MAX_BRIGHTNESS_VALUE;
	chip->cdev_torch.brightness_set_blocking = lm3644_torch_brightness_set;
	err = led_classdev_register((struct device *)
		&client->dev, &chip->cdev_torch);
	if (err < 0) {
		dev_err(chip->dev, "Failed to register ir torch\n");
		goto err_del_cdev;
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

	atomic_set(&chip->ito_exception, 0);

	dev_info(&client->dev, "Exit\n");

	return 0;

err_free_torch_classdev:
	led_classdev_unregister(&chip->cdev_torch);

err_del_cdev:
	if (&chip->cdev)
		cdev_del(&(chip->cdev));
err_destroy_device:
	if (chip->chr_dev)
		device_destroy(chip->chr_class, chip->dev_num);
err_unregister_chrdev:
	unregister_chrdev_region(chip->dev_num, 1);
err_destory_class:
	if (chip->chr_class)
		class_destroy(chip->chr_class);

err_free_ito_irq:
	if (gpio_is_valid(pdata->ito_detect_gpio))
		free_irq(chip->ito_irq, chip);
err_free_ito_gpio:
	if (gpio_is_valid(pdata->ito_detect_gpio))
		gpio_free(pdata->ito_detect_gpio);

err_free_torch_gpio:
	if (gpio_is_valid(pdata->torch_gpio)) {
		gpio_set_value(pdata->torch_gpio, 0);
		gpio_free(pdata->torch_gpio);
	}
err_free_tx_gpio:
	if (gpio_is_valid(pdata->tx_gpio)) {
		gpio_set_value(pdata->tx_gpio, 0);
		gpio_free(pdata->tx_gpio);
	}
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

err_free_pwm:
	pwm_put(chip->pwm);

	return err;
}

static int lm3644_remove(struct i2c_client *client)
{
	struct lm3644_chip_data *chip = i2c_get_clientdata(client);

	if (&chip->cdev)
		cdev_del(&(chip->cdev));

	if (chip->chr_dev)
		device_destroy(chip->chr_class, chip->dev_num);

	unregister_chrdev_region(chip->dev_num, 1);

	if (chip->chr_dev)
		class_destroy(chip->chr_class);

	cancel_work(&chip->ir_stop_work);
	del_timer(&chip->ir_stop_timer);
	led_classdev_unregister(&chip->cdev_torch);
	led_classdev_unregister(&chip->cdev_ir);
	regmap_write(chip->regmap, REG_ENABLE, 0);
	pwm_put(chip->pwm);

	if (gpio_is_valid(chip->pdata->hwen_gpio)) {
		gpio_set_value(chip->pdata->hwen_gpio, 0);
		gpio_free(chip->pdata->hwen_gpio);
	}

	if (gpio_is_valid(chip->pdata->tx_gpio)) {
		gpio_set_value(chip->pdata->tx_gpio, 0);
		gpio_free(chip->pdata->tx_gpio);
	}

	if (gpio_is_valid(chip->pdata->torch_gpio)) {
		gpio_set_value(chip->pdata->torch_gpio, 0);
		gpio_free(chip->pdata->torch_gpio);
	}

	if (gpio_is_valid(chip->pdata->ito_detect_gpio)) {
		free_irq(chip->ito_irq, chip);
		gpio_free(chip->pdata->ito_detect_gpio);
	}

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
