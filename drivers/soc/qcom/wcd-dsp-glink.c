/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <soc/qcom/glink.h>
#include "sound/wcd-dsp-glink.h"

#define WDSP_GLINK_DRIVER_NAME "wcd-dsp-glink"
#define WDSP_MAX_WRITE_SIZE (256 * 1024)
#define WDSP_MAX_READ_SIZE (4 * 1024)
#define WDSP_MAX_NO_OF_INTENTS (20)
#define WDSP_MAX_NO_OF_CHANNELS (10)

#define MINOR_NUMBER_COUNT 1
#define WDSP_EDGE "wdsp"
#define RESP_QUEUE_SIZE 3
#define QOS_PKT_SIZE 1024
#define TIMEOUT_MS 1000

struct wdsp_glink_dev {
	struct class *cls;
	struct device *dev;
	struct cdev cdev;
	dev_t dev_num;
};

struct wdsp_glink_rsp_que {
	/* Size of valid data in buffer */
	u32 buf_size;

	/* Response buffer */
	u8 buf[WDSP_MAX_READ_SIZE];
};

struct wdsp_glink_tx_buf {
	struct work_struct tx_work;

	/* Glink channel information */
	struct wdsp_glink_ch *ch;

	/* Tx buffer to send to glink */
	u8 buf[0];
};

struct wdsp_glink_ch {
	struct wdsp_glink_priv *wpriv;

	/* Glink channel handle */
	void *handle;

	/* Channel states like connect, disconnect */
	int channel_state;
	struct mutex mutex;

	/* To free up the channel memory */
	bool free_mem;

	/* Glink local channel open work */
	struct work_struct lcl_ch_open_wrk;

	/* Glink local channel close work */
	struct work_struct lcl_ch_cls_wrk;

	/* Wait for ch connect state before sending any command */
	wait_queue_head_t ch_connect_wait;

	/*
	 * Glink channel configuration. This has to be the last
	 * member of the strucuture as it has variable size
	 */
	struct wdsp_glink_ch_cfg ch_cfg;
};

struct wdsp_glink_state {
	/* Glink link state information */
	enum glink_link_state link_state;
	void *handle;
};

struct wdsp_glink_priv {
	/* Respone buffer related */
	u8 rsp_cnt;
	struct wdsp_glink_rsp_que rsp[RESP_QUEUE_SIZE];
	struct completion rsp_complete;
	struct mutex rsp_mutex;

	/* Glink channel related */
	struct mutex glink_mutex;
	struct wdsp_glink_state glink_state;
	struct wdsp_glink_ch **ch;
	u8 no_of_channels;
	struct work_struct ch_open_cls_wrk;
	struct workqueue_struct *work_queue;

	wait_queue_head_t link_state_wait;

	struct device *dev;
};

static int wdsp_glink_close_ch(struct wdsp_glink_ch *ch);
static int wdsp_glink_open_ch(struct wdsp_glink_ch *ch);

/*
 * wdsp_glink_notify_rx - Glink notify rx callback for responses
 * handle:      Opaque Channel handle returned by GLink
 * priv:        Private pointer to the channel
 * pkt_priv:    Private pointer to the packet
 * ptr:         Pointer to the Rx data
 * size:        Size of the Rx data
 */
static void wdsp_glink_notify_rx(void *handle, const void *priv,
				 const void *pkt_priv, const void *ptr,
				 size_t size)
{
	u8 *rx_buf;
	u8 rsp_cnt;
	struct wdsp_glink_ch *ch;
	struct wdsp_glink_priv *wpriv;

	if (!ptr || !priv) {
		pr_err("%s: Invalid parameters\n", __func__);
		return;
	}

	ch = (struct wdsp_glink_ch *)priv;
	wpriv = ch->wpriv;
	rx_buf = (u8 *)ptr;
	if (size > WDSP_MAX_READ_SIZE) {
		dev_err(wpriv->dev, "%s: Size %zd is greater than allowed %d\n",
			__func__, size, WDSP_MAX_READ_SIZE);
		size = WDSP_MAX_READ_SIZE;
	}

	mutex_lock(&wpriv->rsp_mutex);
	rsp_cnt = wpriv->rsp_cnt;
	if (rsp_cnt >= RESP_QUEUE_SIZE) {
		dev_err(wpriv->dev, "%s: Resp Queue is Full\n", __func__);
		rsp_cnt = 0;
	}
	dev_dbg(wpriv->dev, "%s: copy into buffer %d\n", __func__, rsp_cnt);

	memcpy(wpriv->rsp[rsp_cnt].buf, rx_buf, size);
	wpriv->rsp[rsp_cnt].buf_size = size;
	wpriv->rsp_cnt = ++rsp_cnt;
	mutex_unlock(&wpriv->rsp_mutex);

	glink_rx_done(handle, ptr, true);
	complete(&wpriv->rsp_complete);
}

/*
 * wdsp_glink_notify_tx_done - Glink notify tx done callback to
 * free tx buffer
 * handle:      Opaque Channel handle returned by GLink
 * priv:        Private pointer to the channel
 * pkt_priv:    Private pointer to the packet
 * ptr:         Pointer to the Tx data
 */
static void wdsp_glink_notify_tx_done(void *handle, const void *priv,
				      const void *pkt_priv, const void *ptr)
{
	if (!pkt_priv) {
		pr_err("%s: Invalid parameter\n", __func__);
		return;
	}
	/* Free tx pkt */
	kfree(pkt_priv);
}

/*
 * wdsp_glink_notify_tx_abort - Glink notify tx abort callback to
 * free tx buffer
 * handle:      Opaque Channel handle returned by GLink
 * priv:        Private pointer to the channel
 * pkt_priv:    Private pointer to the packet
 */
static void wdsp_glink_notify_tx_abort(void *handle, const void *priv,
				       const void *pkt_priv)
{
	if (!pkt_priv) {
		pr_err("%s: Invalid parameter\n", __func__);
		return;
	}
	/* Free tx pkt */
	kfree(pkt_priv);
}

/*
 * wdsp_glink_notify_rx_intent_req - Glink notify rx intent request callback
 * to queue buffer to receive from remote client
 * handle:      Opaque channel handle returned by GLink
 * priv:        Private pointer to the channel
 * req_size:    Size of intent to be queued
 */
static bool wdsp_glink_notify_rx_intent_req(void *handle, const void *priv,
					    size_t req_size)
{
	struct wdsp_glink_priv *wpriv;
	struct wdsp_glink_ch *ch;
	int rc = 0;
	bool ret = false;

	if (!priv) {
		pr_err("%s: Invalid priv\n", __func__);
		goto done;
	}
	if (req_size > WDSP_MAX_READ_SIZE) {
		pr_err("%s: Invalid req_size %zd\n", __func__, req_size);
		goto done;
	}

	ch = (struct wdsp_glink_ch *)priv;
	wpriv = ch->wpriv;

	dev_dbg(wpriv->dev, "%s: intent size %zd requested for ch name %s",
		 __func__, req_size, ch->ch_cfg.name);

	mutex_lock(&ch->mutex);
	rc = glink_queue_rx_intent(ch->handle, ch, req_size);
	if (IS_ERR_VALUE(rc)) {
		dev_err(wpriv->dev, "%s: Failed to queue rx intent, rc = %d\n",
			__func__, rc);
		mutex_unlock(&ch->mutex);
		goto done;
	}
	mutex_unlock(&ch->mutex);
	ret = true;

done:
	return ret;
}

/*
 * wdsp_glink_lcl_ch_open_wrk - Work function to open channel again
 * when local disconnect event happens
 * work:      Work structure
 */
static void wdsp_glink_lcl_ch_open_wrk(struct work_struct *work)
{
	struct wdsp_glink_ch *ch;

	ch = container_of(work, struct wdsp_glink_ch,
			  lcl_ch_open_wrk);

	wdsp_glink_open_ch(ch);
}

/*
 * wdsp_glink_lcl_ch_cls_wrk - Work function to close channel locally
 * when remote disconnect event happens
 * work:      Work structure
 */
static void wdsp_glink_lcl_ch_cls_wrk(struct work_struct *work)
{
	struct wdsp_glink_ch *ch;

	ch = container_of(work, struct wdsp_glink_ch,
			  lcl_ch_cls_wrk);

	wdsp_glink_close_ch(ch);
}

/*
 * wdsp_glink_notify_state - Glink channel state information event callback
 * handle:      Opaque Channel handle returned by GLink
 * priv:        Private pointer to the channel
 * event:       channel state event
 */
static void wdsp_glink_notify_state(void *handle, const void *priv,
				    unsigned event)
{
	struct wdsp_glink_priv *wpriv;
	struct wdsp_glink_ch *ch;
	int i, ret = 0;

	if (!priv) {
		pr_err("%s: Invalid priv\n", __func__);
		return;
	}

	ch = (struct wdsp_glink_ch *)priv;
	wpriv = ch->wpriv;

	mutex_lock(&ch->mutex);
	ch->channel_state = event;
	if (event == GLINK_CONNECTED) {
		dev_dbg(wpriv->dev, "%s: glink channel: %s connected\n",
			__func__, ch->ch_cfg.name);

		for (i = 0; i < ch->ch_cfg.no_of_intents; i++) {
			dev_dbg(wpriv->dev, "%s: intent_size = %d\n", __func__,
				ch->ch_cfg.intents_size[i]);
			ret = glink_queue_rx_intent(ch->handle, ch,
						    ch->ch_cfg.intents_size[i]);
			if (IS_ERR_VALUE(ret))
				dev_warn(wpriv->dev, "%s: Failed to queue intent %d of size %d\n",
					 __func__, i,
					 ch->ch_cfg.intents_size[i]);
		}

		ret = glink_qos_latency(ch->handle, ch->ch_cfg.latency_in_us,
					QOS_PKT_SIZE);
		if (IS_ERR_VALUE(ret))
			dev_warn(wpriv->dev, "%s: Failed to request qos %d for ch %s\n",
				__func__, ch->ch_cfg.latency_in_us,
				ch->ch_cfg.name);

		wake_up(&ch->ch_connect_wait);
		mutex_unlock(&ch->mutex);
	} else if (event == GLINK_LOCAL_DISCONNECTED) {
		/*
		 * Don't use dev_dbg here as dev may not be valid if channel
		 * closed from driver close.
		 */
		pr_debug("%s: channel: %s disconnected locally\n",
			 __func__, ch->ch_cfg.name);
		mutex_unlock(&ch->mutex);

		if (ch->free_mem) {
			kfree(ch);
			ch = NULL;
		}
	} else if (event == GLINK_REMOTE_DISCONNECTED) {
		dev_dbg(wpriv->dev, "%s: remote channel: %s disconnected remotely\n",
			 __func__, ch->ch_cfg.name);
		mutex_unlock(&ch->mutex);
		/*
		 * If remote disconnect happens, local side also has
		 * to close the channel as per glink design in a
		 * separate work_queue.
		 */
		queue_work(wpriv->work_queue, &ch->lcl_ch_cls_wrk);
	}
}

/*
 * wdsp_glink_close_ch - Internal function to close glink channel
 * ch:       Glink Channel structure.
 */
static int wdsp_glink_close_ch(struct wdsp_glink_ch *ch)
{
	struct wdsp_glink_priv *wpriv = ch->wpriv;
	int ret = 0;

	mutex_lock(&wpriv->glink_mutex);
	if (ch->handle) {
		ret = glink_close(ch->handle);
		if (IS_ERR_VALUE(ret)) {
			dev_err(wpriv->dev, "%s: glink_close is failed, ret = %d\n",
				 __func__, ret);
		} else {
			ch->handle = NULL;
			dev_dbg(wpriv->dev, "%s: ch %s is closed\n", __func__,
				ch->ch_cfg.name);
		}
	} else {
		dev_dbg(wpriv->dev, "%s: ch %s is already closed\n", __func__,
			ch->ch_cfg.name);
	}
	mutex_unlock(&wpriv->glink_mutex);


	return ret;
}

/*
 * wdsp_glink_open_ch - Internal function to open glink channel
 * ch:       Glink Channel structure.
 */
static int wdsp_glink_open_ch(struct wdsp_glink_ch *ch)
{
	struct wdsp_glink_priv *wpriv = ch->wpriv;
	struct glink_open_config open_cfg;
	int ret = 0;

	mutex_lock(&wpriv->glink_mutex);
	if (!ch->handle) {
		memset(&open_cfg, 0, sizeof(open_cfg));
		open_cfg.options = GLINK_OPT_INITIAL_XPORT;
		open_cfg.edge = WDSP_EDGE;
		open_cfg.notify_rx = wdsp_glink_notify_rx;
		open_cfg.notify_tx_done = wdsp_glink_notify_tx_done;
		open_cfg.notify_tx_abort = wdsp_glink_notify_tx_abort;
		open_cfg.notify_state = wdsp_glink_notify_state;
		open_cfg.notify_rx_intent_req = wdsp_glink_notify_rx_intent_req;
		open_cfg.priv = ch;
		open_cfg.name = ch->ch_cfg.name;

		dev_dbg(wpriv->dev, "%s: ch->ch_cfg.name = %s, latency_in_us = %d, intents = %d\n",
			__func__, ch->ch_cfg.name, ch->ch_cfg.latency_in_us,
			ch->ch_cfg.no_of_intents);

		ch->handle = glink_open(&open_cfg);
		if (IS_ERR_OR_NULL(ch->handle)) {
			dev_err(wpriv->dev, "%s: glink_open failed for ch %s\n",
				__func__, ch->ch_cfg.name);
			ch->handle = NULL;
			ret = -EINVAL;
		}
	} else {
		dev_err(wpriv->dev, "%s: ch %s is already opened\n", __func__,
			ch->ch_cfg.name);
	}
	mutex_unlock(&wpriv->glink_mutex);

	return ret;
}

/*
 * wdsp_glink_close_all_ch - Internal function to close all glink channels
 * wpriv:       Wdsp_glink private structure
 */
static void wdsp_glink_close_all_ch(struct wdsp_glink_priv *wpriv)
{
	int i;

	for (i = 0; i < wpriv->no_of_channels; i++)
		if (wpriv->ch && wpriv->ch[i])
			wdsp_glink_close_ch(wpriv->ch[i]);
}

/*
 * wdsp_glink_open_all_ch - Internal function to open all glink channels
 * wpriv:       Wdsp_glink private structure
 */
static int wdsp_glink_open_all_ch(struct wdsp_glink_priv *wpriv)
{
	int ret = 0, i, j;

	for (i = 0; i < wpriv->no_of_channels; i++) {
		if (wpriv->ch && wpriv->ch[i]) {
			ret = wdsp_glink_open_ch(wpriv->ch[i]);
			if (IS_ERR_VALUE(ret))
				goto err_open;
		}
	}
	goto done;

err_open:
	for (j = 0; j < i; j++)
		if (wpriv->ch[i])
			wdsp_glink_close_ch(wpriv->ch[j]);

done:
	return ret;
}

/*
 * wdsp_glink_ch_open_wq - Work function to open glink channels
 * work:      Work structure
 */
static void wdsp_glink_ch_open_cls_wrk(struct work_struct *work)
{
	struct wdsp_glink_priv *wpriv;

	wpriv = container_of(work, struct wdsp_glink_priv,
			     ch_open_cls_wrk);

	if (wpriv->glink_state.link_state == GLINK_LINK_STATE_DOWN) {
		dev_info(wpriv->dev, "%s: GLINK_LINK_STATE_DOWN\n",
			 __func__);

		wdsp_glink_close_all_ch(wpriv);
	} else if (wpriv->glink_state.link_state == GLINK_LINK_STATE_UP) {
		dev_info(wpriv->dev, "%s: GLINK_LINK_STATE_UP\n",
			 __func__);

		wdsp_glink_open_all_ch(wpriv);
	}
}

/*
 * wdsp_glink_link_state_cb - Glink link state callback to inform
 * about link states
 * cb_info:     Glink link state callback information structure
 * priv:        Private structure of link state passed while register
 */
static void wdsp_glink_link_state_cb(struct glink_link_state_cb_info *cb_info,
				     void *priv)
{
	struct wdsp_glink_priv *wpriv;

	if (!cb_info || !priv) {
		pr_err("%s: Invalid parameters\n", __func__);
		return;
	}

	wpriv = (struct wdsp_glink_priv *)priv;

	mutex_lock(&wpriv->glink_mutex);
	wpriv->glink_state.link_state = cb_info->link_state;
	wake_up(&wpriv->link_state_wait);
	mutex_unlock(&wpriv->glink_mutex);

	queue_work(wpriv->work_queue, &wpriv->ch_open_cls_wrk);
}

/*
 * wdsp_glink_ch_info_init- Internal function to allocate channel memory
 * and register with glink
 * wpriv:     Wdsp_glink private structure.
 * pkt:       Glink registration packet contains glink channel information.
 */
static int wdsp_glink_ch_info_init(struct wdsp_glink_priv *wpriv,
				   struct wdsp_reg_pkt *pkt)
{
	int ret = 0, i, j;
	struct glink_link_info link_info;
	struct wdsp_glink_ch_cfg *ch_cfg;
	struct wdsp_glink_ch **ch;
	u8 no_of_channels;
	u8 *payload;
	u32 ch_size, ch_cfg_size;

	payload = (u8 *)pkt->payload;
	no_of_channels = pkt->no_of_channels;

	if (no_of_channels > WDSP_MAX_NO_OF_CHANNELS) {
		dev_info(wpriv->dev, "%s: no_of_channels = %d are limited to %d\n",
			 __func__, no_of_channels, WDSP_MAX_NO_OF_CHANNELS);
		no_of_channels = WDSP_MAX_NO_OF_CHANNELS;
	}
	ch = kcalloc(no_of_channels, sizeof(struct wdsp_glink_ch *),
		     GFP_KERNEL);
	if (!ch) {
		ret = -ENOMEM;
		goto done;
	}
	wpriv->ch = ch;
	wpriv->no_of_channels = no_of_channels;

	for (i = 0; i < no_of_channels; i++) {
		ch_cfg = (struct wdsp_glink_ch_cfg *)payload;

		if (ch_cfg->no_of_intents > WDSP_MAX_NO_OF_INTENTS) {
			dev_err(wpriv->dev, "%s: Invalid no_of_intents = %d\n",
				__func__, ch_cfg->no_of_intents);
			ret = -EINVAL;
			goto err_ch_mem;
		}

		ch_cfg_size = sizeof(struct wdsp_glink_ch_cfg) +
					(sizeof(u32) * ch_cfg->no_of_intents);
		ch_size = sizeof(struct wdsp_glink_ch) +
					(sizeof(u32) * ch_cfg->no_of_intents);

		dev_dbg(wpriv->dev, "%s: channels = %d, ch_cfg_size %d",
			 __func__, no_of_channels, ch_cfg_size);

		ch[i] = kzalloc(ch_size, GFP_KERNEL);
		if (!ch[i]) {
			ret = -ENOMEM;
			goto err_ch_mem;
		}
		ch[i]->channel_state = GLINK_LOCAL_DISCONNECTED;
		memcpy(&ch[i]->ch_cfg, payload, ch_cfg_size);
		payload += ch_cfg_size;

		mutex_init(&ch[i]->mutex);
		ch[i]->wpriv = wpriv;
		INIT_WORK(&ch[i]->lcl_ch_open_wrk, wdsp_glink_lcl_ch_open_wrk);
		INIT_WORK(&ch[i]->lcl_ch_cls_wrk, wdsp_glink_lcl_ch_cls_wrk);
		init_waitqueue_head(&ch[i]->ch_connect_wait);
	}

	INIT_WORK(&wpriv->ch_open_cls_wrk, wdsp_glink_ch_open_cls_wrk);

	/* Register glink link_state notification */
	link_info.glink_link_state_notif_cb = wdsp_glink_link_state_cb;
	link_info.transport = NULL;
	link_info.edge = WDSP_EDGE;

	wpriv->glink_state.link_state = GLINK_LINK_STATE_DOWN;
	wpriv->glink_state.handle = glink_register_link_state_cb(&link_info,
								 wpriv);
	if (!wpriv->glink_state.handle) {
		dev_err(wpriv->dev, "%s: Unable to register wdsp link state\n",
			__func__);
		ret = -EINVAL;
		goto err_ch_mem;
	}
	goto done;

err_ch_mem:
	for (j = 0; j < i; j++) {
		mutex_destroy(&ch[j]->mutex);
		kfree(wpriv->ch[j]);
		wpriv->ch[j] = NULL;
	}
	kfree(wpriv->ch);
	wpriv->ch = NULL;
	wpriv->no_of_channels = 0;

done:
	return ret;
}

/*
 * wdsp_glink_tx_buf_work - Work queue function to send tx buffer to glink
 * work:     Work structure
 */
static void wdsp_glink_tx_buf_work(struct work_struct *work)
{
	struct wdsp_glink_priv *wpriv;
	struct wdsp_glink_ch *ch;
	struct wdsp_glink_tx_buf *tx_buf;
	struct wdsp_write_pkt *wpkt;
	struct wdsp_cmd_pkt *cpkt;
	int ret = 0;

	tx_buf = container_of(work, struct wdsp_glink_tx_buf,
			      tx_work);
	ch = tx_buf->ch;
	wpriv = ch->wpriv;
	wpkt = (struct wdsp_write_pkt *)tx_buf->buf;
	cpkt = (struct wdsp_cmd_pkt *)wpkt->payload;
	dev_dbg(wpriv->dev, "%s: ch name = %s, payload size = %d\n",
		__func__, cpkt->ch_name, cpkt->payload_size);

	mutex_lock(&tx_buf->ch->mutex);
	if (ch->channel_state == GLINK_CONNECTED) {
		mutex_unlock(&tx_buf->ch->mutex);
		ret = glink_tx(ch->handle, tx_buf,
			       cpkt->payload, cpkt->payload_size,
			       GLINK_TX_REQ_INTENT);
		if (IS_ERR_VALUE(ret)) {
			dev_err(wpriv->dev, "%s: glink tx failed, ret = %d\n",
				__func__, ret);
			/*
			 * If glink_tx() is failed then free tx_buf here as
			 * there won't be any tx_done notification to
			 * free the buffer.
			 */
			kfree(tx_buf);
		}
	} else {
		mutex_unlock(&tx_buf->ch->mutex);
		dev_err(wpriv->dev, "%s: channel %s is not in connected state\n",
			__func__, ch->ch_cfg.name);
		/*
		 * Free tx_buf here as there won't be any tx_done
		 * notification in this case also.
		 */
		kfree(tx_buf);
	}
}

/*
 * wdsp_glink_read - Read API to send the data to userspace
 * file:    Pointer to the file structure
 * buf:     Pointer to the userspace buffer
 * count:   Number bytes to read from the file
 * ppos:    Pointer to the position into the file
 */
static ssize_t wdsp_glink_read(struct file *file, char __user *buf,
			       size_t count, loff_t *ppos)
{
	int ret = 0, ret1 = 0;
	struct wdsp_glink_rsp_que *rsp;
	struct wdsp_glink_priv *wpriv;

	wpriv = (struct wdsp_glink_priv *)file->private_data;
	if (!wpriv) {
		pr_err("%s: Invalid private data\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	if (count > WDSP_MAX_READ_SIZE) {
		dev_info(wpriv->dev, "%s: count = %zd is more than WDSP_MAX_READ_SIZE\n",
			__func__, count);
		count = WDSP_MAX_READ_SIZE;
	}
	/*
	 * Complete signal has given from glink rx notification callback
	 * or from flush API. Also use interruptible wait_for_completion API
	 * to allow the system to go in suspend.
	 */
	ret = wait_for_completion_interruptible(&wpriv->rsp_complete);
	if (ret)
		goto done;

	mutex_lock(&wpriv->rsp_mutex);
	if (wpriv->rsp_cnt) {
		wpriv->rsp_cnt--;
		dev_dbg(wpriv->dev, "%s: read from buffer %d\n",
			__func__, wpriv->rsp_cnt);

		rsp = &wpriv->rsp[wpriv->rsp_cnt];
		if (count < rsp->buf_size) {
			ret1 = copy_to_user(buf, &rsp->buf, count);
			/* Return the number of bytes copied */
			ret = count;
		} else {
			ret1 = copy_to_user(buf, &rsp->buf, rsp->buf_size);
			/* Return the number of bytes copied */
			ret = rsp->buf_size;
		}

		if (ret1) {
			mutex_unlock(&wpriv->rsp_mutex);
			dev_err(wpriv->dev, "%s: copy_to_user failed %d\n",
				__func__, ret);
			ret = -EFAULT;
			goto done;
		}
	} else {
		/*
		 * This will execute only if flush API is called or
		 * something wrong with ref_cnt
		 */
		dev_dbg(wpriv->dev, "%s: resp count = %d\n", __func__,
			wpriv->rsp_cnt);
		ret = -EINVAL;
	}
	mutex_unlock(&wpriv->rsp_mutex);

done:
	return ret;
}

/*
 * wdsp_glink_write - Write API to receive the data from userspace
 * file:    Pointer to the file structure
 * buf:     Pointer to the userspace buffer
 * count:   Number bytes to read from the file
 * ppos:    Pointer to the position into the file
 */
static ssize_t wdsp_glink_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	int ret = 0, i, tx_buf_size;
	struct wdsp_write_pkt *wpkt;
	struct wdsp_cmd_pkt *cpkt;
	struct wdsp_glink_tx_buf *tx_buf;
	struct wdsp_glink_priv *wpriv;

	wpriv = (struct wdsp_glink_priv *)file->private_data;
	if (!wpriv) {
		pr_err("%s: Invalid private data\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	if ((count < sizeof(struct wdsp_write_pkt)) ||
	    (count > WDSP_MAX_WRITE_SIZE)) {
		dev_err(wpriv->dev, "%s: Invalid count = %zd\n",
			__func__, count);
		ret = -EINVAL;
		goto done;
	}

	dev_dbg(wpriv->dev, "%s: count = %zd\n", __func__, count);

	tx_buf_size = WDSP_MAX_WRITE_SIZE + sizeof(struct wdsp_glink_tx_buf);
	tx_buf = kzalloc(tx_buf_size, GFP_KERNEL);
	if (!tx_buf) {
		ret = -ENOMEM;
		goto done;
	}

	ret = copy_from_user(tx_buf->buf, buf, count);
	if (ret) {
		dev_err(wpriv->dev, "%s: copy_from_user failed %d\n",
			__func__, ret);
		ret = -EFAULT;
		goto free_buf;
	}

	wpkt = (struct wdsp_write_pkt *)tx_buf->buf;
	switch (wpkt->pkt_type) {
	case WDSP_REG_PKT:
		if (count <= (sizeof(struct wdsp_write_pkt) +
			      sizeof(struct wdsp_reg_pkt))) {
			dev_err(wpriv->dev, "%s: Invalid reg pkt size = %zd\n",
				__func__, count);
			ret = -EINVAL;
			goto free_buf;
		}
		ret = wdsp_glink_ch_info_init(wpriv,
					(struct wdsp_reg_pkt *)wpkt->payload);
		if (IS_ERR_VALUE(ret))
			dev_err(wpriv->dev, "%s: glink register failed, ret = %d\n",
				__func__, ret);
		kfree(tx_buf);
		break;
	case WDSP_READY_PKT:
		ret = wait_event_timeout(wpriv->link_state_wait,
					 (wpriv->glink_state.link_state ==
							GLINK_LINK_STATE_UP),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			dev_err(wpriv->dev, "%s: Link state wait timeout\n",
				__func__);
			ret = -ETIMEDOUT;
			goto free_buf;
		}
		ret = 0;
		kfree(tx_buf);
		break;
	case WDSP_CMD_PKT:
		if (count <= (sizeof(struct wdsp_write_pkt) +
			      sizeof(struct wdsp_cmd_pkt))) {
			dev_err(wpriv->dev, "%s: Invalid cmd pkt size = %zd\n",
				__func__, count);
			ret = -EINVAL;
			goto free_buf;
		}
		mutex_lock(&wpriv->glink_mutex);
		if (wpriv->glink_state.link_state == GLINK_LINK_STATE_DOWN) {
			mutex_unlock(&wpriv->glink_mutex);
			dev_err(wpriv->dev, "%s: Link state is Down\n",
				__func__);

			ret = -ENETRESET;
			goto free_buf;
		}
		mutex_unlock(&wpriv->glink_mutex);

		cpkt = (struct wdsp_cmd_pkt *)wpkt->payload;
		dev_dbg(wpriv->dev, "%s: requested ch_name: %s\n", __func__,
			 cpkt->ch_name);
		for (i = 0; i < wpriv->no_of_channels; i++) {
			if (wpriv->ch && wpriv->ch[i] &&
				(!strcmp(cpkt->ch_name,
						wpriv->ch[i]->ch_cfg.name))) {
				tx_buf->ch = wpriv->ch[i];
				break;
			}
		}
		if (!tx_buf->ch) {
			dev_err(wpriv->dev, "%s: Failed to get glink channel\n",
				__func__);
			ret = -EINVAL;
			goto free_buf;
		}

		ret = wait_event_timeout(tx_buf->ch->ch_connect_wait,
					 (tx_buf->ch->channel_state ==
							GLINK_CONNECTED),
					 msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			dev_err(wpriv->dev, "%s: glink channel %s is not in connected state %d\n",
				__func__, tx_buf->ch->ch_cfg.name,
				tx_buf->ch->channel_state);
			ret = -ETIMEDOUT;
			goto free_buf;
		}
		ret = 0;

		INIT_WORK(&tx_buf->tx_work, wdsp_glink_tx_buf_work);
		queue_work(wpriv->work_queue, &tx_buf->tx_work);
		break;
	default:
		dev_err(wpriv->dev, "%s: Invalid packet type\n", __func__);
		ret = -EINVAL;
		kfree(tx_buf);
		break;
	}
	goto done;

free_buf:
	kfree(tx_buf);

done:
	return ret;
}

/*
 * wdsp_glink_open - Open API to initialize private data
 * inode:   Pointer to the inode structure
 * file:    Pointer to the file structure
 */
static int wdsp_glink_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct wdsp_glink_priv *wpriv;
	struct wdsp_glink_dev *wdev;

	if (!inode->i_cdev) {
		pr_err("%s: cdev is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	wdev = container_of(inode->i_cdev, struct wdsp_glink_dev, cdev);

	wpriv = kzalloc(sizeof(struct wdsp_glink_priv), GFP_KERNEL);
	if (!wpriv) {
		ret = -ENOMEM;
		goto done;
	}
	wpriv->dev = wdev->dev;
	wpriv->work_queue = create_singlethread_workqueue("wdsp_glink_wq");
	if (!wpriv->work_queue) {
		dev_err(wpriv->dev, "%s: Error creating wdsp_glink_wq\n",
			__func__);
		ret = -EINVAL;
		goto err_wq;
	}

	init_completion(&wpriv->rsp_complete);
	init_waitqueue_head(&wpriv->link_state_wait);
	mutex_init(&wpriv->rsp_mutex);
	mutex_init(&wpriv->glink_mutex);
	file->private_data = wpriv;

	goto done;

err_wq:
	kfree(wpriv);

done:
	return ret;
}

/*
 * wdsp_glink_flush - Flush API to unblock read.
 * file:    Pointer to the file structure
 * id:      Lock owner ID
 */
static int wdsp_glink_flush(struct file *file, fl_owner_t id)
{
	struct wdsp_glink_priv *wpriv;

	wpriv = (struct wdsp_glink_priv *)file->private_data;
	if (!wpriv) {
		pr_err("%s: Invalid private data\n", __func__);
		return -EINVAL;
	}

	complete(&wpriv->rsp_complete);

	return 0;
}

/*
 * wdsp_glink_release - Release API to clean up resources.
 * Whenever a file structure is shared across multiple threads,
 * release won't be invoked until all copies are closed
 * (file->f_count.counter should be 0). If we need to flush pending
 * data when any copy is closed, you should implement the flush method.
 *
 * inode:   Pointer to the inode structure
 * file:    Pointer to the file structure
 */
static int wdsp_glink_release(struct inode *inode, struct file *file)
{
	int i, ret = 0;
	struct wdsp_glink_priv *wpriv;

	wpriv = (struct wdsp_glink_priv *)file->private_data;
	if (!wpriv) {
		pr_err("%s: Invalid private data\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	if (wpriv->glink_state.handle)
		glink_unregister_link_state_cb(wpriv->glink_state.handle);

	flush_workqueue(wpriv->work_queue);
	destroy_workqueue(wpriv->work_queue);

	/*
	 * Clean up glink channel memory in channel state
	 * callback only if close channels are called from here.
	 */
	if (wpriv->ch) {
		for (i = 0; i < wpriv->no_of_channels; i++) {
			if (wpriv->ch[i]) {
				wpriv->ch[i]->free_mem = true;
				/*
				 * Channel handle NULL means channel is already
				 * closed. Free the channel memory here itself.
				 */
				if (!wpriv->ch[i]->handle) {
					kfree(wpriv->ch[i]);
					wpriv->ch[i] = NULL;
				} else {
					wdsp_glink_close_ch(wpriv->ch[i]);
				}
			}
		}

		kfree(wpriv->ch);
		wpriv->ch = NULL;
	}

	mutex_destroy(&wpriv->glink_mutex);
	mutex_destroy(&wpriv->rsp_mutex);
	kfree(wpriv);
	file->private_data = NULL;

done:
	return ret;
}

static const struct file_operations wdsp_glink_fops = {
	.owner =                THIS_MODULE,
	.open =                 wdsp_glink_open,
	.read =                 wdsp_glink_read,
	.write =                wdsp_glink_write,
	.flush =                wdsp_glink_flush,
	.release =              wdsp_glink_release,
};

/*
 * wdsp_glink_probe - Driver probe to expose char device
 * pdev:    Pointer to device tree data.
 */
static int wdsp_glink_probe(struct platform_device *pdev)
{
	int ret;
	struct wdsp_glink_dev *wdev;

	wdev = devm_kzalloc(&pdev->dev, sizeof(*wdev), GFP_KERNEL);
	if (!wdev) {
		ret = -ENOMEM;
		goto done;
	}

	ret = alloc_chrdev_region(&wdev->dev_num, 0, MINOR_NUMBER_COUNT,
				  WDSP_GLINK_DRIVER_NAME);
	if (IS_ERR_VALUE(ret)) {
		dev_err(&pdev->dev, "%s: Failed to alloc char dev, err = %d\n",
			__func__, ret);
		goto err_chrdev;
	}

	wdev->cls = class_create(THIS_MODULE, WDSP_GLINK_DRIVER_NAME);
	if (IS_ERR(wdev->cls)) {
		ret = PTR_ERR(wdev->cls);
		dev_err(&pdev->dev, "%s: Failed to create class, err = %d\n",
			__func__, ret);
		goto err_class;
	}

	wdev->dev = device_create(wdev->cls, NULL, wdev->dev_num,
				  NULL, WDSP_GLINK_DRIVER_NAME);
	if (IS_ERR(wdev->dev)) {
		ret = PTR_ERR(wdev->dev);
		dev_err(&pdev->dev, "%s: Failed to create device, err = %d\n",
			__func__, ret);
		goto err_dev_create;
	}

	cdev_init(&wdev->cdev, &wdsp_glink_fops);
	ret = cdev_add(&wdev->cdev, wdev->dev_num, MINOR_NUMBER_COUNT);
	if (IS_ERR_VALUE(ret)) {
		dev_err(&pdev->dev, "%s: Failed to register char dev, err = %d\n",
			__func__, ret);
		goto err_cdev_add;
	}
	platform_set_drvdata(pdev, wdev);
	goto done;

err_cdev_add:
	device_destroy(wdev->cls, wdev->dev_num);

err_dev_create:
	class_destroy(wdev->cls);

err_class:
	unregister_chrdev_region(0, MINOR_NUMBER_COUNT);

err_chrdev:
	devm_kfree(&pdev->dev, wdev);

done:
	return ret;
}

/*
 * wdsp_glink_remove - Driver remove to handle cleanup
 * pdev:     Pointer to device tree data.
 */
static int wdsp_glink_remove(struct platform_device *pdev)
{
	struct wdsp_glink_dev *wdev = platform_get_drvdata(pdev);

	if (wdev) {
		cdev_del(&wdev->cdev);
		device_destroy(wdev->cls, wdev->dev_num);
		class_destroy(wdev->cls);
		unregister_chrdev_region(0, MINOR_NUMBER_COUNT);
		devm_kfree(&pdev->dev, wdev);
	} else {
		dev_err(&pdev->dev, "%s: Invalid device data\n", __func__);
	}

	return 0;
}

static const struct of_device_id wdsp_glink_of_match[] = {
	{.compatible = "qcom,wcd-dsp-glink"},
	{ }
};
MODULE_DEVICE_TABLE(of, wdsp_glink_of_match);

static struct platform_driver wdsp_glink_driver = {
	.probe          = wdsp_glink_probe,
	.remove         = wdsp_glink_remove,
	.driver         = {
		.name   = WDSP_GLINK_DRIVER_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = wdsp_glink_of_match,
	},
};

module_platform_driver(wdsp_glink_driver);

MODULE_DESCRIPTION("SoC WCD_DSP GLINK Driver");
MODULE_LICENSE("GPL v2");
