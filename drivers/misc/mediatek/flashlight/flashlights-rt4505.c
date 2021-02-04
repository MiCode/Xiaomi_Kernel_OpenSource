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

/* define device tree */
#ifndef RT4505_DTNAME_I2C
#define RT4505_DTNAME_I2C "mediatek,flashlights_rt4505_i2c"
#endif

#define RT4505_NAME "flashlights-rt4505"

/* define registers */
#define RT4505_REG_ENABLE   (0x0A)
#define RT4505_ENABLE_TORCH (0x71)
#define RT4505_ENABLE_FLASH (0x77)
#define RT4505_DISABLE      (0x70)

#define RT4505_REG_LEVEL (0x09)

#define RT4505_REG_RESET (0x00)
#define RT4505_FLASH_RESET (0x80)

#define RT4505_REG_FLASH_FEATURE (0x08)
#define RT4505_FLASH_TIMEOUT (0x07)

/* define level */
#define RT4505_LEVEL_NUM 18
#define RT4505_LEVEL_TORCH 4
#define RT4505_HW_TIMEOUT 800 /* ms */

/* define mutex and work queue */
static DEFINE_MUTEX(rt4505_mutex);
static struct work_struct rt4505_work;

/* define usage count */
static int use_count;

/* define i2c */
static struct i2c_client *rt4505_i2c_client;

/* platform data */
struct rt4505_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

/* rt4505 chip data */
struct rt4505_chip_data {
	struct i2c_client *client;
	struct rt4505_platform_data *pdata;
	struct mutex lock;
};


/******************************************************************************
 * rt4505 operations
 *****************************************************************************/
static const int rt4505_current[RT4505_LEVEL_NUM] = {
	 49,  93,  140,  187,  281,  375,  468,  562, 656, 750,
	843, 937, 1031, 1125, 1218, 1312, 1406, 1500
};

static const unsigned char rt4505_flash_level[RT4505_LEVEL_NUM] = {
	0x00, 0x20, 0x40, 0x60, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

static int rt4505_level = -1;

static int rt4505_is_torch(int level)
{
	if (level >= RT4505_LEVEL_TORCH)
		return -1;

	return 0;
}

static int rt4505_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= RT4505_LEVEL_NUM)
		level = RT4505_LEVEL_NUM - 1;

	return level;
}

/* i2c wrapper function */
static int rt4505_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct rt4505_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_err("failed writing at 0x%02x\n", reg);

	return ret;
}

/* flashlight enable function */
static int rt4505_enable(void)
{
	unsigned char reg, val;

	reg = RT4505_REG_ENABLE;
	if (!rt4505_is_torch(rt4505_level)) {
		/* torch mode */
		val = RT4505_ENABLE_TORCH;
	} else {
		/* flash mode */
		val = RT4505_ENABLE_FLASH;
	}

	return rt4505_write_reg(rt4505_i2c_client, reg, val);
}

/* flashlight disable function */
static int rt4505_disable(void)
{
	unsigned char reg, val;

	reg = RT4505_REG_ENABLE;
	val = RT4505_DISABLE;

	return rt4505_write_reg(rt4505_i2c_client, reg, val);
}

/* set flashlight level */
static int rt4505_set_level(int level)
{
	unsigned char reg, val;

	level = rt4505_verify_level(level);
	rt4505_level = level;

	reg = RT4505_REG_LEVEL;
	val = rt4505_flash_level[level];

	return rt4505_write_reg(rt4505_i2c_client, reg, val);
}

/* flashlight init */
int rt4505_init(void)
{
	int ret;
	unsigned char reg, val;

	/* reset chip */
	reg = RT4505_REG_RESET;
	val = RT4505_FLASH_RESET;
	ret = rt4505_write_reg(rt4505_i2c_client, reg, val);

	/* set flash timeout */
	reg = RT4505_REG_FLASH_FEATURE;
	val = RT4505_FLASH_TIMEOUT;
	ret = rt4505_write_reg(rt4505_i2c_client, reg, val);

	return ret;
}

/* flashlight uninit */
int rt4505_uninit(void)
{
	rt4505_disable();

	return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer rt4505_timer;
static unsigned int rt4505_timeout_ms;

static void rt4505_work_disable(struct work_struct *data)
{
	pr_debug("work queue callback\n");
	rt4505_disable();
}

static enum hrtimer_restart rt4505_timer_func(struct hrtimer *timer)
{
	schedule_work(&rt4505_work);
	return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int rt4505_ioctl(unsigned int cmd, unsigned long arg)
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
		rt4505_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_debug("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		rt4505_set_level(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_debug("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (rt4505_timeout_ms) {
				s = rt4505_timeout_ms / 1000;
				ns = rt4505_timeout_ms % 1000 * 1000000;
				ktime = ktime_set(s, ns);
				hrtimer_start(&rt4505_timer, ktime,
						HRTIMER_MODE_REL);
			}
			rt4505_enable();
		} else {
			rt4505_disable();
			hrtimer_cancel(&rt4505_timer);
		}
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		pr_debug("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = RT4505_LEVEL_NUM;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_debug("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = RT4505_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = rt4505_verify_level(fl_arg->arg);
		pr_debug("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		fl_arg->arg = rt4505_current[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_debug("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = RT4505_HW_TIMEOUT;
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int rt4505_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int rt4505_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int rt4505_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&rt4505_mutex);
	if (set) {
		if (!use_count)
			ret = rt4505_init();
		use_count++;
		pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = rt4505_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&rt4505_mutex);

	return ret;
}

static ssize_t rt4505_strobe_store(struct flashlight_arg arg)
{
	rt4505_set_driver(1);
	rt4505_set_level(arg.level);
	rt4505_timeout_ms = 0;
	rt4505_enable();
	msleep(arg.dur);
	rt4505_disable();
	rt4505_set_driver(0);

	return 0;
}

static struct flashlight_operations rt4505_ops = {
	rt4505_open,
	rt4505_release,
	rt4505_ioctl,
	rt4505_strobe_store,
	rt4505_set_driver
};


/******************************************************************************
 * I2C device and driver
 *****************************************************************************/
static int rt4505_chip_init(struct rt4505_chip_data *chip)
{
	/* NOTE: Chip initialication move to "set driver" for power saving.
	 * rt4505_init();
	 */

	return 0;
}

static int rt4505_parse_dt(struct device *dev,
		struct rt4505_platform_data *pdata)
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
				RT4505_NAME);
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

static int rt4505_i2c_probe(
		struct i2c_client *client, const struct i2c_device_id *id)
{
	struct rt4505_platform_data *pdata = dev_get_platdata(&client->dev);
	struct rt4505_chip_data *chip;
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
	chip = kzalloc(sizeof(struct rt4505_chip_data), GFP_KERNEL);
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
		err = rt4505_parse_dt(&client->dev, pdata);
		if (err)
			goto err_free;
	}
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
	rt4505_i2c_client = client;

	/* init mutex and spinlock */
	mutex_init(&chip->lock);

	/* init work queue */
	INIT_WORK(&rt4505_work, rt4505_work_disable);

	/* init timer */
	hrtimer_init(&rt4505_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	rt4505_timer.function = rt4505_timer_func;
	rt4505_timeout_ms = 400;

	/* init chip hw */
	rt4505_chip_init(chip);

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&rt4505_ops)) {
				err = -EFAULT;
				goto err_free;
			}
	} else {
		if (flashlight_dev_register(RT4505_NAME, &rt4505_ops)) {
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

static int rt4505_i2c_remove(struct i2c_client *client)
{
	struct rt4505_platform_data *pdata = dev_get_platdata(&client->dev);
	struct rt4505_chip_data *chip = i2c_get_clientdata(client);
	int i;

	pr_debug("Remove start.\n");

	client->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(RT4505_NAME);

	/* flush work queue */
	flush_work(&rt4505_work);

	/* free resource */
	kfree(chip);

	pr_debug("Remove done.\n");

	return 0;
}

static const struct i2c_device_id rt4505_i2c_id[] = {
	{RT4505_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, rt4505_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id rt4505_i2c_of_match[] = {
	{.compatible = RT4505_DTNAME_I2C},
	{},
};
MODULE_DEVICE_TABLE(of, rt4505_i2c_of_match);
#endif

static struct i2c_driver rt4505_i2c_driver = {
	.driver = {
		.name = RT4505_NAME,
#ifdef CONFIG_OF
		.of_match_table = rt4505_i2c_of_match,
#endif
	},
	.probe = rt4505_i2c_probe,
	.remove = rt4505_i2c_remove,
	.id_table = rt4505_i2c_id,
};

module_i2c_driver(rt4505_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Simon Wang <Simon-TCH.Wang@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight RT4505 Driver");

