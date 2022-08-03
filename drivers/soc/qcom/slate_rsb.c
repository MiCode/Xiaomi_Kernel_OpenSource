// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(msg) "slatersb: %s: " msg, __func__
#include "slatersb.h"

struct slatersb_priv {
	void *handle;
	struct mutex glink_mutex;
	struct mutex rsb_state_mutex;
	enum slatersb_state slatersb_current_state;
	void *lhndl;
	char rx_buf[SLATERSB_GLINK_INTENT_SIZE];
	struct work_struct slate_up_work;
	struct work_struct slate_down_work;
	struct work_struct rsb_up_work;
	struct work_struct rsb_down_work;
	struct work_struct rsb_calibration_work;
	struct work_struct bttn_configr_work;
	struct workqueue_struct *slatersb_wq;
	void *slate_subsys_handle;
	struct completion wrk_cmplt;
	struct completion slate_lnikup_cmplt;
	struct completion tx_done;
	struct device *ldev;
	struct wakeup_source slatersb_ws;
	wait_queue_head_t link_state_wait;
	uint32_t calbrtion_intrvl;
	uint32_t calbrtion_cpi;
	uint8_t bttn_configs;
	bool slate_resp_cmplt;
	bool rsb_rpmsg;
	bool is_in_twm;
	bool calibration_needed;
	bool is_calibrd;
	bool is_cnfgrd;
	bool blk_rsb_cmnds;
	bool pending_enable;
};

static void *slatersb_drv;
static int slatersb_enable(struct slatersb_priv *dev, bool enable);

static void slatersb_slatedown_work(struct work_struct *work)
{
	struct slatersb_priv *dev = container_of(work, struct slatersb_priv,
								slate_down_work);
	mutex_lock(&dev->rsb_state_mutex);
	if (dev->slatersb_current_state == SLATERSB_STATE_RSB_ENABLED)
		dev->slatersb_current_state = SLATERSB_STATE_RSB_CONFIGURED;

	if (dev->slatersb_current_state == SLATERSB_STATE_RSB_CONFIGURED)
		dev->slatersb_current_state = SLATERSB_STATE_INIT;

	dev->is_cnfgrd = false;
	dev->blk_rsb_cmnds = false;
	pr_debug("RSB current state is : %d\n", dev->slatersb_current_state);

	if (dev->slatersb_current_state == SLATERSB_STATE_INIT) {
		if (dev->is_calibrd)
			dev->calibration_needed = true;
	}
	mutex_unlock(&dev->rsb_state_mutex);
}

static int slatersb_tx_msg(struct slatersb_priv *dev, void  *msg, size_t len)
{
	int rc = 0;
	uint8_t resp = 0;

	__pm_stay_awake(&dev->slatersb_ws);
	mutex_lock(&dev->glink_mutex);
	if (!dev->rsb_rpmsg) {
		pr_err("slatersb-rpmsg is not probed yet, waiting for it to be probed\n");
		goto err_ret;
	}
	rc = slatersb_rpmsg_tx_msg(msg, len);

	/* wait for sending command to SLATE */
	rc = wait_event_timeout(dev->link_state_wait,
			(rc == 0), msecs_to_jiffies(TIMEOUT_MS));
	if (rc == 0) {
		pr_err("failed to send command to SLATE %d\n", rc);
		goto err_ret;
	}

	/* wait for getting response from SLATE */
	rc = wait_event_timeout(dev->link_state_wait,
			dev->slate_resp_cmplt,
				 msecs_to_jiffies(TIMEOUT_MS));
	if (rc == 0) {
		pr_err("failed to get SLATE response %d\n", rc);
		goto err_ret;
	}

	dev->slate_resp_cmplt = false;
	/* check SLATE response */
	resp = *(uint8_t *)dev->rx_buf;
	if (resp == 0x01) {
		pr_err("Bad SLATE response\n");
		rc = -EINVAL;
		goto err_ret;
	}

	rc = 0;

err_ret:
	mutex_unlock(&dev->glink_mutex);
	__pm_relax(&dev->slatersb_ws);
	return rc;
}

static int slatersb_enable(struct slatersb_priv *dev, bool enable)
{
	struct slatersb_msg req = {0};

	req.cmd_id = SLATERSB_ENABLE;
	req.data = enable ? 0x01 : 0x00;

	pr_debug("req.data = %d, req.cmd_id = %d\n", req.data, req.cmd_id);
	return slatersb_tx_msg(dev, &req, SLATERSB_MSG_SIZE);
}

static int slatersb_configr_rsb(struct slatersb_priv *dev, bool enable)
{
	struct slatersb_msg req = {0};

	req.cmd_id = SLATERSB_CONFIGR_RSB;
	req.data = enable ? 0x01 : 0x00;

	pr_debug("req.data = %d, req.cmd_id = %d\n", req.data, req.cmd_id);
	return slatersb_tx_msg(dev, &req, SLATERSB_MSG_SIZE);
}

void slatersb_notify_glink_channel_state(bool state)
{
	struct slatersb_priv *dev =
		container_of(slatersb_drv, struct slatersb_priv, lhndl);

	pr_debug("%s: RSB-CTRL channel state: %d\n", __func__, state);
	dev->rsb_rpmsg = state;
}
EXPORT_SYMBOL(slatersb_notify_glink_channel_state);

void slatersb_rx_msg(void *data, int len)
{
	struct slatersb_priv *dev =
		container_of(slatersb_drv, struct slatersb_priv, lhndl);

	dev->slate_resp_cmplt = true;
	wake_up(&dev->link_state_wait);
	memcpy(dev->rx_buf, data, len);
}
EXPORT_SYMBOL(slatersb_rx_msg);

static void slatersb_slateup_work(struct work_struct *work)
{
	int ret = 0;
	struct slatersb_priv *dev =
			container_of(work, struct slatersb_priv, slate_up_work);

	mutex_lock(&dev->rsb_state_mutex);
		if (!dev->rsb_rpmsg)
			pr_err("slatersb-rpmsg is not probed yet\n");

		ret = wait_event_timeout(dev->link_state_wait,
			dev->rsb_rpmsg, msecs_to_jiffies(TIMEOUT_MS));
		if (ret == 0) {
			pr_err("channel connection time out %d\n",
						ret);
			goto unlock;
		}
		pr_debug("slatersb-rpmsg is probed\n");
		ret = slatersb_configr_rsb(dev, true);
		if (ret != 0) {
			pr_err("SLATE failed to configure RSB %d\n", ret);
			dev->slatersb_current_state = SLATERSB_STATE_INIT;
			goto unlock;
		}
		dev->is_cnfgrd = true;
		dev->slatersb_current_state = SLATERSB_STATE_RSB_CONFIGURED;
		pr_debug("RSB Cofigured\n");
		if (dev->pending_enable)
			queue_work(dev->slatersb_wq, &dev->rsb_up_work);

unlock:
	mutex_unlock(&dev->rsb_state_mutex);
}

/**
 * ssr_slate_cb(): callback function is called.
 * @arg1: a notifier_block.
 * @arg2: opcode that defines the event.
 * @arg3: void pointer.
 *
 * by ssr framework when SLATE goes down, up and during
 * ramdump collection. It handles SLATE shutdown and
 * power up events.
 *
 * Return: NOTIFY_DONE.
 */
static int ssr_slatersb_cb(struct notifier_block *this,
		unsigned long opcode, void *data)
{
	struct slatersb_priv *dev = container_of(slatersb_drv,
				struct slatersb_priv, lhndl);

	switch (opcode) {
	case SUBSYS_BEFORE_SHUTDOWN:
		if (dev->slatersb_current_state == SLATERSB_STATE_RSB_ENABLED)
			dev->pending_enable = true;
		queue_work(dev->slatersb_wq, &dev->slate_down_work);
		break;
	case SUBSYS_AFTER_POWERUP:
		if (dev->slatersb_current_state == SLATERSB_STATE_INIT)
			queue_work(dev->slatersb_wq, &dev->slate_up_work);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block ssr_slate_nb = {
	.notifier_call = ssr_slatersb_cb,
	.priority = 0,
};

/**
 * slatersb_ssr_register(): callback function is called.
 * @arg1: pointer to slatersb_priv structure.
 *
 * ssr_register checks that domain id should be in range
 * and register SSR framework for value at domain id.
 *
 * Return: 0 for success and -ENODEV otherwise.
 */
static int slatersb_ssr_register(struct slatersb_priv *dev)
{
	struct notifier_block *nb;

	if (!dev)
		return -ENODEV;

	nb = &ssr_slate_nb;
	dev->slate_subsys_handle =
			subsys_notif_register_notifier(SLATERSB_SLATE_SUBSYS, nb);

	if (!dev->slate_subsys_handle) {
		dev->slate_subsys_handle = NULL;
		return -ENODEV;
	}
	return 0;
}

static void slatersb_enable_rsb(struct work_struct *work)
{
	int rc = 0;
	struct slatersb_priv *dev =
		container_of(work, struct slatersb_priv, rsb_up_work);

	mutex_lock(&dev->rsb_state_mutex);
	if (dev->slatersb_current_state == SLATERSB_STATE_RSB_ENABLED) {
		pr_debug("RSB is already enabled\n");
		goto unlock;
	}
	if (dev->slatersb_current_state != SLATERSB_STATE_RSB_CONFIGURED) {
		pr_err("SLATE is not yet configured for RSB\n");
		dev->pending_enable = true;
		goto unlock;
	}
	rc = slatersb_enable(dev, true);
	if (rc != 0) {
		pr_err("Failed to send enable command to SLATE%d\n", rc);
		dev->slatersb_current_state = SLATERSB_STATE_RSB_CONFIGURED;
		goto unlock;
	}

	dev->slatersb_current_state = SLATERSB_STATE_RSB_ENABLED;
	dev->pending_enable = false;
	pr_debug("RSB Enabled\n");
	if (dev->calibration_needed) {
		dev->calibration_needed = false;
		queue_work(dev->slatersb_wq, &dev->rsb_calibration_work);
	}
	pr_debug("RSB Enabled\n");
unlock:
	mutex_unlock(&dev->rsb_state_mutex);

}

static void slatersb_disable_rsb(struct work_struct *work)
{
	int rc = 0;
	struct slatersb_priv *dev = container_of(work, struct slatersb_priv,
								rsb_down_work);
	mutex_lock(&dev->rsb_state_mutex);
	dev->pending_enable = false;
	if (dev->slatersb_current_state == SLATERSB_STATE_RSB_ENABLED) {
		rc = slatersb_enable(dev, false);
		if (rc != 0) {
			pr_err("Failed to send disable command to SLATE\n");
			goto unlock;
		}
	dev->slatersb_current_state = SLATERSB_STATE_RSB_CONFIGURED;
	pr_debug("RSB Disabled\n");
	}
unlock:
	mutex_unlock(&dev->rsb_state_mutex);
}

static void slatersb_calibration(struct work_struct *work)
{
	int rc = 0;
	struct slatersb_msg req = {0};
	struct slatersb_priv *dev =
			container_of(work, struct slatersb_priv,
							rsb_calibration_work);

	mutex_lock(&dev->rsb_state_mutex);
	if (!dev->is_cnfgrd) {
		pr_err("RSB is not configured\n");
		goto unlock;
	}

	req.cmd_id = SLATERSB_CALIBRATION_RESOLUTION;
	req.data = dev->calbrtion_cpi;

	rc = slatersb_tx_msg(dev, &req, 5);
	if (rc != 0) {
		pr_err("Failed to send resolution value to SLATE %d\n", rc);
		goto unlock;
	}

	req.cmd_id = SLATERSB_CALIBRATION_INTERVAL;
	req.data = dev->calbrtion_intrvl;

	rc = slatersb_tx_msg(dev, &req, 5);
	if (rc != 0) {
		pr_err("Failed to send interval value to SLATE %d\n", rc);
		goto unlock;
	}
	dev->is_calibrd = true;
	pr_debug("RSB Calibrated\n");

unlock:
	mutex_unlock(&dev->rsb_state_mutex);
}

static void slatersb_buttn_configration(struct work_struct *work)
{
	int rc = 0;
	struct slatersb_msg req = {0};
	struct slatersb_priv *dev =
			container_of(work, struct slatersb_priv,
							bttn_configr_work);

	mutex_lock(&dev->rsb_state_mutex);
	if (!dev->is_cnfgrd) {
		pr_err("RSB is not configured\n");
		goto unlock;
	}

	req.cmd_id = SLATERSB_BUTTN_CONFIGRATION;
	req.data = dev->bttn_configs;

	rc = slatersb_tx_msg(dev, &req, 5);
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

static int slatersb_handle_cmd_in_ssr(struct slatersb_priv *dev, char *str)
{
	long val;
	int ret = 0;
	char *tmp;

	tmp = strsep(&str, ":");
	if (!tmp)
		return -EINVAL;

	ret = kstrtol(tmp, 10, &val);
	if (ret < 0)
		return ret;

	if (val == SLATERSB_POWER_ENABLE)
		dev->pending_enable = true;
	else if (val == SLATERSB_POWER_DISABLE)
		dev->pending_enable = false;

	return 0;
}

static int split_slate_work(struct slatersb_priv *dev, char *str)
{
	long val;
	int ret = 0;
	char *tmp;

	tmp = strsep(&str, ":");
	if (!tmp)
		return -EINVAL;

	ret = kstrtol(tmp, 10, &val);
	if (ret < 0)
		return ret;

	switch (val) {
	case SLATERSB_IN_TWM:
		dev->is_in_twm = true;
	case SLATERSB_POWER_DISABLE:
		queue_work(dev->slatersb_wq, &dev->rsb_down_work);
		break;
	case SLATERSB_OUT_TWM:
		dev->is_in_twm = false;
	case SLATERSB_POWER_ENABLE:
		queue_work(dev->slatersb_wq, &dev->rsb_up_work);
		break;
	case SLATERSB_POWER_CALIBRATION:
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

		queue_work(dev->slatersb_wq, &dev->rsb_calibration_work);
		break;
	case SLATERSB_BTTN_CONFIGURE:
		tmp = strsep(&str, ":");
		if (!tmp)
			return -EINVAL;

		ret = kstrtol(tmp, 10, &val);
		if (ret < 0)
			return ret;

		dev->bttn_configs = (uint8_t)val;
		queue_work(dev->slatersb_wq, &dev->bttn_configr_work);
		break;
	}
	return 0;
}

static ssize_t store_enable(struct device *pdev, struct device_attribute *attr,
		const char *buff, size_t count)
{
	int rc;
	struct slatersb_priv *dev = dev_get_drvdata(pdev);
	char *arr;

	if (dev->blk_rsb_cmnds) {
		pr_err("Device is in TWM state\n");
		return count;
	}
	arr = kstrdup(buff, GFP_KERNEL);
	if (!arr)
		return -ENOMEM;

	rc = split_slate_work(dev, arr);
	if (!dev->is_cnfgrd) {
		slatersb_handle_cmd_in_ssr(dev, arr);
		kfree(arr);
		return -ENOMEDIUM;
	}

	if (rc != 0)
		pr_err("Not able to process request\n");

	kfree(arr);
	return count;
}

static ssize_t show_enable(struct device *dev, struct device_attribute *attr,
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

static int slatersb_init(struct slatersb_priv *dev)
{
	slatersb_drv = &dev->lhndl;
	mutex_init(&dev->glink_mutex);
	mutex_init(&dev->rsb_state_mutex);

	dev->slatersb_wq =
		create_singlethread_workqueue("slate-work-queue");
	if (!dev->slatersb_wq) {
		pr_err("Failed to init SLATE-RSB work-queue\n");
		return -ENOMEM;
	}

	init_waitqueue_head(&dev->link_state_wait);

	/* set default slatersb state */
	dev->slatersb_current_state = SLATERSB_STATE_INIT;

	/* Init all works */
	INIT_WORK(&dev->slate_up_work, slatersb_slateup_work);
	INIT_WORK(&dev->slate_down_work, slatersb_slatedown_work);
	INIT_WORK(&dev->rsb_up_work, slatersb_enable_rsb);
	INIT_WORK(&dev->rsb_down_work, slatersb_disable_rsb);
	INIT_WORK(&dev->rsb_calibration_work, slatersb_calibration);
	INIT_WORK(&dev->bttn_configr_work, slatersb_buttn_configration);

	return 0;
}

static int slate_rsb_probe(struct platform_device *pdev)
{
	struct slatersb_priv *dev;
	struct device_node *node;
	int rc = 0;

	node = pdev->dev.of_node;
	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	/* Add wake lock for PM suspend */
	wakeup_source_add(&dev->slatersb_ws);
	dev->slatersb_current_state = SLATERSB_STATE_UNKNOWN;
	rc = slatersb_init(dev);
	if (rc)
		pr_err("init failed\n");
	/* register device for slate ssr */
	rc = slatersb_ssr_register(dev);
	if (rc)
		pr_err("Failed to register for slate ssr\n");
	rc = device_create_file(&pdev->dev, &dev_attr_rsb);
	if (rc)
		pr_err("Not able to create the file slate-rsb/enable\n");

	dev_set_drvdata(&pdev->dev, dev);
	pr_debug("RSB probe successfully\n");
	return 0;

}

static int slate_rsb_remove(struct platform_device *pdev)
{
	struct slatersb_priv *dev = platform_get_drvdata(pdev);

	destroy_workqueue(dev->slatersb_wq);
	wakeup_source_trash(&dev->slatersb_ws);
	return 0;
}

static int slate_rsb_resume(struct device *pldev)
{
	return 0;
}

static int slate_rsb_suspend(struct device *pldev)
{
	return 0;
}

static const struct of_device_id slate_rsb_of_match[] = {
	{ .compatible = "qcom,slate-rsb" },
	{ },
};

static const struct dev_pm_ops pm_rsb = {
	.resume		= slate_rsb_resume,
	.suspend	= slate_rsb_suspend,
};

static struct platform_driver slate_rsb_driver = {
	.driver = {
		.name = "slate-rsb",
		.of_match_table = slate_rsb_of_match,
		.pm = &pm_rsb,
	},
	.probe		= slate_rsb_probe,
	.remove		= slate_rsb_remove,
};
module_platform_driver(slate_rsb_driver);

MODULE_DESCRIPTION("SoC SLATE RSB driver");
MODULE_LICENSE("GPL v2");
