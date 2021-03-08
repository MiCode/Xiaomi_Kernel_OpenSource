/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * Version: v1.0.2
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

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/leds-aw2015.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

/* register address */
#define AW2015_REG_RESET				0x00
#define AW2015_REG_GCR					0x01
#define AW2015_REG_STATUS				0x02
#define AW2015_REG_IMAX 				0x03
#define AW2015_REG_LCFG1				0x04
#define AW2015_REG_LCFG2				0x05
#define AW2015_REG_LCFG3				0x06
#define AW2015_REG_LEDEN				0x07
#define AW2015_REG_LEDCTR				0x08
#define AW2015_REG_PAT_RUN				0x09
#define AW2015_REG_ILED1_1				0x10
#define AW2015_REG_ILED2_1				0x11
#define AW2015_REG_ILED3_1				0x12
#define AW2015_REG_ILED1_2				0x13
#define AW2015_REG_ILED2_2				0x14
#define AW2015_REG_ILED3_2				0x15
#define AW2015_REG_ILED1_3				0x16
#define AW2015_REG_ILED2_3				0x17
#define AW2015_REG_ILED3_3				0x18
#define AW2015_REG_ILED1_4				0x19
#define AW2015_REG_ILED2_4				0x1A
#define AW2015_REG_ILED3_4				0x1B
#define AW2015_REG_PWM1 				0x1C
#define AW2015_REG_PWM2 				0x1D
#define AW2015_REG_PWM3 				0x1E
#define AW2015_REG_PAT1_T1				0x30
#define AW2015_REG_PAT1_T2				0x31
#define AW2015_REG_PAT1_T3				0x32
#define AW2015_REG_PAT1_T4				0x33
#define AW2015_REG_PAT1_T5				0x34
#define AW2015_REG_PAT2_T1				0x35
#define AW2015_REG_PAT2_T2				0x36
#define AW2015_REG_PAT2_T3				0x37
#define AW2015_REG_PAT2_T4				0x38
#define AW2015_REG_PAT2_T5				0x39
#define AW2015_REG_PAT3_T1				0x3A
#define AW2015_REG_PAT3_T2				0x3B
#define AW2015_REG_PAT3_T3				0x3C
#define AW2015_REG_PAT3_T4				0x3D
#define AW2015_REG_PAT3_T5				0x3E

/* register bits */
#define AW2015_CHIPID					0x31
#define AW2015_LED_RESET_MASK 			0x55
#define AW2015_LED_CHIP_DISABLE			0x00
#define AW2015_LED_CHIP_ENABLE_MASK 	0x01
#define AW2015_LED_CHARGE_DISABLE_MASK	0x02
#define AW2015_LED_BREATHE_MODE_MASK	0x01
#define AW2015_LED_ON_MODE_MASK			0x00
#define AW2015_LED_BREATHE_PWM_MASK 	0xFF
#define AW2015_LED_ON_PWM_MASK 			0xFF
#define AW2015_LED_FADEIN_MODE_MASK 	0x02
#define AW2015_LED_FADEOUT_MODE_MASK 	0x04

#define MAX_RISE_TIME_MS				15
#define MAX_HOLD_TIME_MS				15
#define MAX_FALL_TIME_MS				15
#define MAX_OFF_TIME_MS 				15
#define MAX_DELAY_TIME_MS				15
#define MAX_SLOT_TIME_MS				7

/* aw2015 register read/write access */
#define REG_NONE_ACCESS 				0
#define REG_RD_ACCESS					1 << 0
#define REG_WR_ACCESS					1 << 1
#define AW2015_REG_MAX					0x7F

const unsigned char aw2015_reg_access[AW2015_REG_MAX] = {
	[AW2015_REG_RESET]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_GCR]     = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_STATUS]  = REG_RD_ACCESS,
	[AW2015_REG_IMAX]    = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_LCFG1]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_LCFG2]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_LCFG3]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_LEDEN]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_LEDCTR]  = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT_RUN] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_ILED1_1] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_ILED2_1] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_ILED3_1] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_ILED1_2] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_ILED2_2] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_ILED3_2] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_ILED1_3] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_ILED2_3] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_ILED3_3] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_ILED1_4] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_ILED2_4] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_ILED3_4] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PWM1]    = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PWM2]    = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PWM3]    = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT1_T1] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT1_T2] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT1_T3] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT1_T4] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT1_T5] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT2_T1] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT2_T2] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT2_T3] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT2_T4] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT2_T5] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT3_T1] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT3_T2] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT3_T3] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT3_T4] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT3_T5] = REG_RD_ACCESS|REG_WR_ACCESS,
};

/* on/off time of pattern, unit = ms */
static const uint32_t aw2015_ton_toff[] = {
	40,
	130,
	260,
	380,
	510,
	770,
	1040,
	1600,
	2100,
	2600,
	3100,
	4200,
	5200,
	6200,
	7300,
	8300,
};

/* aw2015 led struct */
struct aw2015_led {
	struct i2c_client *client;
	struct led_classdev cdev;
	struct aw2015_platform_data *pdata;
	struct work_struct brightness_work;
	struct mutex lock;
	int num_leds;
	int id;
	bool right; /* on the right side */
};

static int right_led_en_gpio = -1;      /* for p1 as4 and as5 hardware */
static bool right_led_en = false;

static int aw2015_write(struct aw2015_led *led, u8 reg, u8 val)
{
	int ret = -EINVAL, retry_times = 0;

	do {
		ret = i2c_smbus_write_byte_data(led->client, reg, val);
		retry_times ++;
		if (retry_times == 5)
			break;
	} while (ret < 0);

	return ret;
}

static int aw2015_read(struct aw2015_led *led, u8 reg, u8 *val)
{
	int ret = -EINVAL, retry_times = 0;

	do {
		ret = i2c_smbus_read_byte_data(led->client, reg);
		retry_times ++;
		if (retry_times == 5)
			break;
	} while (ret < 0);

	if (ret < 0)
		return ret;

	*val = ret;
	return 0;
}

static void aw2015_brightness_work(struct work_struct *work)
{
	struct aw2015_led *led = container_of(work, struct aw2015_led,
					brightness_work);
	u8 val = 0;

	mutex_lock(&led->pdata->led->lock);

	/* enable aw2015 if disabled */
	aw2015_read(led, AW2015_REG_GCR, &val);
	if (!(val&AW2015_LED_CHIP_ENABLE_MASK)) {
		aw2015_write(led, AW2015_REG_GCR, AW2015_LED_CHIP_ENABLE_MASK | AW2015_LED_CHARGE_DISABLE_MASK);
	}

	if (led->cdev.brightness > 0) {
		if (led->cdev.brightness > led->cdev.max_brightness)
			led->cdev.brightness = led->cdev.max_brightness;
		aw2015_read(led, AW2015_REG_LCFG1 + led->id, &val);
		if (val & AW2015_LED_BREATHE_MODE_MASK) {
			aw2015_read(led, AW2015_REG_LEDEN, &val);
			aw2015_write(led, AW2015_REG_LEDEN, val & (~(1 << led->id)));
		}
		aw2015_write(led, AW2015_REG_LCFG1 + led->id, AW2015_LED_ON_MODE_MASK);
		aw2015_write(led, AW2015_REG_IMAX , led->pdata->imax);
		aw2015_write(led, AW2015_REG_ILED1_1 + led->id, led->cdev.brightness);
		aw2015_write(led, AW2015_REG_PWM1 + led->id, AW2015_LED_ON_PWM_MASK);
		aw2015_read(led, AW2015_REG_LEDEN, &val);
		aw2015_write(led, AW2015_REG_LEDEN, val | (1 << led->id));
	} else {
		aw2015_read(led, AW2015_REG_LEDEN, &val);
		aw2015_write(led, AW2015_REG_LEDEN, val & (~(1 << led->id)));
	}

	/*
	 * If value in AW2015_REG_LEDEN is 0, it means the RGB leds are
	 * all off. So we need to power it off.
	 */
	aw2015_read(led, AW2015_REG_LEDEN, &val);
	if (val == 0) {
		aw2015_write(led, AW2015_REG_LEDCTR, 0); /* clear SYNC flag */
		aw2015_write(led, AW2015_REG_GCR, AW2015_LED_CHIP_DISABLE  | AW2015_LED_CHARGE_DISABLE_MASK);
		mutex_unlock(&led->pdata->led->lock);
		return;
	}

	mutex_unlock(&led->pdata->led->lock);
}

static void aw2015_led_breath_set(struct aw2015_led *led, unsigned long breathing)
{
	u8 val = 0;

	/* enable regulators if they are disabled */
	/* enable aw2015 if disabled */
	aw2015_read(led, AW2015_REG_GCR, &val);
	if (!(val&AW2015_LED_CHIP_ENABLE_MASK)) {
		aw2015_write(led, AW2015_REG_GCR, AW2015_LED_CHIP_ENABLE_MASK | AW2015_LED_CHARGE_DISABLE_MASK);
	}

	if (breathing > 0) {
		u8 i = 0;
		u8 reg_ledctr = 0;
		u8 reg_patx_t4 = 0;

		aw2015_write(led, AW2015_REG_LCFG1 + led->id, AW2015_LED_BREATHE_MODE_MASK);
		aw2015_write(led, AW2015_REG_IMAX , led->pdata->imax);

		for (; i < 4; i++) {
			u8 reg = AW2015_REG_ILED1_1 + led->id + i * 3;
			if (!(aw2015_reg_access[reg]&REG_RD_ACCESS))
				continue;
			aw2015_write(led, reg, led->pdata->currents[i]);
		}
		aw2015_write(led, AW2015_REG_PWM1 + led->id, AW2015_LED_BREATHE_PWM_MASK);
		aw2015_write(led, AW2015_REG_PAT1_T1 + led->id*5,
					(led->pdata->rise_time_ms << 4 | led->pdata->hold_time_ms));
		aw2015_write(led, AW2015_REG_PAT1_T2 + led->id*5,
					(led->pdata->fall_time_ms << 4 | led->pdata->off_time_ms));
		aw2015_write(led, AW2015_REG_PAT1_T3 + led->id*5,
					(led->pdata->slot_time_ms << 4 | led->pdata->delay_time_ms));

		reg_patx_t4 |= (led->pdata->mpulse & 0x03) << 4; /* BIT[5:4] - multiple pulse mode selection */
		switch (breathing) {
			case 5: /* individual, oneshot */
				reg_patx_t4 |= BIT(7);
				break;
			case 4:   /* multi-pattern, multi-color, multi-pulse */
				reg_patx_t4 |= BIT(6); /* pattern switch enable, active only in true-color pattern mode */
				reg_patx_t4 |= led->pdata->cex & 0x0f;  /* BIT[3:0] - color#x display enable */
			case 3:   /* pattern stop after repeating specified times */
				reg_patx_t4 |= BIT(7);
			case 2:  /* true-color */
				reg_ledctr |= BIT(3); /* true-color (sync) */
				break;
			case 1:
			default: /* single-color, individual breathing */
				/* nothing to do, go through */
				break;
		}

		aw2015_write(led, AW2015_REG_LEDCTR, reg_ledctr);
		aw2015_write(led, AW2015_REG_PAT1_T4 + led->id*5, reg_patx_t4);

		aw2015_read(led, AW2015_REG_LEDEN, &val);
		aw2015_write(led, AW2015_REG_LEDEN, val | (1 << led->id));

		if (breathing == 4)
			aw2015_write(led, AW2015_REG_PAT_RUN, BIT(0));
		else
			aw2015_write(led, AW2015_REG_PAT_RUN, (1 << led->id));
	} else {
		aw2015_read(led, AW2015_REG_LEDEN, &val);
		aw2015_write(led, AW2015_REG_LEDEN, val & (~(1 << led->id)));
	}

	/*
	 * If value in AW2015_REG_LEDEN is 0, it means the RGB leds are
	 * all off. So we need to power it off.
	 */
	aw2015_read(led, AW2015_REG_LEDEN, &val);
	if (val == 0) {
		aw2015_write(led, AW2015_REG_LEDCTR, 0); /* clear SYNC flag */
		aw2015_write(led, AW2015_REG_GCR, AW2015_LED_CHIP_DISABLE  | AW2015_LED_CHARGE_DISABLE_MASK);
	}
}

static void aw2015_set_brightness(struct led_classdev *cdev,
			     enum led_brightness brightness)
{
	struct aw2015_led *led = container_of(cdev, struct aw2015_led, cdev);

	led->cdev.brightness = brightness;

	schedule_work(&led->brightness_work);
}

static ssize_t aw2015_breath_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	unsigned long breathing = 0;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &breathing);
	if (ret)
		return ret;
	mutex_lock(&led->pdata->led->lock);
	aw2015_led_breath_set(led, breathing);
	mutex_unlock(&led->pdata->led->lock);

	return len;
}

static ssize_t aw2015_led_time_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);

	return snprintf(buf, PAGE_SIZE, "%d %d %d %d %d %d\n",
			led->pdata->delay_time_ms, led->pdata->rise_time_ms,
			led->pdata->hold_time_ms, led->pdata->fall_time_ms,
			led->pdata->slot_time_ms, led->pdata->off_time_ms);
}

static ssize_t aw2015_led_time_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);
	int rc = 0, delay_time_ms = 0, rise_time_ms = 0, hold_time_ms = 0,
		fall_time_ms = 0, slot_time_ms = 0, off_time_ms = 0;

	rc = sscanf(buf, "%d %d %d %d %d %d",
			&delay_time_ms, &rise_time_ms, &hold_time_ms,
			&fall_time_ms, &slot_time_ms, &off_time_ms);

	mutex_lock(&led->pdata->led->lock);
	led->pdata->delay_time_ms = (delay_time_ms > MAX_DELAY_TIME_MS) ?
				MAX_DELAY_TIME_MS : delay_time_ms;
	led->pdata->rise_time_ms = (rise_time_ms > MAX_RISE_TIME_MS) ?
				MAX_RISE_TIME_MS : rise_time_ms;
	led->pdata->hold_time_ms = (hold_time_ms > MAX_HOLD_TIME_MS) ?
				MAX_HOLD_TIME_MS : hold_time_ms;
	led->pdata->fall_time_ms = (fall_time_ms > MAX_FALL_TIME_MS) ?
				MAX_FALL_TIME_MS : fall_time_ms;
	led->pdata->slot_time_ms = (slot_time_ms > MAX_SLOT_TIME_MS) ?
				MAX_SLOT_TIME_MS : slot_time_ms;
	led->pdata->off_time_ms = (off_time_ms > MAX_OFF_TIME_MS) ?
				MAX_OFF_TIME_MS : off_time_ms;
	//aw2015_led_breath_set(led, 1);
	mutex_unlock(&led->pdata->led->lock);

	return len;
}

static ssize_t aw2015_reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);

	unsigned char i = 0, reg_val = 0;
	ssize_t len = 0;

	for (i = 0; i < AW2015_REG_MAX; i++) {
		if(!(aw2015_reg_access[i]&REG_RD_ACCESS))
		continue;
		aw2015_read(led, i, &reg_val);
		len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x\n", i, reg_val);
	}

	return len;
}

static ssize_t aw2015_reg_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);

	unsigned int databuf[2] = {0};

	if (2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
		aw2015_write(led, (unsigned char)databuf[0], (unsigned char)databuf[1]);
	}

	return len;
}

static ssize_t aw2015_currents_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);
	char tmp_buf[40] = { 0 };
	ssize_t len = 0, buf_len = sizeof(tmp_buf) / sizeof(char);
	int i;
	for (i = 0; i < 4; ++i) {
		if (led->pdata->cex & BIT(i))
			len += snprintf(tmp_buf + len, buf_len - len, "[%d] ", led->pdata->currents[i]);
		else
			len += snprintf(tmp_buf + len, buf_len - len, "%d ", led->pdata->currents[i]);
	}
	len += snprintf(tmp_buf + len, buf_len - len, "\n");

	strcpy(buf, tmp_buf);
	return len;
}

static ssize_t aw2015_currents_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);

	sscanf(buf, "%d %d %d %d",
			&led->pdata->currents[0],
			&led->pdata->currents[1],
			&led->pdata->currents[2],
			&led->pdata->currents[3]);
	return len;
}

static ssize_t aw2015_imax_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);
	uint8_t i = 0;
	ssize_t len = 0;
	const char *imax_strs[] = {"3.1875mA",
		"6.375mA",
		"12.75mA",
		"25.5mA"};

	for (; i < sizeof(imax_strs)/sizeof(char*); ++i) {
		const char *imax_str = imax_strs[i];
		if (i == led->pdata->imax)
			len += scnprintf(buf + len, PAGE_SIZE - len, "[%s] ", imax_str);
		else
			len += scnprintf(buf + len, PAGE_SIZE - len, "%s ", imax_str);
	}
	len += scnprintf(buf + len, PAGE_SIZE - len, "\n");

	return len;
}

static ssize_t aw2015_imax_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);

	sscanf(buf, "%d", &led->pdata->imax);

	if (led->pdata->imax < 0)
		led->pdata->imax = 0;
	else if (led->pdata->imax > 3)
		led->pdata->imax = 3;

	return len;
}

static ssize_t aw2015_cex_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);
	int ce1 = led->pdata->cex & BIT(0) ? 1 : 0;
	int ce2 = led->pdata->cex & BIT(1) ? 1 : 0;
	int ce3 = led->pdata->cex & BIT(2) ? 1 : 0;
	int ce4 = led->pdata->cex & BIT(3) ? 1 : 0;

	return snprintf(buf, PAGE_SIZE, "%d %d %d %d\n", ce1, ce2, ce3, ce4);
}

static ssize_t aw2015_cex_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);
	int i, cex[4] = { 0 };
	sscanf(buf, "%d %d %d %d", &cex[0], &cex[1], &cex[2], &cex[3]);

	for (i = 0; i < sizeof(cex)/sizeof(int); ++i) {
		if (cex[i])
			led->pdata->cex |= BIT(i);
		else
			led->pdata->cex &= ~BIT(i);
	}
	return len;
}

static ssize_t aw2015_mpulse_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);

	ssize_t len = snprintf(buf, PAGE_SIZE, "%d\n", led->pdata->mpulse + 1);
	return len;
}

static ssize_t aw2015_mpulse_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);
	int pulse_num = 0;
	sscanf(buf, "%d", &pulse_num);

	if (pulse_num > 4)
		led->pdata->mpulse = 0x03;
	else if (pulse_num < 1)
		led->pdata->mpulse = 0x00;
	else
		led->pdata->mpulse = pulse_num - 1;

	return len;
}

static ssize_t aw2015_right_led_en_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", right_led_en);
}

static ssize_t aw2015_right_led_en_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	int enable = 0;
	sscanf(buf, "%d", &enable);
	right_led_en = !!enable;

	if (gpio_is_valid(right_led_en_gpio))
		gpio_set_value(right_led_en_gpio, right_led_en);

	return len;
}

static DEVICE_ATTR(breath, 0664, NULL, aw2015_breath_store);
static DEVICE_ATTR(led_time, 0664, aw2015_led_time_show, aw2015_led_time_store);
static DEVICE_ATTR(reg, 0664, aw2015_reg_show, aw2015_reg_store);
static DEVICE_ATTR(currents, 0664, aw2015_currents_show, aw2015_currents_store);
static DEVICE_ATTR(imax, 0664, aw2015_imax_show, aw2015_imax_store);
static DEVICE_ATTR(cex, 0664, aw2015_cex_show, aw2015_cex_store);
static DEVICE_ATTR(mpulse, 0664, aw2015_mpulse_show, aw2015_mpulse_store);
static DEVICE_ATTR(right_led_en, 0664, aw2015_right_led_en_show, aw2015_right_led_en_store);

static struct attribute *aw2015_led_attributes[] = {
	&dev_attr_breath.attr,
	&dev_attr_led_time.attr,
	&dev_attr_reg.attr,
	&dev_attr_currents.attr,
	&dev_attr_imax.attr,
	&dev_attr_cex.attr,
	&dev_attr_mpulse.attr,
	&dev_attr_right_led_en.attr,
	NULL,
};

static struct attribute_group aw2015_led_attr_group = {
	.attrs = aw2015_led_attributes
};

static int aw2015_check_chipid(struct aw2015_led *led)
{
	u8 val = 0;

	aw2015_read(led, AW2015_REG_RESET, &val);
	dev_info(&led->client->dev,"AW2015 chip id %0x",val);
	if (val == AW2015_CHIPID)
		return 0;

	return -EINVAL;
}

static int aw2015_led_err_handle(struct aw2015_led *led_array,
				int parsed_leds)
{
	int i = 0;
	/*
	 * If probe fails, cannot free resource of all LEDs, only free
	 * resources of LEDs which have allocated these resource really.
	 */
	for (i = 0; i < parsed_leds; i++) {
		sysfs_remove_group(&led_array[i].cdev.dev->kobj,
				&aw2015_led_attr_group);
		led_classdev_unregister(&led_array[i].cdev);
		cancel_work_sync(&led_array[i].brightness_work);
		devm_kfree(&led_array->client->dev, led_array[i].pdata);
		led_array[i].pdata = NULL;
	}

	if (!led_array->right && gpio_is_valid(right_led_en_gpio)) {
		gpio_free(right_led_en_gpio);
		right_led_en_gpio = -1;
	}

	return i;
}

/*
 * Find the closest time from aw2015_ton_toff[] and return the index.
 */
static u8 aw2015_led_find_closest_time_idx(uint32_t time)
{
	const u8 cnt = sizeof(aw2015_ton_toff)/sizeof(uint32_t);
	u8 idx;
	if (time <= aw2015_ton_toff[0]) {
		idx = 0;
	} else if (time >= aw2015_ton_toff[cnt-1]) {
		idx = cnt - 1;
	} else {
		int i = 0;
		for (; i < cnt - 1; ++i) {
			if (time >= aw2015_ton_toff[i]
					&& time <= aw2015_ton_toff[i+1])
			{
				if ((time - aw2015_ton_toff[i]) <
						(aw2015_ton_toff[i+1] - time))
				{
					idx = i;
				} else {
					idx = i + 1;
				}
				break;
			}
		}
	}
	return idx;
}

/*
 * This function will be triggered by timer.
 */
static int aw2015_led_set_blink(struct led_classdev *led_cdev,
		unsigned long *on_ms, unsigned long *off_ms)
{
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);
	int rise_time_ms = 0, fall_time_ms = 0, delay_time_ms = 0, slot_time_ms = 0;
	int hold_time_ms = aw2015_led_find_closest_time_idx(*on_ms);
	int off_time_ms = aw2015_led_find_closest_time_idx(*off_ms);
	u8 val = 0;
	bool blink = *on_ms > 0;

	/* enable regulators if they are disabled */
	/* enable aw2015 if disabled */
	aw2015_read(led, AW2015_REG_GCR, &val);
	if (!(val&AW2015_LED_CHIP_ENABLE_MASK)) {
		aw2015_write(led, AW2015_REG_GCR, AW2015_LED_CHIP_ENABLE_MASK | AW2015_LED_CHARGE_DISABLE_MASK);
	}

	if (blink) {
		aw2015_write(led, AW2015_REG_LCFG1 + led->id, AW2015_LED_BREATHE_MODE_MASK);
		aw2015_write(led, AW2015_REG_IMAX , led->pdata->imax);
		aw2015_write(led, AW2015_REG_ILED1_1 + led->id, led->cdev.max_brightness);
		aw2015_write(led, AW2015_REG_PWM1 + led->id, AW2015_LED_BREATHE_PWM_MASK);
		aw2015_write(led, AW2015_REG_PAT1_T1 + led->id*5,
					(rise_time_ms << 4 | hold_time_ms));
		aw2015_write(led, AW2015_REG_PAT1_T2 + led->id*5,
					(fall_time_ms << 4 | off_time_ms));
		aw2015_write(led, AW2015_REG_PAT1_T3 + led->id*5,
					(slot_time_ms << 4 | delay_time_ms));

		/* pattern run forever */
		aw2015_write(led, AW2015_REG_PAT1_T4 + led->id*5, 0);

		aw2015_read(led, AW2015_REG_LEDEN, &val);
		aw2015_write(led, AW2015_REG_LEDEN, val | (1 << led->id));

		aw2015_write(led, AW2015_REG_PAT_RUN, (1 << led->id));
	} else {
		aw2015_read(led, AW2015_REG_LEDEN, &val);
		aw2015_write(led, AW2015_REG_LEDEN, val & (~(1 << led->id)));
	}

	/*
	 * If value in AW2015_REG_LEDEN is 0, it means the RGB leds are
	 * all off. So we need to power it off.
	 */
	aw2015_read(led, AW2015_REG_LEDEN, &val);
	if (val == 0) {
		aw2015_write(led, AW2015_REG_GCR, AW2015_LED_CHIP_DISABLE  | AW2015_LED_CHARGE_DISABLE_MASK);
	}

	return 0;
}

static int aw2015_led_parse_child_node(struct aw2015_led *led_array,
				struct device_node *node)
{
	struct aw2015_led *led;
	struct device_node *temp;
	struct aw2015_platform_data *pdata;
	int rc = 0, parsed_leds = 0;

	for_each_child_of_node(node, temp) {
		led = &led_array[parsed_leds];
		led->client = led_array->client;
		led->right = led_array->right;

		pdata = devm_kzalloc(&led->client->dev,
				sizeof(struct aw2015_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&led->client->dev,
				"Failed to allocate memory\n");
			goto free_err;
		}
		pdata->led = led_array;
		led->pdata = pdata;

		rc = of_property_read_string(temp, "aw2015,name",
			&led->cdev.name);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading led name, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2015,id",
			&led->id);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading id, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2015,imax",
			&led->pdata->imax);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading imax, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2015,led-current",
			&led->pdata->currents[0]);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading led-current, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2015,max-brightness",
			&led->cdev.max_brightness);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading max-brightness, rc = %d\n",
				rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2015,rise-time-ms",
			&led->pdata->rise_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading rise-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2015,hold-time-ms",
			&led->pdata->hold_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading hold-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2015,fall-time-ms",
			&led->pdata->fall_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading fall-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2015,off-time-ms",
			&led->pdata->off_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading off-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		led->cdev.default_trigger = of_get_property(temp,
				"linux,default-trigger", NULL);
		led->cdev.flags |= LED_KEEP_TRIGGER;

		led->cdev.blink_set = aw2015_led_set_blink;

		INIT_WORK(&led->brightness_work, aw2015_brightness_work);

		led->cdev.brightness_set = aw2015_set_brightness;

		rc = led_classdev_register(&led->client->dev, &led->cdev);
		if (rc) {
			dev_err(&led->client->dev,
				"unable to register led %d,rc=%d\n",
				led->id, rc);
			goto free_pdata;
		}

		rc = sysfs_create_group(&led->cdev.dev->kobj,
				&aw2015_led_attr_group);
		if (rc) {
			dev_err(&led->client->dev, "led sysfs rc: %d\n", rc);
			goto free_class;
		}
		parsed_leds++;
	}

	return 0;

free_class:
	aw2015_led_err_handle(led_array, parsed_leds);
	led_classdev_unregister(&led_array[parsed_leds].cdev);
	cancel_work_sync(&led_array[parsed_leds].brightness_work);
	devm_kfree(&led->client->dev, led_array[parsed_leds].pdata);
	led_array[parsed_leds].pdata = NULL;
	return rc;

free_pdata:
	aw2015_led_err_handle(led_array, parsed_leds);
	devm_kfree(&led->client->dev, led_array[parsed_leds].pdata);
	return rc;

free_err:
	aw2015_led_err_handle(led_array, parsed_leds);
	return rc;
}

extern char *saved_command_line;
static int aw2015_led_get_board_version(void)
{
	int version = 0;
	char boot[5] = {'\0'};
	char *match = (char *) strnstr(saved_command_line,
				"androidboot.hwlevel=",
				strlen(saved_command_line));
	if (match) {
		memcpy(boot, (match + strlen("androidboot.hwlevel=")),
			sizeof(boot) - 1);
		pr_info("%s: hwlevel is %s\n", __func__, boot);
		if (!strncmp(boot, "P0.1", strlen("P0.1")))
			version = 0;
		else if (!strncmp(boot, "P1", strlen("P1")))
			version = 1;
		else if (!strncmp(boot, "P2", strlen("P2")))
			version = 2;
		else if (!strncmp(boot, "MP", strlen("MP")))
			version = 3;
		else
			version = 4;
	}
	return version;
}

static bool aw2015_led_is_global(void)
{
	bool global = false;
	char boot[3] = {'\0'};
	char *match = (char *) strnstr(saved_command_line,
				"androidboot.hwc=",
				strlen(saved_command_line));
	if (match) {
		memcpy(boot, (match + strlen("androidboot.hwc=")),
			sizeof(boot) - 1);
		pr_info("%s: hw country is %s\n", __func__, boot);
		if (!strncmp(boot, "GL", strlen("GL")))
			global = true;
	}
	return global;
}

static int aw2015_led_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct aw2015_led *led_array;
	struct device_node *node = client->dev.of_node;
	int ret = -EINVAL, num_leds = 0;
	bool right = false;

	if (node == NULL)
		return -EINVAL;

	right = of_property_read_bool(node, "xiaomi,rgb-led-right");

	/* China/India P2 has double aw2015s, one for left, the other for right.
	 * Note: Global P1 is based on China P2 */
	if (right && aw2015_led_get_board_version() < 2 && !aw2015_led_is_global()) {
		pr_info("%s: China/India P0.1 and P1 hardware don't have right aw2015\n", __func__);
		return -EINVAL;
	}

	num_leds = of_get_child_count(node);
	if (!num_leds)
		return -EINVAL;

	/* China/India P1 AS4 and AS5 use gpio to switch right leds */
	if (!right && aw2015_led_get_board_version() == 1 && !aw2015_led_is_global()) {
		right_led_en_gpio = of_get_named_gpio_flags(node, "xiaomi,right-led-en-gpio", 0, NULL);
		if (gpio_is_valid(right_led_en_gpio)) {
			if (gpio_request(right_led_en_gpio, "right_led_en_gpio") == 0) {
				if (gpio_direction_output(right_led_en_gpio, 0)) {
					pr_err("%s: failed to set gpio%d output low\n", __func__, right_led_en_gpio);
					gpio_free(right_led_en_gpio);
					right_led_en_gpio = -1;
				}
			}
		}
	}

	led_array = devm_kzalloc(&client->dev,
			(sizeof(struct aw2015_led) * num_leds), GFP_KERNEL);
	if (!led_array)
		return -ENOMEM;

	led_array->right = right;
	led_array->client = client;
	led_array->num_leds = num_leds;

	mutex_init(&led_array->lock);

	ret = aw2015_led_parse_child_node(led_array, node);
	if (ret) {
		dev_err(&client->dev, "parsed node error\n");
		goto free_led_arry;
	}

	i2c_set_clientdata(client, led_array);

	ret = aw2015_check_chipid(led_array);
	if (ret) {
		dev_err(&client->dev, "Check chip id error\n");
		goto fail_parsed_node;
	}

	return 0;

fail_parsed_node:
	aw2015_led_err_handle(led_array, num_leds);
free_led_arry:
	mutex_destroy(&led_array->lock);
	devm_kfree(&client->dev, led_array);
	led_array = NULL;
	return ret;
}

static int aw2015_led_remove(struct i2c_client *client)
{
	struct aw2015_led *led_array = i2c_get_clientdata(client);
	int i = 0, parsed_leds = led_array->num_leds;

	if (!led_array->right && gpio_is_valid(right_led_en_gpio)) {
		gpio_free(right_led_en_gpio);
		right_led_en_gpio = -1;
	}

	for (i = 0; i < parsed_leds; i++) {
		sysfs_remove_group(&led_array[i].cdev.dev->kobj,
				&aw2015_led_attr_group);
		led_classdev_unregister(&led_array[i].cdev);
		cancel_work_sync(&led_array[i].brightness_work);
		devm_kfree(&client->dev, led_array[i].pdata);
		led_array[i].pdata = NULL;
	}
	mutex_destroy(&led_array->lock);
	devm_kfree(&client->dev, led_array);
	led_array = NULL;
	return 0;
}

static void aw2015_led_shutdown(struct i2c_client *client)
{
	struct aw2015_led *led = i2c_get_clientdata(client);

	aw2015_write(led, AW2015_REG_RESET, AW2015_LED_RESET_MASK);
}

static const struct i2c_device_id aw2015_led_id[] = {
	{"aw2015_led", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, aw2015_led_id);

static struct of_device_id aw2015_match_table[] = {
	{ .compatible = "awinic,aw2015_led",},
	{ },
};

static struct i2c_driver aw2015_led_driver = {
	.probe = aw2015_led_probe,
	.remove = aw2015_led_remove,
	.shutdown = aw2015_led_shutdown,
	.driver = {
		.name = "aw2015_led",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw2015_match_table),
	},
	.id_table = aw2015_led_id,
};

static int __init aw2015_led_init(void)
{
	return i2c_add_driver(&aw2015_led_driver);
}
module_init(aw2015_led_init);

static void __exit aw2015_led_exit(void)
{
	i2c_del_driver(&aw2015_led_driver);
}
module_exit(aw2015_led_exit);

MODULE_AUTHOR("<liweilei@awinic.com.cn>");
MODULE_DESCRIPTION("AWINIC AW2015 LED driver");
MODULE_LICENSE("GPL v2");
