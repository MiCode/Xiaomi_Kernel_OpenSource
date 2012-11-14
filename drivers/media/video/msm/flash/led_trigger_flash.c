/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/export.h>
#include "msm_flash.h"

#define FLASH_NAME "camera-led-flash"
static struct msm_flash_ctrl_t fctrl;

static int msm_camera_led_trigger_flash(struct msm_flash_ctrl_t *fctrl,
	uint8_t led_state)
{
	int rc = 0;
	CDBG("%s:%d called led_state %d\n", __func__, __LINE__, led_state);

	if (!fctrl->led_trigger[0]) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -EINVAL;
	}
	switch (led_state) {
	case MSM_CAMERA_LED_OFF:
		led_trigger_event(fctrl->led_trigger[0], 0);
		break;

	case MSM_CAMERA_LED_LOW:
		led_trigger_event(fctrl->led_trigger[0],
			fctrl->max_current[0] / 2);
		break;

	case MSM_CAMERA_LED_HIGH:
		led_trigger_event(fctrl->led_trigger[0], fctrl->max_current[0]);
		break;

	case MSM_CAMERA_LED_INIT:
	case MSM_CAMERA_LED_RELEASE:
		led_trigger_event(fctrl->led_trigger[0], 0);
		break;

	default:
		rc = -EFAULT;
		break;
	}
	CDBG("flash_set_led_state: return %d\n", rc);
	return rc;
}

static const struct of_device_id msm_camera_flash_dt_match[] = {
	{.compatible = "qcom,camera-led-flash"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_camera_flash_dt_match);

static struct platform_driver msm_led_trigger_flash_driver = {
	.driver = {
		.name = FLASH_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_camera_flash_dt_match,
	},
};

static int32_t msm_led_trigger_flash_probe(struct platform_device *pdev)
{
	int32_t rc = 0, i = 0;
	struct device_node *of_node = pdev->dev.of_node;
	struct device_node *flash_src_node = NULL;
	uint32_t count = 0;

	CDBG("%s called\n", __func__);

	if (!of_node) {
		pr_err("%s of_node NULL\n", __func__);
		return -EINVAL;
	}

	fctrl.pdev = pdev;

	rc = of_property_read_u32(of_node, "cell-index", &pdev->id);
	if (rc < 0) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -EINVAL;
	}
	CDBG("%s:%d pdev id %d\n", __func__, __LINE__, pdev->id);

	if (of_get_property(of_node, "qcom,flash-source", &count)) {
		count /= sizeof(uint32_t);
		CDBG("%s count %d\n", __func__, count);
		if (count > MAX_LED_TRIGGERS) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			return -EINVAL;
		}
		for (i = 0; i < count; i++) {
			flash_src_node = of_parse_phandle(of_node,
				"qcom,flash-source", i);
			if (!flash_src_node) {
				pr_err("%s:%d flash_src_node NULL\n", __func__,
					__LINE__);
				continue;
			}

			rc = of_property_read_string(flash_src_node,
				"linux,default-trigger",
				&fctrl.led_trigger_name[i]);
			if (rc < 0) {
				pr_err("%s:%d failed\n", __func__, __LINE__);
				of_node_put(flash_src_node);
				continue;
			}

			CDBG("%s default trigger %s\n", __func__,
				fctrl.led_trigger_name[i]);

			rc = of_property_read_u32(flash_src_node,
				"qcom,max-current", &fctrl.max_current[i]);
			if (rc < 0) {
				pr_err("%s:%d failed rc %d\n", __func__,
					__LINE__, rc);
				of_node_put(flash_src_node);
				continue;
			}

			of_node_put(flash_src_node);

			CDBG("%s max_current[%d] %d\n", __func__, i,
				fctrl.max_current[i]);

			led_trigger_register_simple(fctrl.led_trigger_name[i],
				&fctrl.led_trigger[i]);
		}
	}
	rc = msm_flash_platform_probe(pdev, &fctrl);
	return rc;
}

static int __init msm_flash_add_driver(void)
{
	CDBG("%s called\n", __func__);
	return platform_driver_probe(&msm_led_trigger_flash_driver,
		msm_led_trigger_flash_probe);
}

static struct msm_flash_fn_t msm_led_trigger_flash_func_tbl = {
	.flash_led_config = msm_camera_led_trigger_flash,
};

static struct msm_flash_ctrl_t fctrl = {
	.func_tbl = &msm_led_trigger_flash_func_tbl,
};

module_init(msm_flash_add_driver);
MODULE_DESCRIPTION("LED TRIGGER FLASH");
MODULE_LICENSE("GPL v2");
