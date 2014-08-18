/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/srcu.h>
#include <linux/termios.h>
#include <linux/workqueue.h>
#include <soc/qcom/smd.h>
#include <soc/qcom/glink.h>
#include "glink_core_if.h"
#include "glink_xprt_if.h"

#define NUM_EDGES 5
#define XPRT_NAME "smd_trans"
#define SMD_DTR_SIG BIT(31)
#define SMD_CTS_SIG BIT(30)
#define SMD_CD_SIG BIT(29)
#define SMD_RI_SIG BIT(28)

/**
 * struct edge_info() - local information for managing an edge
 * @xprt_if:	The transport interface registered with the glink code
 *		associated with this edge.
 * @xprt_cfg:	The transport configuration for the glink core associated with
 *		this edge.
 * @smd_edge:	The smd edge value corresponding to this edge.
 * @channels:	A list of all the channels that currently exist on this edge.
 * @intentless:	Flag indicating this edge is intentless.
 * @ssr_sync:	Synchronizes SSR with any ongoing activity that might conflict.
 * @in_ssr:	Prevents new activity that might conflict with an active SSR.
 * @ssr_work:	Ends SSR processing after giving SMD a chance to wrap up SSR.
 *
 * Each transport registered with the core is represented by a single instance
 * of this structure which allows for complete management of the transport.
 */
struct edge_info {
	struct glink_transport_if xprt_if;
	struct glink_core_transport_cfg xprt_cfg;
	uint32_t smd_edge;
	struct list_head channels;
	bool intentless;
	struct srcu_struct ssr_sync;
	bool in_ssr;
	struct delayed_work ssr_work;
};

/**
 * struct channel() - local information for managing a channel
 * @node:		For chaining this channel on list for its edge.
 * @name:		The name of this channel.
 * @lcid:		The local channel id the core uses for this channel.
 * @pdrv:		The platform driver for the smd device for this channel.
 * @edge:		Handle to the edge_info this channel is associated with.
 * @smd_ch:		Handle to the underlying smd channel.
 * @intents:		List of active intents on this channel.
 * @intents_lock:	Lock to protect @intents.
 * @next_intent_id:	The next id to use for generated intents.
 * @wq:			Handle for running tasks.
 * @work:		Task to process received data.
 * @cur_intent:		The current intent for received data.
 * @intent_req:		Flag indicating if an intent has been requested for rx.
 * @is_closing:		Flag indicating this channel is currently in the closing
 *			state.
 */
struct channel {
	struct list_head node;
	char name[GLINK_NAME_SIZE];
	uint32_t lcid;
	struct platform_driver pdrv;
	struct edge_info *edge;
	smd_channel_t *smd_ch;
	struct list_head intents;
	struct mutex intents_lock;
	uint32_t next_intent_id;
	struct workqueue_struct *wq;
	struct work_struct work;
	struct intent_info *cur_intent;
	bool intent_req;
	bool is_closing;
};

/**
 * struct intent_info() - information for managing an intent
 * @node:	Used for putting this intent in a list for its channel.
 * @llid:	The local intent id the core uses to identify this intent.
 * @size:	The size of the intent in bytes.
 */
struct intent_info {
	struct list_head node;
	uint32_t liid;
	size_t size;
};

/**
 * struct channel_work() - a task to be processed for a specific channel
 * @ch:		The channel associated with this task.
 * @iid:	Intent id associated with this task, may not always be valid.
 * @work:	The task to be processed.
 */
struct channel_work {
	struct channel *ch;
	uint32_t iid;
	struct work_struct work;
};

static uint32_t negotiate_features_v1(struct glink_transport_if *if_ptr,
				      const struct glink_core_version *version,
				      uint32_t features);

static struct edge_info edge_infos[NUM_EDGES] = {
	{
		.xprt_cfg.edge = "dsps",
		.smd_edge = SMD_APPS_DSPS,
	},
	{
		.xprt_cfg.edge = "lpass",
		.smd_edge = SMD_APPS_QDSP,
	},
	{
		.xprt_cfg.edge = "mpss",
		.smd_edge = SMD_APPS_MODEM,
	},
	{
		.xprt_cfg.edge = "wcnss",
		.smd_edge = SMD_APPS_WCNSS,
	},
	{
		.xprt_cfg.edge = "rpm",
		.smd_edge = SMD_APPS_RPM,
		.intentless = true,
	},
};

static struct glink_core_version versions[] = {
	{1, 0x00, negotiate_features_v1},
};

/**
 * ssr_work_func() - process the end of ssr
 * @work:	The ssr task to finish.
 */
static void ssr_work_func(struct work_struct *work)
{
	struct delayed_work *w;
	struct edge_info *einfo;

	w = container_of(work, struct delayed_work, work);
	einfo = container_of(w, struct edge_info, ssr_work);
	einfo->in_ssr = false;
	einfo->xprt_if.glink_core_if_ptr->link_up(&einfo->xprt_if);
}

/**
 * process_tx_done() - process a tx done task
 * @work:	The tx done task to process.
 */
static void process_tx_done(struct work_struct *work)
{
	struct channel_work *ch_work;
	struct channel *ch;
	struct edge_info *einfo;
	uint32_t riid;

	ch_work = container_of(work, struct channel_work, work);
	ch = ch_work->ch;
	riid = ch_work->iid;
	einfo = ch->edge;
	kfree(ch_work);
	einfo->xprt_if.glink_core_if_ptr->rx_cmd_tx_done(&einfo->xprt_if,
								ch->lcid,
								riid);
}

/**
 * process_open_event() - process an open event task
 * @work:	The open task to process.
 */
static void process_open_event(struct work_struct *work)
{
	struct channel_work *ch_work;
	struct channel *ch;
	struct edge_info *einfo;

	ch_work = container_of(work, struct channel_work, work);
	ch = ch_work->ch;
	einfo = ch->edge;
	kfree(ch_work);
	einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_open_ack(&einfo->xprt_if,
								ch->lcid);
}

/**
 * process_close_event() - process a close event task
 * @work:	The close task to process.
 */
static void process_close_event(struct work_struct *work)
{
	struct channel_work *ch_work;
	struct channel *ch;
	struct edge_info *einfo;

	ch_work = container_of(work, struct channel_work, work);
	ch = ch_work->ch;
	einfo = ch->edge;
	kfree(ch_work);
	einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_remote_close(
								&einfo->xprt_if,
								ch->lcid);
}

/**
 * process_status_event() - process a status event task
 * @work:	The status task to process.
 */
static void process_status_event(struct work_struct *work)
{
	struct channel_work *ch_work;
	struct channel *ch;
	struct edge_info *einfo;
	uint32_t sigs = 0;
	int set;

	ch_work = container_of(work, struct channel_work, work);
	ch = ch_work->ch;
	einfo = ch->edge;
	kfree(ch_work);

	set = smd_tiocmget(ch->smd_ch);
	if (set < 0)
		return;

	if (set & TIOCM_DTR)
		sigs |= SMD_DTR_SIG;
	if (set & TIOCM_RTS)
		sigs |= SMD_CTS_SIG;
	if (set & TIOCM_CD)
		sigs |= SMD_CD_SIG;
	if (set & TIOCM_RI)
		sigs |= SMD_RI_SIG;

	einfo->xprt_if.glink_core_if_ptr->rx_cmd_remote_sigs(&einfo->xprt_if,
								ch->lcid,
								sigs);
}

/**
 * process_reopen_event() - process a reopen ready event task
 * @work:	The reopen ready task to process.
 */
static void process_reopen_event(struct work_struct *work)
{
	struct channel_work *ch_work;
	struct channel *ch;
	struct edge_info *einfo;

	ch_work = container_of(work, struct channel_work, work);
	ch = ch_work->ch;
	einfo = ch->edge;
	kfree(ch_work);
	einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_remote_close(
								&einfo->xprt_if,
								ch->lcid);
	einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_close_ack(&einfo->xprt_if,
								ch->lcid);
}

/**
 * process_data_event() - process a data event task
 * @work:	The data task to process.
 */
static void process_data_event(struct work_struct *work)
{
	struct channel *ch;
	struct edge_info *einfo;
	struct glink_core_rx_intent *intent;
	int pkt_size;
	int read_avail;
	struct intent_info *i;
	uint32_t liid;

	ch = container_of(work, struct channel, work);
	einfo = ch->edge;

	while (!ch->is_closing && smd_read_avail(ch->smd_ch)) {
		pkt_size = smd_cur_packet_size(ch->smd_ch);
		if (!ch->cur_intent && !einfo->intentless) {
			mutex_lock(&ch->intents_lock);
			list_for_each_entry(i, &ch->intents, node) {
				if (i->size >= pkt_size) {
					list_del(&i->node);
					ch->cur_intent = i;
					break;
				}
			}
			mutex_unlock(&ch->intents_lock);
			if (!ch->cur_intent) {
				ch->intent_req = true;
				einfo->xprt_if.glink_core_if_ptr->
						rx_cmd_remote_rx_intent_req(
								&einfo->xprt_if,
								ch->lcid,
								pkt_size);
				return;
			}
		}

		liid = einfo->intentless ? 0 : ch->cur_intent->liid;
		read_avail = smd_read_avail(ch->smd_ch);
		intent = einfo->xprt_if.glink_core_if_ptr->rx_get_pkt_ctx(
							&einfo->xprt_if,
							ch->lcid,
							liid);
		if (!intent->data && einfo->intentless) {
			intent->data = kmalloc(pkt_size, GFP_KERNEL);
			if (!intent->data)
				continue;
		}
		smd_read(ch->smd_ch, intent->data + intent->write_offset,
								read_avail);
		intent->write_offset += read_avail;
		intent->pkt_size += read_avail;
		if (read_avail == pkt_size && !einfo->intentless) {
			kfree(ch->cur_intent);
			ch->cur_intent = NULL;
		}
		einfo->xprt_if.glink_core_if_ptr->rx_put_pkt_ctx(
							&einfo->xprt_if,
							ch->lcid,
							intent,
							read_avail == pkt_size);
	}
}

/**
 * smd_notify() - process an event from the smd channel
 * @priv:	The channel the event occurred on.
 * @event:	The event to process
 */
static void smd_notify(void *priv, unsigned event)
{
	struct channel *ch = priv;
	struct channel_work *work;

	switch (event) {
	case SMD_EVENT_DATA:
		queue_work(ch->wq, &ch->work);
		break;
	case SMD_EVENT_OPEN:
		work = kmalloc(sizeof(*work), GFP_ATOMIC);
		if (!work) {
			pr_err("%s: unable to process event %d\n", __func__,
								SMD_EVENT_OPEN);
			return;
		}
		work->ch = ch;
		INIT_WORK(&work->work, process_open_event);
		queue_work(ch->wq, &work->work);
		break;
	case SMD_EVENT_CLOSE:
		work = kmalloc(sizeof(*work), GFP_ATOMIC);
		if (!work) {
			pr_err("%s: unable to process event %d\n", __func__,
							SMD_EVENT_CLOSE);
			return;
		}
		work->ch = ch;
		INIT_WORK(&work->work, process_close_event);
		queue_work(ch->wq, &work->work);
		break;
	case SMD_EVENT_STATUS:
		work = kmalloc(sizeof(*work), GFP_ATOMIC);
		if (!work) {
			pr_err("%s: unable to process event %d\n", __func__,
							SMD_EVENT_STATUS);
			return;
		}
		work->ch = ch;
		INIT_WORK(&work->work, process_status_event);
		queue_work(ch->wq, &work->work);
		break;
	case SMD_EVENT_REOPEN_READY:
		work = kmalloc(sizeof(*work), GFP_ATOMIC);
		if (!work) {
			pr_err("%s: unable to process event %d\n", __func__,
							SMD_EVENT_REOPEN_READY);
			return;
		}
		work->ch = ch;
		INIT_WORK(&work->work, process_reopen_event);
		queue_work(ch->wq, &work->work);
		break;
	}
}

static int channel_probe(struct platform_device *pdev)
{
	struct channel *ch;
	struct platform_driver *drv;
	struct edge_info *einfo;

	drv = container_of(pdev->dev.driver, struct platform_driver, driver);
	ch = container_of(drv, struct channel, pdrv);
	einfo = ch->edge;

	smd_named_open_on_edge(ch->name, einfo->smd_edge, &ch->smd_ch, ch,
								smd_notify);
	smd_disable_read_intr(ch->smd_ch);
	einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_remote_open(&einfo->xprt_if,
								ch->lcid,
								ch->name);

	return 0;
}

/**
 * tx_cmd_version() - convert a version cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @version:	The version number to encode.
 * @features:	The features information to encode.
 *
 * The remote side doesn't speak G-Link, so we fake the version negotiation.
 */
static void tx_cmd_version(struct glink_transport_if *if_ptr, uint32_t version,
			   uint32_t features)
{
	struct edge_info *einfo;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);
	einfo->xprt_if.glink_core_if_ptr->rx_cmd_version_ack(&einfo->xprt_if,
								version,
								features);
	einfo->xprt_if.glink_core_if_ptr->rx_cmd_version(&einfo->xprt_if,
								version,
								features);
}

/**
 * tx_cmd_version_ack() - convert a version ack cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @version:	The version number to encode.
 * @features:	The features information to encode.
 *
 * The remote side doesn't speak G-Link.  The core is acking a version command
 * we faked.  Do nothing.
 */
static void tx_cmd_version_ack(struct glink_transport_if *if_ptr,
			       uint32_t version,
			       uint32_t features)
{
}

/**
 * set_version() - activate a negotiated version and feature set
 * @if_ptr:	The transport to configure.
 * @version:	The version to use.
 * @features:	The features to use.
 *
 * Return: The supported capabilities of the transport.
 */
static uint32_t set_version(struct glink_transport_if *if_ptr, uint32_t version,
			uint32_t features)
{
	struct edge_info *einfo;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	return einfo->intentless ?
				GCAP_INTENTLESS | GCAP_SIGNALS : GCAP_SIGNALS;
}

/**
 * tx_cmd_ch_open() - convert a channel open cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @name:	The channel name to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_ch_open(struct glink_transport_if *if_ptr, uint32_t lcid,
			  const char *name)
{
	struct edge_info *einfo;
	struct channel *ch;
	bool found = false;
	int rcu_id;
	int ret;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->ssr_sync);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->ssr_sync, rcu_id);
		return -EFAULT;
	}

	list_for_each_entry(ch, &einfo->channels, node) {
		if (!strcmp(name, ch->name)) {
			found = true;
			break;
		}
	}

	if (!found) {
		ch = kzalloc(sizeof(*ch), GFP_KERNEL);
		if (!ch) {
			pr_err("%s: channel struct allocation failed\n",
								__func__);
			srcu_read_unlock(&einfo->ssr_sync, rcu_id);
			return -ENOMEM;
		}
		strlcpy(ch->name, name, GLINK_NAME_SIZE);
		ch->pdrv.driver.name = ch->name;
		ch->pdrv.driver.owner = THIS_MODULE;
		ch->pdrv.probe = channel_probe;
		ch->edge = einfo;
		INIT_LIST_HEAD(&ch->intents);
		mutex_init(&ch->intents_lock);
		INIT_WORK(&ch->work, process_data_event);
		ch->wq = create_singlethread_workqueue(ch->name);
		if (!ch->wq) {
			pr_err("%s: channel workqueue create failed\n",
								__func__);
			kfree(ch);
			srcu_read_unlock(&einfo->ssr_sync, rcu_id);
			return -ENOMEM;
		}
		list_add_tail(&ch->node, &einfo->channels);
	}

	ch->lcid = lcid;

	ret = platform_driver_register(&ch->pdrv);
	srcu_read_unlock(&einfo->ssr_sync, rcu_id);
	return ret;
}

/**
 * tx_cmd_ch_close() - convert a channel close cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_ch_close(struct glink_transport_if *if_ptr, uint32_t lcid)
{
	struct edge_info *einfo;
	struct channel *ch;
	struct intent_info *intent;
	int rcu_id;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->ssr_sync);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->ssr_sync, rcu_id);
		return -EFAULT;
	}

	list_for_each_entry(ch, &einfo->channels, node) {
		if (lcid == ch->lcid)
			break;
	}

	ch->is_closing = true;
	flush_workqueue(ch->wq);
	smd_close(ch->smd_ch);
	ch->smd_ch = NULL;
	platform_driver_unregister(&ch->pdrv);

	mutex_lock(&ch->intents_lock);
	while (!list_empty(&ch->intents)) {
		intent = list_first_entry(&ch->intents, struct intent_info,
									node);
		list_del(&intent->node);
		kfree(intent);
	}
	mutex_unlock(&ch->intents_lock);
	ch->is_closing = false;

	srcu_read_unlock(&einfo->ssr_sync, rcu_id);
	return 0;
}

/**
 * tx_cmd_ch_remote_open_ack() - convert a channel open ack cmd to wire format
 *				 and transmit
 * @if_ptr:	The transport to transmit on.
 * @rcid:	The remote channel id to encode.
 *
 * The remote side doesn't speak G-Link.  The core is acking an open command
 * we faked.  Do nothing.
 */
static void tx_cmd_ch_remote_open_ack(struct glink_transport_if *if_ptr,
				      uint32_t rcid)
{
}

/**
 * tx_cmd_ch_remote_close_ack() - convert a channel close ack cmd to wire format
 *				  and transmit
 * @if_ptr:	The transport to transmit on.
 * @rcid:	The remote channel id to encode.
 *
 * The remote side doesn't speak G-Link.  The core is acking a close command
 * we faked.  Do nothing.
 */
static void tx_cmd_ch_remote_close_ack(struct glink_transport_if *if_ptr,
				       uint32_t rcid)
{
}

/**
 * ssr() - process a subsystem restart notification of a transport
 * @if_ptr:	The transport to restart.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int ssr(struct glink_transport_if *if_ptr)
{
	struct edge_info *einfo;
	struct channel *ch;
	struct intent_info *intent;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	einfo->in_ssr = true;
	synchronize_srcu(&einfo->ssr_sync);

	list_for_each_entry(ch, &einfo->channels, node) {
		if (!ch->smd_ch)
			continue;
		ch->is_closing = true;
		flush_workqueue(ch->wq);
		smd_close(ch->smd_ch);
		ch->smd_ch = NULL;
		platform_driver_unregister(&ch->pdrv);

		mutex_lock(&ch->intents_lock);
		while (!list_empty(&ch->intents)) {
			intent = list_first_entry(&ch->intents,
							struct intent_info,
							node);
			list_del(&intent->node);
			kfree(intent);
		}
		mutex_unlock(&ch->intents_lock);
		ch->is_closing = false;
	}

	schedule_delayed_work(&einfo->ssr_work, 5 * HZ);
	return 0;
}

/**
 * allocate_rx_intent() - allocate/reserve space for RX Intent
 * @size:	size of intent.
 * @intent:	Pointer to the intent structure.
 *
 * Assign "data" with the buffer created, since the transport creates
 * a linear buffer and "iovec" with the "intent" itself, so that
 * the data can be passed to a client that receives only vector buffer.
 * Note that returning NULL for the pointer is valid (it means that space has
 * been reserved, but the actual pointer will be provided later).
 *
 * Return: 0 on success or standard Linux error code.
 */
static int allocate_rx_intent(size_t size, struct glink_core_rx_intent *intent)
{
	void *t;

	t = kmalloc(size, GFP_KERNEL);
	if (!t)
		return -ENOMEM;

	intent->data = t;
	intent->iovec = (void *)intent;
	intent->vprovider = rx_linear_vbuf_provider;
	intent->pprovider = NULL;
	return 0;
}

/**
 * deallocate_rx_intent() - Deallocate space created for RX Intent
 * @intent:	Pointer to the intent structure.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int deallocate_rx_intent(struct glink_core_rx_intent *intent)
{
	if (!intent || !intent->data)
		return -EINVAL;

	kfree(intent->data);
	intent->data = NULL;
	intent->iovec = NULL;
	intent->vprovider = NULL;
	return 0;
}

/**
 * tx_cmd_local_rx_intent() - convert an rx intent cmd to wire format and
 *			      transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @size:	The intent size to encode.
 * @liid:	The local intent id to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_local_rx_intent(struct glink_transport_if *if_ptr,
				  uint32_t lcid, size_t size, uint32_t liid)
{
	struct edge_info *einfo;
	struct channel *ch;
	struct intent_info *intent;
	int rcu_id;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->ssr_sync);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->ssr_sync, rcu_id);
		return -EFAULT;
	}

	list_for_each_entry(ch, &einfo->channels, node) {
		if (lcid == ch->lcid)
			break;
	}

	intent = kmalloc(sizeof(*intent), GFP_KERNEL);
	if (!intent) {
		pr_err("%s: no memory for intent\n", __func__);
		srcu_read_unlock(&einfo->ssr_sync, rcu_id);
		return -ENOMEM;
	}

	intent->liid = liid;
	intent->size = size;
	mutex_lock(&ch->intents_lock);
	list_add_tail(&intent->node, &ch->intents);
	mutex_unlock(&ch->intents_lock);

	if (ch->intent_req) {
		ch->intent_req = false;
		queue_work(ch->wq, &ch->work);
	}

	srcu_read_unlock(&einfo->ssr_sync, rcu_id);
	return 0;
}

/**
 * tx_cmd_local_rx_done() - convert an rx done cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @liid:	The local intent id to encode.
 *
 * The remote side doesn't speak G-Link, so ignore rx_done.
 */
static void tx_cmd_local_rx_done(struct glink_transport_if *if_ptr,
				 uint32_t lcid, uint32_t liid)
{
}

/**
 * tx() - convert a data transmit cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @pctx:	The data to encode.
 *
 * Return: Number of bytes written or standard Linux error code.
 */
static int tx(struct glink_transport_if *if_ptr, uint32_t lcid,
	      struct glink_core_tx_pkt *pctx)
{
	struct edge_info *einfo;
	struct channel *ch;
	int rc;
	struct channel_work *tx_done;
	const void *data_start;
	size_t tx_size = 0;
	int rcu_id;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->ssr_sync);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->ssr_sync, rcu_id);
		return -EFAULT;
	}

	list_for_each_entry(ch, &einfo->channels, node) {
		if (lcid == ch->lcid)
			break;
	}

	data_start = get_tx_vaddr(pctx, pctx->size - pctx->size_remaining,
				  &tx_size);
	if (!data_start) {
		srcu_read_unlock(&einfo->ssr_sync, rcu_id);
		return -EINVAL;
	}
	if (pctx->size == pctx->size_remaining) {
		rc = smd_write_avail(ch->smd_ch);
		if (!rc) {
			srcu_read_unlock(&einfo->ssr_sync, rcu_id);
			return 0;
		}
		rc = smd_write_start(ch->smd_ch, pctx->size);
		if (rc) {
			srcu_read_unlock(&einfo->ssr_sync, rcu_id);
			return 0;
		}
	}

	rc = smd_write_segment_avail(ch->smd_ch);
	if (!rc) {
		srcu_read_unlock(&einfo->ssr_sync, rcu_id);
		return 0;
	}
	if (rc > tx_size)
		rc = tx_size;
	rc = smd_write_segment(ch->smd_ch, data_start, rc, 0);
	if (rc < 0) {
		pr_err("%s: write segment failed %d\n", __func__, rc);
		srcu_read_unlock(&einfo->ssr_sync, rcu_id);
		return 0;
	}

	pctx->size_remaining -= rc;
	if (!pctx->size_remaining) {
		smd_write_end(ch->smd_ch);
		tx_done = kmalloc(sizeof(*tx_done), GFP_KERNEL);
		tx_done->ch = ch;
		tx_done->iid = pctx->riid;
		INIT_WORK(&tx_done->work, process_tx_done);
		queue_work(ch->wq, &tx_done->work);
	}

	srcu_read_unlock(&einfo->ssr_sync, rcu_id);
	return rc;
}

/**
 * tx_cmd_rx_intent_req() - convert an rx intent request cmd to wire format and
 *			    transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @size:	The requested intent size to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_rx_intent_req(struct glink_transport_if *if_ptr,
				uint32_t lcid, size_t size)
{
	struct edge_info *einfo;
	struct channel *ch;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);
	list_for_each_entry(ch, &einfo->channels, node) {
		if (lcid == ch->lcid)
			break;
	}
	einfo->xprt_if.glink_core_if_ptr->rx_cmd_rx_intent_req_ack(
								&einfo->xprt_if,
								ch->lcid,
								true);
	einfo->xprt_if.glink_core_if_ptr->rx_cmd_remote_rx_intent_put(
							&einfo->xprt_if,
							ch->lcid,
							ch->next_intent_id++,
							size);
	return 0;
}

/**
 * tx_cmd_rx_intent_req_ack() - convert an rx intent request ack cmd to wire
 *				format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @granted:	The request response to encode.
 *
 * The remote side doesn't speak G-Link.  The core is just acking a request we
 * faked.  Do nothing.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_remote_rx_intent_req_ack(struct glink_transport_if *if_ptr,
					   uint32_t lcid, bool granted)
{
	return 0;
}

/**
 * tx_cmd_set_sigs() - convert a signal cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @sigs:	The signals to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_set_sigs(struct glink_transport_if *if_ptr, uint32_t lcid,
			   uint32_t sigs)
{
	struct edge_info *einfo;
	struct channel *ch;
	uint32_t set = 0;
	uint32_t clear = 0;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);
	list_for_each_entry(ch, &einfo->channels, node) {
		if (lcid == ch->lcid)
			break;
	}

	if (sigs & SMD_DTR_SIG)
		set |= TIOCM_DTR;
	else
		clear |= TIOCM_DTR;

	if (sigs & SMD_CTS_SIG)
		set |= TIOCM_RTS;
	else
		clear |= TIOCM_RTS;

	if (sigs & SMD_CD_SIG)
		set |= TIOCM_CD;
	else
		clear |= TIOCM_CD;

	if (sigs & SMD_RI_SIG)
		set |= TIOCM_RI;
	else
		clear |= TIOCM_RI;

	return smd_tiocmset(ch->smd_ch, set, clear);
}

/**
 * poll() - poll for data on a channel
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id for the channel.
 *
 * Return: 0 if no data available, 1 if data available, or standard Linux error
 * code.
 */
static int poll(struct glink_transport_if *if_ptr, uint32_t lcid)
{
	struct edge_info *einfo;
	struct channel *ch;
	int rc;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);
	list_for_each_entry(ch, &einfo->channels, node) {
		if (lcid == ch->lcid)
			break;
	}
	rc = smd_is_pkt_avail(ch->smd_ch);
	if (rc == 1)
		queue_work(ch->wq, &ch->work);
	return rc;
}

/**
 * mask_rx_irq() - mask the receive irq
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id for the channel.
 * @mask:	True to mask the irq, false to unmask.
 * @pstruct:	Platform defined structure for handling the masking.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int mask_rx_irq(struct glink_transport_if *if_ptr, uint32_t lcid,
		       bool mask, void *pstruct)
{
	struct edge_info *einfo;
	struct channel *ch;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);
	list_for_each_entry(ch, &einfo->channels, node) {
		if (lcid == ch->lcid)
			break;
	}
	return smd_mask_receive_interrupt(ch->smd_ch, mask, pstruct);
}

/**
 * negotiate_features_v1() - determine what features of a version can be used
 * @if_ptr:	The transport for which features are negotiated for.
 * @version:	The version negotiated.
 * @features:	The set of requested features.
 *
 * Return: What set of the requested features can be supported.
 */
static uint32_t negotiate_features_v1(struct glink_transport_if *if_ptr,
				      const struct glink_core_version *version,
				      uint32_t features)
{
	return features & version->features;
}

/**
* init_xprt_if() - initialize the xprt_if for an edge
* @einfo:	The edge to initialize.
*/
static void init_xprt_if(struct edge_info *einfo)
{
	einfo->xprt_if.ssr = ssr;
	einfo->xprt_if.tx_cmd_version = tx_cmd_version;
	einfo->xprt_if.tx_cmd_version_ack = tx_cmd_version_ack;
	einfo->xprt_if.set_version = set_version;
	einfo->xprt_if.tx_cmd_ch_open = tx_cmd_ch_open;
	einfo->xprt_if.tx_cmd_ch_close = tx_cmd_ch_close;
	einfo->xprt_if.tx_cmd_ch_remote_open_ack = tx_cmd_ch_remote_open_ack;
	einfo->xprt_if.tx_cmd_ch_remote_close_ack = tx_cmd_ch_remote_close_ack;
	einfo->xprt_if.allocate_rx_intent = allocate_rx_intent;
	einfo->xprt_if.deallocate_rx_intent = deallocate_rx_intent;
	einfo->xprt_if.tx_cmd_local_rx_intent = tx_cmd_local_rx_intent;
	einfo->xprt_if.tx_cmd_local_rx_done = tx_cmd_local_rx_done;
	einfo->xprt_if.tx = tx;
	einfo->xprt_if.tx_cmd_rx_intent_req = tx_cmd_rx_intent_req;
	einfo->xprt_if.tx_cmd_remote_rx_intent_req_ack =
						tx_cmd_remote_rx_intent_req_ack;
	einfo->xprt_if.tx_cmd_set_sigs = tx_cmd_set_sigs;
	einfo->xprt_if.poll = poll;
	einfo->xprt_if.mask_rx_irq = mask_rx_irq;
}

/**
 * init_xprt_cfg() - initialize the xprt_cfg for an edge
 * @einfo:	The edge to initialize.
 */
static void init_xprt_cfg(struct edge_info *einfo)
{
	einfo->xprt_cfg.name = XPRT_NAME;
	einfo->xprt_cfg.versions = versions;
	einfo->xprt_cfg.versions_entries = ARRAY_SIZE(versions);
	einfo->xprt_cfg.max_cid = SZ_64;
	einfo->xprt_cfg.max_iid = SZ_1;
}

static int __init glink_smd_xprt_init(void)
{
	int i;
	int rc;
	struct edge_info *einfo;

	for (i = 0; i < NUM_EDGES; ++i) {
		einfo = &edge_infos[i];
		init_xprt_cfg(einfo);
		init_xprt_if(einfo);
		INIT_LIST_HEAD(&einfo->channels);
		init_srcu_struct(&einfo->ssr_sync);
		INIT_DELAYED_WORK(&einfo->ssr_work, ssr_work_func);
		rc = glink_core_register_transport(&einfo->xprt_if,
							&einfo->xprt_cfg);
		if (rc)
			pr_err("%s: %s glink register transport failed %d\n",
							__func__,
							einfo->xprt_cfg.edge,
							rc);
		else
			einfo->xprt_if.glink_core_if_ptr->link_up(
							&einfo->xprt_if);
	}

	return 0;
}
module_init(glink_smd_xprt_init);

MODULE_DESCRIPTION("MSM G-Link SMD Transport");
MODULE_LICENSE("GPL v2");
