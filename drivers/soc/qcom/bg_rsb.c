/* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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

#define pr_fmt(msg) "bgrsb: %s: " msg, __func__
#include "bgrsb.h"

struct bgrsb_priv {
	void *handle;
	struct input_dev *input;
	struct mutex glink_mutex;
	struct mutex rsb_state_mutex;
	enum bgrsb_state bgrsb_current_state;
	void *lhndl;
	char rx_buf[BGRSB_GLINK_INTENT_SIZE];
	struct work_struct bg_up_work;
	struct work_struct bg_down_work;
	struct work_struct rsb_up_work;
	struct work_struct rsb_down_work;
	struct work_struct rsb_calibration_work;
	struct work_struct bttn_configr_work;
	struct workqueue_struct *bgrsb_wq;
	struct bgrsb_regulator rgltr;
	enum ldo_task ldo_action;
	void *bgwear_subsys_handle;
	struct completion wrk_cmplt;
	struct completion bg_lnikup_cmplt;
	struct completion tx_done;
	struct device *ldev;
	struct wakeup_source bgrsb_ws;
	wait_queue_head_t link_state_wait;
	uint32_t calbrtion_intrvl;
	uint32_t calbrtion_cpi;
	uint8_t bttn_configs;
	int msmrsb_gpio;
	bool bg_resp_cmplt;
	bool rsb_rpmsg;
	bool rsb_use_msm_gpio;
	bool is_in_twm;
	bool calibration_needed;
	bool is_calibrd;
	bool is_cnfgrd;
	bool blk_rsb_cmnds;
	bool pending_enable;
};

static void *bgrsb_drv;
static int bgrsb_enable(struct bgrsb_priv *dev, bool enable);

void bgrsb_send_input(struct event *evnt)
{
	uint8_t press_code;
	uint8_t value;
	struct bgrsb_priv *dev =
			container_of(bgrsb_drv, struct bgrsb_priv, lhndl);

	pr_debug("%s: Called\n", __func__);
	if (!evnt) {
		pr_err("%s: No event received\n", __func__);
		return;
	}
	if (evnt->sub_id == 1) {
		input_report_rel(dev->input, REL_WHEEL, evnt->evnt_data);
		input_sync(dev->input);
	} else if (evnt->sub_id == 2) {
		press_code = (uint8_t) evnt->evnt_data;
		value = (uint8_t) (evnt->evnt_data >> 8);

		switch (press_code) {
		case 0x1:
			if (value == 0) {
				input_report_key(dev->input, KEY_VOLUMEDOWN, 1);
				input_sync(dev->input);
			} else {
				input_report_key(dev->input, KEY_VOLUMEDOWN, 0);
				input_sync(dev->input);
			}
			break;
		case 0x2:
			if (value == 0) {
				input_report_key(dev->input, KEY_VOLUMEUP, 1);
				input_sync(dev->input);
			} else {
				input_report_key(dev->input, KEY_VOLUMEUP, 0);
				input_sync(dev->input);
			}
			break;
		case 0x3:
			if (value == 0) {
				input_report_key(dev->input, KEY_POWER, 1);
				input_sync(dev->input);
			} else {
				input_report_key(dev->input, KEY_POWER, 0);
				input_sync(dev->input);
			}
			break;
		default:
			pr_info("event: type[%d] , data: %d\n",
						evnt->sub_id, evnt->evnt_data);
		}
	}
	pr_debug("%s: Ended\n", __func__);
}
EXPORT_SYMBOL(bgrsb_send_input);

static int bgrsb_init_regulators(struct device *pdev)
{
	struct regulator *reg11;
	struct regulator *reg15;
	struct bgrsb_priv *dev = dev_get_drvdata(pdev);

	reg11 = devm_regulator_get(pdev, "vdd-ldo1");
	if (IS_ERR_OR_NULL(reg11)) {
		pr_err("Unable to get regulator for LDO-11\n");
		return PTR_ERR(reg11);
	}

	reg15 = devm_regulator_get(pdev, "vdd-ldo2");
	if (IS_ERR_OR_NULL(reg15)) {
		pr_err("Unable to get regulator for LDO-15\n");
		return PTR_ERR(reg15);
	}

	dev->rgltr.regldo11 = reg11;
	dev->rgltr.regldo15 = reg15;
	return 0;
}

static int bgrsb_set_ldo(struct bgrsb_priv *dev, enum ldo_task ldo_action)
{
	int ret = 0;
	bool value;

	switch (ldo_action) {
	case BGRSB_HW_TURN_ON:
		ret = regulator_set_voltage(dev->rgltr.regldo11,
				BGRSB_LDO11_VTG_MIN_UV, BGRSB_LDO11_VTG_MAX_UV);
		if (ret) {
			pr_err("Failed to request LDO-11 voltage %d\n", ret);
			goto err_ret;
		}
		ret = regulator_enable(dev->rgltr.regldo11);
		if (ret) {
			pr_err("Failed to enable LDO-11 %d\n", ret);
			goto err_ret;
		}
		break;
	case BGRSB_ENABLE_WHEEL_EVENTS:
		if (dev->rsb_use_msm_gpio == true) {
			if (!gpio_is_valid(dev->msmrsb_gpio)) {
				pr_err("gpio %d is not valid\n",
					dev->msmrsb_gpio);
				ret = -ENXIO;
				goto err_ret;
			}

			/* Sleep 50ms for h/w to detect signal */
			msleep(50);

			gpio_set_value(dev->msmrsb_gpio, 1);
			value = gpio_get_value(dev->msmrsb_gpio);
			if (value == true) {
				pr_debug("gpio %d set properly\n",
					dev->msmrsb_gpio);
			} else {
				pr_debug("gpio %d set failed\n",
					dev->msmrsb_gpio);
				ret = -ENXIO;
				goto err_ret;
			}
		} else {
			ret = regulator_set_voltage(dev->rgltr.regldo15,
				BGRSB_LDO15_VTG_MIN_UV, BGRSB_LDO15_VTG_MAX_UV);
			if (ret) {
				pr_err("Request failed LDO-15 %d\n",
						ret);
				goto err_ret;
			}
			ret = regulator_enable(dev->rgltr.regldo15);
			if (ret) {
				pr_err("LDO-15 not enabled%d\n",
						ret);
				goto err_ret;
			}
		}
		break;
	case BGRSB_HW_TURN_OFF:
		ret = regulator_disable(dev->rgltr.regldo11);
		if (ret) {
			pr_err("Failed to disable LDO-11 %d\n", ret);
			goto err_ret;
		}
		break;
	case BGRSB_DISABLE_WHEEL_EVENTS:
		if (dev->rsb_use_msm_gpio == true) {
			if (!gpio_is_valid(dev->msmrsb_gpio)) {
				pr_err("Invalid gpio %d\n",
					dev->msmrsb_gpio);
				ret = -ENXIO;
				goto err_ret;
			}
			/* Sleep 50ms for h/w to detect signal */
			msleep(50);
			gpio_set_value(dev->msmrsb_gpio, 0);
		} else {
			ret = regulator_disable(dev->rgltr.regldo15);
			if (ret) {
				pr_err("Failed to disable LDO-15 %d\n", ret);
				goto err_ret;
			}
			regulator_set_load(dev->rgltr.regldo15, 0);
		}
		break;
	default:
		ret = -EINVAL;
	}

err_ret:
	return ret;
}

static void bgrsb_bgdown_work(struct work_struct *work)
{
	int ret = 0;
	struct bgrsb_priv *dev = container_of(work, struct bgrsb_priv,
								bg_down_work);

	mutex_lock(&dev->rsb_state_mutex);
	if (dev->bgrsb_current_state == BGRSB_STATE_RSB_ENABLED) {
		ret = bgrsb_set_ldo(dev, BGRSB_DISABLE_WHEEL_EVENTS);
		if (ret == 0)
			dev->bgrsb_current_state = BGRSB_STATE_RSB_CONFIGURED;
		else
			pr_err("Failed to unvote LDO-15 on BG down\n");
	}

	if (dev->bgrsb_current_state == BGRSB_STATE_RSB_CONFIGURED) {
		ret = bgrsb_set_ldo(dev, BGRSB_HW_TURN_OFF);
		if (ret == 0)
			dev->bgrsb_current_state = BGRSB_STATE_INIT;
		else
			pr_err("Failed to unvote LDO-11 on BG down\n");
	}

	dev->is_cnfgrd = false;
	dev->blk_rsb_cmnds = false;
	pr_debug("RSB current state is : %d\n", dev->bgrsb_current_state);

	if (dev->bgrsb_current_state == BGRSB_STATE_INIT) {
		if (dev->is_calibrd)
			dev->calibration_needed = true;
	}
	mutex_unlock(&dev->rsb_state_mutex);
}

static int bgrsb_tx_msg(struct bgrsb_priv *dev, void  *msg, size_t len)
{
	int rc = 0;
	uint8_t resp = 0;

	__pm_stay_awake(&dev->bgrsb_ws);
	mutex_lock(&dev->glink_mutex);
	if (!dev->rsb_rpmsg) {
		pr_err("bgrsb-rpmsg is not probed yet, waiting for it to be probed\n");
		goto err_ret;
	}
	rc = bgrsb_rpmsg_tx_msg(msg, len);

	/* wait for sending command to BG */
	rc = wait_event_timeout(dev->link_state_wait,
			(rc == 0), msecs_to_jiffies(TIMEOUT_MS));
	if (rc == 0) {
		pr_err("failed to send command to BG %d\n", rc);
		goto err_ret;
	}

	/* wait for getting response from BG */
	rc = wait_event_timeout(dev->link_state_wait,
			(dev->bg_resp_cmplt == true),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (rc == 0) {
		pr_err("failed to get BG response %d\n", rc);
		goto err_ret;
	}
	dev->bg_resp_cmplt = false;
	/* check BG response */
	resp = *(uint8_t *)dev->rx_buf;
	if (!(resp == 0x01)) {
		pr_err("Bad BG response\n");
		rc = -EINVAL;
		goto err_ret;
	}
	rc = 0;

err_ret:
	mutex_unlock(&dev->glink_mutex);
	__pm_relax(&dev->bgrsb_ws);
	return rc;
}

static int bgrsb_enable(struct bgrsb_priv *dev, bool enable)
{
	struct bgrsb_msg req = {0};

	req.cmd_id = 0x02;
	req.data = enable ? 0x01 : 0x00;

	return bgrsb_tx_msg(dev, &req, BGRSB_MSG_SIZE);
}

static int bgrsb_configr_rsb(struct bgrsb_priv *dev, bool enable)
{
	struct bgrsb_msg req = {0};

	req.cmd_id = 0x01;
	req.data = enable ? 0x01 : 0x00;

	return bgrsb_tx_msg(dev, &req, BGRSB_MSG_SIZE);
}

void bgrsb_notify_glink_channel_state(bool state)
{
	struct bgrsb_priv *dev =
		container_of(bgrsb_drv, struct bgrsb_priv, lhndl);

	pr_debug("%s: RSB-CTRL channel state: %d\n", __func__, state);
	dev->rsb_rpmsg = state;
}
EXPORT_SYMBOL(bgrsb_notify_glink_channel_state);

void bgrsb_rx_msg(void *data, int len)
{
	struct bgrsb_priv *dev =
		container_of(bgrsb_drv, struct bgrsb_priv, lhndl);

	dev->bg_resp_cmplt = true;
	memcpy(dev->rx_buf, data, len);
}
EXPORT_SYMBOL(bgrsb_rx_msg);

static void bgrsb_bgup_work(struct work_struct *work)
{
	int ret = 0;
	struct bgrsb_priv *dev =
			container_of(work, struct bgrsb_priv, bg_up_work);

	mutex_lock(&dev->rsb_state_mutex);
	ret = bgrsb_set_ldo(dev, BGRSB_HW_TURN_ON);
	if (ret == 0) {
		if (!dev->rsb_rpmsg)
			pr_err("bgrsb-rpmsg is not probed yet\n");

		ret = wait_event_timeout(dev->link_state_wait,
			(dev->rsb_rpmsg == true), msecs_to_jiffies(TIMEOUT_MS));
		if (ret == 0) {
			pr_err("channel connection time out %d\n",
						ret);
			goto unlock;
		}
		pr_debug("bgrsb-rpmsg is probed\n");
		ret = bgrsb_configr_rsb(dev, true);
		if (ret != 0) {
			pr_err("BG failed to configure RSB %d\n", ret);
			if (bgrsb_set_ldo(dev, BGRSB_HW_TURN_OFF) == 0)
				dev->bgrsb_current_state = BGRSB_STATE_INIT;
			goto unlock;
		}
		dev->is_cnfgrd = true;
		dev->bgrsb_current_state = BGRSB_STATE_RSB_CONFIGURED;
		pr_debug("RSB Cofigured\n");
		if (dev->pending_enable)
			queue_work(dev->bgrsb_wq, &dev->rsb_up_work);
	}
unlock:
	mutex_unlock(&dev->rsb_state_mutex);
}

/**
 * ssr_bg_cb(): callback function is called.
 * @arg1: a notifier_block.
 * @arg2: opcode that defines the event.
 * @arg3: void pointer.
 *
 * by ssr framework when BG goes down, up and during
 * ramdump collection. It handles BG shutdown and
 * power up events.
 *
 * Return: NOTIFY_DONE.
 */
static int ssr_bgrsb_cb(struct notifier_block *this,
		unsigned long opcode, void *data)
{
	struct bgrsb_priv *dev = container_of(bgrsb_drv,
				struct bgrsb_priv, lhndl);

	switch (opcode) {
	case SUBSYS_BEFORE_SHUTDOWN:
		if (dev->bgrsb_current_state == BGRSB_STATE_RSB_ENABLED)
			dev->pending_enable = true;
		queue_work(dev->bgrsb_wq, &dev->bg_down_work);
		break;
	case SUBSYS_AFTER_POWERUP:
		if (dev->bgrsb_current_state == BGRSB_STATE_INIT)
			queue_work(dev->bgrsb_wq, &dev->bg_up_work);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block ssr_bg_nb = {
	.notifier_call = ssr_bgrsb_cb,
	.priority = 0,
};

/**
 * bgrsb_ssr_register(): callback function is called.
 * @arg1: pointer to bgrsb_priv structure.
 *
 * ssr_register checks that domain id should be in range
 * and register SSR framework for value at domain id.
 *
 * Return: 0 for success and -ENODEV otherwise.
 */
static int bgrsb_ssr_register(struct bgrsb_priv *dev)
{
	struct notifier_block *nb;

	if (!dev)
		return -ENODEV;

	nb = &ssr_bg_nb;
	dev->bgwear_subsys_handle =
			subsys_notif_register_notifier(BGRSB_BGWEAR_SUBSYS, nb);

	if (!dev->bgwear_subsys_handle) {
		dev->bgwear_subsys_handle = NULL;
		return -ENODEV;
	}
	return 0;
}

static void bgrsb_enable_rsb(struct work_struct *work)
{
	int rc = 0;
	struct bgrsb_priv *dev =
		container_of(work, struct bgrsb_priv, rsb_up_work);

	mutex_lock(&dev->rsb_state_mutex);
	if (dev->bgrsb_current_state == BGRSB_STATE_RSB_ENABLED) {
		pr_debug("RSB is already enabled\n");
		goto unlock;
	}
	if (dev->bgrsb_current_state != BGRSB_STATE_RSB_CONFIGURED) {
		pr_err("BG is not yet configured for RSB\n");
		dev->pending_enable = true;
		goto unlock;
	}
	rc = bgrsb_set_ldo(dev, BGRSB_ENABLE_WHEEL_EVENTS);
	if (rc == 0) {
		rc = bgrsb_enable(dev, true);
		if (rc != 0) {
			pr_err("Failed to send enable command to BG %d\n", rc);
			bgrsb_set_ldo(dev, BGRSB_DISABLE_WHEEL_EVENTS);
			dev->bgrsb_current_state = BGRSB_STATE_RSB_CONFIGURED;
			goto unlock;
		}
	}
	dev->bgrsb_current_state = BGRSB_STATE_RSB_ENABLED;
	dev->pending_enable = false;
	pr_debug("RSB Enabled\n");

	if (dev->calibration_needed) {
		dev->calibration_needed = false;
		queue_work(dev->bgrsb_wq, &dev->rsb_calibration_work);
	}
unlock:
	mutex_unlock(&dev->rsb_state_mutex);

}

static void bgrsb_disable_rsb(struct work_struct *work)
{
	int rc = 0;
	struct bgrsb_priv *dev = container_of(work, struct bgrsb_priv,
								rsb_down_work);

	mutex_lock(&dev->rsb_state_mutex);
	dev->pending_enable = false;
	if (dev->bgrsb_current_state == BGRSB_STATE_RSB_ENABLED) {
		rc = bgrsb_enable(dev, false);
		if (rc != 0) {
			pr_err("Failed to send disable command to BG\n");
			goto unlock;
		}
		rc = bgrsb_set_ldo(dev, BGRSB_DISABLE_WHEEL_EVENTS);
		if (rc != 0)
			goto unlock;

		dev->bgrsb_current_state = BGRSB_STATE_RSB_CONFIGURED;
		pr_debug("RSB Disabled\n");
	}

unlock:
	mutex_unlock(&dev->rsb_state_mutex);
}

static void bgrsb_calibration(struct work_struct *work)
{
	int rc = 0;
	struct bgrsb_msg req = {0};
	struct bgrsb_priv *dev =
			container_of(work, struct bgrsb_priv,
							rsb_calibration_work);

	mutex_lock(&dev->rsb_state_mutex);
	if (!dev->is_cnfgrd) {
		pr_err("RSB is not configured\n");
		goto unlock;
	}

	req.cmd_id = 0x03;
	req.data = dev->calbrtion_cpi;

	rc = bgrsb_tx_msg(dev, &req, 5);
	if (rc != 0) {
		pr_err("Failed to send resolution value to BG %d\n", rc);
		goto unlock;
	}

	req.cmd_id = 0x04;
	req.data = dev->calbrtion_intrvl;

	rc = bgrsb_tx_msg(dev, &req, 5);
	if (rc != 0) {
		pr_err("Failed to send interval value to BG %d\n", rc);
		goto unlock;
	}
	dev->is_calibrd = true;
	pr_debug("RSB Calibrated\n");

unlock:
	mutex_unlock(&dev->rsb_state_mutex);
}

static void bgrsb_buttn_configration(struct work_struct *work)
{
	int rc = 0;
	struct bgrsb_msg req = {0};
	struct bgrsb_priv *dev =
			container_of(work, struct bgrsb_priv,
							bttn_configr_work);

	mutex_lock(&dev->rsb_state_mutex);
	if (!dev->is_cnfgrd) {
		pr_err("RSB is not configured\n");
		goto unlock;
	}

	req.cmd_id = 0x05;
	req.data = dev->bttn_configs;

	rc = bgrsb_tx_msg(dev, &req, 5);
	if (rc != 0) {
		pr_err("configuration cmnd failed %d\n",
				rc);
		goto unlock;
	}

	dev->bttn_configs = 0;
	pr_debug("RSB Button configured\n");

unlock:
	mutex_unlock(&dev->rsb_state_mutex);
}

static int bgrsb_handle_cmd_in_ssr(struct bgrsb_priv *dev, char *str)
{
	long val;
	int ret;
	char *tmp;

	tmp = strsep(&str, ":");
	if (!tmp)
		return -EINVAL;

	ret = kstrtol(tmp, 10, &val);
	if (ret < 0)
		return ret;

	if (val == BGRSB_POWER_ENABLE)
		dev->pending_enable = true;
	else if (val == BGRSB_POWER_DISABLE)
		dev->pending_enable = false;

	return 0;
}

static int split_bg_work(struct bgrsb_priv *dev, char *str)
{
	long val;
	int ret;
	char *tmp;

	tmp = strsep(&str, ":");
	if (!tmp)
		return -EINVAL;

	ret = kstrtol(tmp, 10, &val);
	if (ret < 0)
		return ret;

	switch (val) {
	case BGRSB_IN_TWM:
		dev->is_in_twm = true;
	case BGRSB_POWER_DISABLE:
		queue_work(dev->bgrsb_wq, &dev->rsb_down_work);
		break;
	case BGRSB_OUT_TWM:
		dev->is_in_twm = false;
	case BGRSB_POWER_ENABLE:
		queue_work(dev->bgrsb_wq, &dev->rsb_up_work);
		break;
	case BGRSB_POWER_CALIBRATION:
		tmp = strsep(&str, ":");
		if (!tmp)
			return -EINVAL;

		ret = kstrtol(tmp, 10, &val);
		if (ret < 0)
			return ret;

		dev->calbrtion_intrvl = (uint32_t)val;

		tmp = strsep(&str, ":");
		if (!tmp)
			return -EINVAL;

		ret = kstrtol(tmp, 10, &val);
		if (ret < 0)
			return ret;

		dev->calbrtion_cpi = (uint32_t)val;

		queue_work(dev->bgrsb_wq, &dev->rsb_calibration_work);
		break;
	case BGRSB_BTTN_CONFIGURE:
		tmp = strsep(&str, ":");
		if (!tmp)
			return -EINVAL;

		ret = kstrtol(tmp, 10, &val);
		if (ret < 0)
			return ret;

		dev->bttn_configs = (uint8_t)val;
		queue_work(dev->bgrsb_wq, &dev->bttn_configr_work);
		break;
	}
	return 0;
}

static int store_enable(struct device *pdev, struct device_attribute *attr,
		const char *buff, size_t count)
{
	int rc;
	struct bgrsb_priv *dev = dev_get_drvdata(pdev);
	char *arr;

	if (dev->blk_rsb_cmnds) {
		pr_err("Device is in TWM state\n");
		return count;
	}
	arr = kstrdup(buff, GFP_KERNEL);
	if (!arr)
		return -ENOMEM;

	rc = split_bg_work(dev, arr);
	if (!dev->is_cnfgrd) {
		bgrsb_handle_cmd_in_ssr(dev, arr);
		kfree(arr);
		return -ENOMEDIUM;
	}

	if (rc != 0)
		pr_err("Not able to process request\n");

	kfree(arr);
	return count;
}

static int show_enable(struct device *dev, struct device_attribute *attr,
			char *buff)
{
	return 0;
}

static struct device_attribute dev_attr_rsb = {
	.attr = {
		.name = "enable",
		.mode = 00660,
	},
	.show = show_enable,
	.store = store_enable,
};

static int bgrsb_init(struct bgrsb_priv *dev)
{
	bgrsb_drv = &dev->lhndl;
	mutex_init(&dev->glink_mutex);
	mutex_init(&dev->rsb_state_mutex);

	dev->ldo_action = BGRSB_NO_ACTION;

	dev->bgrsb_wq =
		create_singlethread_workqueue("bg-work-queue");
	if (!dev->bgrsb_wq) {
		pr_err("Failed to init BG-RSB work-queue\n");
		return -ENOMEM;
	}

	init_waitqueue_head(&dev->link_state_wait);

	/* set default bgrsb state */
	dev->bgrsb_current_state = BGRSB_STATE_INIT;

	/* Init all works */
	INIT_WORK(&dev->bg_up_work, bgrsb_bgup_work);
	INIT_WORK(&dev->bg_down_work, bgrsb_bgdown_work);
	INIT_WORK(&dev->rsb_up_work, bgrsb_enable_rsb);
	INIT_WORK(&dev->rsb_down_work, bgrsb_disable_rsb);
	INIT_WORK(&dev->rsb_calibration_work, bgrsb_calibration);
	INIT_WORK(&dev->bttn_configr_work, bgrsb_buttn_configration);

	return 0;
}

static int bg_rsb_probe(struct platform_device *pdev)
{
	struct bgrsb_priv *dev;
	struct input_dev *input;
	struct device_node *node;
	int rc;
	unsigned int rsb_gpio;

	node = pdev->dev.of_node;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	/* Add wake lock for PM suspend */
	wakeup_source_init(&dev->bgrsb_ws, "BGRSB_wake_lock");

	dev->bgrsb_current_state = BGRSB_STATE_UNKNOWN;
	rc = bgrsb_init(dev);
	if (rc)
		goto err_ret_dev;
	/* Set up input device */
	input = devm_input_allocate_device(&pdev->dev);
	if (!input)
		goto err_ret_dev;

	input_set_capability(input, EV_REL, REL_WHEEL);
	input_set_capability(input, EV_KEY, KEY_VOLUMEUP);
	input_set_capability(input, EV_KEY, KEY_VOLUMEDOWN);
	input->name = "bg-spi";

	rc = input_register_device(input);
	if (rc) {
		pr_err("Input device registration failed\n");
		goto err_ret_inp;
	}
	dev->input = input;

	/* register device for bg-wear ssr */
	rc = bgrsb_ssr_register(dev);
	if (rc) {
		pr_err("Failed to register for bg ssr\n");
		goto err_ret_inp;
	}
	rc = device_create_file(&pdev->dev, &dev_attr_rsb);
	if (rc) {
		pr_err("Not able to create the file bg-rsb/enable\n");
		goto err_ret_inp;
	}

	dev->rsb_use_msm_gpio =
			of_property_read_bool(node, "qcom,rsb-use-msm-gpio");

	if (dev->rsb_use_msm_gpio == true) {
		rsb_gpio = of_get_named_gpio(node, "qcom,bg-rsb-gpio", 0);
		pr_debug("gpio %d is configured\n", rsb_gpio);

		if (!gpio_is_valid(rsb_gpio)) {
			pr_err("gpio %d found is not valid\n", rsb_gpio);
			goto err_ret;
		}

		if (gpio_request(rsb_gpio, "msm_rsb_gpio")) {
			pr_err("gpio %d request failed\n", rsb_gpio);
			goto err_ret;
		}

		if (gpio_direction_output(rsb_gpio, 1)) {
			pr_err("gpio %d direction not set\n", rsb_gpio);
			goto err_ret;
		}
		pr_debug("rsb gpio successfully requested\n");
		dev->msmrsb_gpio = rsb_gpio;
	}
	dev_set_drvdata(&pdev->dev, dev);
	rc = bgrsb_init_regulators(&pdev->dev);
	if (rc) {
		pr_err("Failed to set regulators\n");
		goto err_ret_inp;
	}

	pr_debug("RSB probe successfully\n");
	return 0;
err_ret:
	return 0;
err_ret_inp:
	input_free_device(input);
err_ret_dev:
	devm_kfree(&pdev->dev, dev);
	return -ENODEV;
}

static int bg_rsb_remove(struct platform_device *pdev)
{
	struct bgrsb_priv *dev = platform_get_drvdata(pdev);

	destroy_workqueue(dev->bgrsb_wq);
	input_free_device(dev->input);
	wakeup_source_trash(&dev->bgrsb_ws);
	return 0;
}

static int bg_rsb_resume(struct device *pldev)
{
	struct platform_device *pdev = to_platform_device(pldev);
	struct bgrsb_priv *dev = platform_get_drvdata(pdev);

	mutex_lock(&dev->rsb_state_mutex);
	if (dev->bgrsb_current_state == BGRSB_STATE_RSB_CONFIGURED)
		goto ret_success;

	if (dev->bgrsb_current_state == BGRSB_STATE_INIT) {
		if (dev->is_cnfgrd &&
		    bgrsb_set_ldo(dev, BGRSB_HW_TURN_ON) == 0) {
			dev->bgrsb_current_state = BGRSB_STATE_RSB_CONFIGURED;
			pr_debug("RSB Cofigured\n");
			goto ret_success;
		}
		pr_err("RSB failed to resume\n");
	}
	mutex_unlock(&dev->rsb_state_mutex);
	return -EINVAL;

ret_success:
	mutex_unlock(&dev->rsb_state_mutex);
	return 0;
}

static int bg_rsb_suspend(struct device *pldev)
{
	struct platform_device *pdev = to_platform_device(pldev);
	struct bgrsb_priv *dev = platform_get_drvdata(pdev);

	mutex_lock(&dev->rsb_state_mutex);
	if (dev->bgrsb_current_state == BGRSB_STATE_INIT)
		goto ret_success;

	if (dev->bgrsb_current_state == BGRSB_STATE_RSB_ENABLED) {
		if (bgrsb_set_ldo(dev, BGRSB_DISABLE_WHEEL_EVENTS) != 0)
			goto ret_err;
	}

	if (bgrsb_set_ldo(dev, BGRSB_HW_TURN_OFF) == 0) {
		dev->bgrsb_current_state = BGRSB_STATE_INIT;
		pr_debug("RSB Init\n");
		goto ret_success;
	}

ret_err:
	pr_err("RSB failed to suspend\n");
	mutex_unlock(&dev->rsb_state_mutex);
	return -EINVAL;

ret_success:
	mutex_unlock(&dev->rsb_state_mutex);
	return 0;
}

static const struct of_device_id bg_rsb_of_match[] = {
	{ .compatible = "qcom,bg-rsb", },
	{ }
};

static const struct dev_pm_ops pm_rsb = {
	.resume		= bg_rsb_resume,
	.suspend	= bg_rsb_suspend,
};

static struct platform_driver bg_rsb_driver = {
	.driver = {
		.name = "bg-rsb",
		.of_match_table = bg_rsb_of_match,
		.pm = &pm_rsb,
	},
	.probe		= bg_rsb_probe,
	.remove		= bg_rsb_remove,
}; module_platform_driver(bg_rsb_driver);
MODULE_DESCRIPTION("SoC BG RSB driver");
MODULE_LICENSE("GPL v2");
