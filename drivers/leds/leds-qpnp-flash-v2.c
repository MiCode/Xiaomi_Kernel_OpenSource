/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/leds-qpnp-flash-v2.h>

#define	FLASH_LED_REG_SAFETY_TMR(base)		(base + 0x40)
#define	FLASH_LED_REG_TGR_CURRENT(base)		(base + 0x43)
#define	FLASH_LED_REG_MOD_CTRL(base)		(base + 0x46)
#define	FLASH_LED_REG_IRES(base)		(base + 0x47)
#define	FLASH_LED_REG_STROBE_CTRL(base)		(base + 0x49)
#define	FLASH_LED_REG_CHANNEL_CTRL(base)	(base + 0x4C)
#define	FLASH_LED_REG_HDRM_PRGM(base)		(base + 0x4D)
#define	FLASH_LED_REG_HDRM_AUTO_MODE_CTRL(base)	(base + 0x50)
#define	FLASH_LED_REG_ISC_DELAY(base)		(base + 0x52)

#define	FLASH_LED_HDRM_MODE_PRGM_MASK		0xFF
#define	FLASH_LED_HDRM_VOL_MASK			0xF0
#define	FLASH_LED_CURRENT_MASK			0x3F
#define	FLASH_LED_STROBE_CTRL_MASK		0x07
#define	FLASH_LED_SAFETY_TMR_MASK_MASK		0x7F
#define	FLASH_LED_MOD_CTRL_MASK			0x80
#define	FLASH_LED_ISC_DELAY_MASK		0x03

#define	FLASH_LED_TYPE_FLASH			0
#define	FLASH_LED_TYPE_TORCH			1
#define	FLASH_LED_HEADROOM_AUTO_MODE_ENABLED	true
#define	FLASH_LED_ISC_DELAY_SHIFT		6
#define	FLASH_LED_ISC_DELAY_DEFAULT_US		3
#define	FLASH_LED_SAFETY_TMR_VAL_OFFSET		1
#define	FLASH_LED_SAFETY_TMR_VAL_DIVISOR	10
#define	FLASH_LED_SAFETY_TMR_ENABLED		0x08
#define	FLASH_LED_IRES_BASE			3
#define	FLASH_LED_IRES_DIVISOR			2500
#define	FLASH_LED_IRES_MIN_UA			5000
#define	FLASH_LED_IRES_DEFAULT_UA		12500
#define	FLASH_LED_IRES_DEFAULT_VAL		0x00
#define	FLASH_LED_HDRM_VOL_SHIFT		4
#define	FLASH_LED_HDRM_VOL_DEFAULT_MV		0x80
#define	FLASH_LED_HDRM_VOL_HI_LO_WIN_DEFAULT_MV	0x04
#define	FLASH_LED_HDRM_VOL_BASE_MV		125
#define	FLASH_LED_HDRM_VOL_STEP_MV		25
#define	FLASH_LED_STROBE_ENABLE			0x01
#define	FLASH_LED_MOD_ENABLE			0x80
#define	FLASH_LED_DISABLE			0x00
#define	FLASH_LED_SAFETY_TMR_DISABLED		0x13

/*
 * Flash LED configuration read from device tree
 */
struct flash_led_platform_data {
	u8				isc_delay_us;
	bool				hdrm_auto_mode_en;
};

/*
 * Flash LED data structure containing flash LED attributes
 */
struct qpnp_flash_led {
	struct flash_led_platform_data	*pdata;
	struct platform_device		*pdev;
	struct regmap			*regmap;
	struct flash_node_data		*fnode;
	struct flash_switch_data	*snode;
	spinlock_t			lock;
	int				num_led_nodes;
	int				num_avail_leds;
	u16				base;
};

static int
qpnp_flash_led_masked_write(struct qpnp_flash_led *led, u16 addr, u8 mask,
									u8 val)
{
	int rc;

	rc = regmap_update_bits(led->regmap, addr, mask, val);
	if (rc)
		dev_err(&led->pdev->dev,
			"Unable to update bits from 0x%02X, rc = %d\n",
								addr, rc);

	dev_dbg(&led->pdev->dev, "Write 0x%02X to addr 0x%02X\n", val, addr);

	return rc;
}

static enum
led_brightness qpnp_flash_led_brightness_get(struct led_classdev *led_cdev)
{
	return led_cdev->brightness;
}

static int qpnp_flash_led_init_settings(struct qpnp_flash_led *led)
{
	int rc, i, addr_offset;
	u8 val = 0;

	for (i = 0; i < led->num_avail_leds; i++) {
		addr_offset = led->fnode[i].id;
		rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_HDRM_PRGM(led->base + addr_offset),
			FLASH_LED_HDRM_MODE_PRGM_MASK,
			led->fnode[i].hdrm_val);
		if (rc)
			return rc;

		val |= 0x1 << led->fnode[i].id;
	}

	rc = qpnp_flash_led_masked_write(led,
				FLASH_LED_REG_HDRM_AUTO_MODE_CTRL(led->base),
				FLASH_LED_HDRM_MODE_PRGM_MASK, val);
	if (rc)
		return rc;

	rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_ISC_DELAY(led->base),
			FLASH_LED_ISC_DELAY_MASK, led->pdata->isc_delay_us);
	if (rc)
		return rc;

	return 0;
}

static void qpnp_flash_led_node_set(struct flash_node_data *fnode, int value)
{
	int prgm_current_ma;

	prgm_current_ma = value < 0 ? 0 : value;
	prgm_current_ma = value > fnode->cdev.max_brightness ?
					fnode->cdev.max_brightness : value;
	fnode->cdev.brightness = prgm_current_ma;
	fnode->brightness = prgm_current_ma * 1000 / fnode->ires_ua + 1;
	fnode->led_on = prgm_current_ma != 0;
}

static int qpnp_flash_led_switch_set(struct flash_switch_data *snode, bool on)
{
	struct qpnp_flash_led *led = dev_get_drvdata(&snode->pdev->dev);
	int rc, i, addr_offset;
	u8 val;

	if (!on)
		goto leds_turn_off;

	val = 0;
	for (i = 0; i < led->num_led_nodes; i++)
		val |= led->fnode[i].ires << (led->fnode[i].id * 2);
	rc = qpnp_flash_led_masked_write(led, FLASH_LED_REG_IRES(led->base),
						FLASH_LED_CURRENT_MASK, val);
	if (rc)
		return rc;

	val = 0;
	for (i = 0; i < led->num_avail_leds; i++) {
		if (!led->fnode[i].led_on)
			continue;

		addr_offset = led->fnode[i].id;
		rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_STROBE_CTRL(led->base + addr_offset),
			FLASH_LED_STROBE_CTRL_MASK, FLASH_LED_STROBE_ENABLE);
		if (rc)
			return rc;

		rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_TGR_CURRENT(led->base + addr_offset),
			FLASH_LED_CURRENT_MASK, led->fnode[i].brightness);
		if (rc)
			return rc;

		rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_SAFETY_TMR(led->base + addr_offset),
			FLASH_LED_SAFETY_TMR_MASK_MASK, led->fnode[i].duration);
		if (rc)
			return rc;

		val |= FLASH_LED_STROBE_ENABLE << led->fnode[i].id;

		if (led->fnode[i].pinctrl) {
			rc = pinctrl_select_state(led->fnode[i].pinctrl,
					led->fnode[i].gpio_state_active);
			if (rc) {
				dev_err(&led->pdev->dev,
						"failed to enable GPIO\n");
				return rc;
			}
		}
	}

	rc = qpnp_flash_led_masked_write(led, FLASH_LED_REG_MOD_CTRL(led->base),
				FLASH_LED_MOD_CTRL_MASK, FLASH_LED_MOD_ENABLE);
	if (rc)
		return rc;

	rc = qpnp_flash_led_masked_write(led,
					FLASH_LED_REG_CHANNEL_CTRL(led->base),
					FLASH_LED_STROBE_CTRL_MASK, val);
	if (rc)
		return rc;

	return 0;

leds_turn_off:
	rc = qpnp_flash_led_masked_write(led,
				FLASH_LED_REG_CHANNEL_CTRL(led->base),
				FLASH_LED_STROBE_CTRL_MASK, FLASH_LED_DISABLE);
	if (rc)
		return rc;

	rc = qpnp_flash_led_masked_write(led, FLASH_LED_REG_MOD_CTRL(led->base),
				FLASH_LED_MOD_CTRL_MASK, FLASH_LED_DISABLE);
	if (rc)
		return rc;

	for (i = 0; i < led->num_led_nodes; i++) {
		if (!led->fnode[i].led_on)
			continue;

		addr_offset = led->fnode[i].id;
		rc = qpnp_flash_led_masked_write(led,
			FLASH_LED_REG_TGR_CURRENT(led->base + addr_offset),
			FLASH_LED_CURRENT_MASK, 0);
		if (rc)
			return rc;
		led->fnode[i].led_on = false;

		if (led->fnode[i].pinctrl) {
			rc = pinctrl_select_state(led->fnode[i].pinctrl,
					led->fnode[i].gpio_state_suspend);
			if (rc) {
				dev_err(&led->pdev->dev,
						"failed to disable GPIO\n");
				return rc;
			}
		}
	}

	return 0;
}

static void qpnp_flash_led_brightness_set(struct led_classdev *led_cdev,
						enum led_brightness value)
{
	struct flash_node_data *fnode = NULL;
	struct flash_switch_data *snode = NULL;
	struct qpnp_flash_led *led = dev_get_drvdata(&fnode->pdev->dev);
	int rc;

	if (!strcmp(led_cdev->name, "led:switch")) {
		snode = container_of(led_cdev, struct flash_switch_data, cdev);
		led = dev_get_drvdata(&snode->pdev->dev);
	} else {
		fnode = container_of(led_cdev, struct flash_node_data, cdev);
		led = dev_get_drvdata(&fnode->pdev->dev);
	}

	spin_lock(&led->lock);
	if (!fnode) {
		rc = qpnp_flash_led_switch_set(snode, value > 0);
		if (rc) {
			dev_err(&led->pdev->dev,
					"Failed to set flash LED switch\n");
			goto exit;
		}
	} else {
		qpnp_flash_led_node_set(fnode, value);
	}

exit:
	spin_unlock(&led->lock);
}

static int qpnp_flash_led_parse_each_led_dt(struct qpnp_flash_led *led,
			struct flash_node_data *fnode, struct device_node *node)
{
	const char *temp_string;
	int rc;
	u32 val;

	fnode->pdev = led->pdev;
	fnode->cdev.brightness_set = qpnp_flash_led_brightness_set;
	fnode->cdev.brightness_get = qpnp_flash_led_brightness_get;

	rc = of_property_read_string(node, "qcom,led-name", &fnode->cdev.name);
	if (rc) {
		dev_err(&led->pdev->dev, "Unable to read flash LED names\n");
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,max-current", &val);
	if (!rc) {
		fnode->cdev.max_brightness = val;
	} else {
		dev_err(&led->pdev->dev, "Unable to read max current\n");
		return rc;
	}

	rc = of_property_read_string(node, "label", &temp_string);
	if (!rc) {
		if (!strcmp(temp_string, "flash"))
			fnode->type = FLASH_LED_TYPE_FLASH;
		else if (!strcmp(temp_string, "torch"))
			fnode->type = FLASH_LED_TYPE_TORCH;
		else {
			dev_err(&led->pdev->dev, "Wrong flash LED type\n");
			return rc;
		}
	} else {
		dev_err(&led->pdev->dev, "Unable to read flash LED label\n");
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,id", &val);
	if (!rc) {
		fnode->id = (u8)val;
	} else {
		dev_err(&led->pdev->dev, "Unable to read flash LED ID\n");
		return rc;
	}

	rc = of_property_read_string(node, "qcom,default-led-trigger",
						&fnode->cdev.default_trigger);
	if (rc) {
		dev_err(&led->pdev->dev, "Unable to read trigger name\n");
		return rc;
	}

	fnode->ires_ua = FLASH_LED_IRES_DEFAULT_UA;
	fnode->ires = FLASH_LED_IRES_DEFAULT_VAL;
	rc = of_property_read_u32(node, "qcom,ires-ua", &val);
	if (!rc) {
		fnode->ires_ua = val;
		fnode->ires = FLASH_LED_IRES_BASE -
			(val - FLASH_LED_IRES_MIN_UA) / FLASH_LED_IRES_DIVISOR;
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev, "Unable to read current resolution\n");
		return rc;
	}

	fnode->duration = FLASH_LED_SAFETY_TMR_DISABLED;
	rc = of_property_read_u32(node, "qcom,duration-ms", &val);
	if (!rc) {
		fnode->duration = (u8)(((val -
					FLASH_LED_SAFETY_TMR_VAL_OFFSET) /
					FLASH_LED_SAFETY_TMR_VAL_DIVISOR) |
					FLASH_LED_SAFETY_TMR_ENABLED);
	} else if (rc == -EINVAL) {
		if (fnode->type == FLASH_LED_TYPE_FLASH) {
			dev_err(&led->pdev->dev,
				"Timer duration is required for flash LED\n");
			return rc;
		}
	} else {
		dev_err(&led->pdev->dev,
				"Unable to read timer duration\n");
		return rc;
	}

	fnode->hdrm_val = FLASH_LED_HDRM_VOL_DEFAULT_MV;
	rc = of_property_read_u32(node, "qcom,hdrm-voltage-mv", &val);
	if (!rc) {
		val = (val - FLASH_LED_HDRM_VOL_BASE_MV) /
						FLASH_LED_HDRM_VOL_STEP_MV;
		fnode->hdrm_val = (val << FLASH_LED_HDRM_VOL_SHIFT) &
							FLASH_LED_HDRM_VOL_MASK;
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev, "Unable to read headroom voltage\n");
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,hdrm-vol-hi-lo-win-mv", &val);
	if (!rc) {
		fnode->hdrm_val |= (val / FLASH_LED_HDRM_VOL_STEP_MV) &
						~FLASH_LED_HDRM_VOL_MASK;
	} else if (rc == -EINVAL) {
		fnode->hdrm_val |= FLASH_LED_HDRM_VOL_HI_LO_WIN_DEFAULT_MV;
	} else {
		dev_err(&led->pdev->dev,
				"Unable to read hdrm hi-lo window voltage\n");
		return rc;
	}

	rc = led_classdev_register(&led->pdev->dev, &fnode->cdev);
	if (rc) {
		dev_err(&led->pdev->dev, "Unable to register led node %d\n",
								fnode->id);
		return rc;
	}
	fnode->cdev.dev->of_node = node;

	fnode->pinctrl = devm_pinctrl_get(fnode->cdev.dev);
	if (IS_ERR_OR_NULL(fnode->pinctrl)) {
		dev_err(&led->pdev->dev, "No pinctrl defined\n");
		fnode->pinctrl = NULL;
	} else {
		fnode->gpio_state_active =
			pinctrl_lookup_state(fnode->pinctrl, "led_enable");
		if (IS_ERR_OR_NULL(fnode->gpio_state_active)) {
			dev_err(&led->pdev->dev,
					"Cannot lookup LED active state\n");
			devm_pinctrl_put(fnode->pinctrl);
			fnode->pinctrl = NULL;
			return PTR_ERR(fnode->gpio_state_active);
		}

		fnode->gpio_state_suspend =
			pinctrl_lookup_state(fnode->pinctrl, "led_disable");
		if (IS_ERR_OR_NULL(fnode->gpio_state_suspend)) {
			dev_err(&led->pdev->dev,
					"Cannot lookup LED disable state\n");
			devm_pinctrl_put(fnode->pinctrl);
			fnode->pinctrl = NULL;
			return PTR_ERR(fnode->gpio_state_suspend);
		}
	}

	return 0;
}

static int qpnp_flash_led_parse_and_register_switch(struct qpnp_flash_led *led,
						struct device_node *node)
{
	int rc;

	rc = of_property_read_string(node, "qcom,led-name",
							&led->snode->cdev.name);
	if (rc) {
		dev_err(&led->pdev->dev, "Failed to read switch node name\n");
		return rc;
	}

	rc = of_property_read_string(node, "qcom,default-led-trigger",
					&led->snode->cdev.default_trigger);
	if (rc) {
		dev_err(&led->pdev->dev, "Unable to read trigger name\n");
		return rc;
	}

	led->snode->pdev = led->pdev;
	led->snode->cdev.brightness_set = qpnp_flash_led_brightness_set;
	led->snode->cdev.brightness_get = qpnp_flash_led_brightness_get;
	rc = led_classdev_register(&led->pdev->dev, &led->snode->cdev);
	if (rc) {
		dev_err(&led->pdev->dev,
					"Unable to register led switch node\n");
		return rc;
	}

	return 0;
}

static int qpnp_flash_led_parse_common_dt(struct qpnp_flash_led *led,
						struct device_node *node)
{
	int rc;
	u32 val;

	led->pdata->hdrm_auto_mode_en = FLASH_LED_HEADROOM_AUTO_MODE_ENABLED;
	led->pdata->hdrm_auto_mode_en = of_property_read_bool(node,
							"qcom,hdrm-auto-mode");

	led->pdata->isc_delay_us = FLASH_LED_ISC_DELAY_DEFAULT_US;
	rc = of_property_read_u32(node, "qcom,isc-delay", &val);
	if (!rc) {
		led->pdata->isc_delay_us = val >> FLASH_LED_ISC_DELAY_SHIFT;
	} else if (rc != -EINVAL) {
		dev_err(&led->pdev->dev, "Unable to read ISC delay\n");
		return rc;
	}

	return 0;
}

static int qpnp_flash_led_probe(struct platform_device *pdev)
{
	struct qpnp_flash_led *led;
	struct device_node *node, *temp;
	unsigned int base;
	int rc, i = 0;

	node = pdev->dev.of_node;
	if (!node) {
		dev_info(&pdev->dev, "No flash LED nodes defined\n");
		return -ENODEV;
	}

	rc = of_property_read_u32(node, "reg", &base);
	if (rc) {
		dev_err(&pdev->dev, "Couldn't find reg in node %s, rc = %d\n",
							node->full_name, rc);
		return rc;
	}

	led = devm_kzalloc(&pdev->dev, sizeof(struct qpnp_flash_led),
								GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	led->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!led->regmap) {
		dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
		return -EINVAL;
	}

	led->base = base;
	led->pdev = pdev;
	led->pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct flash_led_platform_data), GFP_KERNEL);
	if (!led->pdata)
		return -ENOMEM;

	rc = qpnp_flash_led_parse_common_dt(led, node);
	if (rc) {
		dev_err(&pdev->dev,
			"Failed to parse common flash LED device tree\n");
		return rc;
	}

	for_each_child_of_node(node, temp)
		led->num_led_nodes++;
	if (!led->num_led_nodes) {
		dev_err(&pdev->dev, "No LED nodes defined\n");
		return -ECHILD;
	}

	led->fnode = devm_kzalloc(&pdev->dev,
			sizeof(struct flash_node_data) * (--led->num_led_nodes),
			GFP_KERNEL);
	if (!led->fnode)
		return -ENOMEM;

	temp = NULL;
	for (i = 0; i < led->num_led_nodes; i++) {
		temp = of_get_next_child(node, temp);
		rc = qpnp_flash_led_parse_each_led_dt(led,
							&led->fnode[i], temp);
		if (rc) {
			dev_err(&pdev->dev,
					"Unable to parse flash node %d\n", i);
			goto error_led_register;
		}
	}
	led->num_avail_leds = i;

	led->snode = devm_kzalloc(&pdev->dev,
				sizeof(struct flash_switch_data), GFP_KERNEL);
	if (!led->snode) {
		rc = -ENOMEM;
		goto error_led_register;
	}

	temp = of_get_next_child(node, temp);
	rc = qpnp_flash_led_parse_and_register_switch(led, temp);
	if (rc) {
		dev_err(&pdev->dev,
				"Unable to parse and register switch node\n");
		goto error_led_register;
	}

	rc = qpnp_flash_led_init_settings(led);
	if (rc) {
		dev_err(&pdev->dev, "Failed to initialize flash LED\n");
		goto error_switch_register;
	}

	spin_lock_init(&led->lock);

	dev_set_drvdata(&pdev->dev, led);

	return 0;

error_switch_register:
	led_classdev_unregister(&led->snode->cdev);
error_led_register:
	while (i > 0)
		led_classdev_unregister(&led->fnode[--i].cdev);

	return rc;
}

static int qpnp_flash_led_remove(struct platform_device *pdev)
{
	struct qpnp_flash_led *led = dev_get_drvdata(&pdev->dev);
	int i = led->num_led_nodes;

	led_classdev_unregister(&led->snode->cdev);
	while (i > 0)
		led_classdev_unregister(&led->fnode[--i].cdev);

	return 0;
}

const struct of_device_id qpnp_flash_led_match_table[] = {
	{ .compatible = "qcom,qpnp-flash-led-v2",},
	{ },
};

static struct platform_driver qpnp_flash_led_driver = {
	.driver		= {
		.name = "qcom,qpnp-flash-led-v2",
		.of_match_table = qpnp_flash_led_match_table,
	},
	.probe		= qpnp_flash_led_probe,
	.remove		= qpnp_flash_led_remove,
};

static int __init qpnp_flash_led_init(void)
{
	return platform_driver_register(&qpnp_flash_led_driver);
}
late_initcall(qpnp_flash_led_init);

static void __exit qpnp_flash_led_exit(void)
{
	platform_driver_unregister(&qpnp_flash_led_driver);
}
module_exit(qpnp_flash_led_exit);

MODULE_DESCRIPTION("QPNP Flash LED driver v2");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("leds:leds-qpnp-flash-v2");
