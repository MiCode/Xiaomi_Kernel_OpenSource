/* Copyright (c) 2010-2011, 2013-2014, 2016, 2018 The Linux Foundation.
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
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <soc/qcom/smd.h>
#include <ipc/apr_tal.h>

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

struct apr_svc_ch_dev apr_svc_ch[APR_DL_MAX][APR_DEST_MAX][APR_CLIENT_MAX];

int __apr_tal_write(struct apr_svc_ch_dev *apr_ch, void *data,
			struct apr_pkt_priv *pkt_priv, int len)
{
	int w_len;
	unsigned long flags;

	spin_lock_irqsave(&apr_ch->w_lock, flags);
	if (smd_write_avail(apr_ch->ch) < len) {
		spin_unlock_irqrestore(&apr_ch->w_lock, flags);
		return -EAGAIN;
	}

	w_len = smd_write(apr_ch->ch, data, len);
	spin_unlock_irqrestore(&apr_ch->w_lock, flags);

	pr_debug("apr_tal:w_len = %d\n", w_len);

	if (w_len != len) {
		pr_err("apr_tal: Error in write\n");
		return -ENETRESET;
	}
	return w_len;
}

int apr_tal_write(struct apr_svc_ch_dev *apr_ch, void *data,
			struct apr_pkt_priv *pkt_priv, int len)
{
	int rc = 0, retries = 0;

	if (!apr_ch->ch)
		return -EINVAL;

	do {
		if (rc == -EAGAIN)
			udelay(50);

		rc = __apr_tal_write(apr_ch, data, pkt_priv, len);
	} while (rc == -EAGAIN && retries++ < 300);

	if (rc == -EAGAIN)
		pr_err("apr_tal: TIMEOUT for write\n");

	return rc;
}

static void apr_tal_notify(void *priv, unsigned int event)
{
	struct apr_svc_ch_dev *apr_ch = priv;
	int len, r_len, sz;
	int pkt_cnt = 0;
	unsigned long flags;

	pr_debug("event = %d\n", event);
	switch (event) {
	case SMD_EVENT_DATA:
		pkt_cnt = 0;
		spin_lock_irqsave(&apr_ch->lock, flags);
check_pending:
		len = smd_read_avail(apr_ch->ch);
		if (len < 0) {
			pr_err("apr_tal: Invalid Read Event :%d\n", len);
			spin_unlock_irqrestore(&apr_ch->lock, flags);
			return;
		}
		sz = smd_cur_packet_size(apr_ch->ch);
		if (sz < 0) {
			pr_debug("pkt size is zero\n");
			spin_unlock_irqrestore(&apr_ch->lock, flags);
			return;
		}
		if (!len && !sz && !pkt_cnt)
			goto check_write_avail;
		if (!len) {
			pr_debug("len = %d pkt_cnt = %d\n", len, pkt_cnt);
			spin_unlock_irqrestore(&apr_ch->lock, flags);
			return;
		}
		r_len = smd_read_from_cb(apr_ch->ch, apr_ch->data, len);
		if (len != r_len) {
			pr_err("apr_tal: Invalid Read\n");
			spin_unlock_irqrestore(&apr_ch->lock, flags);
			return;
		}
		pkt_cnt++;
		pr_debug("%d %d %d\n", len, sz, pkt_cnt);
		if (apr_ch->func)
			apr_ch->func(apr_ch->data, r_len, apr_ch->priv);
		goto check_pending;
check_write_avail:
		if (smd_write_avail(apr_ch->ch))
			wake_up(&apr_ch->wait);
		spin_unlock_irqrestore(&apr_ch->lock, flags);
		break;
	case SMD_EVENT_OPEN:
		pr_debug("apr_tal: SMD_EVENT_OPEN\n");
		apr_ch->smd_state = 1;
		wake_up(&apr_ch->wait);
		break;
	case SMD_EVENT_CLOSE:
		pr_debug("apr_tal: SMD_EVENT_CLOSE\n");
		break;
	}
}

int apr_tal_rx_intents_config(struct apr_svc_ch_dev *apr_ch,
			int num_of_intents, uint32_t size)
{
	/* Rx intents configuration is required for Glink
	 * but not for SMD. No-op for this function.
	 */
	return 0;
}

struct apr_svc_ch_dev *apr_tal_open(uint32_t clnt, uint32_t dest,
				uint32_t dl, apr_svc_cb_fn func, void *priv)
{
	int rc;

	if ((clnt >= APR_CLIENT_MAX) || (dest >= APR_DEST_MAX) ||
						(dl >= APR_DL_MAX)) {
		pr_err("apr_tal: Invalid params\n");
		return NULL;
	}

	if (apr_svc_ch[dl][dest][clnt].ch) {
		pr_err("apr_tal: This channel alreday openend\n");
		return NULL;
	}

	mutex_lock(&apr_svc_ch[dl][dest][clnt].m_lock);
	if (!apr_svc_ch[dl][dest][clnt].dest_state) {
		rc = wait_event_timeout(apr_svc_ch[dl][dest][clnt].dest,
			apr_svc_ch[dl][dest][clnt].dest_state,
				msecs_to_jiffies(APR_OPEN_TIMEOUT_MS));
		if (rc == 0) {
			pr_err("apr_tal:open timeout\n");
			mutex_unlock(&apr_svc_ch[dl][dest][clnt].m_lock);
			return NULL;
		}
		pr_debug("apr_tal:Wakeup done\n");
		apr_svc_ch[dl][dest][clnt].dest_state = 0;
	}
	rc = smd_named_open_on_edge(svc_names[dest][clnt], dest,
			&apr_svc_ch[dl][dest][clnt].ch,
			&apr_svc_ch[dl][dest][clnt],
			apr_tal_notify);
	if (rc < 0) {
		pr_err("apr_tal: smd_open failed %s\n",
					svc_names[dest][clnt]);
		mutex_unlock(&apr_svc_ch[dl][dest][clnt].m_lock);
		return NULL;
	}
	rc = wait_event_timeout(apr_svc_ch[dl][dest][clnt].wait,
		(apr_svc_ch[dl][dest][clnt].smd_state == 1), 5 * HZ);
	if (rc == 0) {
		pr_err("apr_tal:TIMEOUT for OPEN event\n");
		mutex_unlock(&apr_svc_ch[dl][dest][clnt].m_lock);
		apr_tal_close(&apr_svc_ch[dl][dest][clnt]);
		return NULL;
	}

	smd_disable_read_intr(apr_svc_ch[dl][dest][clnt].ch);

	if (!apr_svc_ch[dl][dest][clnt].dest_state) {
		apr_svc_ch[dl][dest][clnt].dest_state = 1;
		pr_debug("apr_tal:Waiting for apr svc init\n");
		msleep(200);
		pr_debug("apr_tal:apr svc init done\n");
	}
	apr_svc_ch[dl][dest][clnt].smd_state = 0;

	apr_svc_ch[dl][dest][clnt].func = func;
	apr_svc_ch[dl][dest][clnt].priv = priv;
	mutex_unlock(&apr_svc_ch[dl][dest][clnt].m_lock);

	return &apr_svc_ch[dl][dest][clnt];
}

int apr_tal_close(struct apr_svc_ch_dev *apr_ch)
{
	int r;

	if (!apr_ch->ch)
		return -EINVAL;

	mutex_lock(&apr_ch->m_lock);
	r = smd_close(apr_ch->ch);
	apr_ch->ch = NULL;
	apr_ch->func = NULL;
	apr_ch->priv = NULL;
	mutex_unlock(&apr_ch->m_lock);
	return r;
}

static int apr_smd_probe(struct platform_device *pdev)
{
	int dest;
	int clnt;

	if (pdev->id == APR_DEST_MODEM) {
		pr_info("apr_tal:Modem Is Up\n");
		dest = APR_DEST_MODEM;
		if (!strcmp(pdev->name, "apr_audio_svc"))
			clnt = APR_CLIENT_AUDIO;
		else
			clnt = APR_CLIENT_VOICE;
		apr_svc_ch[APR_DL_SMD][dest][clnt].dest_state = 1;
		wake_up(&apr_svc_ch[APR_DL_SMD][dest][clnt].dest);
	} else if (pdev->id == APR_DEST_QDSP6) {
		pr_info("apr_tal:Q6 Is Up\n");
		dest = APR_DEST_QDSP6;
		clnt = APR_CLIENT_AUDIO;
		apr_svc_ch[APR_DL_SMD][dest][clnt].dest_state = 1;
		wake_up(&apr_svc_ch[APR_DL_SMD][dest][clnt].dest);
	} else
		pr_err("apr_tal:Invalid Dest Id: %d\n", pdev->id);

	return 0;
}

static struct platform_driver apr_q6_driver = {
	.probe = apr_smd_probe,
	.driver = {
		.name = "apr_audio_svc",
		.owner = THIS_MODULE,
	},
};

static struct platform_driver apr_modem_driver = {
	.probe = apr_smd_probe,
	.driver = {
		.name = "apr_voice_svc",
		.owner = THIS_MODULE,
	},
};

int apr_tal_init(void)
{
	int i, j, k;

	for (i = 0; i < APR_DL_MAX; i++)
		for (j = 0; j < APR_DEST_MAX; j++)
			for (k = 0; k < APR_CLIENT_MAX; k++) {
				init_waitqueue_head(&apr_svc_ch[i][j][k].wait);
				init_waitqueue_head(&apr_svc_ch[i][j][k].dest);
				spin_lock_init(&apr_svc_ch[i][j][k].lock);
				spin_lock_init(&apr_svc_ch[i][j][k].w_lock);
				mutex_init(&apr_svc_ch[i][j][k].m_lock);
			}
	platform_driver_register(&apr_q6_driver);
	platform_driver_register(&apr_modem_driver);
	return 0;
}
