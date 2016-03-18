/* Copyright (c) 2016 The Linux Foundation.
 * All rights reserved.
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
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <soc/qcom/smd.h>
#include <soc/qcom/glink.h>
#include <linux/qdsp6v2/apr_tal.h>

#define APR_MAXIMUM_NUM_OF_RETRIES 2

struct apr_tx_buf {
	struct list_head list;
	char buf[APR_MAX_BUF];
};

struct apr_buf_list {
	struct list_head list;
	spinlock_t lock;
};

struct link_state {
	uint32_t dest;
	void *handle;
	enum glink_link_state link_state;
	wait_queue_head_t wait;
};

static struct link_state link_state[APR_DEST_MAX];
static struct apr_buf_list buf_list;

static char *svc_names[APR_DEST_MAX][APR_CLIENT_MAX] = {
	{
		"apr_audio_svc",
		"apr_voice_svc",
	},
	{
		"apr_audio_svc",
		"apr_voice_svc",
	},
};

static struct apr_svc_ch_dev
	apr_svc_ch[APR_DL_MAX][APR_DEST_MAX][APR_CLIENT_MAX];

static int apr_get_free_buf(int len, void **buf)
{
	struct apr_tx_buf *tx_buf;
	unsigned long flags;

	if (!buf || len > APR_MAX_BUF) {
		pr_err("%s: buf too large [%d]\n", __func__, len);
		return -EINVAL;
	}

	spin_lock_irqsave(&buf_list.lock, flags);
	if (list_empty(&buf_list.list)) {
		spin_unlock_irqrestore(&buf_list.lock, flags);
		pr_err("%s: No buf available\n", __func__);
		return -ENOMEM;
	}

	tx_buf = list_first_entry(&buf_list.list, struct apr_tx_buf, list);
	list_del(&tx_buf->list);
	spin_unlock_irqrestore(&buf_list.lock, flags);

	*buf = tx_buf->buf;
	return 0;
}

static void apr_buf_add_tail(const void *buf)
{
	struct apr_tx_buf *list;
	unsigned long flags;

	if (!buf)
		return;

	spin_lock_irqsave(&buf_list.lock, flags);
	list = container_of((void *)buf, struct apr_tx_buf, buf);
	list_add_tail(&list->list, &buf_list.list);
	spin_unlock_irqrestore(&buf_list.lock, flags);
}

static int __apr_tal_write(struct apr_svc_ch_dev *apr_ch, void *data,
			   struct apr_pkt_priv *pkt_priv, int len)
{
	int rc = 0;
	unsigned long flags;

	spin_lock_irqsave(&apr_ch->w_lock, flags);
	rc = glink_tx(apr_ch->handle, pkt_priv, data, len,
			GLINK_TX_REQ_INTENT | GLINK_TX_ATOMIC);
	spin_unlock_irqrestore(&apr_ch->w_lock, flags);

	if (rc)
		pr_err("%s: glink_tx failed, rc[%d]\n", __func__, rc);
	else
		rc = len;

	return rc;
}

int apr_tal_write(struct apr_svc_ch_dev *apr_ch, void *data,
		  struct apr_pkt_priv *pkt_priv, int len)
{
	int rc = 0, retries = 0;
	void *pkt_data = NULL;

	if (!apr_ch->handle || !pkt_priv)
		return -EINVAL;

	if (pkt_priv->pkt_owner == APR_PKT_OWNER_DRIVER) {
		rc = apr_get_free_buf(len, &pkt_data);
		if (rc)
			goto exit;

		memcpy(pkt_data, data, len);
	} else {
		pkt_data = data;
	}

	do {
		if (rc == -EAGAIN)
			udelay(50);

		rc = __apr_tal_write(apr_ch, pkt_data, pkt_priv, len);
	} while (rc == -EAGAIN && retries++ < APR_MAXIMUM_NUM_OF_RETRIES);

	if (rc < 0) {
		pr_err("%s: Unable to send the packet, rc:%d\n", __func__, rc);
		if (pkt_priv->pkt_owner == APR_PKT_OWNER_DRIVER)
			apr_buf_add_tail(pkt_data);
	}
exit:
	return rc;
}

void apr_tal_notify_rx(void *handle, const void *priv, const void *pkt_priv,
		       const void *ptr, size_t size)
{
	struct apr_svc_ch_dev *apr_ch = (struct apr_svc_ch_dev *)priv;
	unsigned long flags;

	if (!apr_ch || !ptr) {
		pr_err("%s: Invalid apr_ch or ptr\n", __func__);
		return;
	}

	pr_debug("%s: Rx packet received\n", __func__);

	spin_lock_irqsave(&apr_ch->r_lock, flags);
	if (apr_ch->func)
		apr_ch->func((void *)ptr, size, (void *)pkt_priv);
	spin_unlock_irqrestore(&apr_ch->r_lock, flags);
	glink_rx_done(apr_ch->handle, ptr, true);
}

void apr_tal_notify_tx_done(void *handle, const void *priv,
			    const void *pkt_priv, const void *ptr)
{
	struct apr_pkt_priv *apr_pkt_priv = (struct apr_pkt_priv *)pkt_priv;

	if (!pkt_priv || !ptr) {
		pr_err("%s: Invalid pkt_priv or ptr\n", __func__);
		return;
	}

	pr_debug("%s: tx_done received\n", __func__);

	if (apr_pkt_priv->pkt_owner == APR_PKT_OWNER_DRIVER)
		apr_buf_add_tail(ptr);
}

bool apr_tal_notify_rx_intent_req(void *handle, const void *priv,
				  size_t req_size)
{
	struct apr_svc_ch_dev *apr_ch = (struct apr_svc_ch_dev *)priv;

	if (!apr_ch) {
		pr_err("%s: Invalid apr_ch\n", __func__);
		return false;
	}

	pr_err("%s: No rx intents queued, unable to receive\n", __func__);
	return false;
}

void apr_tal_notify_state(void *handle, const void *priv, unsigned event)
{
	struct apr_svc_ch_dev *apr_ch = (struct apr_svc_ch_dev *)priv;

	if (!apr_ch) {
		pr_err("%s: Invalid apr_ch\n", __func__);
		return;
	}

	apr_ch->channel_state = event;
	pr_info("%s: Channel state[%d]\n", __func__, event);

	if (event == GLINK_CONNECTED)
		wake_up(&apr_ch->wait);
}

int apr_tal_rx_intents_config(struct apr_svc_ch_dev *apr_ch,
			      int num_of_intents, uint32_t size)
{
	int i;
	int rc;

	if (!apr_ch || !num_of_intents || !size) {
		pr_err("%s: Invalid parameter\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < num_of_intents; i++) {
		rc = glink_queue_rx_intent(apr_ch->handle, apr_ch, size);
		if (rc) {
			pr_err("%s: Failed to queue rx intent, iteration[%d]\n",
			       __func__, i);
			break;
		}
	}

	return rc;
}

struct apr_svc_ch_dev *apr_tal_open(uint32_t clnt, uint32_t dest, uint32_t dl,
				    apr_svc_cb_fn func, void *priv)
{
	int rc;
	struct glink_open_config open_cfg;
	struct apr_svc_ch_dev *apr_ch;

	if ((clnt >= APR_CLIENT_MAX) || (dest >= APR_DEST_MAX) ||
	    (dl >= APR_DL_MAX)) {
		pr_err("%s: Invalid params, clnt:%d, dest:%d, dl:%d\n",
		       __func__, clnt, dest, dl);
		return NULL;
	}

	apr_ch = &apr_svc_ch[dl][dest][clnt];
	mutex_lock(&apr_ch->m_lock);
	if (apr_ch->handle) {
		pr_err("%s: This channel is already opened\n", __func__);
		rc = -EBUSY;
		goto unlock;
	}

	if (link_state[dest].link_state != GLINK_LINK_STATE_UP) {
		rc = wait_event_timeout(link_state[dest].wait,
			link_state[dest].link_state == GLINK_LINK_STATE_UP,
			msecs_to_jiffies(APR_OPEN_TIMEOUT_MS));
		if (rc == 0) {
			pr_err("%s: Open timeout, dest:%d\n", __func__, dest);
			rc = -ETIMEDOUT;
			goto unlock;
		}
		pr_debug("%s: Wakeup done, dest:%d\n", __func__, dest);
	}

	memset(&open_cfg, 0, sizeof(struct glink_open_config));
	open_cfg.options = GLINK_OPT_INITIAL_XPORT;
	if (dest == APR_DEST_MODEM)
		open_cfg.edge = "mpss";
	else
		open_cfg.edge = "lpass";

	open_cfg.name = svc_names[dest][clnt];
	open_cfg.notify_rx = apr_tal_notify_rx;
	open_cfg.notify_tx_done = apr_tal_notify_tx_done;
	open_cfg.notify_state = apr_tal_notify_state;
	open_cfg.notify_rx_intent_req = apr_tal_notify_rx_intent_req;
	open_cfg.priv = apr_ch;
	/*
	 * The transport name "smd_trans" is required if far end is using SMD.
	 * In that case Glink will fall back to SMD and the client (APR in this
	 * case) will still work as if Glink is the communication channel.
	 * If far end is already using Glink, this property will be ignored in
	 * Glink layer and communication will be through Glink.
	 */
	open_cfg.transport = "smd_trans";

	apr_ch->channel_state = GLINK_REMOTE_DISCONNECTED;
	apr_ch->handle = glink_open(&open_cfg);
	if (IS_ERR_OR_NULL(apr_ch->handle)) {
		pr_err("%s: glink_open failed %s\n", __func__,
		       svc_names[dest][clnt]);
		goto unlock;
	}

	rc = wait_event_timeout(apr_ch->wait,
		(apr_ch->channel_state == GLINK_CONNECTED), 5 * HZ);
	if (rc == 0) {
		pr_err("%s: TIMEOUT for OPEN event\n", __func__);
		rc = -ETIMEDOUT;
		goto close_link;
	}

	rc = apr_tal_rx_intents_config(apr_ch, APR_DEFAULT_NUM_OF_INTENTS,
				       APR_MAX_BUF);
	if (rc) {
		pr_err("%s: Unable to queue intents\n", __func__);
		goto close_link;
	}

	apr_ch->func = func;
	apr_ch->priv = priv;

close_link:
	if (rc) {
		glink_close(apr_ch->handle);
		apr_ch->handle = NULL;
	}
unlock:
	mutex_unlock(&apr_ch->m_lock);

	return rc ? NULL : apr_ch;
}

int apr_tal_close(struct apr_svc_ch_dev *apr_ch)
{
	int rc;

	if (!apr_ch || !apr_ch->handle) {
		rc = -EINVAL;
		goto exit;
	}

	mutex_lock(&apr_ch->m_lock);
	rc = glink_close(apr_ch->handle);
	apr_ch->handle = NULL;
	apr_ch->func = NULL;
	apr_ch->priv = NULL;
	mutex_unlock(&apr_ch->m_lock);
exit:
	return rc;
}

static void apr_tal_link_state_cb(struct glink_link_state_cb_info *cb_info,
				  void *priv)
{
	uint32_t dest;

	if (!cb_info) {
		pr_err("%s: Invalid cb_info\n", __func__);
		return;
	}

	if (!strcmp(cb_info->edge, "mpss"))
		dest = APR_DEST_MODEM;
	else if (!strcmp(cb_info->edge, "lpass"))
		dest = APR_DEST_QDSP6;
	else {
		pr_err("%s:Unknown edge[%s]\n", __func__, cb_info->edge);
		return;
	}

	pr_info("%s: edge[%s] link state[%d]\n", __func__, cb_info->edge,
		cb_info->link_state);

	link_state[dest].link_state = cb_info->link_state;
	if (link_state[dest].link_state == GLINK_LINK_STATE_UP)
		wake_up(&link_state[dest].wait);
}

static struct glink_link_info mpss_link_info = {
	.transport = NULL,
	.edge = "mpss",
	.glink_link_state_notif_cb = apr_tal_link_state_cb,
};

static struct glink_link_info lpass_link_info = {
	.transport = NULL,
	.edge = "lpass",
	.glink_link_state_notif_cb = apr_tal_link_state_cb,
};

static int __init apr_tal_init(void)
{
	int i, j, k;
	struct apr_tx_buf *buf;
	struct list_head *ptr, *next;

	for (i = 0; i < APR_DL_MAX; i++) {
		for (j = 0; j < APR_DEST_MAX; j++) {
			for (k = 0; k < APR_CLIENT_MAX; k++) {
				init_waitqueue_head(&apr_svc_ch[i][j][k].wait);
				spin_lock_init(&apr_svc_ch[i][j][k].w_lock);
				spin_lock_init(&apr_svc_ch[i][j][k].r_lock);
				mutex_init(&apr_svc_ch[i][j][k].m_lock);
			}
		}
	}

	for (i = 0; i < APR_DEST_MAX; i++)
		init_waitqueue_head(&link_state[i].wait);

	spin_lock_init(&buf_list.lock);
	INIT_LIST_HEAD(&buf_list.list);
	for (i = 0; i < APR_NUM_OF_TX_BUF; i++) {
		buf = kzalloc(sizeof(struct apr_tx_buf), GFP_KERNEL);
		if (!buf) {
			pr_err("%s: Unable to allocate tx buf\n", __func__);
			goto tx_buf_alloc_fail;
		}

		INIT_LIST_HEAD(&buf->list);
		spin_lock(&buf_list.lock);
		list_add_tail(&buf->list, &buf_list.list);
		spin_unlock(&buf_list.lock);
	}

	link_state[APR_DEST_MODEM].link_state = GLINK_LINK_STATE_DOWN;
	link_state[APR_DEST_MODEM].handle =
		glink_register_link_state_cb(&mpss_link_info, NULL);
	if (!link_state[APR_DEST_MODEM].handle)
		pr_err("%s: Unable to register mpss link state\n", __func__);

	link_state[APR_DEST_QDSP6].link_state = GLINK_LINK_STATE_DOWN;
	link_state[APR_DEST_QDSP6].handle =
		glink_register_link_state_cb(&lpass_link_info, NULL);
	if (!link_state[APR_DEST_QDSP6].handle)
		pr_err("%s: Unable to register lpass link state\n", __func__);

	return 0;

tx_buf_alloc_fail:
	list_for_each_safe(ptr, next, &buf_list.list) {
		buf = list_entry(ptr, struct apr_tx_buf, list);
		list_del(&buf->list);
		kfree(buf);
	}
	return -ENOMEM;
}
device_initcall(apr_tal_init);
