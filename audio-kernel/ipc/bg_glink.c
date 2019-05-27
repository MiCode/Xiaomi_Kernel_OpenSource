/* Copyright (c) 2017-2018, The Linux Foundation.All rights reserved.
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
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <soc/bg_glink.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <soc/qcom/glink.h>

#define GLINK_LINK_STATE_UP_WAIT_TIMEOUT 5000
#define APR_MAXIMUM_NUM_OF_RETRIES 2
#define BG_RX_INTENT_REQ_TIMEOUT_MS 3000
#define BG_GLINK_NAME "bg-cdc-glink"
#define BG_GLINK_EDGE "bg"
#define BG_MAX_NO_OF_INTENTS 20

struct bg_cdc_glink_drvdata {
	struct device *dev;
	struct platform_device *pdev;
	struct bg_cdc_glink_ch_info *ch_info;
	void *handle;
	wait_queue_head_t wait;
	u8 num_channels;
	u8 active_channel;
	enum glink_link_state link_state;
};

struct bg_cdc_glink_ch_info {
	void *handle;
	struct mutex w_lock;
	struct mutex r_lock;
	struct mutex m_lock;
	bg_glink_cb_fn func;
	wait_queue_head_t wait;
	unsigned channel_state;
	bool if_remote_intent_ready;
};
static int __bg_cdc_glink_write(struct bg_cdc_glink_ch_info *ch_info,
				void *data, char *tx_buf, int len)
{
	int rc = 0;

	if (!ch_info)
		return -EINVAL;
	mutex_lock(&ch_info->w_lock);
	rc = glink_tx(ch_info->handle, tx_buf, data, len, GLINK_TX_REQ_INTENT);
	mutex_unlock(&ch_info->w_lock);

	if (rc)
		pr_err("%s: glink_tx failed, rc[%d]\n", __func__, rc);
	else
		rc = len;

	return rc;
}

int bg_cdc_glink_write(void *ch_info, void *data,
		       int len)
{
	int rc = 0;
	char *tx_buf = NULL;

	if (!((struct bg_cdc_glink_ch_info *)ch_info)->handle || !data)
		return -EINVAL;

	/* check if channel is connected before proceeding */
	if (((struct bg_cdc_glink_ch_info *)ch_info)->channel_state
						!= GLINK_CONNECTED) {
		pr_err("%s: channel is not connected\n", __func__);
		return -EINVAL;
	}

	tx_buf = kzalloc((sizeof(char) * len), GFP_KERNEL);
	if (IS_ERR_OR_NULL(tx_buf)) {
		rc = -EINVAL;
		goto exit;
	}
	memcpy(tx_buf, data, len);

	rc = __bg_cdc_glink_write((struct bg_cdc_glink_ch_info *)ch_info,
				tx_buf, tx_buf, len);

	if (rc < 0) {
		pr_err("%s: Unable to send the packet, rc:%d\n", __func__, rc);
		kfree(tx_buf);
	}
exit:
	return rc;
}
EXPORT_SYMBOL(bg_cdc_glink_write);

static void bg_cdc_glink_notify_rx(void *handle, const void *priv,
			    const void *pkt_priv, const void *ptr,
			    size_t size)
{
	struct bg_cdc_glink_ch_info *ch_info =
				(struct bg_cdc_glink_ch_info *)priv;

	if (!ch_info || !ptr) {
		pr_err("%s: Invalid ch_info or ptr\n", __func__);
		return;
	}

	pr_debug("%s: Rx packet received\n", __func__);

	mutex_lock(&ch_info->r_lock);
	if (ch_info->func)
		ch_info->func((void *)ptr, size);
	mutex_unlock(&ch_info->r_lock);
	glink_rx_done(ch_info->handle, ptr, true);
}

static void bg_cdc_glink_notify_tx_abort(void *handle, const void *priv,
				    const void *pkt_priv)
{
	pr_debug("%s: tx_abort received for pkt_priv:%pK\n",
		 __func__, pkt_priv);
	kfree(pkt_priv);
}

static void bg_cdc_glink_notify_tx_done(void *handle, const void *priv,
			    const void *pkt_priv, const void *ptr)
{
	pr_debug("%s: tx_done received for pkt_priv:%pK\n",
		 __func__, pkt_priv);
	kfree(pkt_priv);
}

static bool bg_cdc_glink_notify_rx_intent_req(void *handle, const void *priv,
				  size_t req_size)
{
	struct bg_cdc_glink_ch_info *ch_info =
				(struct bg_cdc_glink_ch_info *)priv;

	if (!ch_info) {
		pr_err("%s: Invalid ch_info\n", __func__);
		return false;
	}

	pr_debug("%s: No rx intents queued, unable to receive\n", __func__);
	return false;
}

static void bg_cdc_glink_notify_remote_rx_intent(void *handle, const void *priv,
					    size_t size)
{
	struct bg_cdc_glink_ch_info *ch_info =
				(struct bg_cdc_glink_ch_info *)priv;

	if (!ch_info) {
		pr_err("%s: Invalid ch_info\n", __func__);
		return;
	}
	/*
	 * This is to make sure that the far end has queued at least one intent
	 * before we attempt any IPC.
	 */
	pr_debug("%s: remote queued an intent\n", __func__);
	ch_info->if_remote_intent_ready = true;
	wake_up(&ch_info->wait);
}

static void bg_cdc_glink_notify_state(void *handle, const void *priv,
				unsigned event)
{
	struct bg_cdc_glink_ch_info *ch_info =
				(struct bg_cdc_glink_ch_info *)priv;

	if (!ch_info) {
		pr_err("%s: Invalid ch_info\n", __func__);
		return;
	}

	ch_info->channel_state = event;
	pr_debug("%s: Channel state[%d]\n", __func__, event);

	if (event == GLINK_CONNECTED)
		wake_up(&ch_info->wait);
}

static int bg_cdc_glink_rx_intents_config(struct bg_cdc_glink_ch_info *ch_info,
				   int num_of_intents, uint32_t *size)
{
	int i;
	int rc = 0;

	if (!ch_info || !num_of_intents || !size) {
		pr_err("%s: Invalid parameter\n", __func__);
		return -EINVAL;
	}
	if (num_of_intents > BG_MAX_NO_OF_INTENTS) {
		pr_err("%s: Invalid no_of_intents = %d\n",
			__func__, num_of_intents);
		return -EINVAL;
	}

	for (i = 0; i < num_of_intents; i++) {
		rc = glink_queue_rx_intent(ch_info->handle, ch_info, *(size+i));
		if (rc) {
			pr_err("%s: Failed to queue rx intent, iteration[%d]\n",
			       __func__, i);
			break;
		}
	}

	return rc;
}
/*
 * bg_cdc_channel_open - API to open Glink channel.
 * ch_cfg:    glink channel configuration
 * func:     callback function to notify client.
 */
void  *bg_cdc_channel_open(struct platform_device *pdev,
			struct bg_glink_ch_cfg *ch_cfg,
			 bg_glink_cb_fn func)
{
	int rc;
	struct bg_cdc_glink_drvdata *bg_cdc_glink;
	struct glink_open_config open_cfg;
	struct bg_cdc_glink_ch_info *ch_info;

	if (!pdev) {
		pr_err("%s: invalid platform device\n",__func__);
		return NULL;
	}
	bg_cdc_glink = platform_get_drvdata(pdev);

	if (!bg_cdc_glink) {
		dev_err(&pdev->dev, "%s: driver data not found\n",
			__func__);
		return NULL;
	}
	if (bg_cdc_glink->active_channel > bg_cdc_glink->num_channels) {
		dev_err(bg_cdc_glink->dev, "%s: invalid channel number\n",
			__func__);
		return NULL;
	}

	ch_info = &bg_cdc_glink->ch_info[bg_cdc_glink->active_channel];
	mutex_lock(&ch_info->m_lock);
	if (ch_info->handle) {
		dev_err(&pdev->dev, "%s: This channel is already opened\n",
			__func__);
		rc = -EBUSY;
		goto unlock;
	}

	if (bg_cdc_glink->link_state != GLINK_LINK_STATE_UP) {
		rc = wait_event_timeout(bg_cdc_glink->wait,
			bg_cdc_glink->link_state == GLINK_LINK_STATE_UP,
			msecs_to_jiffies(GLINK_LINK_STATE_UP_WAIT_TIMEOUT));
		if (rc == 0) {
			dev_err(bg_cdc_glink->dev, "%s: Open timeout\n",
				__func__);
			rc = -ETIMEDOUT;
			goto unlock;
		}
		dev_dbg(bg_cdc_glink->dev, "%s: Wakeup done\n", __func__);
	}

	memset(&open_cfg, 0, sizeof(struct glink_open_config));
	open_cfg.options = GLINK_OPT_INITIAL_XPORT;
	open_cfg.edge = BG_GLINK_EDGE;
	open_cfg.name = ch_cfg->ch_name;
	open_cfg.notify_rx = bg_cdc_glink_notify_rx;
	open_cfg.notify_tx_done = bg_cdc_glink_notify_tx_done;
	open_cfg.notify_state = bg_cdc_glink_notify_state;
	open_cfg.notify_rx_intent_req = bg_cdc_glink_notify_rx_intent_req;
	open_cfg.notify_remote_rx_intent = bg_cdc_glink_notify_remote_rx_intent;
	open_cfg.notify_tx_abort = bg_cdc_glink_notify_tx_abort;
	open_cfg.rx_intent_req_timeout_ms = BG_RX_INTENT_REQ_TIMEOUT_MS;
	open_cfg.priv = ch_info;

	ch_info->channel_state = GLINK_REMOTE_DISCONNECTED;
	ch_info->handle = glink_open(&open_cfg);
	if (IS_ERR_OR_NULL(ch_info->handle)) {
		dev_err(bg_cdc_glink->dev, "%s: glink_open failed %s\n",
			__func__, ch_cfg->ch_name);
		ch_info->handle = NULL;
		rc = -EINVAL;
		goto unlock;
	}
	bg_cdc_glink->active_channel++;
	rc = wait_event_timeout(ch_info->wait,
		(ch_info->channel_state == GLINK_CONNECTED), 5 * HZ);
	if (rc == 0) {
		dev_err(bg_cdc_glink->dev, "%s: TIMEOUT for OPEN event\n",
			__func__);
		rc = -ETIMEDOUT;
		goto close_link;
	}
	rc = bg_cdc_glink_rx_intents_config(ch_info,
				ch_cfg->num_of_intents, ch_cfg->intents_size);
	if (rc) {
		dev_err(bg_cdc_glink->dev, "%s: Unable to queue intents\n",
			__func__);
		goto close_link;
	}

	ch_info->func = func;

close_link:
	if (rc) {
		if (bg_cdc_glink->active_channel > 0)
			bg_cdc_glink->active_channel--;
		glink_close(ch_info->handle);
		ch_info->handle = NULL;
	}
unlock:
	mutex_unlock(&ch_info->m_lock);

	return rc ? NULL : (void *)ch_info;
}
EXPORT_SYMBOL(bg_cdc_channel_open);


int bg_cdc_channel_close(struct platform_device *pdev,
			void *ch_info)
{
	struct bg_cdc_glink_drvdata *bg_cdc_glink;
	int rc;
	struct bg_cdc_glink_ch_info *channel_info
			= (struct bg_cdc_glink_ch_info *)ch_info;
	bg_cdc_glink = platform_get_drvdata(pdev);
	if (!channel_info || !channel_info->handle) {
		rc = -EINVAL;
		goto exit;
	}

	mutex_lock(&channel_info->m_lock);
	if (bg_cdc_glink->active_channel > 0)
		bg_cdc_glink->active_channel--;
	rc = glink_close(channel_info->handle);
	channel_info->handle = NULL;
	channel_info->func = NULL;
	channel_info->if_remote_intent_ready = false;
	mutex_unlock(&channel_info->m_lock);
exit:
	return rc;
}
EXPORT_SYMBOL(bg_cdc_channel_close);

static void bg_cdc_glink_link_state_cb(struct glink_link_state_cb_info *cb_info,
				  void *priv)
{
	struct bg_cdc_glink_drvdata *bg_cdc_glink =
			(struct bg_cdc_glink_drvdata *) priv;

	if (!cb_info) {
		pr_err("%s: Invalid cb_info\n", __func__);
		return;
	}

	dev_dbg(bg_cdc_glink->dev, "%s: edge[%s] link state[%d]\n", __func__,
		  cb_info->edge, cb_info->link_state);

	bg_cdc_glink->link_state = cb_info->link_state;
	if (bg_cdc_glink->link_state == GLINK_LINK_STATE_UP)
		wake_up(&bg_cdc_glink->wait);
}

static int bg_cdc_glink_probe(struct platform_device *pdev)
{
	struct bg_cdc_glink_drvdata *bg_cdc_glink;
	struct glink_link_info link_info;
	u32 num_channels;
	int ret = 0;
	int i;

	ret = of_property_read_u32(pdev->dev.of_node, "qcom,msm-glink-channels",
				   &num_channels);

	if (ret) {
		dev_err(&pdev->dev, "%s: glink channels from DT file %s\n",
			__func__, "qcom,msm-glink-channels");
		return -EINVAL;
	}

	/* Allocate BG codec Glink structure */
	bg_cdc_glink = kzalloc(sizeof(struct bg_cdc_glink_drvdata), GFP_KERNEL);
	if (!bg_cdc_glink)
		return -ENOMEM;

	bg_cdc_glink->ch_info = kzalloc((sizeof(struct bg_cdc_glink_ch_info) *
					num_channels), GFP_KERNEL);
	if (!bg_cdc_glink->ch_info) {
		ret = -ENOMEM;
		goto err_memory_fail;
	}
	bg_cdc_glink->dev = &pdev->dev;
	bg_cdc_glink->pdev = pdev;
	bg_cdc_glink->num_channels = num_channels;
	platform_set_drvdata(pdev, bg_cdc_glink);

	init_waitqueue_head(&bg_cdc_glink->wait);

	/* Register glink link_state notification */
	link_info.glink_link_state_notif_cb = bg_cdc_glink_link_state_cb;
	link_info.transport = NULL;
	link_info.edge = BG_GLINK_EDGE;
	bg_cdc_glink->link_state = GLINK_LINK_STATE_DOWN;
	bg_cdc_glink->handle =
		glink_register_link_state_cb(&link_info, bg_cdc_glink);
	if (!bg_cdc_glink->handle) {
		dev_err(&pdev->dev, "%s: Unable to register link state\n",
			 __func__);
		ret = -EINVAL;
		goto err_glink_register_fail;
	}

	for (i = 0; i < num_channels; i++) {
		mutex_init(&bg_cdc_glink->ch_info[i].w_lock);
		mutex_init(&bg_cdc_glink->ch_info[i].r_lock);
		mutex_init(&bg_cdc_glink->ch_info[i].m_lock);
		init_waitqueue_head(&bg_cdc_glink->ch_info[i].wait);
	}
	return ret;
err_glink_register_fail:
	kfree(bg_cdc_glink->ch_info);

err_memory_fail:
	kfree(bg_cdc_glink);

	return ret;
}

static const struct of_device_id bg_cdc_glink_of_match[] = {
	{ .compatible = "qcom,bg-cdc-glink", },
	{},
};

static int bg_cdc_glink_remove(struct platform_device *pdev)
{
	struct bg_cdc_glink_drvdata *bg_cdc_glink = platform_get_drvdata(pdev);
	int i;

	if (!bg_cdc_glink) {
		dev_err(&pdev->dev, "%s: invalid data\n",
			__func__);
		return -EINVAL;
	}
	if (bg_cdc_glink->handle)
		glink_unregister_link_state_cb(bg_cdc_glink->handle);

	for (i = 0; i < bg_cdc_glink->num_channels; i++) {
		mutex_destroy(&bg_cdc_glink->ch_info[i].w_lock);
		mutex_destroy(&bg_cdc_glink->ch_info[i].r_lock);
		mutex_destroy(&bg_cdc_glink->ch_info[i].m_lock);
	}
	kfree(bg_cdc_glink->ch_info);
	kfree(bg_cdc_glink);
	return 0;
}

static struct platform_driver msm_bg_cdc_glink_driver = {
	.driver = {
		.owner          = THIS_MODULE,
		.name           = BG_GLINK_NAME,
		.of_match_table = bg_cdc_glink_of_match,
	},
	.probe          = bg_cdc_glink_probe,
	.remove         = bg_cdc_glink_remove,
};
module_platform_driver(msm_bg_cdc_glink_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("BG Glink driver");
