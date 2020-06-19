// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
/* TODO: modify temp device tree name */
#ifndef DUMMY_DTNAME
#define DUMMY_DTNAME "mediatek,flashlights_dummy"
#endif
#ifndef DUMMY_DTNAME_I2C
#define DUMMY_DTNAME_I2C "mediatek,flashlights_dummy_i2c"
#endif

/* TODO: define driver name */
#define DUMMY_NAME "flashlights-dummy"

/* define registers */
/* TODO: define register */

/* define level */
/* TODO: define brightness level and hardware timeout */
#define DUMMY_LEVEL_NUM 2
#define DUMMY_LEVEL_TORCH 1
#define DUMMY_HW_TIMEOUT 100 /* ms */

/* define mutex and work queue */
static DEFINE_MUTEX(dummy_mutex);
static struct work_struct dummy_work;

/* define usage count */
static int use_count;

/* define i2c */
static struct i2c_client *dummy_i2c_client;

/* platform data */
struct dummy_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

/* dummy chip data */
struct dummy_chip_data {
	struct i2c_client *client;
	struct dummy_platform_data *pdata;
	struct mutex lock;
};


/******************************************************************************
 * dummy operations
 *****************************************************************************/
static const int dummy_current[DUMMY_LEVEL_NUM] = {
	/* TODO: define current */
	100, 1000
};

static const unsigned char dummy_flash_level[DUMMY_LEVEL_NUM] = {
	/* TODO: define register value */
	0x00, 0x01
};

static int dummy_level = -1;

static int dummy_is_torch(int level)
{
	if (level >= DUMMY_LEVEL_TORCH)
		return -1;

	return 0;
}

static int dummy_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= DUMMY_LEVEL_NUM)
		level = DUMMY_LEVEL_NUM - 1;

	return level;
}

/* i2c wrapper function */
static int dummy_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct dummy_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_info("failed writing at 0x%02x\n", reg);

	return ret;
}

static int dummy_read_reg(struct i2c_client *client, u8 reg)
{
	int val;
	struct dummy_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	val = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);

	return val;
}

/* flashlight enable function */
static int dummy_enable(void)
{
	unsigned char reg = 0, val = 0;

	/* TODO: wrap enable function */
	if (!dummy_is_torch(dummy_level)) {
		/* torch mode */
		;
	} else {
		/* flash mode */
		;
	}

	return dummy_write_reg(dummy_i2c_client, reg, val);
}

/* flashlight disable function */
static int dummy_disable(void)
{
	unsigned char reg = 0, val = 0;

	/* TODO: wrap disable function */

	return dummy_write_reg(dummy_i2c_client, reg, val);
}

/* set flashlight level */
static int dummy_set_level(int level)
{
	unsigned char reg = 0, val = 0;

	/* TODO: wrap set level function */

	return dummy_write_reg(dummy_i2c_client, reg, val);
}

static int dummy_get_hw_fault(int num)
{
	unsigned char reg = 0;

	if (num == 1)
		return dummy_read_reg(dummy_i2c_client, reg);
	else if (num == 2)
		return dummy_read_reg(dummy_i2c_client, reg);

	pr_info("Error num\n");
	return 0;
}

/* flashlight init */
static int dummy_init(void)
{
	unsigned char reg = 0, val = 0;

	/* TODO: wrap init function */

	return dummy_write_reg(dummy_i2c_client, reg, val);
}

/* flashlight uninit */
static int dummy_uninit(void)
{
	unsigned char reg = 0, val = 0;

	/* TODO: wrap uninit function */

	return dummy_write_reg(dummy_i2c_client, reg, val);
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer dummy_timer;
static unsigned int dummy_timeout_ms;

static void dummy_work_disable(struct work_struct *data)
{
	pr_debug("work queue callback\n");
	dummy_disable();
}

static enum hrtimer_restart dummy_timer_func(struct hrtimer *timer)
{
	schedule_work(&dummy_work);
	return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int dummy_ioctl(unsigned int cmd, unsigned long arg)
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
		dummy_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_debug("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		dummy_set_level(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_debug("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (dummy_timeout_ms) {
				s = dummy_timeout_ms / 1000;
				ns = dummy_timeout_ms % 1000 * 1000000;
				ktime = ktime_set(s, ns);
				hrtimer_start(&dummy_timer, ktime,
						HRTIMER_MODE_REL);
			}
			dummy_enable();
		} else {
			dummy_disable();
			hrtimer_cancel(&dummy_timer);
		}
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		pr_debug("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = DUMMY_LEVEL_NUM;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_debug("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = DUMMY_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = dummy_verify_level(fl_arg->arg);
		pr_debug("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		fl_arg->arg = dummy_current[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_debug("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = DUMMY_HW_TIMEOUT;
		break;

	case FLASH_IOC_GET_HW_FAULT:
		pr_debug("FLASH_IOC_GET_HW_FAULT(%d)\n", channel);
		fl_arg->arg = dummy_get_hw_fault(1);
		break;

	case FLASH_IOC_GET_HW_FAULT2:
		pr_debug("FLASH_IOC_GET_HW_FAULT2(%d)\n", channel);
		fl_arg->arg = dummy_get_hw_fault(2);
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int dummy_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int dummy_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int dummy_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&dummy_mutex);
	if (set) {
		if (!use_count)
			ret = dummy_init();
		use_count++;
		pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = dummy_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&dummy_mutex);

	return ret;
}

static ssize_t dummy_strobe_store(struct flashlight_arg arg)
{
	dummy_set_driver(1);
	dummy_set_level(arg.level);
	dummy_timeout_ms = 0;
	dummy_enable();
	msleep(arg.dur);
	dummy_disable();
	dummy_set_driver(0);

	return 0;
}

static struct flashlight_operations dummy_ops = {
	dummy_open,
	dummy_release,
	dummy_ioctl,
	dummy_strobe_store,
	dummy_set_driver
};


/******************************************************************************
 * I2C device and driver
 *****************************************************************************/
static int dummy_chip_init(struct dummy_chip_data *chip)
{
	/* NOTE: Chip initialication move to "set driver" for power saving.
	 * dummy_init();
	 */

	return 0;
}

static int dummy_parse_dt(struct device *dev,
		struct dummy_platform_data *pdata)
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
				DUMMY_NAME);
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

static int dummy_i2c_probe(
		struct i2c_client *client, const struct i2c_device_id *id)
{
	struct dummy_chip_data *chip;
	int err;

	pr_debug("Probe start.\n");

	/* check i2c */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_info("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}

	/* init chip private data */
	chip = kzalloc(sizeof(struct dummy_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}
	chip->client = client;

	i2c_set_clientdata(client, chip);
	dummy_i2c_client = client;

	/* init mutex and spinlock */
	mutex_init(&chip->lock);

	/* init chip hw */
	dummy_chip_init(chip);

	pr_debug("Probe done.\n");

	return 0;

err_out:
	return err;
}

static int dummy_i2c_remove(struct i2c_client *client)
{
	struct dummy_chip_data *chip = i2c_get_clientdata(client);

	pr_debug("Remove start.\n");

	client->dev.platform_data = NULL;

	/* free resource */
	kfree(chip);

	pr_debug("Remove done.\n");

	return 0;
}

static const struct i2c_device_id dummy_i2c_id[] = {
	{DUMMY_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, dummy_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id dummy_i2c_of_match[] = {
	{.compatible = DUMMY_DTNAME_I2C},
	{},
};
MODULE_DEVICE_TABLE(of, dummy_i2c_of_match);
#endif

static struct i2c_driver dummy_i2c_driver = {
	.driver = {
		.name = DUMMY_NAME,
#ifdef CONFIG_OF
		.of_match_table = dummy_i2c_of_match,
#endif
	},
	.probe = dummy_i2c_probe,
	.remove = dummy_i2c_remove,
	.id_table = dummy_i2c_id,
};

/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int dummy_probe(struct platform_device *pdev)
{
	struct dummy_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct dummy_chip_data *chip = NULL;
	int err;
	int i;

	pr_debug("Probe start.\n");

	if (i2c_add_driver(&dummy_i2c_driver)) {
		pr_debug("Failed to add i2c driver.\n");
		return -1;
	}

	/* init platform data */
	if (!pdata) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			err = -ENOMEM;
			goto err_free;
		}
		pdev->dev.platform_data = pdata;
		err = dummy_parse_dt(&pdev->dev, pdata);
		if (err)
			goto err_free;
	}

	/* init work queue */
	INIT_WORK(&dummy_work, dummy_work_disable);

	/* init timer */
	hrtimer_init(&dummy_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dummy_timer.function = dummy_timer_func;
	dummy_timeout_ms = 100;

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&dummy_ops)) {
				err = -EFAULT;
				goto err_free;
			}
	} else {
		if (flashlight_dev_register(DUMMY_NAME, &dummy_ops)) {
			err = -EFAULT;
			goto err_free;
		}
	}

	pr_debug("Probe done.\n");

	return 0;
err_free:
	chip = i2c_get_clientdata(dummy_i2c_client);
	i2c_set_clientdata(dummy_i2c_client, NULL);
	kfree(chip);
	return err;
}

static int dummy_remove(struct platform_device *pdev)
{
	struct dummy_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	pr_debug("Remove start.\n");

	i2c_del_driver(&dummy_i2c_driver);

	pdev->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(DUMMY_NAME);

	/* flush work queue */
	flush_work(&dummy_work);

	pr_debug("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dummy_of_match[] = {
	{.compatible = DUMMY_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, dummy_of_match);
#else
static struct platform_device dummy_platform_device[] = {
	{
		.name = DUMMY_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, dummy_platform_device);
#endif

static struct platform_driver dummy_platform_driver = {
	.probe = dummy_probe,
	.remove = dummy_remove,
	.driver = {
		.name = DUMMY_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = dummy_of_match,
#endif
	},
};

static int __init flashlight_dummy_init(void)
{
	int ret;

	pr_debug("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&dummy_platform_device);
	if (ret) {
		pr_info("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&dummy_platform_driver);
	if (ret) {
		pr_info("Failed to register platform driver\n");
		return ret;
	}

	pr_debug("Init done.\n");

	return 0;
}

static void __exit flashlight_dummy_exit(void)
{
	pr_debug("Exit start.\n");

	platform_driver_unregister(&dummy_platform_driver);

	pr_debug("Exit done.\n");
}

module_init(flashlight_dummy_init);
module_exit(flashlight_dummy_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Simon Wang <Simon-TCH.Wang@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight DUMMY Driver");

