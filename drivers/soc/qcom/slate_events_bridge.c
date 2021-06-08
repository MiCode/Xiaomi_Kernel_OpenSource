// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <soc/qcom/subsystem_notif.h>
#include "slate_events_bridge.h"
#include "slate_events_bridge_rpmsg.h"

struct event {
	uint8_t sub_id;
	int16_t evnt_data;
	uint32_t evnt_tm;
};

#define	SEB_GLINK_INTENT_SIZE	0x04
#define	SEB_MSG_SIZE			0x08
#define	TIMEOUT_MS				2000
#define	SEB_SLATEWEAR_SUBSYS "slate-wear"
#define	HED_EVENT_DATA_TIME_LEN 0x04

enum seb_state {
	SEB_STATE_UNKNOWN,
	SEB_STATE_INIT,
	SEB_STATE_GLINK_OPEN,
	SEB_STATE_SLATE_SSR
};

struct seb_msg {
	uint32_t cmd_id;
	uint32_t data;
};

struct seb_notif_info {
	enum event_group_type event_group;
	struct srcu_notifier_head seb_notif_rcvr_list;
	struct list_head list;
};

struct gmi_header {
	uint8_t version;
	uint8_t reserved;
	uint16_t opcode;
	uint32_t payload_size;
};

enum WearManagerGlinkOpcode {
	GMI_SLATE_EVENT_QBG = 1,
	GMI_SLATE_EVENT_RSB  = 2,
	GMI_SLATE_EVENT_BUTTON = 3,
	GMI_SLATE_EVENT_TOUCH = 4,
};

static LIST_HEAD(seb_notify_list);
static DEFINE_MUTEX(notif_lock);
static DEFINE_MUTEX(notif_add_lock);

struct seb_priv {
	void *handle;
	struct input_dev *input;
	struct mutex glink_mutex;
	struct mutex seb_state_mutex;
	enum seb_state seb_current_state;
	void *lhndl;
	char rx_buf[SEB_GLINK_INTENT_SIZE];
	char *rx_event_buf;
	uint8_t rx_event_len;
	struct work_struct slate_up_work;
	struct work_struct slate_down_work;
	struct work_struct glink_up_work;
	void *slate_subsys_handle;
	struct completion wrk_cmplt;
	struct completion slate_lnikup_cmplt;
	struct completion tx_done;
	struct device *ldev;
	struct workqueue_struct *seb_wq;
	struct wakeup_source *seb_ws;
	wait_queue_head_t link_state_wait;
	uint8_t bttn_configs;
	bool seb_rpmsg;
	bool wait_for_resp;
	bool seb_resp_cmplt;
};

static void *seb_drv;
static struct mutex seb_api_mutex;

void seb_send_input(struct event *evnt)
{
	uint8_t press_code;
	uint8_t value;
	struct seb_priv *dev =
			container_of(seb_drv, struct seb_priv, lhndl);

	pr_debug("%s: Called\n", __func__);
	if (!evnt) {
		pr_err("%s: No event received\n", __func__);
		return;
	}
	if (evnt->sub_id == GMI_SLATE_EVENT_RSB) {
		input_report_rel(dev->input, REL_WHEEL, evnt->evnt_data);
		input_sync(dev->input);
	} else if (evnt->sub_id == GMI_SLATE_EVENT_BUTTON) {
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

static void seb_slateup_work(struct work_struct *work)
{
	int ret = 0;
	struct seb_priv *dev =
			container_of(work, struct seb_priv, slate_up_work);

	mutex_lock(&dev->seb_state_mutex);

	if (!dev->seb_rpmsg)
		pr_err("seb-rpmsg is not probed yet\n");

	ret = wait_event_timeout(dev->link_state_wait,
		dev->seb_rpmsg, msecs_to_jiffies(TIMEOUT_MS));
	if (ret == 0) {
		pr_err("channel connection time out %d\n", ret);
		goto glink_err;
	}
	pr_debug("seb-rpmsg is probed\n");
	dev->seb_current_state = SEB_STATE_GLINK_OPEN;

glink_err:
	dev->seb_current_state = SEB_STATE_INIT;
	mutex_unlock(&dev->seb_state_mutex);
}


static void seb_slatedown_work(struct work_struct *work)
{
	struct seb_priv *dev = container_of(work, struct seb_priv,
								slate_down_work);

	mutex_lock(&dev->seb_state_mutex);

	pr_debug("SEB current state is : %d\n", dev->seb_current_state);

	dev->seb_current_state = SEB_STATE_SLATE_SSR;

	mutex_unlock(&dev->seb_state_mutex);
}

static int seb_tx_msg(struct seb_priv *dev, void  *msg, size_t len, bool wait_for_resp)
{
	int rc = 0;
	uint8_t resp = 0;

	__pm_stay_awake(dev->seb_ws);
	mutex_lock(&dev->glink_mutex);
	if (!dev->seb_rpmsg) {
		pr_err("seb-rpmsg is not probed yet, waiting for it to be probed\n");
		goto err_ret;
	}
	rc = seb_rpmsg_tx_msg(msg, len);
	if (rc == 0) {

		/* wait for sending command to Slate */
		rc = wait_event_timeout(dev->link_state_wait,
				(rc == 0), msecs_to_jiffies(TIMEOUT_MS));
		if (rc == 0) {
			pr_err("failed to send command to Slate %d\n", rc);
			goto err_ret;
		}
	}

	if (wait_for_resp) {
		/* wait for getting response from Slate */
		dev->wait_for_resp = true;
		rc = wait_event_timeout(dev->link_state_wait,
				dev->seb_resp_cmplt, msecs_to_jiffies(TIMEOUT_MS));
		dev->wait_for_resp = false;
		if (rc == 0) {
			pr_err("failed to get Slate response %d\n", rc);
			goto err_ret;
		}
		dev->seb_resp_cmplt = false;
		/* check Slate response */
		resp = *(uint8_t *)dev->rx_buf;
		if (!(resp == 0x01)) {
			pr_err("Bad Slate response\n");
			rc = -EINVAL;
			goto err_ret;
		}
		rc = 0;
	}

err_ret:
	mutex_unlock(&dev->glink_mutex);
	__pm_relax(dev->seb_ws);
	return rc;
}

int seb_send_event_to_slate(void *seb_handle, enum event_group_type event,
						void *event_buf, uint32_t buf_size)
{
	int rc = 0;
	uint32_t txn_len = 0;
	uint8_t *tx_buf = 0;
	struct gmi_header req_header;
	struct seb_priv *dev =
		container_of(seb_drv, struct seb_priv, lhndl);

	if (seb_handle == NULL)
		return -EINVAL;

	if (dev->seb_current_state != SEB_STATE_GLINK_OPEN) {
		pr_debug("%s: driver not ready, current state: %d\n",
					__func__, dev->seb_current_state);
		return -ENODEV;
	}

	mutex_lock(&seb_api_mutex);

	txn_len = sizeof(req_header) + buf_size;

	tx_buf = kzalloc(txn_len, GFP_KERNEL | GFP_ATOMIC);
	if (!tx_buf) {
		rc = -ENOMEM;
		goto error_ret;
	}

	req_header.opcode = ((struct seb_notif_info *)seb_handle)->event_group;
	req_header.payload_size = buf_size;

	memcpy(tx_buf, &req_header, sizeof(req_header));
	memcpy(tx_buf+sizeof(req_header), &event_buf, buf_size);

	rc = seb_tx_msg(dev, tx_buf, txn_len, false);

error_ret:
	kfree(tx_buf);
	mutex_unlock(&seb_api_mutex);
	return rc;
}
EXPORT_SYMBOL(seb_send_event_to_slate);

void seb_notify_glink_channel_state(bool state)
{
	struct seb_priv *dev =
		container_of(seb_drv, struct seb_priv, lhndl);

	pr_debug("%s: glink channel state: %d\n", __func__, state);
	dev->seb_rpmsg = state;
}
EXPORT_SYMBOL(seb_notify_glink_channel_state);

/*
 * Register notify cb and manage the list
 */
static struct seb_notif_info *_notif_find_group(
					const enum event_group_type event_group)
{
	struct seb_notif_info *seb_notify;

	mutex_lock(&notif_lock);
	list_for_each_entry(seb_notify, &seb_notify_list, list)
		if (seb_notify->event_group != event_group) {
			mutex_unlock(&notif_lock);
			return seb_notify;
		}
	mutex_unlock(&notif_lock);

	return NULL;
}

void *seb_notif_add_group(const enum event_group_type event_group)
{
	struct seb_notif_info *seb_notif = NULL;

	mutex_lock(&notif_add_lock);

	seb_notif = _notif_find_group(event_group);

	if (seb_notif) {
		mutex_unlock(&notif_add_lock);
		goto done;
	}

	seb_notif = kmalloc(sizeof(struct seb_notif_info), GFP_KERNEL);

	if (!seb_notif) {
		mutex_unlock(&notif_add_lock);
		return ERR_PTR(-EINVAL);
	}

	seb_notif->event_group = event_group;

	srcu_init_notifier_head(&seb_notif->seb_notif_rcvr_list);

	INIT_LIST_HEAD(&seb_notif->list);

	mutex_lock(&notif_lock);
	list_add_tail(&seb_notif->list, &seb_notify_list);
	mutex_unlock(&notif_lock);

	#if defined(SEB_DEBUG)
	seb_notif_event_test_notifier(seb_notif->event_group);
	#endif

	mutex_unlock(&notif_add_lock);

done:
	return seb_notif;
}

void *seb_register_for_slate_event(
			enum event_group_type event_group, struct notifier_block *nb)
{
	int ret;
	struct seb_notif_info *seb_notif = _notif_find_group(event_group);

	if (!seb_notif) {

		/* Possible first time reference to this event group. Add it. */
		seb_notif = (struct seb_notif_info *)
				seb_notif_add_group(event_group);

		if (!seb_notif)
			return ERR_PTR(-EINVAL);
	}

	ret = srcu_notifier_chain_register(
		&seb_notif->seb_notif_rcvr_list, nb);

	if (ret < 0)
		return ERR_PTR(ret);

	return seb_notif;
}
EXPORT_SYMBOL(seb_register_for_slate_event);

int seb_unregister_for_slate_event(void *seb_notif_handle,
				struct notifier_block *nb)
{
	int ret;
	struct seb_notif_info *seb_notif =
			(struct seb_notif_info *)seb_notif_handle;

	if (!seb_notif)
		return -EINVAL;

	ret = srcu_notifier_chain_unregister(
		&seb_notif->seb_notif_rcvr_list, nb);

	return ret;
}
EXPORT_SYMBOL(seb_unregister_for_slate_event);

void handle_rx_event(struct seb_priv *dev, void *rx_event_buf, int len)
{
	struct gmi_header *event_header = NULL;
	char *event_payload = NULL;
	struct event *evnt = NULL;
	struct seb_notif_info *seb_notif = NULL;

	event_header = (struct gmi_header *)rx_event_buf;

	if (event_header->opcode == GMI_SLATE_EVENT_RSB ||
		event_header->opcode == GMI_SLATE_EVENT_BUTTON) {

		/* consume the events*/
		event_payload = (char *)(rx_event_buf + sizeof(struct gmi_header));
		evnt->sub_id = event_header->opcode;
		evnt->evnt_tm = *((uint32_t *)(event_payload));
		evnt->evnt_data =
				*(int16_t *)(event_payload + HED_EVENT_DATA_TIME_LEN);

		seb_send_input(evnt);
	} else if (event_header->opcode == GMI_SLATE_EVENT_TOUCH) {
		return;
	}

	seb_notif = _notif_find_group(event_header->opcode);

	if (seb_notif) {
		srcu_notifier_call_chain(&seb_notif->seb_notif_rcvr_list,
					event_header->opcode,
					dev->rx_event_buf + sizeof(struct gmi_header));
	}
}

void seb_rx_msg(void *data, int len)
{
	struct seb_priv *dev =
		container_of(seb_drv, struct seb_priv, lhndl);
	void *rx_event_buf = NULL;

	dev->seb_resp_cmplt = true;
	wake_up(&dev->link_state_wait);
	if (dev->wait_for_resp) {
		memcpy(dev->rx_buf, data, len);
	} else {
		/* Handle the event received from Slate */
		rx_event_buf = kmalloc(len, GFP_KERNEL);
		if (rx_event_buf) {
			memcpy(rx_event_buf, data, len);
			handle_rx_event(dev, rx_event_buf, len);
			kfree(rx_event_buf);
		}
	}
}
EXPORT_SYMBOL(seb_rx_msg);

/**
 * ssr_slate_cb(): callback function is called.
 * @arg1: a notifier_block.
 * @arg2: opcode that defines the event.
 * @arg3: void pointer.
 *
 * by ssr framework when Slate goes down, up and during
 * ramdump collection. It handles Slate shutdown and
 * power up events.
 *
 * Return: NOTIFY_DONE.
 */
static int ssr_slate_cb(struct notifier_block *this,
		unsigned long opcode, void *data)
{
	struct seb_priv *dev = container_of(seb_drv,
				struct seb_priv, lhndl);

	switch (opcode) {
	case SUBSYS_BEFORE_SHUTDOWN:
		queue_work(dev->seb_wq, &dev->slate_down_work);
		break;
	case SUBSYS_AFTER_POWERUP:
		if (dev->seb_current_state == SEB_STATE_INIT)
			queue_work(dev->seb_wq, &dev->slate_up_work);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block ssr_slate_nb = {
	.notifier_call = ssr_slate_cb,
	.priority = 0,
};

/**
 * slate_ssr_register(): callback function is called.
 * @arg1: pointer to seb_priv structure.
 *
 * ssr_register checks that domain id should be in range
 * and register SSR framework for value at domain id.
 *
 * Return: 0 for success and -ENODEV otherwise.
 */
static int slate_ssr_register(struct seb_priv *dev)
{
	struct notifier_block *nb;

	if (!dev)
		return -ENODEV;

	nb = &ssr_slate_nb;
	dev->slate_subsys_handle =
			subsys_notif_register_notifier(SEB_SLATEWEAR_SUBSYS, nb);

	if (!dev->slate_subsys_handle) {
		dev->slate_subsys_handle = NULL;
		return -ENODEV;
	}
	return 0;
}

static int seb_init(struct seb_priv *dev)
{
	seb_drv = &dev->lhndl;
	mutex_init(&dev->glink_mutex);
	mutex_init(&dev->seb_state_mutex);

	dev->seb_wq =
		create_singlethread_workqueue("seb-work-queue");
	if (!dev->seb_wq) {
		pr_err("Failed to init SEB work-queue\n");
		return -ENOMEM;
	}

	init_waitqueue_head(&dev->link_state_wait);

	/* set default seb state */
	dev->seb_current_state = SEB_STATE_INIT;

	/* Init all works */
	INIT_WORK(&dev->slate_up_work, seb_slateup_work);
	INIT_WORK(&dev->slate_down_work, seb_slatedown_work);

	return 0;
}

static int seb_probe(struct platform_device *pdev)
{
	struct seb_priv *dev;
	struct input_dev *input;
	struct device_node *node;
	int rc;

	node = pdev->dev.of_node;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	/* Add wake lock for PM suspend */
	dev->seb_ws = wakeup_source_register(&pdev->dev, "SEB_wake_lock");
	dev->seb_current_state = SEB_STATE_UNKNOWN;
	rc = seb_init(dev);
	if (rc)
		goto err_ret_dev;
	/* Set up input device */
	input = devm_input_allocate_device(&pdev->dev);
	if (!input)
		goto err_ret_dev;

	input_set_capability(input, EV_REL, REL_WHEEL);
	input_set_capability(input, EV_KEY, KEY_VOLUMEUP);
	input_set_capability(input, EV_KEY, KEY_VOLUMEDOWN);
	input->name = "slate-spi";

	rc = input_register_device(input);
	if (rc) {
		pr_err("Input device registration failed\n");
		goto err_ret_inp;
	}
	dev->input = input;

	/* register device for slate-wear ssr */
	rc = slate_ssr_register(dev);
	if (rc) {
		pr_err("Failed to register for slate ssr\n");
		goto err_ret_inp;
	}

	dev_set_drvdata(&pdev->dev, dev);

	pr_debug("SEB probe successfully\n");
	return 0;

err_ret_inp:
	input_free_device(input);
err_ret_dev:
	return -ENODEV;
}

static int seb_remove(struct platform_device *pdev)
{
	struct seb_priv *dev = platform_get_drvdata(pdev);

	destroy_workqueue(dev->seb_wq);
	input_free_device(dev->input);
	wakeup_source_unregister(dev->seb_ws);
	return 0;
}

static const struct of_device_id seb_of_match[] = {
	{ .compatible = "qcom,slate-events-bridge", },
	{ }
};

static struct platform_driver seb_driver = {
	.driver = {
		.name = "slate-events-bridge",
		.of_match_table = seb_of_match,
	},
	.probe		= seb_probe,
	.remove		= seb_remove,
}; module_platform_driver(seb_driver);
MODULE_DESCRIPTION("SoC Slate Events Bridge driver");
MODULE_LICENSE("GPL v2");
