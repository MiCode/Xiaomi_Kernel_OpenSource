/* Copyright (c) 2016-2017 The Linux Foundation.
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
	struct apr_pkt_priv pkt_priv;
	char buf[APR_MAX_BUF];
};

struct link_state {
	uint32_t dest;
	void *handle;
	enum glink_link_state link_state;
	wait_queue_head_t wait;
};

static struct link_state link_state[APR_DEST_MAX];

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

static struct apr_tx_buf *apr_alloc_buf(int len)
{

	if (len > APR_MAX_BUF) {
		pr_err("%s: buf too large [%d]\n", __func__, len);
		return ERR_PTR(-EINVAL);
	}

	return kzalloc(sizeof(struct apr_tx_buf), GFP_ATOMIC);
}

static void apr_free_buf(const void *ptr)
{

	struct apr_pkt_priv *apr_pkt_priv = (struct apr_pkt_priv *)ptr;
	struct apr_tx_buf *tx_buf;

	if (!apr_pkt_priv) {
		pr_err("%s: Invalid apr_pkt_priv\n", __func__);
		return;
	}

	if (apr_pkt_priv->pkt_owner == APR_PKT_OWNER_DRIVER) {
		tx_buf = container_of((void *)apr_pkt_priv,
				      struct apr_tx_buf, pkt_priv);
		pr_debug("%s: Freeing buffer %pK", __func__, tx_buf);
		kfree(tx_buf);
	}
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
	struct apr_tx_buf *tx_buf;
	struct apr_pkt_priv *pkt_priv_ptr = pkt_priv;

	if (!apr_ch->handle || !pkt_priv)
		return -EINVAL;

	if (pkt_priv->pkt_owner == APR_PKT_OWNER_DRIVER) {
		tx_buf = apr_alloc_buf(len);
		if (IS_ERR_OR_NULL(tx_buf)) {
			rc = -EINVAL;
			goto exit;
		}
		memcpy(tx_buf->buf, data, len);
		memcpy(&tx_buf->pkt_priv, pkt_priv, sizeof(tx_buf->pkt_priv));
		pkt_priv_ptr = &tx_buf->pkt_priv;
		pkt_data = tx_buf->buf;
	} else {
		pkt_data = data;
	}

	do {
		if (rc == -EAGAIN)
			udelay(50);

		rc = __apr_tal_write(apr_ch, pkt_data, pkt_priv_ptr, len);
	} while (rc == -EAGAIN && retries++ < APR_MAXIMUM_NUM_OF_RETRIES);

	if (rc < 0) {
		pr_err("%s: Unable to send the packet, rc:%d\n", __func__, rc);
		if (pkt_priv->pkt_owner == APR_PKT_OWNER_DRIVER)
			kfree(tx_buf);
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

static void apr_tal_notify_tx_abort(void *handle, const void *priv,
				    const void *pkt_priv)
{
	pr_debug("%s: tx_abort received for pkt_priv:%pK\n",
		 __func__, pkt_priv);
	apr_free_buf(pkt_priv);
}

void apr_tal_notify_tx_done(void *handle, const void *priv,
			    const void *pkt_priv, const void *ptr)
{
	pr_debug("%s: tx_done received for pkt_priv:%pK\n",
		 __func__, pkt_priv);
	apr_free_buf(pkt_priv);
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

static void apr_tal_notify_remote_rx_intent(void *handle, const void *priv,
					    size_t size)
{
	struct apr_svc_ch_dev *apr_ch = (struct apr_svc_ch_dev *)priv;

	if (!apr_ch) {
		pr_err("%s: Invalid apr_ch\n", __func__);
		return;
	}
	/*
	 * This is to make sure that the far end has queued at least one intent
	 * before we attmpt any IPC. A simple bool flag is used here instead of
	 * a counter, as the far end is required to guarantee intent
	 * availability for all use cases once the channel is fully opened.
	 */
	pr_debug("%s: remote queued an intent\n", __func__);
	apr_ch->if_remote_intent_ready = true;
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
	open_cfg.notify_remote_rx_intent = apr_tal_notify_remote_rx_intent;
	open_cfg.notify_tx_abort = apr_tal_notify_tx_abort;
	open_cfg.priv = apr_ch;
	open_cfg.transport = "smem";

	apr_ch->channel_state = GLINK_REMOTE_DISCONNECTED;
	apr_ch->handle = glink_open(&open_cfg);
	if (IS_ERR_OR_NULL(apr_ch->handle)) {
		pr_err("%s: glink_open failed %s\n", __func__,
		       svc_names[dest][clnt]);
		apr_ch->handle = NULL;
		rc = -EINVAL;
		goto unlock;
	}

	rc = wait_event_timeout(apr_ch->wait,
		(apr_ch->channel_state == GLINK_CONNECTED), 5 * HZ);
	if (rc == 0) {
		pr_err("%s: TIMEOUT for OPEN event\n", __func__);
		rc = -ETIMEDOUT;
		goto close_link;
	}

	/*
	 * Remote intent is not required for GLINK <--> SMD IPC, so this is
	 * designed not to fail the open call.
	 */
	rc = wait_event_timeout(apr_ch->wait,
		apr_ch->if_remote_intent_ready, 5 * HZ);
	if (rc == 0)
		pr_err("%s: TIMEOUT for remote intent readiness\n", __func__);

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
	apr_ch->if_remote_intent_ready = false;
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
	.transport = "smem",
	.edge = "mpss",
	.glink_link_state_notif_cb = apr_tal_link_state_cb,
};

static struct glink_link_info lpass_link_info = {
	.transport = "smem",
	.edge = "lpass",
	.glink_link_state_notif_cb = apr_tal_link_state_cb,
};

static int __init apr_tal_init(void)
{
	int i, j, k;

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
}
device_initcall(apr_tal_init);
