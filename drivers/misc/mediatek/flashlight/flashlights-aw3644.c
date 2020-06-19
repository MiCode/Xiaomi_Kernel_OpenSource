// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/pinctrl/consumer.h>


#include "flashlight-core.h"
#include "flashlight-dt.h"
/* define device tree */
/* TODO: modify temp device tree name */
#ifndef AW3644_DTNAME_I2C
#define AW3644_DTNAME_I2C "mediatek,strobe_main"
#endif
/* define device tree */
/* TODO: modify temp device tree name */
#ifndef AW3644_DTNAME
#define AW3644_DTNAME "mediatek,flashlights_aw3644"
#endif
/* TODO: define driver name */
#define AW3644_NAME "flashlights-aw3644"

/* define registers */
#define AW3644_REG_ENABLE			0x01
#define AW3644_REG_IVFM				0x02
#define AW3644_REG_FLASH_LEVEL_LED1	0x03
#define AW3644_REG_FLASH_LEVEL_LED2 0x04
#define AW3644_REG_TORCH_LEVEL_LED1	0x05
#define AW3644_REG_TORCH_LEVEL_LED2	0x06
#define AW3644_REG_BOOST_CONFIG		0x07
#define AW3644_REG_TIMING_CONFIG	0x08
#define AW3644_REG_TEMP				0x09
#define AW3644_REG_FLAG1			0x0A
#define AW3644_REG_FLAG2			0x0B
#define AW3644_REG_DEVICE_ID		0x0C

#if defined(TRAN_KB7J_H615) || defined(TRAN_LC6)///////KB7J///////
#define AW3644_LED2_LEVEL_NUM 30
#define AW3644_LED2_LEVEL_TORCH 8
#define AW3644_LED2_LEVEL_FLASH 30

#define AW3644_LEVEL_NUM 2
#define AW3644_LEVEL_TORCH 2
#define AW3644_LEVEL_FLASH 2
#else
#define AW3644_LEVEL_NUM 30
#define AW3644_LEVEL_TORCH 8
#define AW3644_LEVEL_FLASH AW3644_LEVEL_NUM

#define AW3644_LED2_LEVEL_NUM 2
#define AW3644_LED2_LEVEL_TORCH 2
#define AW3644_LED2_LEVEL_FLASH 2
#endif



#define AW3644_PINCTRL_PIN_HWEN 0
#define AW3644_PINCTRL_PINSTATE_LOW 0
#define AW3644_PINCTRL_PINSTATE_HIGH 1
#define AW3644_PINCTRL_STATE_HWEN_HIGH "hwen_high"
#define AW3644_PINCTRL_STATE_HWEN_LOW  "hwen_low"
static struct pinctrl *aw3644_pinctrl;
static struct pinctrl_state *aw3644_hwen_high;
static struct pinctrl_state *aw3644_hwen_low;

/* define channel, level */
#define AW3644_CHANNEL_NUM 2
#define AW3644_CHANNEL_CH1 0
#define AW3644_CHANNEL_CH2 1

#define AW3644_NONE (-1)
#define AW3644_DISABLE 0
#define AW3644_ENABLE 1
#define AW3644_ENABLE_TORCH 1
#define AW3644_ENABLE_FLASH 2
#define AW3644_WAIT_TIME 3
#define AW3644_RETRY_TIMES 3

/* TODO: define register */

/* define mutex, work queue and timer */
static DEFINE_MUTEX(aw3644_mutex);
static struct work_struct aw3644_work_ch1;
static struct work_struct aw3644_work_ch2;
static struct hrtimer aw3644_timer_ch1;
static struct hrtimer aw3644_timer_ch2;
static unsigned int aw3644_timeout_ms[AW3644_CHANNEL_NUM];

/* define usage count */
static int use_count;
/* define i2c */
static struct i2c_client *AW3644_i2c_client;

/* platform data */
struct aw3644_platform_data {
	u8 torch_pin_enable;
	u8 pam_sync_pin_enable;
	u8 thermal_comp_mode_enable;
	u8 strobe_pin_disable;
	u8 vout_mode_enable;
};

/* aw3644 chip data */
struct aw3644_chip_data {
	struct i2c_client *client;
	struct aw3644_platform_data *pdata;
	struct mutex lock;
	u8 last_flag;
	u8 no_pdata;
};

static int aw3644_flash_read(struct i2c_client *client, u8 reg)
{
	int ret;
	//char data = 0;
	struct aw3644_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);
	pr_info("%s reg:0x%x val:0x%x\n", __func__, reg, ret);
	if (ret < 0)
		pr_info("failed reading at 0x%02x\n", reg);

	return ret;
}

/******************************************************************************
 * aw3644 operations
 *****************************************************************************/
/* i2c wrapper function */
static int aw3644_flash_write(struct i2c_client *client, u8 reg, u8 val)
{

	int ret;
	struct aw3644_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_info("failed writing at 0x%02x\n", reg);

	return ret;

}

/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int aw3644_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("%s in\n", __func__);
	//return 1;
	/* get pinctrl */
	aw3644_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(aw3644_pinctrl)) {
		pr_info("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(aw3644_pinctrl);
		return ret;
	}

	/* Flashlight HWEN pin initialization */
	aw3644_hwen_high = pinctrl_lookup_state(aw3644_pinctrl,
		AW3644_PINCTRL_STATE_HWEN_HIGH);
	if (IS_ERR(aw3644_hwen_high)) {
		pr_info("Failed to init (%s)\n",
			AW3644_PINCTRL_STATE_HWEN_HIGH);
		ret = PTR_ERR(aw3644_hwen_high);
	}
	aw3644_hwen_low = pinctrl_lookup_state(aw3644_pinctrl,
		AW3644_PINCTRL_STATE_HWEN_LOW);
	if (IS_ERR(aw3644_hwen_low)) {
		pr_info("Failed to init (%s)\n", AW3644_PINCTRL_STATE_HWEN_LOW);
		ret = PTR_ERR(aw3644_hwen_low);
	}
	pr_info("%s out\n", __func__);
	return ret;
}

static int aw3644_pinctrl_set(int pin, int state)
{
	int ret = 0;

	pr_info("%s in\n", __func__);
	//return 0;

	if (IS_ERR(aw3644_pinctrl)) {
		pr_info("pinctrl is not available\n");
		return -1;
	}

	switch (pin) {
	case AW3644_PINCTRL_PIN_HWEN:
		if (state == AW3644_PINCTRL_PINSTATE_LOW &&
			!IS_ERR(aw3644_hwen_low))
			pinctrl_select_state(aw3644_pinctrl, aw3644_hwen_low);
		else if (state == AW3644_PINCTRL_PINSTATE_HIGH &&
			!IS_ERR(aw3644_hwen_high))
			pinctrl_select_state(aw3644_pinctrl, aw3644_hwen_high);
		else
			pr_info("set err, pin(%d) state(%d)\n", pin, state);
		break;
	default:
		pr_info("set err, pin(%d) state(%d)\n", pin, state);
		break;
	}
	pr_info("pin(%d) state(%d)\n", pin, state);
	pr_info("%s out\n", __func__);
	return ret;
}

static int aw3644_decouple_mode;
static int aw3644_keepstate_decouple_mode;
static int aw3644_en_ch1;
static int aw3644_en_ch2;
static int aw3644_level_ch1;
static int aw3644_level_ch2;

static int aw3644_set_torch_brightness(int channel, int regval)
{
	int led1regval = 0;

	if (channel == AW3644_CHANNEL_CH1) {
		aw3644_flash_write(AW3644_i2c_client,
			AW3644_REG_TORCH_LEVEL_LED1, regval);
	} else if (channel == AW3644_CHANNEL_CH2) {
		if (aw3644_keepstate_decouple_mode == 1) {//360 torch
			led1regval = aw3644_flash_read(AW3644_i2c_client,
				AW3644_REG_TORCH_LEVEL_LED1);
			aw3644_flash_write(AW3644_i2c_client,
				AW3644_REG_TORCH_LEVEL_LED1, (0x7f&led1regval));
		} else {
			aw3644_flash_write(AW3644_i2c_client,
				AW3644_REG_TORCH_LEVEL_LED1, 0);
		}
		aw3644_flash_write(AW3644_i2c_client,
			AW3644_REG_TORCH_LEVEL_LED2, regval);
	} else {
		pr_info("Error channel\n");
		return -1;
	}
	mdelay(AW3644_WAIT_TIME);
	return 0;
}

static int aw3644_set_strobe_brightness(int channel, int regval)
{
	int led1regval = 0;

	if (channel == AW3644_CHANNEL_CH1) {
		aw3644_flash_write(AW3644_i2c_client,
			AW3644_REG_FLASH_LEVEL_LED1, regval);
	} else if (channel == AW3644_CHANNEL_CH2) {
		if (aw3644_keepstate_decouple_mode == 1) {//360 torch
			led1regval = aw3644_flash_read(AW3644_i2c_client,
				AW3644_REG_FLASH_LEVEL_LED1);
			aw3644_flash_write(AW3644_i2c_client,
				AW3644_REG_FLASH_LEVEL_LED1, (0x7f&led1regval));
		} else {
			aw3644_flash_write(AW3644_i2c_client,
				AW3644_REG_FLASH_LEVEL_LED1, 0);
		}
		aw3644_flash_write(AW3644_i2c_client,
			AW3644_REG_FLASH_LEVEL_LED2, regval);
	} else {
		pr_info("Error channel\n");
		return -1;
	}
	mdelay(AW3644_WAIT_TIME);
	return 0;
}

#if defined(TRAN_KB7J_H615) || defined(TRAN_LC6)///////KB7J///////
static const int aw3644_led2_current[AW3644_LED2_LEVEL_NUM] = {//y=25 + 50*x
	25,   75,   125,  175,  225,  275,  325,  375,
	425,  475,  525,  575,  625,  675,  725,  775,
	825,  875,  925,  975,  1025, 1075, 1125, 1175,
	1225, 1275, 1325, 1375, 1425, 1475
};
//ITORCH(mA)=(Brightness Code*2.91mA)+2.55mA
#if defined(TRAN_LC6)
static const unsigned char aw3644_led2_torch_level[AW3644_LED2_LEVEL_TORCH] = {
	0x7,  0x18, 0x2A, 0x3B, 0x4C, 0x5D, 0x60, 0x66
};
#else
static const unsigned char aw3644_led2_torch_level[AW3644_LED2_LEVEL_TORCH] = {
	0x7,  0x18, 0x2A, 0x3B, 0x4C, 0x5D, 0x6E, 0x7F
};
#endif
//IFLASH(mA)=(Brightness Code*11.72mA)+11.35mA
#if defined(TRAN_LC6)
static const unsigned char aw3644_led2_strobe_level[AW3644_LED2_LEVEL_FLASH] = {
	0x1,  0x5,  0x9,  0xD,  0x12, 0x16, 0x1A, 0x1F,
	0x23, 0x27, 0x2B, 0x30, 0x34, 0x38, 0x3C, 0x41,
	0x45, 0x49, 0x4D, 0x52, 0x56, 0x5A, 0x5F, 0x63,
	0x67, 0x6B, 0x70, 0x74, 0x75, 0x7E
};
#else
static const unsigned char aw3644_led2_strobe_level[AW3644_LED2_LEVEL_FLASH] = {
	0x1,  0x5,  0x9,  0xD,  0x12, 0x16, 0x1A, 0x1F,
	0x23, 0x27, 0x2B, 0x30, 0x34, 0x38, 0x3C, 0x41,
	0x45, 0x49, 0x4D, 0x52, 0x56, 0x5A, 0x5F, 0x63,
	0x67, 0x6B, 0x70, 0x74, 0x78, 0x7E
};
#endif
static const int aw3644_current[AW3644_LEVEL_NUM] = {
	70,   80
};
#if defined(TRAN_LC6)
static const unsigned char aw3644_torch_level[30] = {
	0x32, 0x46
};
static const unsigned char aw3644_strobe_level[30] = {
	0x5,  0xA
};
#else
static const unsigned char aw3644_torch_level[30] = {
	0x1E, 0x20
};
static const unsigned char aw3644_strobe_level[30] = {
	0x2,  0x3
};
#endif

#else
//ITD:modify CBQHLES-5 by quan.chang 181018 start
static const int aw3644_current[AW3644_LEVEL_NUM] = {//y=25 + 50*x
	25,   75,   125,  175,  225,  275,  325,  375,
	425,  475,  525,  575,  625,  675,  725,  775,
	825,  875,  925,  975,  1025, 1075, 1125, 1175,
	1225, 1275, 1325, 1375, 1425, 1475
};
//ITORCH(mA)=(Brightness Code*2.91mA)+2.55mA
static const unsigned char aw3644_torch_level[AW3644_LEVEL_TORCH] = {
	0x7,  0x18, 0x2A, 0x3B, 0x4C, 0x5D, 0x6E, 0x7F
};
//IFLASH(mA)=(Brightness Code*11.72mA)+11.35mA
#if defined(TRAN_CB7) || defined(TRAN_CB7J)
static const unsigned char aw3644_strobe_level[AW3644_LEVEL_FLASH] = {
	0x1,  0x5,  0x9,  0xD,  0x12, 0x16, 0x1A, 0x1F,
	0x23, 0x27, 0x2B, 0x30, 0x34, 0x38, 0x3C, 0x41,
	0x45, 0x49, 0x4D, 0x52, 0x56, 0x5A, 0x5F, 0x63,
	0x67, 0x6B, 0x70, 0x74, 0x78, 0x7F
};
#else
static const unsigned char aw3644_strobe_level[AW3644_LEVEL_FLASH] = {
	0x1,  0x5,  0x9,  0xD,  0x12, 0x16, 0x1A, 0x1F,
	0x23, 0x27, 0x2B, 0x30, 0x34, 0x38, 0x3C, 0x41,
	0x45, 0x49, 0x4D, 0x52, 0x56, 0x5A, 0x5F, 0x63,
	0x67, 0x6B, 0x70, 0x74, 0x78, 0x7C
};
#endif

static const int aw3644_led2_current[30] = {
	70,   80
};
//ITD:modify CBQHLES-5 by quan.chang 181018 start
#if defined(TRAN_X625) || defined(TRAN_X625D) || \
	defined(TRAN_KB3) || defined(TRAN_KB8)
static const unsigned char aw3644_led2_torch_level[30] = {
	0x32, 0x34
};
#else
static const unsigned char aw3644_led2_torch_level[30] = {
	0x17, 0x1B
};
#endif
//ITD:modify CBQHLES-5 by quan.chang 181018 end
static const unsigned char aw3644_led2_strobe_level[30] = {
	0x2,  0x3
};
#endif///////KB7J///////

static int aw3644_is_torch(int channel, int level)
{
	int torch_level = 0;

	if (channel ==  AW3644_CHANNEL_CH1)
		torch_level = AW3644_LEVEL_TORCH;
	else if (channel ==  AW3644_CHANNEL_CH2)
		torch_level = AW3644_LED2_LEVEL_TORCH;

	if (level >= torch_level)
		return -1;

	return 0;
}

static int aw3644_verify_level(int channel, int level)
{
	int level_num = 0;

	if (channel ==  AW3644_CHANNEL_CH1)
		level_num = AW3644_LEVEL_NUM;
	else if (channel ==  AW3644_CHANNEL_CH2)
		level_num = AW3644_LED2_LEVEL_NUM;

	if (level < 0)
		level = 0;
	else if (level >= level_num)
		level = level_num - 1;

	return level;
}

/* flashlight enable function */
static int aw3644_enable(int channel)
{
	int enableregval = 0;

	if (channel == AW3644_CHANNEL_CH1) {
		if (aw3644_en_ch1 == AW3644_ENABLE_FLASH) {
			aw3644_flash_write(AW3644_i2c_client,
				AW3644_REG_ENABLE, 0x0D);
		} else {
			if (aw3644_keepstate_decouple_mode == 1) {//360 torch
				enableregval =
					aw3644_flash_read(AW3644_i2c_client,
						AW3644_REG_ENABLE);
				aw3644_flash_write(AW3644_i2c_client,
					AW3644_REG_ENABLE, (0x09|enableregval));
			} else {
				aw3644_flash_write(AW3644_i2c_client,
					AW3644_REG_ENABLE, 0x09);
			}
		}
	} else {
		if (aw3644_en_ch2 == AW3644_ENABLE_FLASH) {
			aw3644_flash_write(AW3644_i2c_client,
				AW3644_REG_ENABLE, 0x0E);
		} else {
			if (aw3644_keepstate_decouple_mode == 1) {//360 torch
				enableregval =
					aw3644_flash_read(AW3644_i2c_client,
						AW3644_REG_ENABLE);
				aw3644_flash_write(AW3644_i2c_client,
					AW3644_REG_ENABLE, (0x0A|enableregval));
			} else {
				aw3644_flash_write(AW3644_i2c_client,
					AW3644_REG_ENABLE, 0x0A);
			}
		}
	}

	return 0;
}

/* flashlight disable function */
static int aw3644_disable(void)
{
	pr_info("%s\n", __func__);
	aw3644_flash_write(AW3644_i2c_client, AW3644_REG_ENABLE, 0x00);
	return 0;
}

/* set flashlight level */
static int aw3644_set_level(int channel, int lel)
{
	int level = 0;

	level = aw3644_verify_level(channel, lel);
	if (channel == AW3644_CHANNEL_CH1) {
		aw3644_level_ch1 = level;
		if (!aw3644_is_torch(channel, level)) {
			aw3644_set_torch_brightness(
				channel, aw3644_torch_level[level]);
		} else {
			aw3644_set_strobe_brightness(
				channel, aw3644_strobe_level[level]);
		}
	} else if (channel == AW3644_CHANNEL_CH2) {
		aw3644_level_ch2 = level;
		if (!aw3644_is_torch(channel, level)) {
			aw3644_set_torch_brightness(
				channel, aw3644_led2_torch_level[level]);
		} else {
			aw3644_set_strobe_brightness(
				channel, aw3644_led2_strobe_level[level]);
		}
	}
	return 0;
}

static int aw3644_set_scenario(int scenario)
{
	/* set decouple mode */
	aw3644_decouple_mode = scenario & FLASHLIGHT_SCENARIO_DECOUPLE_MASK;
	aw3644_keepstate_decouple_mode =
		scenario & FLASHLIGHT_SCENARIO_KEEPSTATE_DECOUPLE_MASK;
	return 0;
}

/* flashlight init */
static int aw3644_init(void)
{
	pr_info("%s\n", __func__);
	/* clear flashlight state */
	aw3644_en_ch1 = AW3644_DISABLE;
	aw3644_en_ch2 = AW3644_DISABLE;
	/* clear decouple mode */
	aw3644_decouple_mode = FLASHLIGHT_SCENARIO_COUPLE;
	aw3644_keepstate_decouple_mode = FLASHLIGHT_SCENARIO_KEEPSTATE_COUPLE;
	aw3644_pinctrl_set(AW3644_PINCTRL_PIN_HWEN,
		AW3644_PINCTRL_PINSTATE_HIGH);
	mdelay(AW3644_WAIT_TIME);
	aw3644_flash_write(AW3644_i2c_client, AW3644_REG_ENABLE, 0x00);
	aw3644_flash_write(AW3644_i2c_client, AW3644_REG_BOOST_CONFIG, 0x09);
	aw3644_flash_write(AW3644_i2c_client, AW3644_REG_TIMING_CONFIG, 0x1f);

	return 0;
}

/* flashlight uninit */
static int aw3644_uninit(void)
{
	/* clear flashlight state */
	aw3644_en_ch1 = AW3644_NONE;
	aw3644_en_ch2 = AW3644_NONE;
	aw3644_decouple_mode = FLASHLIGHT_SCENARIO_COUPLE;
	aw3644_keepstate_decouple_mode = FLASHLIGHT_SCENARIO_KEEPSTATE_COUPLE;
	aw3644_disable();
	aw3644_pinctrl_set(AW3644_PINCTRL_PIN_HWEN,
		AW3644_PINCTRL_PINSTATE_LOW);

	return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static void aw3644_work_disable_ch1(struct work_struct *data)
{
	pr_debug("ht work queue callback\n");
	aw3644_disable();
}

static void aw3644_work_disable_ch2(struct work_struct *data)
{
	pr_debug("lt work queue callback\n");
	aw3644_disable();
}

static enum hrtimer_restart aw3644_timer_func_ch1(struct hrtimer *timer)
{
	schedule_work(&aw3644_work_ch1);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart aw3644_timer_func_ch2(struct hrtimer *timer)
{
	schedule_work(&aw3644_work_ch2);
	return HRTIMER_NORESTART;
}

static int aw3644_timer_start(int channel, ktime_t ktime)
{
	if (channel == AW3644_CHANNEL_CH1)
		hrtimer_start(&aw3644_timer_ch1, ktime, HRTIMER_MODE_REL);
	else if (channel == AW3644_CHANNEL_CH2)
		hrtimer_start(&aw3644_timer_ch2, ktime, HRTIMER_MODE_REL);
	else {
		pr_info("Error channel\n");
		return -1;
	}

	return 0;
}

static int aw3644_timer_cancel(int channel)
{
	if (channel == AW3644_CHANNEL_CH1)
		hrtimer_cancel(&aw3644_timer_ch1);
	else if (channel == AW3644_CHANNEL_CH2)
		hrtimer_cancel(&aw3644_timer_ch2);
	else {
		pr_info("Error channel\n");
		return -1;
	}

	return 0;
}

static int aw3644_operate(int channel, int enable)
{
	ktime_t ktime;
	unsigned int s;
	unsigned int ns;

	/* setup enable/disable */
	if (channel == AW3644_CHANNEL_CH1) {
		aw3644_en_ch1 = enable;
		if (aw3644_en_ch1)
			if (aw3644_is_torch(channel, aw3644_level_ch1))
				aw3644_en_ch1 = AW3644_ENABLE_FLASH;
	} else if (channel == AW3644_CHANNEL_CH2) {
		aw3644_en_ch2 = enable;
		if (aw3644_en_ch2)
			if (aw3644_is_torch(channel, aw3644_level_ch2))
				aw3644_en_ch2 = AW3644_ENABLE_FLASH;
	} else {
		pr_info("Error channel\n");
		return -1;
	}

	/* decouple mode */
	if (aw3644_decouple_mode) {
		if (channel == AW3644_CHANNEL_CH1) {
			aw3644_en_ch2 = AW3644_DISABLE;
			aw3644_timeout_ms[AW3644_CHANNEL_CH2] = 0;
		} else if (channel == AW3644_CHANNEL_CH2) {
			aw3644_en_ch1 = AW3644_DISABLE;
			aw3644_timeout_ms[AW3644_CHANNEL_CH1] = 0;
		}
	}

	/* operate flashlight and setup timer */
	if ((aw3644_en_ch1 != AW3644_NONE) && (aw3644_en_ch2 != AW3644_NONE)) {
		if ((aw3644_en_ch1 == AW3644_DISABLE) &&
				(aw3644_en_ch2 == AW3644_DISABLE)) {
			aw3644_disable();
			aw3644_timer_cancel(AW3644_CHANNEL_CH1);
			aw3644_timer_cancel(AW3644_CHANNEL_CH2);
		} else {
			if (aw3644_timeout_ms[AW3644_CHANNEL_CH1] &&
				aw3644_en_ch1 != AW3644_DISABLE) {
				s = aw3644_timeout_ms[AW3644_CHANNEL_CH1] /
					1000;
				ns = aw3644_timeout_ms[AW3644_CHANNEL_CH1] %
					1000 * 1000000;
				ktime = ktime_set(s, ns);
				aw3644_timer_start(AW3644_CHANNEL_CH1, ktime);
			}
			if (aw3644_timeout_ms[AW3644_CHANNEL_CH2] &&
				aw3644_en_ch2 != AW3644_DISABLE) {
				s = aw3644_timeout_ms[AW3644_CHANNEL_CH2] /
					1000;
				ns = aw3644_timeout_ms[AW3644_CHANNEL_CH2] %
					1000 * 1000000;
				ktime = ktime_set(s, ns);
				aw3644_timer_start(AW3644_CHANNEL_CH2, ktime);
			}
			aw3644_enable(channel);
		}

		/* clear flashlight state */
//ITD:modify KBSHLES-43 by quan.chang 180919 start
		if ((aw3644_keepstate_decouple_mode == 0)
				|| (channel != AW3644_CHANNEL_CH1))
			aw3644_en_ch1 = AW3644_NONE;
		if ((aw3644_keepstate_decouple_mode == 0)
				|| (channel != AW3644_CHANNEL_CH2))
			aw3644_en_ch2 = AW3644_NONE;
//ITD:modify KBSHLES-43 by quan.chang 180919 end
	}

	return 0;
}

/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int aw3644_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_debug("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		aw3644_timeout_ms[channel] = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_debug("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		aw3644_set_level(channel, fl_arg->arg);
		break;
	case FLASH_IOC_SET_SCENARIO:
		pr_debug("FLASH_IOC_SET_SCENARIO(%d): %d\n",
				channel, (int)fl_arg->arg);
		aw3644_set_scenario(fl_arg->arg);
		break;
	case FLASH_IOC_SET_ONOFF:
		pr_debug("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		aw3644_operate(channel, fl_arg->arg);
		break;
	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int aw3644_open(void)
{
	return 0;
}

static int aw3644_release(void)
{
	/* uninit chip and clear usage count */
	mutex_lock(&aw3644_mutex);
	use_count--;
	if (!use_count)
		aw3644_uninit();
	if (use_count < 0)
		use_count = 0;
	mutex_unlock(&aw3644_mutex);

	pr_info("Release: %d\n", use_count);

	return 0;
}

static int aw3644_set_driver(int set)
{
	int ret = 0;
	/* init chip and set usage count */
	mutex_lock(&aw3644_mutex);
	if (set) {
		if (!use_count)
			ret = aw3644_init();
		use_count++;
		pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = aw3644_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&aw3644_mutex);

	return 0;
}

static ssize_t aw3644_strobe_store(struct flashlight_arg arg)
{
	aw3644_set_driver(0);
	aw3644_set_level(arg.channel, arg.level);
	aw3644_enable(arg.channel);
	mdelay(arg.dur);
	aw3644_disable();
	aw3644_release();

	return 0;
}

static struct flashlight_operations aw3644_ops = {
	aw3644_open,
	aw3644_release,
	aw3644_ioctl,
	aw3644_strobe_store,
	aw3644_set_driver
};


/******************************************************************************
 * I2C device and driver
 *****************************************************************************/
static int aw3644_chip_init(struct aw3644_chip_data *chip)
{
	/* NOTE: Chip initialication move to "set driver" operation
	 * aw3644_init();
	 */

	return 0;
}

static int aw3644_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct aw3644_chip_data *chip;
	struct aw3644_platform_data *pdata = client->dev.platform_data;
	int err;

	pr_info("%s start.\n", __func__);
	/* check i2c */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_info("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}
	/* init chip private data */
	chip = kzalloc(sizeof(struct aw3644_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}
	chip->client = client;

	/* init platform data */
	if (!pdata) {
		pdata = kzalloc(sizeof(struct aw3644_platform_data),
			GFP_KERNEL);
		chip->no_pdata = 1;
	}
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
	AW3644_i2c_client = client;

	/* init mutex and spinlock */
	mutex_init(&chip->lock);

	/* init work queue */
	INIT_WORK(&aw3644_work_ch1, aw3644_work_disable_ch1);
	INIT_WORK(&aw3644_work_ch2, aw3644_work_disable_ch2);

	/* init timer */
	hrtimer_init(&aw3644_timer_ch1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw3644_timer_ch1.function = aw3644_timer_func_ch1;
	hrtimer_init(&aw3644_timer_ch2, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw3644_timer_ch2.function = aw3644_timer_func_ch2;
	aw3644_timeout_ms[AW3644_CHANNEL_CH1] = 100;
	aw3644_timeout_ms[AW3644_CHANNEL_CH2] = 100;
	/* init chip hw */
	aw3644_chip_init(chip);
	/* register flashlight operations */
	if (flashlight_dev_register(AW3644_NAME, &aw3644_ops)) {
		err = -EFAULT;
		goto err_free;
	}
	/* clear usage count */
	use_count = 0;

	pr_info("Probe done.\n");

	return 0;

err_free:
	kfree(chip->pdata);
	i2c_set_clientdata(client, NULL);
	kfree(chip);
err_out:
	return err;
}

static int aw3644_i2c_remove(struct i2c_client *client)
{
	struct aw3644_chip_data *chip = i2c_get_clientdata(client);

	pr_info("Remove start.\n");

	/* flush work queue */
	flush_work(&aw3644_work_ch1);
	flush_work(&aw3644_work_ch2);
	/* unregister flashlight operations */
	flashlight_dev_unregister(AW3644_NAME);

	/* free resource */
	if (chip->no_pdata)
		kfree(chip->pdata);
	kfree(chip);

	pr_info("Remove done.\n");

	return 0;
}

static const struct i2c_device_id aw3644_i2c_id[] = {
	{AW3644_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, aw3644_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id aw3644_i2c_of_match[] = {
	{.compatible = AW3644_DTNAME_I2C},
	{},
};
MODULE_DEVICE_TABLE(of, aw3644_i2c_of_match);
#endif

static struct i2c_driver aw3644_i2c_driver = {
	.driver = {
		   .name = AW3644_NAME,
#ifdef CONFIG_OF
		   .of_match_table = aw3644_i2c_of_match,
#endif
		   },
	.probe = aw3644_i2c_probe,
	.remove = aw3644_i2c_remove,
	.id_table = aw3644_i2c_id,
};

/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int aw3644_probe(struct platform_device *dev)
{
	pr_info("aw3644_platform_probe start.\n");
	/* init pinctrl */
	if (aw3644_pinctrl_init(dev)) {
		pr_debug("Failed to init pinctrl.\n");
		return -1;
	}

	if (i2c_add_driver(&aw3644_i2c_driver)) {
		pr_debug("Failed to add i2c driver.\n");
		return -1;
	}

	pr_info("%s done.\n", __func__);

	return 0;
}

static int aw3644_remove(struct platform_device *dev)
{
	pr_debug("Remove start.\n");

	i2c_del_driver(&aw3644_i2c_driver);

	pr_debug("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aw3644_of_match[] = {
	{.compatible = AW3644_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, aw3644_of_match);
#else
static struct platform_device aw3644_platform_device = {

		.name = AW3644_NAME,
		.id = 0,
		.dev = {}

};
MODULE_DEVICE_TABLE(platform, aw3644_platform_device);
#endif

static struct platform_driver aw3644_platform_driver = {
	.probe = aw3644_probe,
	.remove = aw3644_remove,
	.driver = {
		.name = AW3644_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aw3644_of_match,
#endif
	},
};

static int __init flashlight_aw3644_init(void)
{
	int ret;

	pr_info("flashlight_aw3644_initInit start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&aw3644_platform_device);
	if (ret) {
		pr_info("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&aw3644_platform_driver);
	if (ret) {
		pr_info("Failed to register platform driver\n");
		return ret;
	}

	pr_debug("Init done.\n");

	return 0;
}

static void __exit flashlight_aw3644_exit(void)
{
	pr_debug("Exit start.\n");

	platform_driver_unregister(&aw3644_platform_driver);

	pr_debug("Exit done.\n");
}

late_initcall(flashlight_aw3644_init);
module_exit(flashlight_aw3644_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Simon Wang <Simon-TCH.Wang@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight AW3644 Driver");
