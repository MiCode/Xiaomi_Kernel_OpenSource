
/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/spmi.h>

#define WLED_MOD_EN_REG(base, n)	(base + 0x60 + n*0x10)
#define WLED_IDAC_DLY_REG(base, n)	(WLED_MOD_EN_REG(base, n) + 0x01)
#define WLED_FULL_SCALE_REG(base, n)	(WLED_IDAC_DLY_REG(base, n) + 0x01)

/* wled control registers */
#define WLED_BRIGHTNESS_CNTL_LSB(base, n)	(base + 0x40 + 2*n)
#define WLED_BRIGHTNESS_CNTL_MSB(base, n)	(base + 0x41 + 2*n)
#define WLED_MOD_CTRL_REG(base)			(base + 0x46)
#define WLED_SYNC_REG(base)			(base + 0x47)
#define WLED_FDBCK_CTRL_REG(base)		(base + 0x48)
#define WLED_SWITCHING_FREQ_REG(base)		(base + 0x4C)
#define WLED_OVP_CFG_REG(base)			(base + 0x4D)
#define WLED_BOOST_LIMIT_REG(base)		(base + 0x4E)
#define WLED_CURR_SINK_REG(base)		(base + 0x4F)
#define WLED_HIGH_POLE_CAP_REG(base)		(base + 0x58)
#define WLED_CURR_SINK_MASK		0xE0
#define WLED_CURR_SINK_SHFT		0x05
#define WLED_SWITCH_FREQ_MASK		0x02
#define WLED_OVP_VAL_MASK		0x03
#define WLED_OVP_VAL_BIT_SHFT		0x00
#define WLED_BOOST_LIMIT_MASK		0x07
#define WLED_BOOST_LIMIT_BIT_SHFT	0x00
#define WLED_BOOST_ON			0x80
#define WLED_BOOST_OFF			0x00
#define WLED_EN_MASK			0x80
#define WLED_NO_MASK			0x00
#define WLED_CP_SELECT_MAX		0x03
#define WLED_CP_SELECT_MASK		0x02
#define WLED_USE_EXT_GEN_MOD_SRC	0x01
#define WLED_CTL_DLY_STEP		200
#define WLED_CTL_DLY_MAX		1400
#define WLED_MAX_CURR			25
#define WLED_MSB_MASK			0x0F
#define WLED_MAX_CURR_MASK		0x19
#define WLED_OP_FDBCK_MASK		0x07
#define WLED_OP_FDBCK_BIT_SHFT		0x00

#define WLED_MAX_LEVEL			255
#define WLED_8_BIT_MASK			0xFF
#define WLED_4_BIT_MASK			0x0F
#define WLED_8_BIT_SHFT			0x08
#define WLED_MAX_DUTY_CYCLE		0xFFF

#define WLED_SYNC_VAL			0x07
#define WLED_SYNC_RESET_VAL		0x00

#define WLED_TRIGGER_DEFAULT		"none"
#define WLED_FLAGS_DEFAULT		0x00
#define WLED_DEFAULT_STRINGS		0x01
#define WLED_DEFAULT_OVP_VAL		0x02
#define WLED_BOOST_LIM_DEFAULT		0x03
#define WLED_CP_SEL_DEFAULT		0x00
#define WLED_CTRL_DLY_DEFAULT		0x00
#define WLED_SWITCH_FREQ_DEFAULT	0x02

/**
 * enum qpnp_leds - QPNP supported led ids
 * @QPNP_ID_WLED - White led backlight
 */
enum qpnp_leds {
	QPNP_ID_WLED,
};

/* current boost limit */
enum wled_current_boost_limit {
	WLED_CURR_LIMIT_105mA,
	WLED_CURR_LIMIT_385mA,
	WLED_CURR_LIMIT_525mA,
	WLED_CURR_LIMIT_805mA,
	WLED_CURR_LIMIT_980mA,
	WLED_CURR_LIMIT_1260mA,
	WLED_CURR_LIMIT_1400mA,
	WLED_CURR_LIMIT_1680mA,
};

/* over voltage protection threshold */
enum wled_ovp_threshold {
	WLED_OVP_35V,
	WLED_OVP_32V,
	WLED_OVP_29V,
	WLED_OVP_37V,
};

/* switch frquency */
enum wled_switch_freq {
	WLED_800kHz = 0,
	WLED_960kHz,
	WLED_1600kHz,
	WLED_3200kHz,
};

static u8 wled_debug_regs[] = {
	/* common registers */
	0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
	/* LED1 */
	0x60, 0x61, 0x62, 0x63, 0x66,
	/* LED1 */
	0x70, 0x71, 0x72, 0x73, 0x76,
	/* LED1 */
	0x80, 0x81, 0x82, 0x83, 0x86,
};

/**
 *  wled_config_data - wled configuration data
 *  @num_strings - number of wled strings supported
 *  @ovp_val - over voltage protection threshold
 *  @boost_curr_lim - boot current limit
 *  @cp_select - high pole capacitance
 *  @ctrl_delay_us - delay in activation of led
 *  @dig_mod_gen_en - digital module generator
 *  @cs_out_en - current sink output enable
 *  @op_fdbck - selection of output as feedback for the boost
 */
struct wled_config_data {
	u8	num_strings;
	u8	ovp_val;
	u8	boost_curr_lim;
	u8	cp_select;
	u8	ctrl_delay_us;
	u8	switch_freq;
	bool	dig_mod_gen_en;
	bool	cs_out_en;
	bool	op_fdbck;
};

/**
 * struct qpnp_led_data - internal led data structure
 * @led_classdev - led class device
 * @id - led index
 * @base_reg - base register given in device tree
 * @lock - to protect the transactions
 * @reg - cached value of led register
 * @max_current - maximum current supported by LED
 * @default_on - true: default state max, false, default state 0
 */
struct qpnp_led_data {
	struct led_classdev	cdev;
	struct spmi_device	*spmi_dev;
	int			id;
	u16			base;
	u8			reg;
	struct mutex		lock;
	struct wled_config_data *wled_cfg;
	int			max_current;
	bool			default_on;
};

static int
qpnp_led_masked_write(struct qpnp_led_data *led, u16 addr, u8 mask, u8 val)
{
	int rc;
	u8 reg;

	rc = spmi_ext_register_readl(led->spmi_dev->ctrl, led->spmi_dev->sid,
		addr, &reg, 1);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
			"Unable to read from addr=%x, rc(%d)\n", addr, rc);
	}

	reg &= ~mask;
	reg |= val;

	rc = spmi_ext_register_writel(led->spmi_dev->ctrl, led->spmi_dev->sid,
		addr, &reg, 1);
	if (rc)
		dev_err(&led->spmi_dev->dev,
			"Unable to write to addr=%x, rc(%d)\n", addr, rc);
	return rc;
}

static int qpnp_wled_set(struct qpnp_led_data *led)
{
	int rc, duty;
	u8 level, val, i, num_wled_strings;

	level = led->cdev.brightness;

	if (level > WLED_MAX_LEVEL)
		level = WLED_MAX_LEVEL;
	if (level == 0) {
		val = WLED_BOOST_OFF;
		rc = spmi_ext_register_writel(led->spmi_dev->ctrl,
			led->spmi_dev->sid, WLED_MOD_CTRL_REG(led->base),
			&val, 1);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"WLED write ctrl reg failed(%d)\n", rc);
			return rc;
		}
	} else {
		val = WLED_BOOST_ON;
		rc = spmi_ext_register_writel(led->spmi_dev->ctrl,
			led->spmi_dev->sid, WLED_MOD_CTRL_REG(led->base),
			&val, 1);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"WLED write ctrl reg failed(%d)\n", rc);
			return rc;
		}
	}

	duty = (WLED_MAX_DUTY_CYCLE * level) / WLED_MAX_LEVEL;

	num_wled_strings = led->wled_cfg->num_strings;

	/* program brightness control registers */
	for (i = 0; i < num_wled_strings; i++) {
		rc = qpnp_led_masked_write(led,
			WLED_BRIGHTNESS_CNTL_MSB(led->base, i), WLED_MSB_MASK,
			(duty >> WLED_8_BIT_SHFT) & WLED_4_BIT_MASK);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"WLED set brightness MSB failed(%d)\n", rc);
			return rc;
		}
		val = duty & WLED_8_BIT_MASK;
		rc = spmi_ext_register_writel(led->spmi_dev->ctrl,
			led->spmi_dev->sid,
			WLED_BRIGHTNESS_CNTL_LSB(led->base, i), &val, 1);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"WLED set brightness LSB failed(%d)\n", rc);
			return rc;
		}
	}

	/* sync */
	val = WLED_SYNC_VAL;
	rc = spmi_ext_register_writel(led->spmi_dev->ctrl, led->spmi_dev->sid,
		WLED_SYNC_REG(led->base), &val, 1);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
				"WLED set sync reg failed(%d)\n", rc);
		return rc;
	}

	val = WLED_SYNC_RESET_VAL;
	rc = spmi_ext_register_writel(led->spmi_dev->ctrl, led->spmi_dev->sid,
		WLED_SYNC_REG(led->base), &val, 1);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
				"WLED reset sync reg failed(%d)\n", rc);
		return rc;
	}
	return 0;
}

static void qpnp_wled_dump_regs(struct qpnp_led_data *led)
{
	int i;
	u8 val;

	pr_debug("===== WLED register dump start =====\n");
	for (i = 0; i < ARRAY_SIZE(wled_debug_regs); i++) {
		spmi_ext_register_readl(led->spmi_dev->ctrl,
					led->spmi_dev->sid,
					led->base + wled_debug_regs[i],
					&val, sizeof(val));
		pr_debug("0x%x = 0x%x\n", led->base + wled_debug_regs[i], val);
	}
	pr_debug("===== WLED register dump end =====\n");
}

static void qpnp_led_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	int rc;
	struct qpnp_led_data *led;

	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	if (value < LED_OFF || value > led->cdev.max_brightness) {
		dev_err(led->cdev.dev, "Invalid brightness value\n");
		return;
	}

	mutex_lock(&led->lock);
	led->cdev.brightness = value;

	switch (led->id) {
	case QPNP_ID_WLED:
		rc = qpnp_wled_set(led);
		if (rc < 0)
			dev_err(led->cdev.dev,
				"WLED set brightness failed (%d)\n", rc);
		break;
	default:
		dev_err(led->cdev.dev, "Invalid LED(%d)\n", led->id);
		break;
	}
	mutex_unlock(&led->lock);
}

static int __devinit qpnp_led_set_max_brightness(struct qpnp_led_data *led)
{
	switch (led->id) {
	case QPNP_ID_WLED:
		led->cdev.max_brightness = WLED_MAX_LEVEL;
		break;
	default:
		dev_err(led->cdev.dev, "Invalid LED(%d)\n", led->id);
		return -EINVAL;
	}

	return 0;
}

static enum led_brightness qpnp_led_get(struct led_classdev *led_cdev)
{
	struct qpnp_led_data *led;

	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	return led->cdev.brightness;
}

static int __devinit qpnp_wled_init(struct qpnp_led_data *led)
{
	int rc, i;
	u8 num_wled_strings;

	num_wled_strings = led->wled_cfg->num_strings;

	/* verify ranges */
	if (led->wled_cfg->ovp_val > WLED_OVP_37V) {
		dev_err(&led->spmi_dev->dev, "Invalid ovp value\n");
		return -EINVAL;
	}

	if (led->wled_cfg->boost_curr_lim > WLED_CURR_LIMIT_1680mA) {
		dev_err(&led->spmi_dev->dev, "Invalid boost current limit\n");
		return -EINVAL;
	}

	if (led->wled_cfg->cp_select > WLED_CP_SELECT_MAX) {
		dev_err(&led->spmi_dev->dev, "Invalid pole capacitance\n");
		return -EINVAL;
	}

	if ((led->max_current > WLED_MAX_CURR)) {
		dev_err(&led->spmi_dev->dev, "Invalid max current\n");
		return -EINVAL;
	}

	if ((led->wled_cfg->ctrl_delay_us % WLED_CTL_DLY_STEP) ||
		(led->wled_cfg->ctrl_delay_us > WLED_CTL_DLY_MAX)) {
		dev_err(&led->spmi_dev->dev, "Invalid control delay\n");
		return -EINVAL;
	}

	/* program over voltage protection threshold */
	rc = qpnp_led_masked_write(led, WLED_OVP_CFG_REG(led->base),
		WLED_OVP_VAL_MASK,
		(led->wled_cfg->ovp_val << WLED_OVP_VAL_BIT_SHFT));
	if (rc) {
		dev_err(&led->spmi_dev->dev,
				"WLED OVP reg write failed(%d)\n", rc);
		return rc;
	}

	/* program current boost limit */
	rc = qpnp_led_masked_write(led, WLED_BOOST_LIMIT_REG(led->base),
		WLED_BOOST_LIMIT_MASK, led->wled_cfg->boost_curr_lim);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
				"WLED boost limit reg write failed(%d)\n", rc);
		return rc;
	}

	/* program output feedback */
	rc = qpnp_led_masked_write(led, WLED_FDBCK_CTRL_REG(led->base),
		WLED_OP_FDBCK_MASK,
		(led->wled_cfg->op_fdbck << WLED_OP_FDBCK_BIT_SHFT));
	if (rc) {
		dev_err(&led->spmi_dev->dev,
				"WLED fdbck ctrl reg write failed(%d)\n", rc);
		return rc;
	}

	/* program switch frequency */
	rc = qpnp_led_masked_write(led, WLED_SWITCHING_FREQ_REG(led->base),
		WLED_SWITCH_FREQ_MASK, led->wled_cfg->switch_freq);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
				"WLED switch freq reg write failed(%d)\n", rc);
		return rc;
	}

	/* program current sink */
	if (led->wled_cfg->cs_out_en) {
		rc = qpnp_led_masked_write(led, WLED_CURR_SINK_REG(led->base),
			WLED_CURR_SINK_MASK,
			(led->wled_cfg->num_strings << WLED_CURR_SINK_SHFT));
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"WLED curr sink reg write failed(%d)\n", rc);
			return rc;
		}
	}

	/* program high pole capacitance */
	rc = qpnp_led_masked_write(led, WLED_HIGH_POLE_CAP_REG(led->base),
		WLED_CP_SELECT_MASK, led->wled_cfg->cp_select);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
				"WLED pole cap reg write failed(%d)\n", rc);
		return rc;
	}

	/* program modulator, current mod src and cabc */
	for (i = 0; i < num_wled_strings; i++) {
		rc = qpnp_led_masked_write(led, WLED_MOD_EN_REG(led->base, i),
			WLED_NO_MASK, WLED_EN_MASK);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"WLED mod enable reg write failed(%d)\n", rc);
			return rc;
		}

		if (led->wled_cfg->dig_mod_gen_en) {
			rc = qpnp_led_masked_write(led,
				WLED_MOD_EN_REG(led->base, i),
				WLED_NO_MASK, WLED_USE_EXT_GEN_MOD_SRC);
			if (rc) {
				dev_err(&led->spmi_dev->dev,
				"WLED dig mod en reg write failed(%d)\n", rc);
			}
		}

		rc = qpnp_led_masked_write(led,
			WLED_FULL_SCALE_REG(led->base, i), WLED_MAX_CURR_MASK,
			led->max_current);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"WLED max current reg write failed(%d)\n", rc);
			return rc;
		}

	}

	/* dump wled registers */
	qpnp_wled_dump_regs(led);

	return 0;
}

static int __devinit qpnp_led_initialize(struct qpnp_led_data *led)
{
	int rc;

	switch (led->id) {
	case QPNP_ID_WLED:
		rc = qpnp_wled_init(led);
		if (rc)
			dev_err(led->cdev.dev,
				"WLED initialize failed(%d)\n", rc);
		break;
	default:
		dev_err(led->cdev.dev, "Invalid LED(%d)\n", led->id);
		rc = -EINVAL;
	}

	return rc;
}

/*
 * Handlers for alternative sources of platform_data
 */
static int __devinit qpnp_get_config_wled(struct qpnp_led_data *led,
				struct device_node *node)
{
	u32 val;
	int rc;
	const char *temp_string;

	led->id = QPNP_ID_WLED;

	led->wled_cfg = devm_kzalloc(&led->spmi_dev->dev,
				sizeof(struct wled_config_data), GFP_KERNEL);
	if (!led->wled_cfg) {
		dev_err(&led->spmi_dev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	led->cdev.default_trigger = WLED_TRIGGER_DEFAULT;
	rc = of_property_read_string(node, "linux,default-trigger",
		&temp_string);
	if (!rc)
		led->cdev.default_trigger = temp_string;
	else if (rc != -EINVAL)
		return rc;


	led->cdev.flags = WLED_FLAGS_DEFAULT;
	rc = of_property_read_u32(node, "qcom,flags", &val);
	if (!rc)
		led->cdev.flags = (int) val;
	else if (rc != -EINVAL)
		return rc;

	led->default_on = true;
	rc = of_property_read_string(node, "qcom,default-state",
		&temp_string);
	if (!rc) {
		if (!strncmp(temp_string, "off", sizeof("off")))
			led->default_on = false;
	} else if (rc != -EINVAL)
		return rc;

	led->wled_cfg->num_strings = WLED_DEFAULT_STRINGS;
	rc = of_property_read_u32(node, "qcom,num-strings", &val);
	if (!rc)
		led->wled_cfg->num_strings = (u8) val;
	else if (rc != -EINVAL)
		return rc;

	led->wled_cfg->ovp_val = WLED_DEFAULT_OVP_VAL;
	rc = of_property_read_u32(node, "qcom,ovp-val", &val);
	if (!rc)
		led->wled_cfg->ovp_val = (u8) val;
	else if (rc != -EINVAL)
		return rc;

	led->wled_cfg->boost_curr_lim = WLED_BOOST_LIM_DEFAULT;
	rc = of_property_read_u32(node, "qcom,boost-curr-lim", &val);
	if (!rc)
		led->wled_cfg->boost_curr_lim = (u8) val;
	else if (rc != -EINVAL)
		return rc;

	led->wled_cfg->cp_select = WLED_CP_SEL_DEFAULT;
	rc = of_property_read_u32(node, "qcom,cp-sel", &val);
	if (!rc)
		led->wled_cfg->cp_select = (u8) val;
	else if (rc != -EINVAL)
		return rc;

	led->wled_cfg->ctrl_delay_us = WLED_CTRL_DLY_DEFAULT;
	rc = of_property_read_u32(node, "qcom,ctrl-delay-us", &val);
	if (!rc)
		led->wled_cfg->ctrl_delay_us = (u8) val;
	else if (rc != -EINVAL)
		return rc;

	led->wled_cfg->switch_freq = WLED_SWITCH_FREQ_DEFAULT;
	rc = of_property_read_u32(node, "qcom,switch-freq", &val);
	if (!rc)
		led->wled_cfg->switch_freq = (u8) val;
	else if (rc != -EINVAL)
		return rc;

	led->wled_cfg->dig_mod_gen_en =
		of_property_read_bool(node, "qcom,dig-mod-gen-en");

	led->wled_cfg->cs_out_en =
		of_property_read_bool(node, "qcom,cs-out-en");

	led->wled_cfg->op_fdbck =
		of_property_read_bool(node, "qcom,op-fdbck");

	return 0;
}

static int __devinit qpnp_leds_probe(struct spmi_device *spmi)
{
	struct qpnp_led_data *led;
	struct resource *led_resource;
	struct device_node *node;
	int rc;
	const char *led_label;

	led = devm_kzalloc(&spmi->dev, (sizeof(struct qpnp_led_data)),
		GFP_KERNEL);
	if (!led) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	led->spmi_dev = spmi;

	led_resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!led_resource) {
		dev_err(&spmi->dev, "Unable to get LED base address\n");
		return -ENXIO;
	}
	led->base = led_resource->start;

	dev_set_drvdata(&spmi->dev, led);

	node = led->spmi_dev->dev.of_node;
	if (node == NULL)
		return -ENODEV;

	rc = of_property_read_string(node, "qcom,label", &led_label);
	if (rc < 0) {
		dev_err(&led->spmi_dev->dev,
			"Failure reading label, rc = %d\n", rc);
		return rc;
	}

	rc = of_property_read_string(node, "qcom,name", &led->cdev.name);
	if (rc < 0) {
		dev_err(&led->spmi_dev->dev,
			"Failure reading led name, rc = %d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,max-current", &led->max_current);
	if (rc < 0) {
		dev_err(&led->spmi_dev->dev,
			"Failure reading max_current, rc =  %d\n", rc);
		return rc;
	}

	led->cdev.brightness_set    = qpnp_led_set;
	led->cdev.brightness_get    = qpnp_led_get;
	led->cdev.brightness	= LED_OFF;

	if (strncmp(led_label, "wled", sizeof("wled")) == 0) {
		rc = qpnp_get_config_wled(led, node);
		if (rc < 0) {
			dev_err(&led->spmi_dev->dev,
				"Unable to read wled config data\n");
			return rc;
		}
	} else {
		dev_err(&led->spmi_dev->dev, "No LED matching label\n");
		return -EINVAL;
	}

	mutex_init(&led->lock);

	rc =  qpnp_led_initialize(led);
	if (rc < 0)
		goto fail_id_check;

	rc = qpnp_led_set_max_brightness(led);
	if (rc < 0)
		goto fail_id_check;


	rc = led_classdev_register(&spmi->dev, &led->cdev);
	if (rc) {
		dev_err(&spmi->dev, "unable to register led %d,rc=%d\n",
					 led->id, rc);
		goto fail_id_check;
	}

	/* configure default state */
	if (led->default_on)
		led->cdev.brightness = led->cdev.max_brightness;

	qpnp_led_set(&led->cdev, led->cdev.brightness);

	return 0;

fail_id_check:
	mutex_destroy(&led->lock);
	led_classdev_unregister(&led->cdev);
	return rc;
}

static int __devexit qpnp_leds_remove(struct spmi_device *spmi)
{
	struct qpnp_led_data *led  = dev_get_drvdata(&spmi->dev);

	mutex_destroy(&led->lock);
	led_classdev_unregister(&led->cdev);

	return 0;
}
static struct of_device_id spmi_match_table[] = {
	{	.compatible = "qcom,leds-qpnp",
	}
};

static struct spmi_driver qpnp_leds_driver = {
	.driver		= {
		.name	= "qcom,leds-qpnp",
		.of_match_table = spmi_match_table,
	},
	.probe		= qpnp_leds_probe,
	.remove		= __devexit_p(qpnp_leds_remove),
};

static int __init qpnp_led_init(void)
{
	return spmi_driver_register(&qpnp_leds_driver);
}
module_init(qpnp_led_init);

static void __exit qpnp_led_exit(void)
{
	spmi_driver_unregister(&qpnp_leds_driver);
}
module_exit(qpnp_led_exit);

MODULE_DESCRIPTION("QPNP LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("leds:leds-qpnp");
