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
#include <linux/pinctrl/consumer.h>

#include "flashlight-core.h"
#include "flashlight-dt.h"

/* device tree should be defined in flashlight-dt.h */
#ifndef LM3643_DTNAME
#define LM3643_DTNAME "mediatek,flashlights_lm3643"
#endif
#ifndef LM3643_DTNAME_I2C
#define LM3643_DTNAME_I2C "mediatek,flashlights_lm3643_i2c"
#endif

#define LM3643_NAME "flashlights-lm3643"

/* define registers */
#define LM3643_REG_ENABLE (0x01)
#define LM3643_MASK_ENABLE_LED1 (0x01)
#define LM3643_MASK_ENABLE_LED2 (0x02)
#define LM3643_DISABLE (0x00)
#define LM3643_ENABLE_LED1 (0x01)
#define LM3643_ENABLE_LED1_TORCH (0x09)
#define LM3643_ENABLE_LED1_FLASH (0x0D)
#define LM3643_ENABLE_LED2 (0x02)
#define LM3643_ENABLE_LED2_TORCH (0x0A)
#define LM3643_ENABLE_LED2_FLASH (0x0E)

#define LM3643_REG_TORCH_LEVEL_LED1 (0x05)
#define LM3643_REG_FLASH_LEVEL_LED1 (0x03)
#define LM3643_REG_TORCH_LEVEL_LED2 (0x06)
#define LM3643_REG_FLASH_LEVEL_LED2 (0x04)

#define LM3643_REG_TIMING_CONF (0x08)
#define LM3643_TORCH_RAMP_TIME (0x00)
#define LM3643_FLASH_TIMEOUT   (0x0F)

#define LM3643_REG_FLAG1 (0x0A)
#define LM3643_REG_FLAG2 (0x0B)

/* define channel, level */
#define LM3643_CHANNEL_NUM 2
#define LM3643_CHANNEL_CH1 0
#define LM3643_CHANNEL_CH2 1

#define LM3643_LEVEL_NUM 26
#define LM3643_LEVEL_TORCH 7

#define LM3643_HW_TIMEOUT 400 /* ms */

/* define mutex and work queue */
static DEFINE_MUTEX(lm3643_mutex);
static struct work_struct lm3643_work_ch1;
static struct work_struct lm3643_work_ch2;

/* define pinctrl */
#define LM3643_PINCTRL_PIN_HWEN 0
#define LM3643_PINCTRL_PINSTATE_LOW 0
#define LM3643_PINCTRL_PINSTATE_HIGH 1
#define LM3643_PINCTRL_STATE_HWEN_HIGH "hwen_high"
#define LM3643_PINCTRL_STATE_HWEN_LOW  "hwen_low"
static struct pinctrl *lm3643_pinctrl;
static struct pinctrl_state *lm3643_hwen_high;
static struct pinctrl_state *lm3643_hwen_low;

/* define usage count */
static int use_count;

/* define i2c */
static struct i2c_client *lm3643_i2c_client;

/* platform data */
struct lm3643_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

/* lm3643 chip data */
struct lm3643_chip_data {
	struct i2c_client *client;
	struct lm3643_platform_data *pdata;
	struct mutex lock;
};


/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int lm3643_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	lm3643_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(lm3643_pinctrl)) {
		pr_info("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(lm3643_pinctrl);
	}

	/* Flashlight HWEN pin initialization */
	lm3643_hwen_high = pinctrl_lookup_state(
			lm3643_pinctrl, LM3643_PINCTRL_STATE_HWEN_HIGH);
	if (IS_ERR(lm3643_hwen_high)) {
		pr_info("Failed to init (%s)\n",
			LM3643_PINCTRL_STATE_HWEN_HIGH);
		ret = PTR_ERR(lm3643_hwen_high);
	}
	lm3643_hwen_low = pinctrl_lookup_state(
			lm3643_pinctrl, LM3643_PINCTRL_STATE_HWEN_LOW);
	if (IS_ERR(lm3643_hwen_low)) {
		pr_info("Failed to init (%s)\n", LM3643_PINCTRL_STATE_HWEN_LOW);
		ret = PTR_ERR(lm3643_hwen_low);
	}

	return ret;
}

static int lm3643_pinctrl_set(int pin, int state)
{
	int ret = 0;

	if (IS_ERR(lm3643_pinctrl)) {
		pr_info("pinctrl is not available\n");
		return -1;
	}

	switch (pin) {
	case LM3643_PINCTRL_PIN_HWEN:
		if (state == LM3643_PINCTRL_PINSTATE_LOW &&
				!IS_ERR(lm3643_hwen_low))
			pinctrl_select_state(lm3643_pinctrl, lm3643_hwen_low);
		else if (state == LM3643_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(lm3643_hwen_high))
			pinctrl_select_state(lm3643_pinctrl, lm3643_hwen_high);
		else
			pr_info("set err, pin(%d) state(%d)\n", pin, state);
		break;
	default:
		pr_info("set err, pin(%d) state(%d)\n", pin, state);
		break;
	}
	pr_debug("pin(%d) state(%d)\n", pin, state);

	return ret;
}


/******************************************************************************
 * lm3643 operations
 *****************************************************************************/
static const int lm3643_current[LM3643_LEVEL_NUM] = {
	 22,  46,  70,  93,  116, 140, 163, 198, 245, 304,
	351, 398, 445, 503,  550, 597, 656, 703, 750, 796,
	855, 902, 949, 996, 1054, 1101
};

static const unsigned char lm3643_torch_level[LM3643_LEVEL_NUM] = {
	0x0F, 0x20, 0x31, 0x42, 0x52, 0x63, 0x74, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char lm3643_flash_level[LM3643_LEVEL_NUM] = {
	0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x10, 0x14, 0x19,
	0x1D, 0x21, 0x25, 0x2A, 0x2E, 0x32, 0x37, 0x3B, 0x3F, 0x43,
	0x48, 0x4C, 0x50, 0x54, 0x59, 0x5D
};

static unsigned char lm3643_reg_enable;
static int lm3643_level_ch1 = -1;
static int lm3643_level_ch2 = -1;

static int lm3643_is_torch(int level)
{
	if (level >= LM3643_LEVEL_TORCH)
		return -1;

	return 0;
}

static int lm3643_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= LM3643_LEVEL_NUM)
		level = LM3643_LEVEL_NUM - 1;

	return level;
}

/* i2c wrapper function */
static int lm3643_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct lm3643_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_info("failed writing at 0x%02x\n", reg);

	return ret;
}

static int lm3643_read_reg(struct i2c_client *client, u8 reg)
{
	int val;
	struct lm3643_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	val = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);

	return val;
}

/* flashlight enable function */
static int lm3643_enable_ch1(void)
{
	unsigned char reg, val;

	reg = LM3643_REG_ENABLE;
	if (!lm3643_is_torch(lm3643_level_ch1)) {
		/* torch mode */
		lm3643_reg_enable |= LM3643_ENABLE_LED1_TORCH;
	} else {
		/* flash mode */
		lm3643_reg_enable |= LM3643_ENABLE_LED1_FLASH;
	}
	val = lm3643_reg_enable;

	return lm3643_write_reg(lm3643_i2c_client, reg, val);
}

static int lm3643_enable_ch2(void)
{
	unsigned char reg, val;

	reg = LM3643_REG_ENABLE;
	if (!lm3643_is_torch(lm3643_level_ch2)) {
		/* torch mode */
		lm3643_reg_enable |= LM3643_ENABLE_LED2_TORCH;
	} else {
		/* flash mode */
		lm3643_reg_enable |= LM3643_ENABLE_LED2_FLASH;
	}
	val = lm3643_reg_enable;

	return lm3643_write_reg(lm3643_i2c_client, reg, val);
}

static int lm3643_enable(int channel)
{
	if (channel == LM3643_CHANNEL_CH1)
		lm3643_enable_ch1();
	else if (channel == LM3643_CHANNEL_CH2)
		lm3643_enable_ch2();
	else {
		pr_info("Error channel\n");
		return -1;
	}

	return 0;
}

/* flashlight disable function */
static int lm3643_disable_ch1(void)
{
	unsigned char reg, val;

	reg = LM3643_REG_ENABLE;
	if (lm3643_reg_enable & LM3643_MASK_ENABLE_LED2) {
		/* if LED 2 is enable, disable LED 1 */
		lm3643_reg_enable &= (~LM3643_ENABLE_LED1);
	} else {
		/* if LED 2 is disable, disable LED 1 and clear mode */
		lm3643_reg_enable &= (~LM3643_ENABLE_LED1_FLASH);
	}
	val = lm3643_reg_enable;

	return lm3643_write_reg(lm3643_i2c_client, reg, val);
}

static int lm3643_disable_ch2(void)
{
	unsigned char reg, val;

	reg = LM3643_REG_ENABLE;
	if (lm3643_reg_enable & LM3643_MASK_ENABLE_LED1) {
		/* if LED 1 is enable, disable LED 2 */
		lm3643_reg_enable &= (~LM3643_ENABLE_LED2);
	} else {
		/* if LED 1 is disable, disable LED 2 and clear mode */
		lm3643_reg_enable &= (~LM3643_ENABLE_LED2_FLASH);
	}
	val = lm3643_reg_enable;

	return lm3643_write_reg(lm3643_i2c_client, reg, val);
}

static int lm3643_disable(int channel)
{
	if (channel == LM3643_CHANNEL_CH1)
		lm3643_disable_ch1();
	else if (channel == LM3643_CHANNEL_CH2)
		lm3643_disable_ch2();
	else {
		pr_info("Error channel\n");
		return -1;
	}

	return 0;
}

/* set flashlight level */
static int lm3643_set_level_ch1(int level)
{
	int ret;
	unsigned char reg, val;

	level = lm3643_verify_level(level);

	/* set torch brightness level */
	reg = LM3643_REG_TORCH_LEVEL_LED1;
	val = lm3643_torch_level[level];
	ret = lm3643_write_reg(lm3643_i2c_client, reg, val);

	lm3643_level_ch1 = level;

	/* set flash brightness level */
	reg = LM3643_REG_FLASH_LEVEL_LED1;
	val = lm3643_flash_level[level];
	ret = lm3643_write_reg(lm3643_i2c_client, reg, val);

	return ret;
}

static int lm3643_set_level_ch2(int level)
{
	int ret;
	unsigned char reg, val;

	level = lm3643_verify_level(level);

	/* set torch brightness level */
	reg = LM3643_REG_TORCH_LEVEL_LED2;
	val = lm3643_torch_level[level];
	ret = lm3643_write_reg(lm3643_i2c_client, reg, val);

	lm3643_level_ch2 = level;

	/* set flash brightness level */
	reg = LM3643_REG_FLASH_LEVEL_LED2;
	val = lm3643_flash_level[level];
	ret = lm3643_write_reg(lm3643_i2c_client, reg, val);

	return ret;
}

static int lm3643_set_level(int channel, int level)
{
	if (channel == LM3643_CHANNEL_CH1)
		lm3643_set_level_ch1(level);
	else if (channel == LM3643_CHANNEL_CH2)
		lm3643_set_level_ch2(level);
	else {
		pr_info("Error channel\n");
		return -1;
	}

	return 0;
}

static int lm3643_get_flag(int num)
{
	if (num == 1)
		return lm3643_read_reg(lm3643_i2c_client, LM3643_REG_FLAG1);
	else if (num == 2)
		return lm3643_read_reg(lm3643_i2c_client, LM3643_REG_FLAG2);

	pr_info("Error num\n");
	return 0;
}

/* flashlight init */
int lm3643_init(void)
{
	int ret;
	unsigned char reg, val;

	lm3643_pinctrl_set(
			LM3643_PINCTRL_PIN_HWEN, LM3643_PINCTRL_PINSTATE_HIGH);
	msleep(20);

	/* clear enable register */
	reg = LM3643_REG_ENABLE;
	val = LM3643_DISABLE;
	ret = lm3643_write_reg(lm3643_i2c_client, reg, val);

	lm3643_reg_enable = val;

	/* set torch current ramp time and flash timeout */
	reg = LM3643_REG_TIMING_CONF;
	val = LM3643_TORCH_RAMP_TIME | LM3643_FLASH_TIMEOUT;
	ret = lm3643_write_reg(lm3643_i2c_client, reg, val);

	return ret;
}

/* flashlight uninit */
int lm3643_uninit(void)
{
	lm3643_disable(LM3643_CHANNEL_CH1);
	lm3643_disable(LM3643_CHANNEL_CH2);
	lm3643_pinctrl_set(
			LM3643_PINCTRL_PIN_HWEN, LM3643_PINCTRL_PINSTATE_LOW);

	return 0;
}


/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer lm3643_timer_ch1;
static struct hrtimer lm3643_timer_ch2;
static unsigned int lm3643_timeout_ms[LM3643_CHANNEL_NUM];

static void lm3643_work_disable_ch1(struct work_struct *data)
{
	pr_debug("ht work queue callback\n");
	lm3643_disable_ch1();
}

static void lm3643_work_disable_ch2(struct work_struct *data)
{
	pr_debug("lt work queue callback\n");
	lm3643_disable_ch2();
}

static enum hrtimer_restart lm3643_timer_func_ch1(struct hrtimer *timer)
{
	schedule_work(&lm3643_work_ch1);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart lm3643_timer_func_ch2(struct hrtimer *timer)
{
	schedule_work(&lm3643_work_ch2);
	return HRTIMER_NORESTART;
}

static int lm3643_timer_start(int channel, ktime_t ktime)
{
	if (channel == LM3643_CHANNEL_CH1)
		hrtimer_start(&lm3643_timer_ch1, ktime, HRTIMER_MODE_REL);
	else if (channel == LM3643_CHANNEL_CH2)
		hrtimer_start(&lm3643_timer_ch2, ktime, HRTIMER_MODE_REL);
	else {
		pr_info("Error channel\n");
		return -1;
	}

	return 0;
}

static int lm3643_timer_cancel(int channel)
{
	if (channel == LM3643_CHANNEL_CH1)
		hrtimer_cancel(&lm3643_timer_ch1);
	else if (channel == LM3643_CHANNEL_CH2)
		hrtimer_cancel(&lm3643_timer_ch2);
	else {
		pr_info("Error channel\n");
		return -1;
	}

	return 0;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int lm3643_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;
	unsigned int s;
	unsigned int ns;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	/* verify channel */
	if (channel < 0 || channel >= LM3643_CHANNEL_NUM) {
		pr_info("Failed with error channel\n");
		return -EINVAL;
	}

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_debug("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		lm3643_timeout_ms[channel] = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_debug("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		lm3643_set_level(channel, fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_debug("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (lm3643_timeout_ms[channel]) {
				s = lm3643_timeout_ms[channel] / 1000;
				ns = lm3643_timeout_ms[channel] % 1000
					* 1000000;
				ktime = ktime_set(s, ns);
				lm3643_timer_start(channel, ktime);
			}
			lm3643_enable(channel);
		} else {
			lm3643_disable(channel);
			lm3643_timer_cancel(channel);
		}
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		pr_debug("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = LM3643_LEVEL_NUM;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_debug("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = LM3643_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = lm3643_verify_level(fl_arg->arg);
		pr_debug("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		fl_arg->arg = lm3643_current[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_debug("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = LM3643_HW_TIMEOUT;
		break;

	case FLASH_IOC_GET_HW_FAULT:
		pr_debug("FLASH_IOC_GET_HW_FAULT(%d)\n", channel);
		fl_arg->arg = lm3643_get_flag(1);
		break;

	case FLASH_IOC_GET_HW_FAULT2:
		pr_debug("FLASH_IOC_GET_HW_FAULT2(%d)\n", channel);
		fl_arg->arg = lm3643_get_flag(2);
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int lm3643_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int lm3643_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int lm3643_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&lm3643_mutex);
	if (set) {
		if (!use_count)
			ret = lm3643_init();
		use_count++;
		pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = lm3643_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&lm3643_mutex);

	return ret;
}

static ssize_t lm3643_strobe_store(struct flashlight_arg arg)
{
	lm3643_set_driver(1);
	lm3643_set_level(arg.channel, arg.level);
	lm3643_timeout_ms[arg.channel] = 0;
	lm3643_enable(arg.channel);
	msleep(arg.dur);
	lm3643_disable(arg.channel);
	lm3643_set_driver(0);

	return 0;
}

static struct flashlight_operations lm3643_ops = {
	lm3643_open,
	lm3643_release,
	lm3643_ioctl,
	lm3643_strobe_store,
	lm3643_set_driver
};


/******************************************************************************
 * I2C device and driver
 *****************************************************************************/
static int lm3643_chip_init(struct lm3643_chip_data *chip)
{
	/* NOTE: Chip initialication move to "set driver" for power saving.
	 * lm3643_init();
	 */

	return 0;
}

static int lm3643_parse_dt(struct device *dev,
		struct lm3643_platform_data *pdata)
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
				LM3643_NAME);
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

static int lm3643_i2c_probe(
		struct i2c_client *client, const struct i2c_device_id *id)
{
	struct lm3643_chip_data *chip;
	int err;

	pr_debug("i2c probe start.\n");

	/* check i2c */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_info("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}

	/* init chip private data */
	chip = kzalloc(sizeof(struct lm3643_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}
	chip->client = client;

	i2c_set_clientdata(client, chip);
	lm3643_i2c_client = client;

	/* init mutex and spinlock */
	mutex_init(&chip->lock);

	/* init chip hw */
	lm3643_chip_init(chip);

	pr_debug("i2c probe done.\n");

	return 0;

err_out:
	return err;
}

static int lm3643_i2c_remove(struct i2c_client *client)
{
	struct lm3643_chip_data *chip = i2c_get_clientdata(client);

	pr_debug("Remove start.\n");

	client->dev.platform_data = NULL;

	/* free resource */
	kfree(chip);

	pr_debug("Remove done.\n");

	return 0;
}

static const struct i2c_device_id lm3643_i2c_id[] = {
	{LM3643_NAME, 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id lm3643_i2c_of_match[] = {
	{.compatible = LM3643_DTNAME_I2C},
	{},
};
#endif

static struct i2c_driver lm3643_i2c_driver = {
	.driver = {
		.name = LM3643_NAME,
#ifdef CONFIG_OF
		.of_match_table = lm3643_i2c_of_match,
#endif
	},
	.probe = lm3643_i2c_probe,
	.remove = lm3643_i2c_remove,
	.id_table = lm3643_i2c_id,
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int lm3643_probe(struct platform_device *pdev)
{
	struct lm3643_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct lm3643_chip_data *chip = NULL;
	int err;
	int i;

	pr_debug("Probe start.\n");

	/* init pinctrl */
	if (lm3643_pinctrl_init(pdev)) {
		pr_debug("Failed to init pinctrl.\n");
		return -1;
	}

	if (i2c_add_driver(&lm3643_i2c_driver)) {
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
		err = lm3643_parse_dt(&pdev->dev, pdata);
		if (err)
			goto err_free;
	}

	/* init work queue */
	INIT_WORK(&lm3643_work_ch1, lm3643_work_disable_ch1);
	INIT_WORK(&lm3643_work_ch2, lm3643_work_disable_ch2);

	/* init timer */
	hrtimer_init(&lm3643_timer_ch1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	lm3643_timer_ch1.function = lm3643_timer_func_ch1;
	hrtimer_init(&lm3643_timer_ch2, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	lm3643_timer_ch2.function = lm3643_timer_func_ch2;
	lm3643_timeout_ms[LM3643_CHANNEL_CH1] = 100;
	lm3643_timeout_ms[LM3643_CHANNEL_CH2] = 100;

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
					&pdata->dev_id[i],
					&lm3643_ops)) {
				err = -EFAULT;
				goto err_free;
			}
	} else {
		if (flashlight_dev_register(LM3643_NAME, &lm3643_ops)) {
			err = -EFAULT;
			goto err_free;
		}
	}

	pr_debug("Probe done.\n");

	return 0;
err_free:
	chip = i2c_get_clientdata(lm3643_i2c_client);
	i2c_set_clientdata(lm3643_i2c_client, NULL);
	kfree(chip);
	return err;
}

static int lm3643_remove(struct platform_device *pdev)
{
	struct lm3643_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	pr_debug("Remove start.\n");

	i2c_del_driver(&lm3643_i2c_driver);

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(LM3643_NAME);

	/* flush work queue */
	flush_work(&lm3643_work_ch1);
	flush_work(&lm3643_work_ch2);

	pr_debug("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id lm3643_of_match[] = {
	{.compatible = LM3643_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, lm3643_of_match);
#else
static struct platform_device lm3643_platform_device[] = {
	{
		.name = LM3643_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, lm3643_platform_device);
#endif

static struct platform_driver lm3643_platform_driver = {
	.probe = lm3643_probe,
	.remove = lm3643_remove,
	.driver = {
		.name = LM3643_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = lm3643_of_match,
#endif
	},
};

static int __init flashlight_lm3643_init(void)
{
	int ret;

	pr_debug("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&lm3643_platform_device);
	if (ret) {
		pr_info("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&lm3643_platform_driver);
	if (ret) {
		pr_info("Failed to register platform driver\n");
		return ret;
	}

	pr_debug("Init done.\n");

	return 0;
}

static void __exit flashlight_lm3643_exit(void)
{
	pr_debug("Exit start.\n");

	platform_driver_unregister(&lm3643_platform_driver);

	pr_debug("Exit done.\n");
}

module_init(flashlight_lm3643_init);
module_exit(flashlight_lm3643_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Simon Wang <Simon-TCH.Wang@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight LM3643 Driver");

