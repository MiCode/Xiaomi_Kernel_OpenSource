/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/spmi.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include "leds.h"

#define FLASH_LED_PERIPHERAL_SUBTYPE(base)			(base + 0x05)
#define FLASH_SAFETY_TIMER(base)				(base + 0x40)
#define FLASH_MAX_CURRENT(base)					(base + 0x41)
#define FLASH_LED0_CURRENT(base)				(base + 0x42)
#define FLASH_LED1_CURRENT(base)				(base + 0x43)
#define	FLASH_CLAMP_CURRENT(base)				(base + 0x44)
#define FLASH_MODULE_ENABLE_CTRL(base)				(base + 0x46)
#define	FLASH_LED_STROBE_CTRL(base)				(base + 0x47)
#define FLASH_LED_TMR_CTRL(base)				(base + 0x48)
#define FLASH_HEADROOM(base)					(base + 0x4A)
#define	FLASH_STARTUP_DELAY(base)				(base + 0x4B)
#define FLASH_VREG_OK_FORCE(base)				(base + 0x4F)
#define FLASH_FAULT_DETECT(base)				(base + 0x51)
#define	FLASH_THERMAL_DRATE(base)				(base + 0x52)
#define	FLASH_CURRENT_RAMP(base)				(base + 0x54)
#define	FLASH_VPH_PWR_DROOP(base)				(base + 0x5A)
#define	FLASH_LED_UNLOCK_SECURE(base)				(base + 0xD0)
#define	FLASH_TORCH(base)					(base + 0xE4)

#define	FLASH_HEADROOM_MASK					0x03
#define FLASH_STARTUP_DLY_MASK					0x03
#define	FLASH_VREG_OK_FORCE_MASK				0xC0
#define	FLASH_FAULT_DETECT_MASK					0x80
#define	FLASH_THERMAL_DERATE_MASK				0xBF
#define FLASH_SECURE_MASK					0xFF
#define FLASH_TORCH_MASK					0x03
#define FLASH_CURRENT_MASK					0x7F
#define FLASH_TMR_MASK						0x03
#define FLASH_TMR_SAFETY					0x00
#define FLASH_SAFETY_TIMER_MASK					0x7F
#define FLASH_MODULE_ENABLE_MASK				0xE0
#define FLASH_STROBE_MASK					0xC0
#define FLASH_CURRENT_RAMP_MASK					0xBF
#define FLASH_VPH_PWR_DROOP_MASK				0xF3

#define FLASH_LED_TRIGGER_DEFAULT				"none"
#define FLASH_LED_HEADROOM_DEFAULT_MV				500
#define FLASH_LED_STARTUP_DELAY_DEFAULT_US			128
#define FLASH_LED_CLAMP_CURRENT_DEFAULT_MA			200
#define	FLASH_LED_THERMAL_DERATE_THRESHOLD_DEFAULT_C		80
#define	FLASH_LED_RAMP_UP_STEP_DEFAULT_US			3
#define	FLASH_LED_RAMP_DN_STEP_DEFAULT_US			3
#define	FLASH_LED_VPH_PWR_DROOP_THRESHOLD_DEFAULT_MV		3200
#define	FLASH_LED_VPH_PWR_DROOP_DEBOUNCE_TIME_DEFAULT_US	10
#define FLASH_LED_THERMAL_DERATE_RATE_DEFAULT_PERCENT		2
#define FLASH_RAMP_UP_DELAY_US					1000
#define FLASH_RAMP_DN_DELAY_US					2160
#define FLASH_BOOST_REGULATOR_PROBE_DELAY_MS			2000
#define	FLASH_TORCH_MAX_LEVEL					0x0F
#define	FLASH_MAX_LEVEL						0x4F
#define	FLASH_LED_FLASH_HW_VREG_OK				0x40
#define	FLASH_LED_FLASH_SW_VREG_OK				0x80
#define FLASH_LED_STROBE_TYPE_HW				0x40
#define	FLASH_DURATION_DIVIDER					10
#define	FLASH_LED_HEADROOM_DIVIDER				100
#define	FLASH_LED_HEADROOM_OFFSET				2
#define	FLASH_LED_MAX_CURRENT_MA				1000
#define	FLASH_LED_THERMAL_THRESHOLD_MIN				80
#define	FLASH_LED_THERMAL_DEVIDER				10
#define	FLASH_LED_VPH_DROOP_THRESHOLD_MIN_MV			2500
#define	FLASH_LED_VPH_DROOP_THRESHOLD_DIVIDER			100

#define FLASH_UNLOCK_SECURE					0xA5
#define FLASH_LED_TORCH_ENABLE					0x00
#define FLASH_LED_TORCH_DISABLE					0x03
#define FLASH_MODULE_ENABLE					0x80
#define FLASH_LED0_TRIGGER					0x80
#define FLASH_LED1_TRIGGER					0x40
#define FLASH_LED0_ENABLEMENT					0x40
#define FLASH_LED1_ENABLEMENT					0x20
#define FLASH_LED_DISABLE					0x00
#define	FLASH_LED_MIN_CURRENT_MA				13
#define FLASH_SUBTYPE_DUAL					0x01
#define FLASH_SUBTYPE_SINGLE					0x02

/*
 * ID represents physical LEDs for individual control purpose.
 */
enum flash_led_id {
	FLASH_LED_0 = 0,
	FLASH_LED_1,
};

enum flash_led_type {
	FLASH = 0,
	TORCH,
};

enum thermal_derate_rate {
	RATE_2_PERCENT = 0,
	RATE_2P5_PERCENT,
	RATE_4_PERCENT,
	RATE_5_PERCENT,
};

enum current_ramp_steps {
	RAMP_STEP_0P2_US = 0,
	RAMP_STEP_0P4_US,
	RAMP_STEP_0P8_US,
	RAMP_STEP_1P6_US,
	RAMP_STEP_3P3_US,
	RAMP_STEP_6P7_US,
	RAMP_STEP_13P5_US,
	RAMP_STEP_27US,
};

/*
 * Configurations for each individual LED
 */
struct flash_node_data {
	struct spmi_device		*spmi_dev;
	struct led_classdev		cdev;
	struct regulator		*boost_regulator;
	struct work_struct		work;
	struct delayed_work		dwork;
	u32				boost_voltage_max;
	u16				duration;
	u16				max_current;
	u16				current_addr;
	u16				prgm_current;
	u8				id;
	u8				type;
	u8				trigger;
	u8				enable;
	bool				flash_on;
};

/*
 * Flash LED configuration read from device tree
 */
struct flash_led_platform_data {
	u16				ramp_up_step;
	u16				ramp_dn_step;
	u16				vph_pwr_droop_threshold;
	u16				headroom;
	u16				clamp_current;
	u8				thermal_derate_threshold;
	u8				vph_pwr_droop_debounce_time;
	u8				startup_dly;
	u8				thermal_derate_rate;
	bool				pmic_charger_support;
	bool				self_check_en;
	bool				thermal_derate_en;
	bool				current_ramp_en;
	bool				vph_pwr_droop_en;
};

/*
 * Flash LED data structure containing flash LED attributes
 */
struct qpnp_flash_led {
	struct spmi_device		*spmi_dev;
	struct flash_led_platform_data	*pdata;
	struct flash_node_data		*flash_node;
	struct mutex			flash_led_lock;
	int				num_leds;
	u16				base;
	u8				peripheral_type;
};

static u8 qpnp_flash_led_ctrl_dbg_regs[] = {
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
	0x4A, 0x4B, 0x4F, 0x51, 0x52, 0x54, 0x55, 0x5A
};

static ssize_t qpnp_led_strobe_type_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct flash_node_data *flash_node;
	unsigned long state;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &state);
	if (ret)
		return ret;

	flash_node = container_of(led_cdev, struct flash_node_data, cdev);

	/* '0' for sw strobe; '1' for hw strobe */
	if (state == 1)
		flash_node->trigger |= FLASH_LED_STROBE_TYPE_HW;
	else
		flash_node->trigger &= ~FLASH_LED_STROBE_TYPE_HW;

	return count;
}

static ssize_t qpnp_flash_led_dump_regs_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct qpnp_flash_led *led;
	struct flash_node_data *flash_node;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	int rc, i, count = 0;
	u16 addr;
	u8 val;

	flash_node = container_of(led_cdev, struct flash_node_data, cdev);
	led = dev_get_drvdata(&flash_node->spmi_dev->dev);
	for (i = 0; i < ARRAY_SIZE(qpnp_flash_led_ctrl_dbg_regs); i++) {
		addr = led->base + qpnp_flash_led_ctrl_dbg_regs[i];
		rc = spmi_ext_register_readl(led->spmi_dev->ctrl,
			led->spmi_dev->sid, addr, &val, 1);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Unable to read from addr=%x, rc(%d)\n",
				addr, rc);
			return -EINVAL;
		}

		count += snprintf(buf + count, PAGE_SIZE - count,
				"REG_0x%x = 0x%x\n", addr, val);

		if (count >= PAGE_SIZE)
			return PAGE_SIZE - 1;
	}

	return count;
}

static struct device_attribute qpnp_flash_led_attrs[] = {
	__ATTR(strobe, (S_IRUGO | S_IWUSR | S_IWGRP),
				NULL,
				qpnp_led_strobe_type_store),
	__ATTR(reg_dump, (S_IRUGO | S_IWUSR | S_IWGRP),
				qpnp_flash_led_dump_regs_show,
				NULL),
};

static int
qpnp_led_masked_write(struct spmi_device *spmi_dev, u16 addr, u8 mask, u8 val)
{
	int rc;
	u8 reg;

	rc = spmi_ext_register_readl(spmi_dev->ctrl, spmi_dev->sid,
				addr, &reg, 1);
	if (rc)
		dev_err(&spmi_dev->dev,
			"Unable to read from addr=%x, rc(%d)\n", addr, rc);

	reg &= ~mask;
	reg |= val;

	rc = spmi_ext_register_writel(spmi_dev->ctrl, spmi_dev->sid,
				addr, &reg, 1);
	if (rc)
		dev_err(&spmi_dev->dev,
			"Unable to write to addr=%x, rc(%d)\n", addr, rc);

	dev_dbg(&spmi_dev->dev, "Write 0x%02X to addr 0x%02X\n", val, addr);

	return rc;
}

static int qpnp_flash_led_get_thermal_derate_rate(const char *rate)
{
	/*
	 * return 4% derate as default value if user specifies
	 * a value un-supported
	 */
	if (strcmp(rate, "2_PERCENT") == 0)
		return RATE_2_PERCENT;
	else if (strcmp(rate, "2P5_PERCENT") == 0)
		return RATE_2P5_PERCENT;
	else if (strcmp(rate, "5_PERCENT") == 0)
		return RATE_5_PERCENT;
	else
		return RATE_4_PERCENT;
}

static int qpnp_flash_led_get_ramp_step(const char *step)
{
	/*
	 * return 27 us as default value if user specifies
	 * a value un-supported
	 */
	if (strcmp(step, "0P2_US") == 0)
		return RAMP_STEP_0P2_US;
	else if (strcmp(step, "0P4_US") == 0)
		return RAMP_STEP_0P4_US;
	else if (strcmp(step, "0P8_US") == 0)
		return RAMP_STEP_0P8_US;
	else if (strcmp(step, "1P6_US") == 0)
		return RAMP_STEP_1P6_US;
	else if (strcmp(step, "3P3_US") == 0)
		return RAMP_STEP_3P3_US;
	else if (strcmp(step, "6P7_US") == 0)
		return RAMP_STEP_6P7_US;
	else if (strcmp(step, "13P5_US") == 0)
		return RAMP_STEP_13P5_US;
	else
		return RAMP_STEP_27US;
}

static u8 qpnp_flash_led_get_droop_debounce_time(u8 val)
{
	/*
	 * return 10 us as default value if user specifies
	 * a value un-supported
	 */
	switch (val) {
	case 0:
		return 0;
	case 10:
		return 1;
	case 32:
		return 2;
	case 64:
		return 3;
	default:
		return 1;
	}
}

static u8 qpnp_flash_led_get_startup_dly(u8 val)
{
	/*
	 * return 128 us as default value if user specifies
	 * a value un-supported
	 */
	switch (val) {
	case 10:
		return 0;
	case 32:
		return 1;
	case 64:
		return 2;
	case 128:
		return 3;
	default:
		return 3;
	}
}

static int
qpnp_flash_led_get_peripheral_type(struct qpnp_flash_led *led)
{
	int rc;
	u8 val;

	rc = spmi_ext_register_readl(led->spmi_dev->ctrl,
				led->spmi_dev->sid,
				FLASH_LED_PERIPHERAL_SUBTYPE(led->base),
				&val, 1);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
				"Unable to read peripheral subtype\n");
		return -EINVAL;
	}

	return val;
}

static int qpnp_flash_led_module_disable(struct qpnp_flash_led *led,
				struct flash_node_data *flash_node)
{
	int rc;
	u8 val, tmp;

	rc = spmi_ext_register_readl(led->spmi_dev->ctrl,
				led->spmi_dev->sid,
				FLASH_LED_STROBE_CTRL(led->base),
				&val, 1);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
				"Unable to read module enable reg\n");
		return -EINVAL;
	}

	tmp = ~flash_node->trigger & val;
	if (!tmp) {
		rc = qpnp_led_masked_write(led->spmi_dev,
			FLASH_MODULE_ENABLE_CTRL(led->base),
			FLASH_MODULE_ENABLE_MASK, FLASH_LED_DISABLE);
		if (rc) {
			dev_err(&led->spmi_dev->dev, "Module disable failed\n");
			return -EINVAL;
		}
	} else {
		rc = qpnp_led_masked_write(led->spmi_dev,
			FLASH_MODULE_ENABLE_CTRL(led->base),
			flash_node->enable, flash_node->enable);
		if (rc) {
			dev_err(&led->spmi_dev->dev, "Module disable failed\n");
			return -EINVAL;
		}
	}

	return 0;
}

static enum
led_brightness qpnp_flash_led_brightness_get(struct led_classdev *led_cdev)
{
	return led_cdev->brightness;
}

static void qpnp_flash_led_work(struct work_struct *work)
{
	struct flash_node_data *flash_node = container_of(work,
				struct flash_node_data, work);
	struct qpnp_flash_led *led =
			dev_get_drvdata(&flash_node->spmi_dev->dev);
	int rc, brightness = flash_node->cdev.brightness;
	u8 val;

	mutex_lock(&led->flash_led_lock);

	if (!brightness)
		goto turn_off;

	if (brightness < FLASH_LED_MIN_CURRENT_MA)
		brightness = FLASH_LED_MIN_CURRENT_MA;

	flash_node->prgm_current = brightness;

	if (flash_node->boost_regulator && !flash_node->flash_on) {
		if (regulator_count_voltages(flash_node->boost_regulator)
									> 0) {
			rc = regulator_set_voltage(flash_node->boost_regulator,
				flash_node->boost_voltage_max,
				flash_node->boost_voltage_max);
			if (rc) {
				dev_err(&led->spmi_dev->dev,
				"boost regulator set voltage failed\n");
				mutex_unlock(&led->flash_led_lock);
				return;
			}
		}

		rc = regulator_enable(flash_node->boost_regulator);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Boost regulator enablement failed\n");
			goto error_regulator_enable;
		}
	}

	if (flash_node->type == TORCH) {
		rc = qpnp_led_masked_write(led->spmi_dev,
			FLASH_LED_UNLOCK_SECURE(led->base),
			FLASH_SECURE_MASK, FLASH_UNLOCK_SECURE);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Secure reg write failed\n");
			goto exit_flash_led_work;
		}

		rc = qpnp_led_masked_write(led->spmi_dev,
			FLASH_TORCH(led->base),
			FLASH_TORCH_MASK, FLASH_LED_TORCH_ENABLE);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Torch reg write failed\n");
			goto exit_flash_led_work;
		}

		val = (u8)(flash_node->prgm_current * FLASH_TORCH_MAX_LEVEL
				/ flash_node->max_current);
		rc = qpnp_led_masked_write(led->spmi_dev,
			flash_node->current_addr,
			FLASH_CURRENT_MASK, val);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Current reg write failed\n");
			goto exit_flash_led_work;
		}

		rc = qpnp_led_masked_write(led->spmi_dev,
			FLASH_MAX_CURRENT(led->base),
			FLASH_CURRENT_MASK, FLASH_TORCH_MAX_LEVEL);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
					"Max current reg write failed\n");
			goto exit_flash_led_work;
		}

		rc = qpnp_led_masked_write(led->spmi_dev,
			FLASH_MODULE_ENABLE_CTRL(led->base),
			FLASH_MODULE_ENABLE | flash_node->enable,
			FLASH_MODULE_ENABLE | flash_node->enable);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Module enable reg write failed\n");
			goto exit_flash_led_work;
		}

		rc = qpnp_led_masked_write(led->spmi_dev,
			FLASH_LED_STROBE_CTRL(led->base),
			flash_node->trigger,
			flash_node->trigger);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Strobe ctrl reg write failed\n");
			goto exit_flash_led_work;
		}
	} else if (flash_node->type == FLASH) {
		val = (u8)((flash_node->duration - FLASH_DURATION_DIVIDER)
						/ FLASH_DURATION_DIVIDER);
		rc = qpnp_led_masked_write(led->spmi_dev,
			FLASH_SAFETY_TIMER(led->base),
			FLASH_SAFETY_TIMER_MASK, val);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Safety timer reg write failed\n");
			goto exit_flash_led_work;
		}

		rc = qpnp_led_masked_write(led->spmi_dev,
			FLASH_MAX_CURRENT(led->base),
			FLASH_CURRENT_MASK, FLASH_MAX_LEVEL);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Max current reg write failed\n");
			goto exit_flash_led_work;
		}

		val = (u8)(flash_node->prgm_current * FLASH_MAX_LEVEL
				/ flash_node->max_current);
		rc = qpnp_led_masked_write(led->spmi_dev,
				flash_node->current_addr,
				FLASH_CURRENT_MASK, val);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Current reg write failed\n");
			goto exit_flash_led_work;
		}

		rc = qpnp_led_masked_write(led->spmi_dev,
				FLASH_MODULE_ENABLE_CTRL(led->base),
				FLASH_MODULE_ENABLE |
				flash_node->enable,
				FLASH_MODULE_ENABLE |
				flash_node->enable);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Module enable reg write failed\n");
			goto exit_flash_led_work;
		}

		usleep(FLASH_RAMP_UP_DELAY_US);

		rc = qpnp_led_masked_write(led->spmi_dev,
			FLASH_LED_STROBE_CTRL(led->base),
			flash_node->trigger,
			flash_node->trigger);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Strobe reg write failed\n");
			goto exit_flash_led_work;
		}
	}

	flash_node->flash_on = true;
	mutex_unlock(&led->flash_led_lock);

	return;

turn_off:
	rc = qpnp_led_masked_write(led->spmi_dev,
			FLASH_LED_STROBE_CTRL(led->base),
			flash_node->trigger, FLASH_LED_DISABLE);
	if (rc) {
		dev_err(&led->spmi_dev->dev, "Strobe disable failed\n");
		goto exit_flash_led_work;
	}

	if (flash_node->type == TORCH) {
		rc = qpnp_led_masked_write(led->spmi_dev,
			FLASH_LED_UNLOCK_SECURE(led->base),
			FLASH_SECURE_MASK, FLASH_UNLOCK_SECURE);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Secure reg write failed\n");
			goto exit_flash_led_work;
		}

		rc = qpnp_led_masked_write(led->spmi_dev,
			FLASH_TORCH(led->base),
			FLASH_TORCH_MASK, FLASH_LED_TORCH_DISABLE);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Torch reg write failed\n");
			goto exit_flash_led_work;
		}
	}

	usleep(FLASH_RAMP_DN_DELAY_US);

	rc = qpnp_flash_led_module_disable(led, flash_node);
	if (rc) {
		dev_err(&led->spmi_dev->dev, "Module disable failed\n");
		goto exit_flash_led_work;
	}

exit_flash_led_work:
	if (flash_node->boost_regulator && flash_node->flash_on) {
		regulator_disable(flash_node->boost_regulator);
error_regulator_enable:
		if (regulator_count_voltages(flash_node->boost_regulator) > 0)
			regulator_set_voltage(flash_node->boost_regulator,
				0, flash_node->boost_voltage_max);
	}

	flash_node->flash_on = false;
	mutex_unlock(&led->flash_led_lock);

	return;
}

static void qpnp_flash_led_brightness_set(struct led_classdev *led_cdev,
						enum led_brightness value)
{
	struct flash_node_data *flash_node;

	flash_node = container_of(led_cdev, struct flash_node_data, cdev);
	if (value < LED_OFF) {
		pr_err("Invalid brightness value\n");
		return;
	}

	if (value > flash_node->cdev.max_brightness)
		value = flash_node->cdev.max_brightness;

	flash_node->cdev.brightness = value;

	schedule_work(&flash_node->work);

	return;
}

static int qpnp_flash_led_init_settings(struct qpnp_flash_led *led)
{
	int rc;
	u8 val, temp_val;

	rc = qpnp_led_masked_write(led->spmi_dev,
			FLASH_MODULE_ENABLE_CTRL(led->base),
			FLASH_MODULE_ENABLE_MASK, FLASH_LED_DISABLE);
	if (rc) {
		dev_err(&led->spmi_dev->dev, "Module disable failed\n");
		return rc;
	}

	rc = qpnp_led_masked_write(led->spmi_dev,
			FLASH_LED_STROBE_CTRL(led->base),
			FLASH_STROBE_MASK, FLASH_LED_DISABLE);
	if (rc) {
		dev_err(&led->spmi_dev->dev, "Strobe disable failed\n");
		return rc;
	}

	rc = qpnp_led_masked_write(led->spmi_dev,
					FLASH_LED_TMR_CTRL(led->base),
					FLASH_TMR_MASK, FLASH_TMR_SAFETY);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
			"LED timer ctrl reg write failed(%d)\n", rc);
		return rc;
	}

	val = (u8)(led->pdata->headroom / FLASH_LED_HEADROOM_DIVIDER -
						FLASH_LED_HEADROOM_OFFSET);
	rc = qpnp_led_masked_write(led->spmi_dev,
						FLASH_HEADROOM(led->base),
						FLASH_HEADROOM_MASK, val);
	if (rc) {
		dev_err(&led->spmi_dev->dev, "Headroom reg write failed\n");
		return rc;
	}

	val = qpnp_flash_led_get_startup_dly(led->pdata->startup_dly);

	rc = qpnp_led_masked_write(led->spmi_dev,
					FLASH_STARTUP_DELAY(led->base),
						FLASH_STARTUP_DLY_MASK, val);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
					"Startup delay reg write failed\n");
		return rc;
	}

	val = (u8)(led->pdata->clamp_current * FLASH_MAX_LEVEL /
						FLASH_LED_MAX_CURRENT_MA);
	rc = qpnp_led_masked_write(led->spmi_dev,
					FLASH_CLAMP_CURRENT(led->base),
						FLASH_CURRENT_MASK, val);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
					"Clamp current reg write failed\n");
		return rc;
	}

	if (led->pdata->pmic_charger_support)
		val = FLASH_LED_FLASH_HW_VREG_OK;
	else
		val = FLASH_LED_FLASH_SW_VREG_OK;
	rc = qpnp_led_masked_write(led->spmi_dev,
					FLASH_VREG_OK_FORCE(led->base),
						FLASH_VREG_OK_FORCE_MASK, val);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
					"VREG OK force reg write failed\n");
		return rc;
	}

	if (led->pdata->self_check_en)
		val = FLASH_MODULE_ENABLE;
	else
		val = FLASH_LED_DISABLE;
	rc = qpnp_led_masked_write(led->spmi_dev,
					FLASH_FAULT_DETECT(led->base),
						FLASH_FAULT_DETECT_MASK, val);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
					"Fault detect reg write failed\n");
		return rc;
	}

	if (!led->pdata->thermal_derate_en)
		val = 0x0;
	else {
		val = led->pdata->thermal_derate_en << 7;
		val |= led->pdata->thermal_derate_rate << 3;
		val |= (led->pdata->thermal_derate_threshold -
				FLASH_LED_THERMAL_THRESHOLD_MIN) /
				FLASH_LED_THERMAL_DEVIDER;
	}
	rc = qpnp_led_masked_write(led->spmi_dev,
					FLASH_THERMAL_DRATE(led->base),
					FLASH_THERMAL_DERATE_MASK, val);
	if (rc) {
		dev_err(&led->spmi_dev->dev, "Thermal derate reg write failed\n");
		return rc;
	}

	if (!led->pdata->current_ramp_en)
		val = 0x0;
	else {
		val = led->pdata->current_ramp_en << 7;
		val |= led->pdata->ramp_up_step << 3;
		val |= led->pdata->ramp_dn_step;
	}
	rc = qpnp_led_masked_write(led->spmi_dev,
						FLASH_CURRENT_RAMP(led->base),
						FLASH_CURRENT_RAMP_MASK, val);
	if (rc) {
		dev_err(&led->spmi_dev->dev, "Current ramp reg write failed\n");
		return rc;
	}

	if (!led->pdata->vph_pwr_droop_en)
		val = 0x0;
	else {
		val = led->pdata->vph_pwr_droop_en << 7;
		val |= ((led->pdata->vph_pwr_droop_threshold -
				FLASH_LED_VPH_DROOP_THRESHOLD_MIN_MV) /
				FLASH_LED_VPH_DROOP_THRESHOLD_DIVIDER) << 4;
		temp_val =
			qpnp_flash_led_get_droop_debounce_time(
				led->pdata->vph_pwr_droop_debounce_time);
		if (temp_val == 0xFF) {
			dev_err(&led->spmi_dev->dev, "Invalid debounce time\n");
			return temp_val;
		}

		val |= temp_val;
	}
	rc = qpnp_led_masked_write(led->spmi_dev,
						FLASH_VPH_PWR_DROOP(led->base),
						FLASH_VPH_PWR_DROOP_MASK, val);
	if (rc) {
		dev_err(&led->spmi_dev->dev, "VPH PWR droop reg write failed\n");
		return rc;
	}
	return 0;
}

/*
 * Boost regulator probes later than flash.
 * Delay 2s to make sure it has been registered.
 */
static void qpnp_flash_led_delayed_reg_work(struct work_struct *work)
{
	struct flash_node_data *flash_node = container_of(work,
					struct flash_node_data, dwork.work);
	int rc;

	flash_node->boost_regulator = regulator_get(flash_node->cdev.dev,
								"boost");
	if (IS_ERR(flash_node->boost_regulator)) {
		rc = PTR_ERR(flash_node->boost_regulator);
		flash_node->boost_regulator = NULL;
		pr_err("boost regulator get failed\n");
		return;
	}

	return;
}

static int qpnp_flash_led_parse_each_led_dt(struct qpnp_flash_led *led,
					struct flash_node_data *flash_node)
{
	const char *temp_string;
	struct device_node *node = flash_node->cdev.dev->of_node;
	int rc = 0;
	u32 val;

	rc = of_property_read_string(node, "label", &temp_string);
	if (!rc) {
		if (strcmp(temp_string, "flash") == 0)
			flash_node->type = FLASH;
		else if (strcmp(temp_string, "torch") == 0)
			flash_node->type = TORCH;
		else {
			dev_err(&led->spmi_dev->dev,
					"Wrong flash LED type\n");
			return -EINVAL;
		}
	} else if (rc < 0) {
		dev_err(&led->spmi_dev->dev,
					"Unable to read flash type\n");
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,current", &val);
	if (!rc) {
		if (val < FLASH_LED_MIN_CURRENT_MA)
			val = FLASH_LED_MIN_CURRENT_MA;
		flash_node->prgm_current = (u16)val;
	} else if (rc != -EINVAL) {
		dev_err(&led->spmi_dev->dev,
					"Unable to read current settings\n");
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,duration", &val);
	if (!rc)
		flash_node->duration = (u16)val;
	else if (rc != -EINVAL) {
		dev_err(&led->spmi_dev->dev, "Unable to read clamp current\n");
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,id", &val);
	if (!rc)
		flash_node->id = (u8)val;
	else if (rc != -EINVAL) {
		dev_err(&led->spmi_dev->dev, "Unable to read led ID\n");
		return rc;
	}

	switch (led->peripheral_type) {
	case FLASH_SUBTYPE_SINGLE:
		flash_node->current_addr = FLASH_LED0_CURRENT(led->base);
		flash_node->enable = FLASH_LED0_ENABLEMENT;
		flash_node->trigger = FLASH_LED0_TRIGGER;
		break;
	case FLASH_SUBTYPE_DUAL:
		if (flash_node->id == FLASH_LED_0) {
			flash_node->enable = FLASH_LED0_ENABLEMENT;
			if (flash_node->type == TORCH)
				flash_node->enable = FLASH_MODULE_ENABLE;
			flash_node->current_addr =
						FLASH_LED0_CURRENT(led->base);
			flash_node->trigger = FLASH_LED0_TRIGGER;
		} else if (flash_node->id == FLASH_LED_1) {
			flash_node->enable = FLASH_LED1_ENABLEMENT;
			if (flash_node->type == TORCH)
				flash_node->enable = FLASH_MODULE_ENABLE;
			flash_node->current_addr =
						FLASH_LED1_CURRENT(led->base);
			flash_node->trigger = FLASH_LED1_TRIGGER;
		}
		break;
	default:
		dev_err(&led->spmi_dev->dev, "Invalid peripheral type\n");
	}

	if (of_find_property(node, "boost-supply", NULL)) {
		INIT_DELAYED_WORK(&flash_node->dwork,
					qpnp_flash_led_delayed_reg_work);

		flash_node->boost_regulator =
				regulator_get(flash_node->cdev.dev, "boost");
		if (!flash_node->boost_regulator ||
				IS_ERR(flash_node->boost_regulator))
			schedule_delayed_work(&flash_node->dwork,
					FLASH_BOOST_REGULATOR_PROBE_DELAY_MS);

		rc = of_property_read_u32(node, "boost-voltage-max", &val);
		if (!rc)
			flash_node->boost_voltage_max = val;
		else {
			dev_err(&led->spmi_dev->dev,
			"Unable to read maximum boost regulator voltage\n");
			goto error_regulator_config;
		}
	}

	return rc;

error_regulator_config:
	regulator_put(flash_node->boost_regulator);
	return rc;
}

static int qpnp_flash_led_parse_common_dt(
				struct qpnp_flash_led *led,
						struct device_node *node)
{
	int rc;
	u32 val, temp_val;
	const char *temp;

	led->pdata->headroom = FLASH_LED_HEADROOM_DEFAULT_MV;
	rc = of_property_read_u32(node, "qcom,headroom", &val);
	if (!rc)
		led->pdata->headroom = (u16)val;
	else if (rc != -EINVAL) {
		dev_err(&led->spmi_dev->dev, "Unable to read headroom\n");
		return rc;
	}

	led->pdata->startup_dly = FLASH_LED_STARTUP_DELAY_DEFAULT_US;
	rc = of_property_read_u32(node, "qcom,startup-dly", &val);
	if (!rc)
		led->pdata->startup_dly = (u8)val;
	else if (rc != -EINVAL) {
		dev_err(&led->spmi_dev->dev,
					"Unable to read startup delay\n");
		return rc;
	}

	led->pdata->clamp_current = FLASH_LED_CLAMP_CURRENT_DEFAULT_MA;
	rc = of_property_read_u32(node, "qcom,clamp-current", &val);
	if (!rc) {
		if (val < FLASH_LED_MIN_CURRENT_MA)
			val = FLASH_LED_MIN_CURRENT_MA;
		led->pdata->clamp_current = (u16)val;
	} else if (rc != -EINVAL) {
		dev_err(&led->spmi_dev->dev,
					"Unable to read clamp current\n");
		return rc;
	}

	led->pdata->pmic_charger_support =
			of_property_read_bool(node,
						"qcom,pmic-charger-support");

	led->pdata->self_check_en =
			of_property_read_bool(node, "qcom,self-check-enabled");

	led->pdata->thermal_derate_en =
			of_property_read_bool(node,
						"qcom,thermal-derate-enabled");

	if (led->pdata->thermal_derate_en) {
		led->pdata->thermal_derate_rate =
				FLASH_LED_THERMAL_DERATE_RATE_DEFAULT_PERCENT;
		rc = of_property_read_string(node, "qcom,thermal-derate-rate",
									&temp);
		if (!rc) {
			temp_val =
				qpnp_flash_led_get_thermal_derate_rate(temp);
			if (temp_val < 0) {
				dev_err(&led->spmi_dev->dev,
					"Invalid thermal derate rate\n");
				return -EINVAL;
			}

			led->pdata->thermal_derate_rate = (u8)temp_val;
		} else {
			dev_err(&led->spmi_dev->dev,
				"Unable to read thermal derate rate\n");
			return -EINVAL;
		}

		led->pdata->thermal_derate_threshold =
				FLASH_LED_THERMAL_DERATE_THRESHOLD_DEFAULT_C;
		rc = of_property_read_u32(node, "qcom,thermal-derate-threshold",
									&val);
		if (!rc)
			led->pdata->thermal_derate_threshold = (u8)val;
		else if (rc != -EINVAL) {
			dev_err(&led->spmi_dev->dev,
				"Unable to read thermal derate threshold\n");
			return rc;
		}
	}

	led->pdata->current_ramp_en =
			of_property_read_bool(node,
						"qcom,current-ramp-enabled");
	if (led->pdata->current_ramp_en) {
		led->pdata->ramp_up_step = FLASH_LED_RAMP_DN_STEP_DEFAULT_US;
		rc = of_property_read_string(node, "qcom,ramp_up_step", &temp);
		if (!rc) {
			temp_val = qpnp_flash_led_get_ramp_step(temp);
			if (temp_val < 0) {
				dev_err(&led->spmi_dev->dev,
					"Invalid ramp up step values\n");
				return -EINVAL;
			}
			led->pdata->ramp_up_step = (u8)temp_val;
		} else if (rc != -EINVAL) {
			dev_err(&led->spmi_dev->dev,
					"Unable to read ramp up steps\n");
			return rc;
		}

		led->pdata->ramp_dn_step = FLASH_LED_RAMP_DN_STEP_DEFAULT_US;
		rc = of_property_read_string(node, "qcom,ramp_dn_step", &temp);
		if (!rc) {
			temp_val = qpnp_flash_led_get_ramp_step(temp);
			if (temp_val < 0) {
				dev_err(&led->spmi_dev->dev,
					"Invalid ramp down step values\n");
				return rc;
			}
			led->pdata->ramp_dn_step = (u8)temp_val;
		} else if (rc != -EINVAL) {
			dev_err(&led->spmi_dev->dev,
					"Unable to read ramp down steps\n");
			return rc;
		}
	}

	led->pdata->vph_pwr_droop_en = of_property_read_bool(node,
						"qcom,vph-pwr-droop-enabled");
	if (led->pdata->vph_pwr_droop_en) {
		led->pdata->vph_pwr_droop_threshold =
				FLASH_LED_VPH_PWR_DROOP_THRESHOLD_DEFAULT_MV;
		rc = of_property_read_u32(node,
					"qcom,vph-pwr-droop-threshold", &val);
		if (!rc)
			led->pdata->vph_pwr_droop_threshold = (u16)val;
		else if (rc != -EINVAL) {
			dev_err(&led->spmi_dev->dev,
				"Unable to read VPH PWR droop threshold\n");
			return rc;
		}

		led->pdata->vph_pwr_droop_debounce_time =
			FLASH_LED_VPH_PWR_DROOP_DEBOUNCE_TIME_DEFAULT_US;
		rc = of_property_read_u32(node,
				"qcom,vph-pwr-droop-debounce-time", &val);
		if (!rc)
			led->pdata->vph_pwr_droop_debounce_time = (u8)val;
		else if (rc != -EINVAL) {
			dev_err(&led->spmi_dev->dev,
				"Unable to read VPH PWR droop debounce time\n");
			return rc;
		}
	}

	return 0;
}

static int qpnp_flash_led_probe(struct spmi_device *spmi)
{
	struct qpnp_flash_led *led;
	struct resource *flash_resource;
	struct device_node *node, *temp;
	int rc, i = 0, j, num_leds = 0;
	u32 val;

	node = spmi->dev.of_node;
	if (node == NULL) {
		dev_info(&spmi->dev, "No flash device defined\n");
		return -ENODEV;
	}

	flash_resource = spmi_get_resource(spmi, 0, IORESOURCE_MEM, 0);
	if (!flash_resource) {
		dev_err(&spmi->dev, "Unable to get flash LED base address\n");
		return -EINVAL;
	}

	led = devm_kzalloc(&spmi->dev, sizeof(struct qpnp_flash_led),
								GFP_KERNEL);
	if (!led) {
		dev_err(&spmi->dev,
			"Unable to allocate memory for flash LED\n");
		return -ENOMEM;
	}

	led->base = flash_resource->start;
	led->spmi_dev = spmi;

	led->pdata = devm_kzalloc(&spmi->dev,
			sizeof(struct flash_led_platform_data), GFP_KERNEL);
	if (!led->pdata) {
		dev_err(&spmi->dev,
			"Unable to allocate memory for platform data\n");
		return -ENOMEM;
	}

	led->peripheral_type =
			(u8)qpnp_flash_led_get_peripheral_type(led);
	if (led->peripheral_type < 0) {
		dev_err(&spmi->dev, "Failed to get peripheral type\n");
		return rc;
	}

	rc = qpnp_flash_led_parse_common_dt(led, node);
	if (rc) {
		dev_err(&spmi->dev,
			"Failed to get common config for flash LEDs\n");
		return rc;
	}

	rc = qpnp_flash_led_init_settings(led);
	if (rc) {
		dev_err(&spmi->dev, "Failed to initialize flash LED\n");
		return rc;
	}

	temp = NULL;
	while ((temp = of_get_next_child(node, temp)))
		num_leds++;

	if (!num_leds)
		return -ECHILD;

	led->flash_node = devm_kzalloc(&spmi->dev,
			(sizeof(struct flash_node_data) * num_leds),
			GFP_KERNEL);
	if (!led->flash_node) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	mutex_init(&led->flash_led_lock);

	for_each_child_of_node(node, temp) {
		led->flash_node[i].cdev.brightness_set =
						qpnp_flash_led_brightness_set;
		led->flash_node[i].cdev.brightness_get =
						qpnp_flash_led_brightness_get;
		led->flash_node[i].spmi_dev = spmi;

		INIT_WORK(&led->flash_node[i].work, qpnp_flash_led_work);
		rc = of_property_read_string(temp, "qcom,led-name",
						&led->flash_node[i].cdev.name);
		if (rc < 0) {
			dev_err(&led->spmi_dev->dev,
					"Unable to read flash name\n");
			return rc;
		}

		rc = of_property_read_string(temp, "qcom,default-led-trigger",
				&led->flash_node[i].cdev.default_trigger);
		if (rc < 0) {
			dev_err(&led->spmi_dev->dev,
					"Unable to read trigger name\n");
			return rc;
		}

		rc = of_property_read_u32(temp, "qcom,max-current", &val);
		if (!rc) {
			if (val < FLASH_LED_MIN_CURRENT_MA)
				val = FLASH_LED_MIN_CURRENT_MA;
			led->flash_node[i].max_current = (u16)val;
			led->flash_node[i].cdev.max_brightness = val;
		} else if (rc < 0) {
			dev_err(&led->spmi_dev->dev,
						"Unable to read max current\n");
			return rc;
		}

		rc = led_classdev_register(&spmi->dev,
						&led->flash_node[i].cdev);
		if (rc) {
			dev_err(&spmi->dev, "Unable to register led\n");
			goto error_led_register;
		}

		led->flash_node[i].cdev.dev->of_node = temp;

		rc = qpnp_flash_led_parse_each_led_dt(led, &led->flash_node[i]);
		if (rc) {
			dev_err(&spmi->dev,
				"Failed to parse config for each LED\n");
			goto error_led_register;
		}

		for (j = 0; j < ARRAY_SIZE(qpnp_flash_led_attrs); j++) {
			rc =
			sysfs_create_file(&led->flash_node[i].cdev.dev->kobj,
						&qpnp_flash_led_attrs[j].attr);
			if (rc)
				goto error_led_register;
		}

		i++;
	}

	led->num_leds = i;

	dev_set_drvdata(&spmi->dev, led);

	return 0;

error_led_register:
	for (; i >= 0; i--) {
		for (; j >= 0; j--)
			sysfs_remove_file(&led->flash_node[i].cdev.dev->kobj,
						&qpnp_flash_led_attrs[j].attr);
		j = ARRAY_SIZE(qpnp_flash_led_attrs) - 1;
		led_classdev_unregister(&led->flash_node[i].cdev);
	}
	mutex_destroy(&led->flash_led_lock);
	return rc;
}

static int qpnp_flash_led_remove(struct spmi_device *spmi)
{
	struct qpnp_flash_led *led  = dev_get_drvdata(&spmi->dev);
	int i, j;

	for (i = led->num_leds - 1; i >= 0; i--) {
		if (led->flash_node[i].boost_regulator)
			regulator_put(led->flash_node[i].boost_regulator);
		for (j = 0; j < ARRAY_SIZE(qpnp_flash_led_attrs); j++)
			sysfs_remove_file(&led->flash_node[i].cdev.dev->kobj,
						&qpnp_flash_led_attrs[j].attr);
		led_classdev_unregister(&led->flash_node[i].cdev);
	}

	mutex_destroy(&led->flash_led_lock);

	return 0;
}

static struct of_device_id spmi_match_table[] = {
	{ .compatible = "qcom,qpnp-flash-led",},
	{ },
};

static struct spmi_driver qpnp_flash_led_driver = {
	.driver		= {
		.name = "qcom,qpnp-flash-led",
		.of_match_table = spmi_match_table,
	},
	.probe		= qpnp_flash_led_probe,
	.remove		= qpnp_flash_led_remove,
};

static int __init qpnp_flash_led_init(void)
{
	return spmi_driver_register(&qpnp_flash_led_driver);
}
module_init(qpnp_flash_led_init);

static void __exit qpnp_flash_led_exit(void)
{
	spmi_driver_unregister(&qpnp_flash_led_driver);
}
module_exit(qpnp_flash_led_exit);

MODULE_DESCRIPTION("QPNP Flash LED driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("leds:leds-qpnp-flash");
