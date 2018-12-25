/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>

#include "flashlight-core.h"
#include "flashlight-dt.h"

/* device tree should be defined in flashlight-dt.h */
#ifndef LM3642_DTNAME
#define LM3642_DTNAME "mediatek,flashlights_lm3642"
#endif
#ifndef LM3642_DTNAME_I2C
#define LM3642_DTNAME_I2C "mediatek,flashlights_lm3642_i2c"
#endif

#define LM3642_NAME "flashlights-lm3642"

/* define registers */
#define LM3642_REG_SILICON_REVISION (0x00)

#define LM3642_REG_FLASH_FEATURE      (0x08)
#define LM3642_INDUCTOR_CURRENT_LIMIT (0x40)
#define LM3642_FLASH_RAMP_TIME        (0x00)
#define LM3642_FLASH_TIMEOUT          (0x07)

#define LM3642_REG_CURRENT_CONTROL (0x09)

#define LM3642_REG_ENABLE (0x0A)
#define LM3642_ENABLE_STANDBY (0x00)
#define LM3642_ENABLE_TORCH (0x02)
#define LM3642_ENABLE_FLASH (0x03)

/* define level */
#define LM3642_LEVEL_NUM 18
#define LM3642_LEVEL_TORCH 4
#define LM3642_HW_TIMEOUT 800 /* ms */

/* define mutex and work queue */
static DEFINE_MUTEX(lm3642_mutex);
static struct work_struct lm3642_work;

/* lm3642 revision */
static int is_lm3642lt;

/* define usage count */
static int use_count;

/* define i2c */
static struct i2c_client *lm3642_i2c_client;

/* platform data */
struct lm3642_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

/* lm3642 chip data */
struct lm3642_chip_data {
	struct i2c_client *client;
	struct lm3642_platform_data *pdata;
	struct mutex lock;
};


/******************************************************************************
 * lm3642 operations
 *****************************************************************************/
static const int lm3642_current[LM3642_LEVEL_NUM] = {
	 48,  93,  141,  188,  281,  375,  469,  563, 656, 750,
	844, 938, 1031, 1125, 1219, 1313, 1406, 1500
};

static const unsigned char lm3642_flash_level[LM3642_LEVEL_NUM] = {
	0x00, 0x10, 0x20, 0x30, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

static const unsigned char lm3642lt_flash_level[LM3642_LEVEL_NUM] = {
	0x10, 0x30, 0x50, 0x70, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

static int lm3642_level = -1;

static int lm3642_is_torch(int level)
{
	if (level >= LM3642_LEVEL_TORCH)
		return -1;

	return 0;
}

static int lm3642_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= LM3642_LEVEL_NUM)
		level = LM3642_LEVEL_NUM - 1;

	return level;
}

/* i2c wrapper function */
static int lm3642_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct lm3642_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_err("failed writing at 0x%02x\n", reg);

	return ret;
}

static int lm3642_read_reg(struct i2c_client *client, u8 reg)
{
	int val = 0;
	struct lm3642_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	val = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);

	return val;
}

/* flashlight enable function */
static int lm3642_enable(void)
{
	unsigned char reg, val;

	reg = LM3642_REG_ENABLE;
	if (!lm3642_is_torch(lm3642_level)) {
		/* torch mode */
		val = LM3642_ENABLE_TORCH;
	} else {
		/* flash mode */
		val = LM3642_ENABLE_FLASH;
	}

	return lm3642_write_reg(lm3642_i2c_client, reg, val);
}

/* flashlight disable function */
static int lm3642_disable(void)
{
	unsigned char reg, val;

	reg = LM3642_REG_ENABLE;
	val = LM3642_ENABLE_STANDBY;

	return lm3642_write_reg(lm3642_i2c_client, reg, val);
}

/* set flashlight level */
static int lm3642_set_level(int level)
{
	unsigned char reg, val;

	level = lm3642_verify_level(level);
	lm3642_level = level;

	reg = LM3642_REG_CURRENT_CONTROL;
	if (is_lm3642lt)
		val = lm3642lt_flash_level[level];
	else
		val = lm3642_flash_level[level];

	return lm3642_write_reg(lm3642_i2c_client, reg, val);
}

/* flashlight init */
int lm3642_init(void)
{
	int ret;

	/* get silicon revision */
	is_lm3642lt = lm3642_read_reg(
			lm3642_i2c_client, LM3642_REG_SILICON_REVISION);
	pr_info("LM3642(LT) revision(%d).\n", is_lm3642lt);

	/* disable */
	ret = lm3642_write_reg(lm3642_i2c_client, LM3642_REG_ENABLE,
			LM3642_ENABLE_STANDBY);

	/* set flash ramp time and timeout */
	ret = lm3642_write_reg(lm3642_i2c_client, LM3642_REG_FLASH_FEATURE,
			LM3642_INDUCTOR_CURRENT_LIMIT |
			LM3642_FLASH_RAMP_TIME |
			LM3642_FLASH_TIMEOUT);

	return ret;
}

/* flashlight uninit */
int lm3642_uninit(void)
{
	lm3642_disable();

	return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer lm3642_timer;
static unsigned int lm3642_timeout_ms;

static void lm3642_work_disable(struct work_struct *data)
{
	pr_debug("work queue callback\n");
	lm3642_disable();
}

static enum hrtimer_restart lm3642_timer_func(struct hrtimer *timer)
{
	schedule_work(&lm3642_work);
	return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int lm3642_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;
	unsigned int s;
	unsigned int ns;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_debug("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		lm3642_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_debug("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		lm3642_set_level(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_debug("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (lm3642_timeout_ms) {
				s = lm3642_timeout_ms / 1000;
				ns = lm3642_timeout_ms % 1000 * 1000000;
				ktime = ktime_set(s, ns);
				hrtimer_start(&lm3642_timer, ktime,
						HRTIMER_MODE_REL);
			}
			lm3642_enable();
		} else {
			lm3642_disable();
			hrtimer_cancel(&lm3642_timer);
		}
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		pr_debug("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = LM3642_LEVEL_NUM;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_debug("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = LM3642_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = lm3642_verify_level(fl_arg->arg);
		pr_debug("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		fl_arg->arg = lm3642_current[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_debug("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = LM3642_HW_TIMEOUT;
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int lm3642_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int lm3642_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int lm3642_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&lm3642_mutex);
	if (set) {
		if (!use_count)
			ret = lm3642_init();
		use_count++;
		pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = lm3642_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&lm3642_mutex);

	return ret;
}

static ssize_t lm3642_strobe_store(struct flashlight_arg arg)
{
	lm3642_set_driver(1);
	lm3642_set_level(arg.level);
	lm3642_timeout_ms = 0;
	lm3642_enable();
	msleep(arg.dur);
	lm3642_disable();
	lm3642_set_driver(0);

	return 0;
}

static struct flashlight_operations lm3642_ops = {
	lm3642_open,
	lm3642_release,
	lm3642_ioctl,
	lm3642_strobe_store,
	lm3642_set_driver
};


/******************************************************************************
 * I2C device and driver
 *****************************************************************************/
static int lm3642_chip_init(struct lm3642_chip_data *chip)
{
	/* NOTE: Chip initialication move to "set driver" for power saving.
	 * lm3642_init();
	 */

	return 0;
}

static int lm3642_parse_dt(struct device *dev,
		struct lm3642_platform_data *pdata)
{
	struct device_node *np, *cnp;
	u32 decouple = 0;
	int i = 0;

	if (!dev || !dev->of_node || !pdata)
		return -ENODEV;

	np = dev->of_node;

	pdata->channel_num = of_get_child_count(np);
	if (!pdata->channel_num) {
		pr_info("Parse no dt, node.\n");
		return 0;
	}
	pr_info("Channel number(%d).\n", pdata->channel_num);

	if (of_property_read_u32(np, "decouple", &decouple))
		pr_info("Parse no dt, decouple.\n");

	pdata->dev_id = devm_kzalloc(dev,
			pdata->channel_num *
			sizeof(struct flashlight_device_id),
			GFP_KERNEL);
	if (!pdata->dev_id)
		return -ENOMEM;

	for_each_child_of_node(np, cnp) {
		if (of_property_read_u32(cnp, "type", &pdata->dev_id[i].type))
			goto err_node_put;
		if (of_property_read_u32(cnp, "ct", &pdata->dev_id[i].ct))
			goto err_node_put;
		if (of_property_read_u32(cnp, "part", &pdata->dev_id[i].part))
			goto err_node_put;
		snprintf(pdata->dev_id[i].name, FLASHLIGHT_NAME_SIZE,
				LM3642_NAME);
		pdata->dev_id[i].channel = i;
		pdata->dev_id[i].decouple = decouple;

		pr_info("Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				pdata->dev_id[i].type, pdata->dev_id[i].ct,
				pdata->dev_id[i].part, pdata->dev_id[i].name,
				pdata->dev_id[i].channel,
				pdata->dev_id[i].decouple);
		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

static int lm3642_i2c_probe(
		struct i2c_client *client, const struct i2c_device_id *id)
{
	struct lm3642_platform_data *pdata = dev_get_platdata(&client->dev);
	struct lm3642_chip_data *chip;
	int err;
	int i;

	pr_debug("Probe start.\n");

	/* check i2c */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}

	/* init chip private data */
	chip = kzalloc(sizeof(struct lm3642_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}
	chip->client = client;

	/* init platform data */
	if (!pdata) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			err = -ENOMEM;
			goto err_free;
		}
		client->dev.platform_data = pdata;
		err = lm3642_parse_dt(&client->dev, pdata);
		if (err)
			goto err_free;
	}
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
	lm3642_i2c_client = client;

	/* init mutex and spinlock */
	mutex_init(&chip->lock);

	/* init work queue */
	INIT_WORK(&lm3642_work, lm3642_work_disable);

	/* init timer */
	hrtimer_init(&lm3642_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	lm3642_timer.function = lm3642_timer_func;
	lm3642_timeout_ms = 800;

	/* init chip hw */
	lm3642_chip_init(chip);

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&lm3642_ops)) {
				err = -EFAULT;
				goto err_free;
			}
	} else {
		if (flashlight_dev_register(LM3642_NAME, &lm3642_ops)) {
			err = -EFAULT;
			goto err_free;
		}
	}

	pr_debug("Probe done.\n");

	return 0;

err_free:
	i2c_set_clientdata(client, NULL);
	kfree(chip);
err_out:
	return err;
}

static int lm3642_i2c_remove(struct i2c_client *client)
{
	struct lm3642_platform_data *pdata = dev_get_platdata(&client->dev);
	struct lm3642_chip_data *chip = i2c_get_clientdata(client);
	int i;

	pr_debug("Remove start.\n");

	client->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(LM3642_NAME);

	/* flush work queue */
	flush_work(&lm3642_work);

	/* free resource */
	kfree(chip);

	pr_debug("Remove done.\n");

	return 0;
}

static const struct i2c_device_id lm3642_i2c_id[] = {
	{LM3642_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, lm3642_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id lm3642_i2c_of_match[] = {
	{.compatible = LM3642_DTNAME_I2C},
	{},
};
MODULE_DEVICE_TABLE(of, lm3642_i2c_of_match);
#endif

static struct i2c_driver lm3642_i2c_driver = {
	.driver = {
		.name = LM3642_NAME,
#ifdef CONFIG_OF
		.of_match_table = lm3642_i2c_of_match,
#endif
	},
	.probe = lm3642_i2c_probe,
	.remove = lm3642_i2c_remove,
	.id_table = lm3642_i2c_id,
};

module_i2c_driver(lm3642_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xi Chen <xixi.chen@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight LM3642 Driver");

