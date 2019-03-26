/*
 * Copyright (c) 2017-2018 The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/rpmsg.h>
#include <ipc/apr_tal.h>

enum apr_channel_state {
	APR_CH_DISCONNECTED,
	APR_CH_CONNECTED,
};

#define APR_MAXIMUM_NUM_OF_RETRIES 2

static struct apr_svc_ch_dev
	apr_svc_ch[APR_DL_MAX][APR_DEST_MAX][APR_CLIENT_MAX];

/**
 * apr_tal_write() - Write a message across to the remote processor
 * @apr_ch: apr channel handle
 * @data: buffer that needs to be transferred over the channel
 * @pkt_priv: private data of the packet
 * @len: length of the buffer
 *
 * Returns len of buffer successfully transferred on success
 * and an appropriate error value on failure.
 */
int apr_tal_write(struct apr_svc_ch_dev *apr_ch, void *data,
			   struct apr_pkt_priv *pkt_priv, int len)
{
	int rc = 0, retries = 0;
	unsigned long flags;
	struct rpmsg_device *rpdev = NULL;

	if (!apr_ch || len > APR_MAX_BUF ||
	    apr_ch->channel_state != APR_CH_CONNECTED)
		return -EINVAL;

	spin_lock_irqsave(&apr_ch->w_lock, flags);
	rpdev = apr_ch->handle;
	if (!rpdev) {
		spin_unlock_irqrestore(&apr_ch->w_lock, flags);
		return -EINVAL;
	}

	do {
		if (rc == -EAGAIN)
			udelay(50);
		rc = rpmsg_trysend(rpdev->ept, data, len);
	} while (rc == -EAGAIN && retries++ < APR_MAXIMUM_NUM_OF_RETRIES);
	spin_unlock_irqrestore(&apr_ch->w_lock, flags);

	if (rc)
		pr_err("%s: Unable to send the packet, rc:%d\n", __func__, rc);
	else
		rc = len;

	return rc;
}
EXPORT_SYMBOL(apr_tal_write);

/**
 * apr_tal_rx_intents_config() - Configure glink intents for remote processor
 * @apr_ch: apr channel handle
 * @num_of_intents: number of intents
 * @size: size of the intents
 *
 * This api is not supported with RPMSG. Returns 0 to indicate success
 */
int apr_tal_rx_intents_config(struct apr_svc_ch_dev *apr_ch,
			      int num_of_intents, uint32_t size)
{
	pr_debug("%s: NO-OP\n", __func__);
	return 0;
}
EXPORT_SYMBOL(apr_tal_rx_intents_config);

/**
 * apr_tal_start_rx_rt() - Set RT thread priority for APR RX transfer
 * @apr_ch: apr channel handle
 *
 * This api is not supported with RPMSG as message transfer occurs
 * in client's context. Returns 0 to indicate success.
 */
int apr_tal_start_rx_rt(struct apr_svc_ch_dev *apr_ch)
{
	pr_debug("%s: NO-OP\n", __func__);
	return 0;
}
EXPORT_SYMBOL(apr_tal_start_rx_rt);

/**
 * apr_tal_end_rx_rt() - Remove RT thread priority for APR RX transfer
 * @apr_ch: apr channel handle
 *
 * This api is not supported with RPMSG. Returns 0 to indicate success
 */
int apr_tal_end_rx_rt(struct apr_svc_ch_dev *apr_ch)
{
	pr_debug("%s: NO-OP\n", __func__);
	return 0;
}
EXPORT_SYMBOL(apr_tal_end_rx_rt);

/**
 * apr_tal_open() - Open a transport channel for data transfer
 * on remote processor.
 * @clnt: apr client, audio or voice
 * @dest: destination remote processor for which apr channel is requested for.
 * @dl: type of data link
 * @func: callback function to handle data transfer from remote processor
 * @priv: private data of the client
 *
 * Returns apr_svc_ch_dev handle on success and NULL on failure.
 */
struct apr_svc_ch_dev *apr_tal_open(uint32_t clnt, uint32_t dest, uint32_t dl,
				    apr_svc_cb_fn func, void *priv)
{
	int rc = 0;
	struct apr_svc_ch_dev *apr_ch = NULL;

	if ((clnt != APR_CLIENT_AUDIO) || (dest != APR_DEST_QDSP6) ||
	    (dl != APR_DL_SMD)) {
		pr_err("%s: Invalid params, clnt:%d, dest:%d, dl:%d\n",
		       __func__, clnt, dest, dl);
		return NULL;
	}

	apr_ch = &apr_svc_ch[APR_DL_SMD][APR_DEST_QDSP6][APR_CLIENT_AUDIO];
	mutex_lock(&apr_ch->m_lock);
	if (!apr_ch->handle) {
		rc = wait_event_timeout(apr_ch->wait,
			(apr_ch->channel_state == APR_CH_CONNECTED), 5 * HZ);

		if (rc == 0) {
			pr_err("%s: TIMEOUT for APR_CH_CONNECTED event\n",
				__func__);
			rc = -ETIMEDOUT;
			goto unlock;
		}
	}

	pr_debug("%s: Channel connected, returning handle :%pK\n",
			 __func__, apr_ch->handle);
	apr_ch->func = func;
	apr_ch->priv = priv;

unlock:
	mutex_unlock(&apr_ch->m_lock);
	return rc ? NULL : apr_ch;
}
EXPORT_SYMBOL(apr_tal_open);

/**
 * apr_tal_close() - Close transport channel on remote processor.
 * @apr_ch: apr channel handle
 *
 * Returns 0 on success and an appropriate error value on failure.
 */
int apr_tal_close(struct apr_svc_ch_dev *apr_ch)
{
	int rc = 0;

	if (!apr_ch || !apr_ch->handle) {
		rc = -EINVAL;
		goto exit;
	}

	mutex_lock(&apr_ch->m_lock);
	apr_ch->func = NULL;
	apr_ch->priv = NULL;
	mutex_unlock(&apr_ch->m_lock);

exit:
	return rc;
}
EXPORT_SYMBOL(apr_tal_close);

static int apr_tal_rpmsg_callback(struct rpmsg_device *rpdev,
				  void *data, int len, void *priv, u32 addr)
{
	struct apr_svc_ch_dev *apr_ch = dev_get_drvdata(&rpdev->dev);
	unsigned long flags;

	if (!apr_ch || !data) {
		pr_err("%s: Invalid apr_ch or ptr\n", __func__);
		return -EINVAL;
	}

	dev_dbg(&rpdev->dev, "%s: Rx packet received, len:%d\n",
		__func__, len);

	spin_lock_irqsave(&apr_ch->r_lock, flags);
	if (apr_ch->func)
		apr_ch->func((void *)data, len, apr_ch->priv);
	spin_unlock_irqrestore(&apr_ch->r_lock, flags);

	return 0;
}

static int apr_tal_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct apr_svc_ch_dev *apr_ch = NULL;

	if (!strcmp(rpdev->id.name, "apr_audio_svc")) {
		dev_info(&rpdev->dev, "%s: Channel[%s] state[Up]\n",
			 __func__, rpdev->id.name);

		apr_ch =
		&apr_svc_ch[APR_DL_SMD][APR_DEST_QDSP6][APR_CLIENT_AUDIO];
		apr_ch->handle = rpdev;
		apr_ch->channel_state = APR_CH_CONNECTED;
		dev_set_drvdata(&rpdev->dev, apr_ch);
		wake_up(&apr_ch->wait);
	} else {
		dev_err(&rpdev->dev, "%s, Invalid Channel [%s]\n",
			__func__, rpdev->id.name);
		return -EINVAL;
	}

	return 0;
}

static void apr_tal_rpmsg_remove(struct rpmsg_device *rpdev)
{
	struct apr_svc_ch_dev *apr_ch = dev_get_drvdata(&rpdev->dev);

	if (!apr_ch) {
		dev_err(&rpdev->dev, "%s: Invalid apr_ch\n", __func__);
		return;
	}

	dev_info(&rpdev->dev, "%s: Channel[%s] state[Down]\n",
		 __func__, rpdev->id.name);
	apr_ch->handle = NULL;
	apr_ch->channel_state = APR_CH_DISCONNECTED;
	dev_set_drvdata(&rpdev->dev, NULL);
}

static const struct rpmsg_device_id apr_tal_rpmsg_match[] = {
	{ "apr_audio_svc" },
	{}
};

static struct rpmsg_driver apr_tal_rpmsg_driver = {
	.probe = apr_tal_rpmsg_probe,
	.remove = apr_tal_rpmsg_remove,
	.callback = apr_tal_rpmsg_callback,
	.id_table = apr_tal_rpmsg_match,
	.drv = {
		.name = "apr_tal_rpmsg",
	},
};

/**
 * apr_tal_int() - Registers rpmsg driver with rpmsg framework.
 *
 * Returns 0 on success and an appropriate error value on failure.
 */
int apr_tal_init(void)
{
	int i, j, k;
	int ret;

	memset(apr_svc_ch, 0, sizeof(struct apr_svc_ch_dev));
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
	ret = register_rpmsg_driver(&apr_tal_rpmsg_driver);
	return ret;
}
EXPORT_SYMBOL(apr_tal_init);

/**
 * apr_tal_exit() - De-register rpmsg driver with rpmsg framework.
 */
void apr_tal_exit(void)
{
	unregister_rpmsg_driver(&apr_tal_rpmsg_driver);
}
EXPORT_SYMBOL(apr_tal_exit);

