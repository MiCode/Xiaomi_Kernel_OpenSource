/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <soc/qcom/glink.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/regulator/consumer.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>

#include "bgrsb.h"

#define BGRSB_GLINK_INTENT_SIZE 0x04
#define BGRSB_MSG_SIZE 0x08
#define TIMEOUT_MS 2000

#define BGRSB_LDO15_VTG_MIN_UV 3300000
#define BGRSB_LDO15_VTG_MAX_UV 3300000

#define BGRSB_LDO11_VTG_MIN_UV 1800000
#define BGRSB_LDO11_VTG_MAX_UV 1800000

#define BGRSB_BGWEAR_SUBSYS "bg-wear"

#define BGRSB_BTTN_CONFIGURE 5
#define BGRSB_POWER_CALIBRATION 2
#define BGRSB_POWER_ENABLE 1
#define BGRSB_POWER_DISABLE 0
#define BGRSB_GLINK_POWER_ENABLE 6
#define BGRSB_GLINK_POWER_DISABLE 7


struct bgrsb_regulator {
	struct regulator *regldo11;
	struct regulator *regldo15;
};

enum ldo_task {
	BGRSB_ENABLE_LDO11,
	BGRSB_ENABLE_LDO15,
	BGRSB_DISABLE_LDO11,
	BGRSB_DISABLE_LDO15,
	BGRSB_NO_ACTION
};

enum bgrsb_state {
	BGRSB_STATE_UNKNOWN,
	BGRSB_STATE_INIT,
	BGRSB_STATE_LDO11_ENABLED,
	BGRSB_STATE_RSB_CONFIGURED,
	BGRSB_STATE_LDO15_ENABLED,
	BGRSB_STATE_RSB_ENABLED
};

struct bgrsb_msg {
	uint32_t cmd_id;
	uint32_t data;
};

struct bgrsb_priv {
	void *handle;
	struct input_dev *input;
	struct mutex glink_mutex;

	enum bgrsb_state bgrsb_current_state;
	enum glink_link_state link_state;

	bool chnl_state;
	void *lhndl;

	struct work_struct bg_up_work;
	struct work_struct bg_down_work;

	struct work_struct rsb_up_work;
	struct work_struct rsb_down_work;

	struct work_struct rsb_glink_up_work;
	struct work_struct rsb_glink_down_work;

	struct work_struct rsb_calibration_work;
	struct work_struct bttn_configr_work;

	struct work_struct glink_work;

	struct workqueue_struct *bgrsb_event_wq;
	struct workqueue_struct *bgrsb_wq;

	struct bg_glink_chnl chnl;
	char rx_buf[BGRSB_GLINK_INTENT_SIZE];

	struct bgrsb_regulator rgltr;

	enum ldo_task ldo_action;

	void *bgwear_subsys_handle;

	struct completion bg_resp_cmplt;
	struct completion wrk_cmplt;
	struct completion bg_lnikup_cmplt;
	struct completion tx_done;

	struct device *ldev;

	struct wakeup_source bgrsb_ws;

	wait_queue_head_t link_state_wait;

	uint32_t calbrtion_intrvl;
	uint32_t calbrtion_cpi;

	uint8_t bttn_configs;

	bool calibration_needed;
	bool is_calibrd;
};

static void *bgrsb_drv;
static int bgrsb_enable(struct bgrsb_priv *dev, bool enable);

int bgrsb_send_input(struct event *evnt)
{
	uint8_t press_code;
	uint8_t value;

	struct bgrsb_priv *dev =
			container_of(bgrsb_drv, struct bgrsb_priv, lhndl);

	if (!evnt)
		return -EINVAL;

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
	return 0;
}
EXPORT_SYMBOL(bgrsb_send_input);

static void bgrsb_glink_notify_rx(void *handle, const void *priv,
	const void *pkt_priv, const void *ptr, size_t size)
{
	struct bgrsb_priv *dev = (struct bgrsb_priv *)priv;

	memcpy(dev->rx_buf, ptr, size);
	glink_rx_done(dev->handle, ptr, false);
	complete(&dev->bg_resp_cmplt);
}

static void bgrsb_glink_notify_state(void *handle, const void *priv,
	unsigned int event)
{
	struct bgrsb_priv *dev = (struct bgrsb_priv *)priv;

	switch (event) {
	case GLINK_CONNECTED:
		complete(&dev->bg_lnikup_cmplt);
		break;
	case GLINK_REMOTE_DISCONNECTED:
	case GLINK_LOCAL_DISCONNECTED:
		dev->chnl_state = false;
		break;
	}
}

static void bgrsb_glink_notify_tx_done(void *handle, const void *priv,
	const void *pkt_priv, const void *ptr)
{
	struct bgrsb_priv *dev = (struct bgrsb_priv *)priv;

	complete(&dev->tx_done);
}

static void bgrsb_glink_close_work(struct work_struct *work)
{
	struct bgrsb_priv *dev =
			container_of(work, struct bgrsb_priv, glink_work);

	if (dev->handle)
		glink_close(dev->handle);
	dev->handle = NULL;
}

static void bgrsb_glink_open_work(struct work_struct *work)
{
	struct glink_open_config open_cfg;
	void *hndl = NULL;
	int rc = 0;
	struct bgrsb_priv *dev =
			container_of(work, struct bgrsb_priv, glink_work);

	if (dev->handle)
		return;

	memset(&open_cfg, 0, sizeof(struct glink_open_config));
	open_cfg.priv = (void *)dev;
	open_cfg.edge = dev->chnl.chnl_edge;
	open_cfg.transport = dev->chnl.chnl_trnsprt;
	open_cfg.name = dev->chnl.chnl_name;
	open_cfg.notify_tx_done = bgrsb_glink_notify_tx_done;
	open_cfg.notify_state = bgrsb_glink_notify_state;
	open_cfg.notify_rx = bgrsb_glink_notify_rx;

	init_completion(&dev->bg_lnikup_cmplt);
	hndl = glink_open(&open_cfg);

	if (IS_ERR_OR_NULL(hndl)) {
		pr_err("Glink open failed[%s]\n",
						dev->chnl.chnl_name);
		dev->handle = NULL;
		return;
	}

	rc = wait_for_completion_timeout(&dev->bg_lnikup_cmplt,
						msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("Channel open failed. Time out\n");
		return;
	}
	dev->chnl_state = true;
	dev->handle = hndl;
}

static void bgrsb_glink_state_cb(struct glink_link_state_cb_info *cb_info,
	void *data)
{
	struct bgrsb_priv *dev = (struct bgrsb_priv *)data;

	dev->link_state = cb_info->link_state;
	switch (dev->link_state) {
	case GLINK_LINK_STATE_UP:
		INIT_WORK(&dev->glink_work, bgrsb_glink_open_work);
		queue_work(dev->bgrsb_event_wq, &dev->glink_work);
		break;
	case GLINK_LINK_STATE_DOWN:
		INIT_WORK(&dev->glink_work, bgrsb_glink_close_work);
		queue_work(dev->bgrsb_event_wq, &dev->glink_work);
		break;
	}
}

static int bgrsb_init_link_inf(struct bgrsb_priv *dev)
{
	struct glink_link_info link_info;
	void *hndl;

	link_info.glink_link_state_notif_cb = bgrsb_glink_state_cb;
	link_info.transport = dev->chnl.chnl_trnsprt;
	link_info.edge = dev->chnl.chnl_edge;

	hndl = glink_register_link_state_cb(&link_info, (void *)dev);
	if (IS_ERR_OR_NULL(hndl)) {
		pr_err("Unable to register link[%s]\n",
							dev->chnl.chnl_name);
		return -EFAULT;
	}
	return 0;
}

static int bgrsb_init_regulators(struct device *pdev)
{
	struct regulator *reg11;
	struct regulator *reg15;
	struct bgrsb_priv *dev = dev_get_drvdata(pdev);

	reg11 = regulator_get(pdev, "vdd-ldo1");
	if (IS_ERR_OR_NULL(reg11)) {
		pr_err("Unable to get regulator for LDO-11\n");
		return PTR_ERR(reg11);
	}

	reg15 = regulator_get(pdev, "vdd-ldo2");
	if (IS_ERR_OR_NULL(reg15)) {
		pr_err("Unable to get regulator for LDO-15\n");
		return PTR_ERR(reg15);
	}

	dev->rgltr.regldo11 = reg11;
	dev->rgltr.regldo15 = reg15;

	return 0;
}

static int bgrsb_ldo_work(struct bgrsb_priv *dev, enum ldo_task ldo_action)
{
	int ret = 0;

	switch (ldo_action) {
	case BGRSB_ENABLE_LDO11:
		ret = regulator_set_voltage(dev->rgltr.regldo11,
				BGRSB_LDO11_VTG_MIN_UV, BGRSB_LDO11_VTG_MAX_UV);
		if (ret) {
			pr_err("Failed to request LDO-11 voltage.\n");
			goto err_ret;
		}
		ret = regulator_enable(dev->rgltr.regldo11);
		if (ret) {
			pr_err("Failed to enable LDO-11 %d\n", ret);
			goto err_ret;
		}
		break;

	case BGRSB_ENABLE_LDO15:
		ret = regulator_set_voltage(dev->rgltr.regldo15,
				BGRSB_LDO15_VTG_MIN_UV, BGRSB_LDO15_VTG_MAX_UV);
		if (ret) {
			pr_err("Failed to request LDO-15 voltage.\n");
			goto err_ret;
		}
		ret = regulator_enable(dev->rgltr.regldo15);
		if (ret) {
			pr_err("Failed to enable LDO-15 %d\n", ret);
			goto err_ret;
		}
		break;
	case BGRSB_DISABLE_LDO11:
		ret = regulator_disable(dev->rgltr.regldo11);
		if (ret) {
			pr_err("Failed to disable LDO-11 %d\n", ret);
			goto err_ret;
		}
		break;

	case BGRSB_DISABLE_LDO15:
		ret = regulator_disable(dev->rgltr.regldo15);
		if (ret) {
			pr_err("Failed to disable LDO-15 %d\n", ret);
			goto err_ret;
		}
		regulator_set_load(dev->rgltr.regldo15, 0);
		break;
	default:
		ret = -EINVAL;
	}

err_ret:
	return ret;
}

static void bgrsb_bgdown_work(struct work_struct *work)
{
	struct bgrsb_priv *dev = container_of(work, struct bgrsb_priv,
								bg_down_work);

	if (dev->bgrsb_current_state == BGRSB_STATE_RSB_ENABLED) {
		if (bgrsb_ldo_work(dev, BGRSB_DISABLE_LDO15) == 0)
			dev->bgrsb_current_state = BGRSB_STATE_RSB_CONFIGURED;
		else
			pr_err("Failed to unvote LDO-15 on BG down\n");
	}

	if (dev->bgrsb_current_state == BGRSB_STATE_RSB_CONFIGURED) {
		if (bgrsb_ldo_work(dev, BGRSB_DISABLE_LDO11) == 0)
			dev->bgrsb_current_state = BGRSB_STATE_INIT;
		else
			pr_err("Failed to unvote LDO-11 on BG down\n");
	}

	pr_info("RSB current state is : %d\n", dev->bgrsb_current_state);

	if (dev->bgrsb_current_state == BGRSB_STATE_INIT) {
		if (dev->is_calibrd)
			dev->calibration_needed = true;
	}
}

static void bgrsb_glink_bgdown_work(struct work_struct *work)
{
	int rc;
	struct bgrsb_priv *dev = container_of(work, struct bgrsb_priv,
							rsb_glink_down_work);

	if (dev->bgrsb_current_state == BGRSB_STATE_RSB_ENABLED) {

		rc = bgrsb_enable(dev, false);
		if (rc != 0) {
			pr_err("Failed to send disable command to BG\n");
			return;
		}

		if (bgrsb_ldo_work(dev, BGRSB_DISABLE_LDO15) != 0) {
			pr_err("Failed to un-vote LDO-15\n");
			return;
		}

		dev->bgrsb_current_state = BGRSB_STATE_RSB_CONFIGURED;
		pr_info("RSB Disabled\n");
	}

	if (dev->bgrsb_current_state == BGRSB_STATE_RSB_CONFIGURED) {
		if (bgrsb_ldo_work(dev, BGRSB_DISABLE_LDO11) == 0)
			dev->bgrsb_current_state = BGRSB_STATE_INIT;
		else
			pr_err("Failed to unvote LDO-11 on BG Glink down\n");
	}
	if (dev->handle)
		glink_close(dev->handle);
	dev->handle = NULL;
	pr_debug("BG Glink Close connection\n");
}

static int bgrsb_tx_msg(struct bgrsb_priv *dev, void  *msg, size_t len)
{
	int rc = 0;
	uint8_t resp = 0;

	if (!dev->chnl_state)
		return -ENODEV;

	__pm_stay_awake(&dev->bgrsb_ws);
	mutex_lock(&dev->glink_mutex);
	init_completion(&dev->tx_done);
	init_completion(&dev->bg_resp_cmplt);

	rc = glink_queue_rx_intent(dev->handle,
					(void *)dev, BGRSB_GLINK_INTENT_SIZE);

	if (rc) {
		pr_err("Failed to queue intent\n");
		goto err_ret;
	}

	rc = glink_tx(dev->handle, (void *)dev, msg,
					len, GLINK_TX_REQ_INTENT);
	if (rc) {
		pr_err("Failed to send command\n");
		goto err_ret;
	}

	rc = wait_for_completion_timeout(&dev->tx_done,
						msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("Timed out waiting for Command to send\n");
		rc = -ETIMEDOUT;
		goto err_ret;
	}

	rc = wait_for_completion_timeout(&dev->bg_resp_cmplt,
						msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("Timed out waiting for response\n");
		rc = -ETIMEDOUT;
		goto err_ret;
	}

	resp = *(uint8_t *)dev->rx_buf;
	if (!(resp == 0x01)) {
		pr_err("Bad RSB response\n");
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
	int rc = 0;
	struct bgrsb_msg req = {0};

	req.cmd_id = 0x02;
	req.data = enable ? 0x01 : 0x00;

	rc = bgrsb_tx_msg(dev, &req, BGRSB_MSG_SIZE);
	return rc;
}

static int bgrsb_configr_rsb(struct bgrsb_priv *dev, bool enable)
{
	int rc = 0;
	struct bgrsb_msg req = {0};

	req.cmd_id = 0x01;
	req.data = enable ? 0x01 : 0x00;

	rc = bgrsb_tx_msg(dev, &req, BGRSB_MSG_SIZE);
	return rc;
}

static void bgrsb_bgup_work(struct work_struct *work)
{
	int rc = 0;
	struct bgrsb_priv *dev = container_of(work, struct bgrsb_priv,
								bg_up_work);

	if (bgrsb_ldo_work(dev, BGRSB_ENABLE_LDO11) == 0) {

		rc = wait_event_timeout(dev->link_state_wait,
				(dev->chnl_state == true),
					msecs_to_jiffies(TIMEOUT_MS));
		if (rc == 0) {
			pr_err("Glink channel connection time out\n");
			return;
		}
		rc = bgrsb_configr_rsb(dev, true);
		if (rc != 0) {
			pr_err("BG failed to configure RSB %d\n", rc);
			if (bgrsb_ldo_work(dev, BGRSB_DISABLE_LDO11) == 0)
				dev->bgrsb_current_state = BGRSB_STATE_INIT;
			return;
		}
		dev->bgrsb_current_state = BGRSB_STATE_RSB_CONFIGURED;
		pr_debug("RSB Cofigured\n");
	}
}

static void bgrsb_glink_bgup_work(struct work_struct *work)
{
	int rc = 0;
	struct bgrsb_priv *dev = container_of(work, struct bgrsb_priv,
							rsb_glink_up_work);

	if (bgrsb_ldo_work(dev, BGRSB_ENABLE_LDO11) == 0) {

		INIT_WORK(&dev->glink_work, bgrsb_glink_open_work);
		queue_work(dev->bgrsb_event_wq, &dev->glink_work);

		rc = wait_event_timeout(dev->link_state_wait,
					(dev->chnl_state == true),
						msecs_to_jiffies(TIMEOUT_MS));
		if (rc == 0) {
			pr_err("Glink channel connection time out\n");
			return;
		}
		rc = bgrsb_configr_rsb(dev, true);
		if (rc != 0) {
			pr_err("BG Glink failed to configure RSB %d\n", rc);
			if (bgrsb_ldo_work(dev, BGRSB_DISABLE_LDO11) == 0)
				dev->bgrsb_current_state = BGRSB_STATE_INIT;
			return;
		}
		dev->bgrsb_current_state = BGRSB_STATE_RSB_CONFIGURED;
		pr_debug("Glink RSB Cofigured\n");
	}
}

/**
 *ssr_bg_cb(): callback function is called
 *by ssr framework when BG goes down, up and during ramdump
 *collection. It handles BG shutdown and power up events.
 */
static int ssr_bgrsb_cb(struct notifier_block *this,
		unsigned long opcode, void *data)
{
	struct bgrsb_priv *dev = container_of(bgrsb_drv,
				struct bgrsb_priv, lhndl);

	switch (opcode) {
	case SUBSYS_BEFORE_SHUTDOWN:
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
 * ssr_register checks that domain id should be in range and register
 * SSR framework for value at domain id.
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
		return -EFAULT;
	}
	return 0;
}

static void bgrsb_enable_rsb(struct work_struct *work)
{
	int rc = 0;
	struct bgrsb_priv *dev = container_of(work, struct bgrsb_priv,
								rsb_up_work);

	if (dev->bgrsb_current_state != BGRSB_STATE_RSB_CONFIGURED) {
		pr_err("BG is not yet configured for RSB\n");
		return;
	}

	if (bgrsb_ldo_work(dev, BGRSB_ENABLE_LDO15) == 0) {

		rc = bgrsb_enable(dev, true);
		if (rc != 0) {
			pr_err("Failed to send enable command to BG\n");
			bgrsb_ldo_work(dev, BGRSB_DISABLE_LDO15);
			dev->bgrsb_current_state = BGRSB_STATE_RSB_CONFIGURED;
			return;
		}
	}
	dev->bgrsb_current_state = BGRSB_STATE_RSB_ENABLED;
	pr_debug("RSB Enabled\n");

	if (dev->calibration_needed) {
		dev->calibration_needed = false;
		queue_work(dev->bgrsb_wq, &dev->rsb_calibration_work);
	}
}

static void bgrsb_disable_rsb(struct work_struct *work)
{
	int rc = 0;
	struct bgrsb_priv *dev = container_of(work, struct bgrsb_priv,
								rsb_down_work);

	if (dev->bgrsb_current_state == BGRSB_STATE_RSB_ENABLED) {

		rc = bgrsb_enable(dev, false);
		if (rc != 0) {
			pr_err("Failed to send disable command to BG\n");
			return;
		}

		if (bgrsb_ldo_work(dev, BGRSB_DISABLE_LDO15) != 0)
			return;

		dev->bgrsb_current_state = BGRSB_STATE_RSB_CONFIGURED;
		pr_debug("RSB Disabled\n");
	}
}

static void bgrsb_calibration(struct work_struct *work)
{
	int rc = 0;
	struct bgrsb_msg req = {0};
	struct bgrsb_priv *dev =
			container_of(work, struct bgrsb_priv,
							rsb_calibration_work);

	req.cmd_id = 0x03;
	req.data = dev->calbrtion_cpi;

	rc = bgrsb_tx_msg(dev, &req, 5);
	if (rc != 0) {
		pr_err("Failed to send resolution value to BG\n");
		return;
	}

	req.cmd_id = 0x04;
	req.data = dev->calbrtion_intrvl;

	rc = bgrsb_tx_msg(dev, &req, 5);
	if (rc != 0) {
		pr_err("Failed to send interval value to BG\n");
		return;
	}
	dev->is_calibrd = true;
	pr_debug("RSB Calibbered\n");
}

static void bgrsb_buttn_configration(struct work_struct *work)
{
	int rc = 0;
	struct bgrsb_msg req = {0};
	struct bgrsb_priv *dev =
			container_of(work, struct bgrsb_priv,
							bttn_configr_work);

	req.cmd_id = 0x05;
	req.data = dev->bttn_configs;

	rc = bgrsb_tx_msg(dev, &req, 5);
	if (rc != 0) {
		pr_err("Failed to send button configuration cmnd to BG\n");
		return;
	}

	dev->bttn_configs = 0;
	pr_debug("Button configured\n");
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
	case BGRSB_POWER_DISABLE:
		if (dev->bgrsb_current_state == BGRSB_STATE_RSB_CONFIGURED)
			return 0;
		queue_work(dev->bgrsb_wq, &dev->rsb_down_work);
		break;
	case BGRSB_POWER_ENABLE:
		if (dev->bgrsb_current_state == BGRSB_STATE_RSB_ENABLED)
			return 0;
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
	case BGRSB_GLINK_POWER_DISABLE:
		queue_work(dev->bgrsb_wq, &dev->rsb_glink_down_work);
		break;
	case BGRSB_GLINK_POWER_ENABLE:
		queue_work(dev->bgrsb_wq, &dev->rsb_glink_up_work);
		break;
	}
	return 0;
}

static int store_enable(struct device *pdev, struct device_attribute *attr,
		const char *buff, size_t count)
{
	int rc;
	struct bgrsb_priv *dev = dev_get_drvdata(pdev);
	char *arr = kstrdup(buff, GFP_KERNEL);

	if (!arr)
		goto err_ret;

	rc = split_bg_work(dev, arr);
	if (rc != 0)
		pr_err("Not able to process request\n");

err_ret:
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
	dev->chnl.chnl_name = "RSB_CTRL";
	dev->chnl.chnl_edge = "bg";
	dev->chnl.chnl_trnsprt = "bgcom";
	mutex_init(&dev->glink_mutex);
	dev->link_state = GLINK_LINK_STATE_DOWN;

	dev->ldo_action = BGRSB_NO_ACTION;

	dev->bgrsb_event_wq =
		create_singlethread_workqueue(dev->chnl.chnl_name);
	if (!dev->bgrsb_event_wq) {
		pr_err("Failed to init Glink work-queue\n");
		goto err_ret;
	}

	dev->bgrsb_wq =
		create_singlethread_workqueue("bg-work-queue");
	if (!dev->bgrsb_wq) {
		pr_err("Failed to init BG-RSB work-queue\n");
		goto free_rsb_wq;
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
	INIT_WORK(&dev->rsb_glink_down_work, bgrsb_glink_bgdown_work);
	INIT_WORK(&dev->rsb_glink_up_work, bgrsb_glink_bgup_work);

	return 0;

free_rsb_wq:
	destroy_workqueue(dev->bgrsb_event_wq);
err_ret:
	return -EFAULT;
}

static int bg_rsb_probe(struct platform_device *pdev)
{
	struct bgrsb_priv *dev;
	struct input_dev *input;
	int rc;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	/* Add wake lock for PM suspend */
	wakeup_source_init(&dev->bgrsb_ws, "BGRSB_wake_lock");

	dev->bgrsb_current_state = BGRSB_STATE_UNKNOWN;
	rc = bgrsb_init(dev);
	if (rc)
		goto err_ret_dev;

	rc = bgrsb_init_link_inf(dev);
	if (rc)
		goto err_ret_dev;

	/* Set up input device */
	input = input_allocate_device();
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
	dev_set_drvdata(&pdev->dev, dev);
	rc = bgrsb_init_regulators(&pdev->dev);
	if (rc) {
		pr_err("Failed to set regulators\n");
		goto err_ret_inp;
	}
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

	destroy_workqueue(dev->bgrsb_event_wq);
	destroy_workqueue(dev->bgrsb_wq);
	input_free_device(dev->input);
	wakeup_source_trash(&dev->bgrsb_ws);

	return 0;
}

static int bg_rsb_resume(struct device *pldev)
{
	struct platform_device *pdev = to_platform_device(pldev);
	struct bgrsb_priv *dev = platform_get_drvdata(pdev);

	if (dev->bgrsb_current_state == BGRSB_STATE_RSB_CONFIGURED)
		return 0;

	if (dev->bgrsb_current_state == BGRSB_STATE_INIT) {
		if (bgrsb_ldo_work(dev, BGRSB_ENABLE_LDO11) == 0) {
			dev->bgrsb_current_state = BGRSB_STATE_RSB_CONFIGURED;
			pr_debug("RSB Cofigured\n");
			return 0;
		}
		pr_err("RSB failed to resume\n");
	}
	return -EINVAL;
}

static int bg_rsb_suspend(struct device *pldev)
{
	struct platform_device *pdev = to_platform_device(pldev);
	struct bgrsb_priv *dev = platform_get_drvdata(pdev);

	if (dev->bgrsb_current_state == BGRSB_STATE_INIT)
		return 0;

	if (dev->bgrsb_current_state == BGRSB_STATE_RSB_ENABLED) {
		if (bgrsb_ldo_work(dev, BGRSB_DISABLE_LDO15) != 0)
			goto ret_err;
	}

	if (bgrsb_ldo_work(dev, BGRSB_DISABLE_LDO11) == 0) {
		dev->bgrsb_current_state = BGRSB_STATE_INIT;
		pr_debug("RSB Init\n");
		return 0;
	}

ret_err:
	pr_err("RSB failed to suspend\n");
	return -EINVAL;
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
};

module_platform_driver(bg_rsb_driver);
MODULE_DESCRIPTION("SoC BG RSB driver");
MODULE_LICENSE("GPL v2");
