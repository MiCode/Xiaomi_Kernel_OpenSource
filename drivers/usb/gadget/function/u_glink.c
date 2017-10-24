/* Copyright (c) 2017, Linux Foundation. All rights reserved.
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
#include <linux/errno.h>
#include <linux/termios.h>
#include <linux/debugfs.h>
#include <soc/qcom/glink.h>

#include "u_rmnet.h"

static struct workqueue_struct	*glink_ctrl_wq;

struct gl_link_info {
	void	*handle;
	enum	glink_link_state link_state;
};

struct glink_channel {
	struct grmnet		*port;
	void			*handle;
	struct glink_open_config open_cfg;
	unsigned		channel_state;

	struct list_head	tx_q;
	unsigned		tx_q_count;
	uint32_t		cbits_to_modem;

	spinlock_t		port_lock;
	struct work_struct	write_w;

	struct work_struct	connect_w;
	struct work_struct	disconnect_w;

	struct completion	close_done;

	unsigned long		to_modem;
	unsigned long		to_host;
};

#define MAX_CHANNELS	2
#define MAX_INTENTS	20
#define RX_INTENT_SIZE	2048

struct glink_ctrl_intent_work {
	struct glink_channel *ch_info;
	struct work_struct work;
};

static struct gl_link_info  link_info;
static struct glink_ctrl_intent_work	intent_work;
static struct glink_channel *glink_channels[MAX_CHANNELS];

static struct rmnet_ctrl_pkt *alloc_rmnet_cpkt(unsigned len, gfp_t flags)
{
	struct rmnet_ctrl_pkt *pkt;

	pkt = kzalloc(sizeof(struct rmnet_ctrl_pkt), flags);
	if (!pkt)
		return ERR_PTR(-ENOMEM);

	pkt->buf = kmalloc(len, flags);
	if (!pkt->buf) {
		kfree(pkt);
		return ERR_PTR(-ENOMEM);
	}

	pkt->len = len;

	return pkt;
}

static void glink_ctrl_connect_w(struct work_struct *w)
{
	struct glink_channel *ch_info = container_of(w, struct glink_channel,
							connect_w);
	unsigned long flags;
	int set_bits = 0;
	int clear_bits = 0;
	uint32_t sig = 0;

	pr_debug("%s\n", __func__);
	if (!ch_info->port) {
		pr_err("%s: no port\n", __func__);
		return;
	}

	/**
	 * If the channel is not opened yet, open the channel and queue
	 * connect work again once the remote client has also opened the
	 * channel.
	 */
	if (!ch_info->handle) {
		ch_info->handle = glink_open(&ch_info->open_cfg);
		if (IS_ERR(ch_info->handle)) {
			pr_err("%s failed to open GLINK channel %lu\n",
					__func__, PTR_ERR(ch_info->handle));
			ch_info->handle = NULL;
			return;
		}
	}

	if (ch_info->channel_state != GLINK_CONNECTED) {
		pr_debug("%s: remote not yet connected wait for notification\n",
				__func__);
		return;
	}

	set_bits = ch_info->cbits_to_modem;
	clear_bits = ~(ch_info->cbits_to_modem | TIOCM_RTS);

	sig |= set_bits;
	sig &= ~clear_bits;

	spin_lock_irqsave(&ch_info->port_lock, flags);
	glink_sigs_set(ch_info->handle, sig);
	spin_unlock_irqrestore(&ch_info->port_lock, flags);

	queue_work(glink_ctrl_wq, &ch_info->write_w);
}

static void glink_ctrl_disconnect_w(struct work_struct *w)
{
	struct glink_channel *ch_info = container_of(w, struct glink_channel,
							disconnect_w);

	pr_debug("%s: close glink channel %pK\n", __func__, ch_info->handle);
	if (ch_info->handle) {
		reinit_completion(&ch_info->close_done);
		glink_close(ch_info->handle);
		wait_for_completion(&ch_info->close_done);
		pr_debug("%s: glink channel closed\n", __func__);
	}
}

static void glink_ctrl_intent_worker(struct work_struct *w)
{
	struct glink_ctrl_intent_work *intent_work = container_of(w,
					struct glink_ctrl_intent_work, work);
	struct glink_channel *ch_info = intent_work->ch_info;
	int i;

	for (i = 0; i < MAX_INTENTS; i++) {
		glink_queue_rx_intent(ch_info->handle, (void *)ch_info,
					RX_INTENT_SIZE);
	}
}

static void glink_ctrl_write_w(struct work_struct *w)
{
	struct glink_channel *ch_info = container_of(w,
					struct glink_channel, write_w);
	struct rmnet_ctrl_pkt *cpkt;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&ch_info->port_lock, flags);
	pr_debug("%s: Tx Q count:%u\n", __func__, ch_info->tx_q_count);
	while (ch_info->channel_state == GLINK_CONNECTED) {
		if (list_empty(&ch_info->tx_q)) {
			pr_debug("%s: list is empty\n", __func__);
			break;
		}
		cpkt = list_first_entry(&ch_info->tx_q,
					struct rmnet_ctrl_pkt, list);
		list_del(&cpkt->list);
		spin_unlock_irqrestore(&ch_info->port_lock, flags);

		ret = glink_tx(ch_info->handle, (void *)ch_info,
				(void *)cpkt->buf, cpkt->len,
				GLINK_TX_REQ_INTENT);

		spin_lock_irqsave(&ch_info->port_lock, flags);

		if (ret < 0) {
			pr_err("%s: G-link Tx fail\n", __func__);
			kfree(cpkt->buf);
		}

		/*
		 * If glink_tx succeeds then the buffer(cpkt->buf) is now owned
		 * by the remote glink client/glink core and should not be
		 * freed. The buffer should be freed once the tx_done
		 * notification is received which is when the ownership is
		 * returned back to the client.
		 */
		kfree(cpkt);
		ch_info->tx_q_count--;
	}
	spin_unlock_irqrestore(&ch_info->port_lock, flags);
}

#define GLINK_CTRL_PKT_Q_LIMIT 50

static int glink_send_cpkt_tomodem(u8 client_num, void *buf, size_t len)
{
	struct glink_channel *ch_info;
	struct rmnet_ctrl_pkt *cpkt;
	unsigned long flags;

	if (client_num >= MAX_CHANNELS) {
		pr_err("%s: Invalid client number\n", __func__);
		return -EINVAL;
	}

	ch_info = glink_channels[client_num];
	if (!ch_info) {
		pr_err("%s: channel not set up\n", __func__);
		return -EINVAL;
	}

	cpkt = alloc_rmnet_cpkt(len, GFP_ATOMIC);

	memcpy(cpkt->buf, buf, len);
	cpkt->len = len;

	spin_lock_irqsave(&ch_info->port_lock, flags);
	if (ch_info->tx_q_count > GLINK_CTRL_PKT_Q_LIMIT) {
		pr_err_ratelimited("%s Dropping GLINK CTRL Pkt: limit: %u\n",
				__func__, ch_info->tx_q_count);
		spin_unlock_irqrestore(&ch_info->port_lock, flags);
		kfree(cpkt->buf);
		kfree(cpkt);
		return 0;
	}

	list_add_tail(&cpkt->list, &ch_info->tx_q);
	ch_info->tx_q_count++;
	spin_unlock_irqrestore(&ch_info->port_lock, flags);

	if (ch_info->handle && (ch_info->channel_state == GLINK_CONNECTED))
		queue_work(glink_ctrl_wq, &ch_info->write_w);

	return 0;
}

void glink_ctrl_tx_done(void *handle, const void *priv,
				const void *pkt_priv, const void *ptr)
{
	struct glink_channel *c	= (struct glink_channel *)priv;

	c->to_modem++;
	kfree(ptr);
}

#define RMNET_CTRL_DTR		0x01

static void glink_send_cbits_to_modem(void *gptr, u8 client_num, int cbits)
{
	struct glink_channel *ch_info;
	int set_bits		= 0;
	int clear_bits		= 0;
	uint32_t sig		= 0;

	if (client_num >= MAX_CHANNELS) {
		pr_err("%s: Invalid client number\n", __func__);
		return;
	}

	ch_info = glink_channels[client_num];
	if (!ch_info) {
		pr_err("%s: channel not set up\n", __func__);
		return;
	}

	cbits = cbits & RMNET_CTRL_DTR;
	if (cbits & RMNET_CTRL_DTR)
		set_bits |= TIOCM_DTR;
	else
		clear_bits |= TIOCM_DTR;

	if (!ch_info->handle)
		return;

	glink_sigs_local_get(ch_info->handle, &sig);
	sig |= set_bits;
	sig &= ~clear_bits;

	if (sig == ch_info->cbits_to_modem)
		return;

	ch_info->cbits_to_modem = sig;

	glink_sigs_set(ch_info->handle, sig);
}

static void glink_ctrl_notify_rx(void *handle, const void *priv,
				const void *pkt_priv, const void *ptr,
				size_t size)
{
	struct glink_channel *ch_info = (struct glink_channel *)priv;
	unsigned long flags;

	spin_lock_irqsave(&ch_info->port_lock, flags);
	if (ch_info->port && ch_info->port->send_cpkt_response)
		ch_info->port->send_cpkt_response(ch_info->port,
							(void *)ptr, size);
	spin_unlock_irqrestore(&ch_info->port_lock, flags);

	ch_info->to_host++;
	glink_rx_done(ch_info->handle, ptr, true);
}

void glink_purge_tx_q(struct glink_channel *ch_info)
{
	struct rmnet_ctrl_pkt *cpkt;

	pr_debug("%s\n", __func__);
	while (!list_empty(&ch_info->tx_q)) {
		cpkt = list_first_entry(&ch_info->tx_q,
					struct rmnet_ctrl_pkt, list);
		list_del(&cpkt->list);
		kfree(cpkt->buf);
		kfree(cpkt);
	}

	ch_info->tx_q_count = 0;
}

/**
 * Callback function notifying the change in the channel state.
 *
 * @handle:	handle corresponding to the channel.
 * @priv:	private data passed as part of glink_open.
 * @event:	state event sent by the glink core.
 *
 * The event indicates the current state of the channel.
 *
 * GLINK_CONNECTED:
 * The channel is opened by both client and remote and is ready for
 * transfers.
 *
 * GLINK_LOCAL_DISCONNECTED:
 * The channel is closed by the local client and no transfers
 * should be initiated on this channel.
 *
 * GLINK_REMOTE_DISCONNECTED:
 * The channel is closed by the remote client and no transfers should
 * initiated further.
 */
static void glink_notify_state(void *handle, const void *priv, unsigned event)
{
	struct glink_channel *ch_info = (struct glink_channel *)priv;
	struct grmnet	*gr;
	unsigned long flags;

	ch_info->channel_state = event;

	spin_lock_irqsave(&ch_info->port_lock, flags);
	gr = ch_info->port;
	pr_debug("%s: notify link state: %u\n", __func__, event);
	switch (event) {
	case GLINK_CONNECTED:
		if (gr && gr->connect) {
			gr->connect(gr);
			queue_work(glink_ctrl_wq, &ch_info->connect_w);
			intent_work.ch_info = ch_info;
			queue_work(glink_ctrl_wq, &intent_work.work);
		}
		break;
	case GLINK_LOCAL_DISCONNECTED:
		ch_info->handle = NULL;
		complete(&ch_info->close_done);
	case GLINK_REMOTE_DISCONNECTED:
		if (gr && gr->disconnect)
			gr->disconnect(gr);
		glink_purge_tx_q(ch_info);
		break;
	default:
		pr_err("%s: invalid channel state notification\n", __func__);
	}
	spin_unlock_irqrestore(&ch_info->port_lock, flags);
}

int glink_ctrl_connect(struct grmnet *gr, u8 client_num)
{
	struct glink_channel *ch_info;
	unsigned long flags;

	if (client_num >= MAX_CHANNELS) {
		pr_err("%s: Invalid client number\n", __func__);
		return -EINVAL;
	}

	ch_info = glink_channels[client_num];
	if (!ch_info) {
		pr_err("%s: channel not set up\n", __func__);
		return -EINVAL;
	}

	if (!gr) {
		pr_err("%s: no control port\n", __func__);
		return -EINVAL;
	}

	memset(&ch_info->open_cfg, 0, sizeof(struct glink_open_config));
	ch_info->open_cfg.options =  GLINK_OPT_INITIAL_XPORT;
	ch_info->open_cfg.edge = "mpss";
	ch_info->open_cfg.name = "DATA39_CNTL";
	ch_info->open_cfg.transport = "smem";
	ch_info->open_cfg.notify_rx = glink_ctrl_notify_rx;
	ch_info->open_cfg.notify_tx_done = glink_ctrl_tx_done;
	ch_info->open_cfg.notify_state = glink_notify_state;
	ch_info->open_cfg.priv = ch_info;

	spin_lock_irqsave(&ch_info->port_lock, flags);
	ch_info->port = gr;
	gr->send_encap_cmd = glink_send_cpkt_tomodem;
	gr->notify_modem = glink_send_cbits_to_modem;
	spin_unlock_irqrestore(&ch_info->port_lock, flags);

	queue_work(glink_ctrl_wq, &ch_info->connect_w);
	return 0;
}

void glink_ctrl_disconnect(struct grmnet *gr, u8 client_num)
{
	struct glink_channel *ch_info;
	unsigned long flags;

	pr_debug("%s: glink ctrl disconnect\n", __func__);
	if (client_num >= MAX_CHANNELS) {
		pr_err("%s: Invalid client number\n", __func__);
		return;
	}

	ch_info = glink_channels[client_num];
	if (!ch_info) {
		pr_err("%s: channel not set up\n", __func__);
		return;
	}

	spin_lock_irqsave(&ch_info->port_lock, flags);
	ch_info->port->send_encap_cmd = 0;
	ch_info->port->notify_modem = 0;
	ch_info->port = 0;

	glink_purge_tx_q(ch_info);
	spin_unlock_irqrestore(&ch_info->port_lock, flags);

	if (ch_info->handle) {
		pr_debug("%s queue disconnect work\n", __func__);
		queue_work(glink_ctrl_wq, &ch_info->disconnect_w);
	}
}

/**
 * Callback function notifying the change in transport(smem) link state.
 * G-Link acts as a wrapper around the underlying transport. This callback
 * notifies any change in the state of that underlying transport.
 *
 * @cb_info:	Structure containing the info of transport,
 *		edge and the link state.
 * @priv:	Private Data passed as part of link state callback register.
 *
 * GLINK_LINK_STATE_DOWN:	The underlying transport link is down and no
 *				channels can be opened over this transport.
 *
 * GLINK_LINK_STATE_UP:		The underlying transport link is up and the
 *				glink channels can be opened over this
 *				transport.
 */
static void glink_link_state_cb(struct glink_link_state_cb_info *cb_info,
				void *priv)
{
	int i = 0;

	link_info.link_state = cb_info->link_state;

	switch (link_info.link_state) {
	case GLINK_LINK_STATE_DOWN:
		pr_debug("%s: %s link is down\n", __func__, cb_info->transport);
		for (i = 0; i < MAX_CHANNELS; i++) {
			if (glink_channels[i] && glink_channels[i]->port) {
				queue_work(glink_ctrl_wq,
					&glink_channels[i]->disconnect_w);
			}
		}
		break;
	case GLINK_LINK_STATE_UP:
		pr_debug("%s: %s link is up\n", __func__, cb_info->transport);
		for (i = 0; i < MAX_CHANNELS; i++) {
			if (glink_channels[i] && glink_channels[i]->port) {
				queue_work(glink_ctrl_wq,
						&glink_channels[i]->connect_w);
			}
		}
		break;
	default:
		pr_err("%s: invalid link state notification\n", __func__);
	}
}

static struct glink_link_info link_cb_info = {
	.transport	=	"smem",
	.edge		=	"mpss",
	.glink_link_state_notif_cb =	glink_link_state_cb,
};

int glink_ctrl_setup(enum ctrl_client client_num, unsigned int count,
		u8 *port_idx)
{
	struct glink_channel		*ch_info;

	pr_debug("%s: G-Link ctrl setup\n", __func__);

	if (client_num >= MAX_CHANNELS) {
		pr_err("%s: Invalid client number\n", __func__);
		return -EINVAL;
	}

	ch_info = kzalloc(sizeof(struct glink_channel), GFP_ATOMIC);
	if (!ch_info)
		return -ENOMEM;

	glink_channels[client_num] = ch_info;

	link_info.link_state = GLINK_LINK_STATE_DOWN;
	link_info.handle = glink_register_link_state_cb(&link_cb_info, NULL);
	if (IS_ERR(link_info.handle)) {
		pr_err("%s: Unable to register link cb %lu\n", __func__,
				PTR_ERR(link_info.handle));
		kfree(ch_info);
		glink_channels[client_num] = NULL;
		return PTR_ERR(link_info.handle);
	}

	glink_ctrl_wq = alloc_ordered_workqueue("glink_ctrl", WQ_MEM_RECLAIM);
	if (!glink_ctrl_wq) {
		pr_err("%s: Unable to create workqueue glink_ctrl\n",
				__func__);
		kfree(ch_info);
		glink_channels[client_num] = NULL;
		glink_unregister_link_state_cb(link_info.handle);
		link_info.handle = NULL;
		return -ENOMEM;
	}

	INIT_WORK(&intent_work.work, glink_ctrl_intent_worker);

	ch_info->channel_state =  GLINK_LOCAL_DISCONNECTED;
	ch_info->handle = NULL;

	spin_lock_init(&ch_info->port_lock);
	INIT_LIST_HEAD(&ch_info->tx_q);
	INIT_WORK(&ch_info->write_w, glink_ctrl_write_w);
	INIT_WORK(&ch_info->connect_w, glink_ctrl_connect_w);
	INIT_WORK(&ch_info->disconnect_w, glink_ctrl_disconnect_w);
	init_completion(&ch_info->close_done);
	*port_idx = client_num;

	return 0;
}

#if defined(CONFIG_DEBUG_FS)
#define DEBUG_BUF_SIZE	1024
static ssize_t glink_ctrl_read_stats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct glink_channel	*c;
	char			*buf;
	unsigned long		flags;
	int			ret;
	int			i;
	int			temp = 0;

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < MAX_CHANNELS; i++) {
		if (!glink_channels[i] || !glink_channels[i]->port)
			continue;

		c = glink_channels[i];
		spin_lock_irqsave(&c->port_lock, flags);

		temp += scnprintf(buf + temp, DEBUG_BUF_SIZE - temp,
				"#CHANNEL:%d CHANNEL:%s ctrl_ch:%pK#\n"
				"to_usbhost: %lu\n"
				"to_modem:   %lu\n"
				"Tx_q_count: %u\n"
				"DTR:        %s\n"
				"LINK UP:    %d\n"
				"ch_open:    %d\n"
				"ch_ready:   %d\n",
				i, c->open_cfg.name, c->handle,
				c->to_host, c->to_modem, c->tx_q_count,
				c->cbits_to_modem ? "HIGH" : "LOW",
				(link_info.link_state) ? 0 : 1,
				(c->handle) ? 1 : 0,
				(c->channel_state == GLINK_CONNECTED) ? 1 : 0);

		spin_unlock_irqrestore(&c->port_lock, flags);
	}

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, temp);

	kfree(buf);

	return ret;
}

static ssize_t glink_ctrl_reset_stats(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct glink_channel	*c;
	int			i;
	unsigned long		flags;

	for (i = 0; i < MAX_CHANNELS; i++) {
		if (!glink_channels[i] || !glink_channels[i]->port)
			continue;

		c = glink_channels[i];

		spin_lock_irqsave(&c->port_lock, flags);

		c->to_host = 0;
		c->to_modem = 0;

		spin_unlock_irqrestore(&c->port_lock, flags);
	}
	return count;
}

const struct file_operations glink_ctrl_stats_ops = {
	.read = glink_ctrl_read_stats,
	.write = glink_ctrl_reset_stats,
};

static struct dentry *glink_ctrl_dent;
static struct dentry *glink_ctrl_dfile;
static void glink_ctrl_debugfs_init(void)
{
	glink_ctrl_dent = debugfs_create_dir("usb_glink_ctrl", 0);
	if (IS_ERR(glink_ctrl_dent))
		return;

	glink_ctrl_dfile = debugfs_create_file("status", 0444, glink_ctrl_dent,
						0, &glink_ctrl_stats_ops);
	if (!glink_ctrl_dfile || IS_ERR(glink_ctrl_dfile))
		debugfs_remove(glink_ctrl_dent);
}

static void glink_ctrl_debugfs_exit(void)
{
	debugfs_remove(glink_ctrl_dfile);
	debugfs_remove(glink_ctrl_dent);
}

#else
static void glink_ctrl_debugfs_init(void) { }
static void glink_ctrl_debugfs_exit(void) { }
#endif

static int __init glink_ctrl_init(void)
{
	glink_ctrl_debugfs_init();

	return 0;
}
module_init(glink_ctrl_init);

static void __exit glink_ctrl_exit(void)
{
	glink_ctrl_debugfs_exit();
}
module_exit(glink_ctrl_exit);
