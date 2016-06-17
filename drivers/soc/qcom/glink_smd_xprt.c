/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/srcu.h>
#include <linux/termios.h>
#include <linux/workqueue.h>
#include <soc/qcom/smd.h>
#include <soc/qcom/glink.h>
#include "glink_core_if.h"
#include "glink_private.h"
#include "glink_xprt_if.h"

#define NUM_EDGES 5
#define XPRT_NAME "smd_trans"
#define SMD_DTR_SIG BIT(31)
#define SMD_CTS_SIG BIT(30)
#define SMD_CD_SIG BIT(29)
#define SMD_RI_SIG BIT(28)

/**
 * enum command_types - commands send/received from remote system
 * @CMD_OPEN:		Channel open request
 * @CMD_OPEN_ACK:	Response to @CMD_OPEN
 * @CMD_CLOSE:		Channel close request
 * @CMD_CLOSE_ACK:	Response to @CMD_CLOSE
 */
enum command_types {
	CMD_OPEN,
	CMD_OPEN_ACK,
	CMD_CLOSE,
	CMD_CLOSE_ACK,
};

/*
 * Max of 64 channels, the 128 offset puts the rcid out of the
 * range the remote might use
 */
#define LEGACY_RCID_CHANNEL_OFFSET	128

#define SMDXPRT_ERR(einfo, x...) GLINK_XPRT_IF_ERR(einfo->xprt_if, x)
#define SMDXPRT_INFO(einfo, x...) GLINK_XPRT_IF_INFO(einfo->xprt_if, x)
#define SMDXPRT_DBG(einfo, x...) GLINK_XPRT_IF_DBG(einfo->xprt_if, x)

/**
 * struct edge_info() - local information for managing an edge
 * @xprt_if:		The transport interface registered with the glink code
 *			associated with this edge.
 * @xprt_cfg:		The transport configuration for the glink core
 *			associated with this edge.
 * @smd_edge:		The smd edge value corresponding to this edge.
 * @channels:		A list of all the channels that currently exist on this
 *			edge.
 * @channels_lock:	Protects @channels "reads" from "writes".
 * @intentless:		Flag indicating this edge is intentless.
 * @irq_disabled:	Flag indicating whether interrupt is enabled or
 *			disabled.
 * @ssr_sync:		Synchronizes SSR with any ongoing activity that might
 *			conflict.
 * @in_ssr:		Prevents new activity that might conflict with an active
 *			SSR.
 * @ssr_work:		Ends SSR processing after giving SMD a chance to wrap up
 *			SSR.
 * @smd_ch:		Private SMD channel for channel migration.
 * @smd_lock:		Serializes write access to @smd_ch.
 * @in_ssr_lock:	Lock to protect the @in_ssr.
 * @smd_ctl_ch_open:	Indicates that @smd_ch is fully open.
 * @work:		Work item for processing migration data.
 * @rx_cmd_lock:	The transport interface lock to notify about received
 *			commands in a sequential manner.
 *
 * Each transport registered with the core is represented by a single instance
 * of this structure which allows for complete management of the transport.
 */
struct edge_info {
	struct glink_transport_if xprt_if;
	struct glink_core_transport_cfg xprt_cfg;
	uint32_t smd_edge;
	struct list_head channels;
	spinlock_t channels_lock;
	bool intentless;
	bool irq_disabled;
	struct srcu_struct ssr_sync;
	bool in_ssr;
	struct delayed_work ssr_work;
	smd_channel_t *smd_ch;
	struct mutex smd_lock;
	struct mutex in_ssr_lock;
	bool smd_ctl_ch_open;
	struct work_struct work;
	struct mutex rx_cmd_lock;
};

/**
 * struct channel() - local information for managing a channel
 * @node:		For chaining this channel on list for its edge.
 * @name:		The name of this channel.
 * @lcid:		The local channel id the core uses for this channel.
 * @rcid:		The true remote channel id for this channel.
 * @ch_probe_lock:	Lock to protect channel probe status.
 * @wait_for_probe:	This channel is waiting for a probe from SMD.
 * @had_probed:		This channel probed in the past and may skip probe.
 * @edge:		Handle to the edge_info this channel is associated with.
 * @smd_ch:		Handle to the underlying smd channel.
 * @intents:		List of active intents on this channel.
 * @used_intents:	List of consumed intents on this channel.
 * @intents_lock:	Lock to protect @intents and @used_intents.
 * @next_intent_id:	The next id to use for generated intents.
 * @wq:			Handle for running tasks.
 * @work:		Task to process received data.
 * @cur_intent:		The current intent for received data.
 * @intent_req:		Flag indicating if an intent has been requested for rx.
 * @is_closing:		Flag indicating this channel is currently in the closing
 *			state.
 * @local_legacy:	The local side of the channel is in legacy mode.
 * @remote_legacy:	The remote side of the channel is in legacy mode.
 * @rx_data_lock:	Used to serialize RX data processing.
 * @streaming_ch:	Indicates the underlying SMD channel is streaming type.
 * @tx_resume_needed:	Indicates whether a tx_resume call should be triggered.
 */
struct channel {
	struct list_head node;
	char name[GLINK_NAME_SIZE];
	uint32_t lcid;
	uint32_t rcid;
	struct mutex ch_probe_lock;
	bool wait_for_probe;
	bool had_probed;
	struct edge_info *edge;
	smd_channel_t *smd_ch;
	struct list_head intents;
	struct list_head used_intents;
	spinlock_t intents_lock;
	uint32_t next_intent_id;
	struct workqueue_struct *wq;
	struct work_struct work;
	struct intent_info *cur_intent;
	bool intent_req;
	bool is_closing;
	bool local_legacy;
	bool remote_legacy;
	size_t intent_req_size;
	spinlock_t rx_data_lock;
	bool streaming_ch;
	bool tx_resume_needed;
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

/**
 * struct pdrvs - Tracks a platform driver and its use among channels
 * @node:	For tracking in the pdrv_list.
 * @pdrv:	The platform driver to track.
 */
struct pdrvs {
	struct list_head node;
	struct platform_driver pdrv;
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

static LIST_HEAD(pdrv_list);
static DEFINE_MUTEX(pdrv_list_mutex);

static void process_data_event(struct work_struct *work);
static int add_platform_driver(struct channel *ch);
static void smd_data_ch_close(struct channel *ch);

/**
 * check_write_avail() - Check if there is space to to write on the smd channel,
 *			 and enable the read interrupt if there is not.
 * @check_fn:	The function to use to check if there is space to write
 * @ch:		The channel to check
 *
 * Return: 0 on success or standard Linux error codes.
 */
static int check_write_avail(int (*check_fn)(smd_channel_t *),
			     struct channel *ch)
{
	int rc = check_fn(ch->smd_ch);

	if (rc == 0) {
		ch->tx_resume_needed = true;
		smd_enable_read_intr(ch->smd_ch);
		rc = check_fn(ch->smd_ch);
		if (rc > 0) {
			ch->tx_resume_needed = false;
			smd_disable_read_intr(ch->smd_ch);
		}
	}

	return rc;
}

/**
 * process_ctl_event() - process a control channel event task
 * @work:	The migration task to process.
 */
static void process_ctl_event(struct work_struct *work)
{
	struct command {
		uint32_t cmd;
		uint32_t id;
		uint32_t priority;
	};
	struct command cmd;
	struct edge_info *einfo;
	struct channel *ch;
	struct channel *temp_ch;
	int pkt_size;
	int read_avail;
	char name[GLINK_NAME_SIZE];
	bool found;
	unsigned long flags;

	einfo = container_of(work, struct edge_info, work);

	mutex_lock(&einfo->in_ssr_lock);
	if (einfo->in_ssr) {
		einfo->in_ssr = false;
		einfo->xprt_if.glink_core_if_ptr->link_up(&einfo->xprt_if);
	}
	mutex_unlock(&einfo->in_ssr_lock);

	while (smd_read_avail(einfo->smd_ch)) {
		found = false;
		pkt_size = smd_cur_packet_size(einfo->smd_ch);
		read_avail = smd_read_avail(einfo->smd_ch);

		if (pkt_size != read_avail)
			continue;

		smd_read(einfo->smd_ch, &cmd, sizeof(cmd));
		if (cmd.cmd == CMD_OPEN) {
			smd_read(einfo->smd_ch, name, GLINK_NAME_SIZE);
			SMDXPRT_INFO(einfo, "%s RX OPEN '%s'\n",
					__func__, name);

			spin_lock_irqsave(&einfo->channels_lock, flags);
			list_for_each_entry(ch, &einfo->channels, node) {
				if (!strcmp(name, ch->name)) {
					found = true;
					break;
				}
			}
			spin_unlock_irqrestore(&einfo->channels_lock, flags);

			if (!found) {
				ch = kzalloc(sizeof(*ch), GFP_KERNEL);
				if (!ch) {
					SMDXPRT_ERR(einfo,
						"%s: ch alloc failed\n",
						__func__);
					continue;
				}
				strlcpy(ch->name, name, GLINK_NAME_SIZE);
				ch->edge = einfo;
				mutex_init(&ch->ch_probe_lock);
				INIT_LIST_HEAD(&ch->intents);
				INIT_LIST_HEAD(&ch->used_intents);
				spin_lock_init(&ch->intents_lock);
				spin_lock_init(&ch->rx_data_lock);
				INIT_WORK(&ch->work, process_data_event);
				ch->wq = create_singlethread_workqueue(
								ch->name);
				if (!ch->wq) {
					SMDXPRT_ERR(einfo,
						"%s: ch wq create failed\n",
						__func__);
					kfree(ch);
					continue;
				}

				/*
				 * Channel could have been added to the list by
				 * someone else so scan again.  Channel creation
				 * is non-atomic, so unlock and recheck is
				 * necessary
				 */
				temp_ch = ch;
				spin_lock_irqsave(&einfo->channels_lock, flags);
				list_for_each_entry(ch, &einfo->channels, node)
					if (!strcmp(name, ch->name)) {
						found = true;
						break;
					}

				if (!found) {
					ch = temp_ch;
					list_add_tail(&ch->node,
							&einfo->channels);
					spin_unlock_irqrestore(
						&einfo->channels_lock, flags);
				} else {
					spin_unlock_irqrestore(
						&einfo->channels_lock, flags);
					destroy_workqueue(temp_ch->wq);
					kfree(temp_ch);
				}
			}

			if (ch->remote_legacy) {
				SMDXPRT_DBG(einfo, "%s SMD Remote Open '%s'\n",
						__func__, name);
				cmd.cmd = CMD_OPEN_ACK;
				cmd.priority = SMD_TRANS_XPRT_ID;
				mutex_lock(&einfo->smd_lock);
				while (smd_write_avail(einfo->smd_ch) <
								sizeof(cmd))
					msleep(20);
				smd_write(einfo->smd_ch, &cmd, sizeof(cmd));
				mutex_unlock(&einfo->smd_lock);
				continue;
			} else {
				SMDXPRT_DBG(einfo,
						"%s G-Link Remote Open '%s'\n",
						__func__, name);
			}

			ch->rcid = cmd.id;
			mutex_lock(&einfo->rx_cmd_lock);
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_remote_open(
								&einfo->xprt_if,
								cmd.id,
								name,
								cmd.priority);
			mutex_unlock(&einfo->rx_cmd_lock);
		} else if (cmd.cmd == CMD_OPEN_ACK) {
			SMDXPRT_INFO(einfo,
				"%s RX OPEN ACK lcid %u; xprt_req %u\n",
				__func__, cmd.id, cmd.priority);

			spin_lock_irqsave(&einfo->channels_lock, flags);
			list_for_each_entry(ch, &einfo->channels, node)
				if (cmd.id == ch->lcid) {
					found = true;
					break;
				}
			spin_unlock_irqrestore(&einfo->channels_lock, flags);
			if (!found) {
				SMDXPRT_ERR(einfo, "%s No channel match %u\n",
						__func__, cmd.id);
				continue;
			}

			add_platform_driver(ch);
			mutex_lock(&einfo->rx_cmd_lock);
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_open_ack(
								&einfo->xprt_if,
								cmd.id,
								cmd.priority);
			mutex_unlock(&einfo->rx_cmd_lock);
		} else if (cmd.cmd == CMD_CLOSE) {
			SMDXPRT_INFO(einfo, "%s RX REMOTE CLOSE rcid %u\n",
					__func__, cmd.id);
			spin_lock_irqsave(&einfo->channels_lock, flags);
			list_for_each_entry(ch, &einfo->channels, node)
				if (cmd.id == ch->rcid) {
					found = true;
					break;
				}
			spin_unlock_irqrestore(&einfo->channels_lock, flags);

			if (!found)
				SMDXPRT_ERR(einfo, "%s no matching rcid %u\n",
						__func__, cmd.id);

			if (found && !ch->remote_legacy) {
				mutex_lock(&einfo->rx_cmd_lock);
				einfo->xprt_if.glink_core_if_ptr->
							rx_cmd_ch_remote_close(
								&einfo->xprt_if,
								cmd.id);
				mutex_unlock(&einfo->rx_cmd_lock);
			} else {
				/* not found or a legacy channel */
				SMDXPRT_INFO(einfo,
						"%s Sim RX CLOSE ACK lcid %u\n",
						__func__, cmd.id);
				cmd.cmd = CMD_CLOSE_ACK;
				mutex_lock(&einfo->smd_lock);
				while (smd_write_avail(einfo->smd_ch) <
								sizeof(cmd))
					msleep(20);
				smd_write(einfo->smd_ch, &cmd, sizeof(cmd));
				mutex_unlock(&einfo->smd_lock);
			}
		} else if (cmd.cmd == CMD_CLOSE_ACK) {
			int rcu_id;

			SMDXPRT_INFO(einfo, "%s RX CLOSE ACK lcid %u\n",
					__func__, cmd.id);

			spin_lock_irqsave(&einfo->channels_lock, flags);
			list_for_each_entry(ch, &einfo->channels, node) {
				if (cmd.id == ch->lcid) {
					found = true;
					break;
				}
			}
			spin_unlock_irqrestore(&einfo->channels_lock, flags);
			if (!found) {
				SMDXPRT_ERR(einfo, "%s LCID not found %u\n",
						__func__, cmd.id);
				continue;
			}

			rcu_id = srcu_read_lock(&einfo->ssr_sync);
			smd_data_ch_close(ch);
			srcu_read_unlock(&einfo->ssr_sync, rcu_id);
			mutex_lock(&einfo->rx_cmd_lock);
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_close_ack(
								&einfo->xprt_if,
								cmd.id);
			mutex_unlock(&einfo->rx_cmd_lock);
		}
	}
}

/**
 * ctl_ch_notify() - process an event from the smd channel for ch migration
 * @priv:	The edge the event occurred on.
 * @event:	The event to process
 */
static void ctl_ch_notify(void *priv, unsigned event)
{
	struct edge_info *einfo = priv;

	switch (event) {
	case SMD_EVENT_DATA:
		schedule_work(&einfo->work);
		break;
	case SMD_EVENT_OPEN:
		einfo->smd_ctl_ch_open = true;
		break;
	case SMD_EVENT_CLOSE:
		einfo->smd_ctl_ch_open = false;
		break;
	}
}

static int ctl_ch_probe(struct platform_device *pdev)
{
	int i;
	struct edge_info *einfo;
	int ret = 0;

	for (i = 0; i < NUM_EDGES; ++i)
		if (pdev->id == edge_infos[i].smd_edge)
			break;

	einfo = &edge_infos[i];
	ret = smd_named_open_on_edge("GLINK_CTRL", einfo->smd_edge,
			&einfo->smd_ch, einfo, ctl_ch_notify);
	if (ret != 0)
		SMDXPRT_ERR(einfo,
			"%s Opening failed %d for %d:'GLINK_CTRL'\n",
			__func__, ret, einfo->smd_edge);
	return ret;
}

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

	mutex_lock(&einfo->in_ssr_lock);
	if (einfo->in_ssr) {
		einfo->in_ssr = false;
		einfo->xprt_if.glink_core_if_ptr->link_up(&einfo->xprt_if);
	}
	mutex_unlock(&einfo->in_ssr_lock);
}

/**
 * deferred_close_ack() - Generate a deferred channel close ack
 * @work:	The channel close ack work to generate.
 */
static void deferred_close_ack(struct work_struct *work)
{
	struct channel_work *ch_work;
	struct channel *ch;

	ch_work = container_of(work, struct channel_work, work);
	ch = ch_work->ch;
	mutex_lock(&ch->edge->rx_cmd_lock);
	ch->edge->xprt_if.glink_core_if_ptr->rx_cmd_ch_close_ack(
				&ch->edge->xprt_if, ch->lcid);
	mutex_unlock(&ch->edge->rx_cmd_lock);
	kfree(ch_work);
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
								ch->rcid,
								riid,
								false);
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
	int ret;

	ch_work = container_of(work, struct channel_work, work);
	ch = ch_work->ch;
	einfo = ch->edge;
	/*
	 * The SMD client is supposed to already know its channel type, but we
	 * are just a translation layer, so we need to dynamically detect the
	 * channel type.
	 */
	ret = smd_write_segment_avail(ch->smd_ch);
	if (ret == -ENODEV)
		ch->streaming_ch = true;
	if (ch->remote_legacy || !ch->rcid) {
		ch->remote_legacy = true;
		ch->rcid = ch->lcid + LEGACY_RCID_CHANNEL_OFFSET;
		mutex_lock(&einfo->rx_cmd_lock);
		einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_remote_open(
							&einfo->xprt_if,
							ch->rcid,
							ch->name,
							SMD_TRANS_XPRT_ID);
		mutex_unlock(&einfo->rx_cmd_lock);
	}
	kfree(ch_work);
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
	if (ch->remote_legacy) {
		mutex_lock(&einfo->rx_cmd_lock);
		einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_remote_close(
								&einfo->xprt_if,
								ch->rcid);
		mutex_unlock(&einfo->rx_cmd_lock);
	}
	ch->rcid = 0;
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
								ch->rcid,
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
	if (ch->remote_legacy) {
		mutex_lock(&einfo->rx_cmd_lock);
		einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_remote_close(
								&einfo->xprt_if,
								ch->rcid);
		mutex_unlock(&einfo->rx_cmd_lock);
	}
	if (ch->local_legacy) {
		mutex_lock(&einfo->rx_cmd_lock);
		einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_close_ack(
								&einfo->xprt_if,
								ch->lcid);
		mutex_unlock(&einfo->rx_cmd_lock);
	}
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
	int pkt_remaining;
	int read_avail;
	struct intent_info *i;
	uint32_t liid;
	unsigned long intents_flags;
	unsigned long rx_data_flags;

	ch = container_of(work, struct channel, work);
	einfo = ch->edge;

	if (ch->tx_resume_needed && smd_write_avail(ch->smd_ch) > 0) {
		ch->tx_resume_needed = false;
		smd_disable_read_intr(ch->smd_ch);
		einfo->xprt_if.glink_core_if_ptr->tx_resume(&einfo->xprt_if);
	}

	spin_lock_irqsave(&ch->rx_data_lock, rx_data_flags);
	while (!ch->is_closing && smd_read_avail(ch->smd_ch)) {
		if (!ch->streaming_ch)
			pkt_remaining = smd_cur_packet_size(ch->smd_ch);
		else
			pkt_remaining = smd_read_avail(ch->smd_ch);
		SMDXPRT_DBG(einfo, "%s Reading packet chunk %u '%s' %u:%u\n",
				__func__, pkt_remaining, ch->name, ch->lcid,
				ch->rcid);
		if (!ch->cur_intent && !einfo->intentless) {
			spin_lock_irqsave(&ch->intents_lock, intents_flags);
			ch->intent_req = true;
			ch->intent_req_size = pkt_remaining;
			list_for_each_entry(i, &ch->intents, node) {
				if (i->size >= pkt_remaining) {
					list_del(&i->node);
					ch->cur_intent = i;
					ch->intent_req = false;
					break;
				}
			}
			spin_unlock_irqrestore(&ch->intents_lock,
								intents_flags);
			if (!ch->cur_intent) {
				spin_unlock_irqrestore(&ch->rx_data_lock,
								rx_data_flags);
				SMDXPRT_DBG(einfo,
					"%s Reqesting intent '%s' %u:%u\n",
					__func__, ch->name,
					ch->lcid, ch->rcid);
				einfo->xprt_if.glink_core_if_ptr->
						rx_cmd_remote_rx_intent_req(
								&einfo->xprt_if,
								ch->rcid,
								pkt_remaining);
				return;
			}
		}

		liid = einfo->intentless ? 0 : ch->cur_intent->liid;
		read_avail = smd_read_avail(ch->smd_ch);
		if (ch->streaming_ch && read_avail > pkt_remaining)
			read_avail = pkt_remaining;
		intent = einfo->xprt_if.glink_core_if_ptr->rx_get_pkt_ctx(
							&einfo->xprt_if,
							ch->rcid,
							liid);
		if (!intent->data && einfo->intentless) {
			intent->data = kmalloc(pkt_remaining, GFP_ATOMIC);
			if (!intent->data) {
				SMDXPRT_DBG(einfo,
					"%s kmalloc failed '%s' %u:%u\n",
					__func__, ch->name,
					ch->lcid, ch->rcid);
				continue;
			}
		}
		smd_read(ch->smd_ch, intent->data + intent->write_offset,
								read_avail);
		spin_unlock_irqrestore(&ch->rx_data_lock, rx_data_flags);
		intent->write_offset += read_avail;
		intent->pkt_size += read_avail;
		if (read_avail == pkt_remaining && !einfo->intentless) {
			spin_lock_irqsave(&ch->intents_lock, intents_flags);
			list_add_tail(&ch->cur_intent->node, &ch->used_intents);
			spin_unlock_irqrestore(&ch->intents_lock,
								intents_flags);
			ch->cur_intent = NULL;
		}
		einfo->xprt_if.glink_core_if_ptr->rx_put_pkt_ctx(
						&einfo->xprt_if,
						ch->rcid,
						intent,
						read_avail == pkt_remaining);
		spin_lock_irqsave(&ch->rx_data_lock, rx_data_flags);
	}
	spin_unlock_irqrestore(&ch->rx_data_lock, rx_data_flags);
}

/**
 * smd_data_ch_notify() - process an event from the smd channel
 * @priv:	The channel the event occurred on.
 * @event:	The event to process
 */
static void smd_data_ch_notify(void *priv, unsigned event)
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
			SMDXPRT_ERR(ch->edge,
					"%s: unable to process event %d\n",
					__func__, SMD_EVENT_OPEN);
			return;
		}
		work->ch = ch;
		INIT_WORK(&work->work, process_open_event);
		queue_work(ch->wq, &work->work);
		break;
	case SMD_EVENT_CLOSE:
		work = kmalloc(sizeof(*work), GFP_ATOMIC);
		if (!work) {
			SMDXPRT_ERR(ch->edge,
					"%s: unable to process event %d\n",
					__func__, SMD_EVENT_CLOSE);
			return;
		}
		work->ch = ch;
		INIT_WORK(&work->work, process_close_event);
		queue_work(ch->wq, &work->work);
		break;
	case SMD_EVENT_STATUS:
		SMDXPRT_DBG(ch->edge,
				"%s Processing STATUS for '%s' %u:%u\n",
				__func__, ch->name, ch->lcid, ch->rcid);

		work = kmalloc(sizeof(*work), GFP_ATOMIC);
		if (!work) {
			SMDXPRT_ERR(ch->edge,
					"%s: unable to process event %d\n",
					__func__, SMD_EVENT_STATUS);
			return;
		}
		work->ch = ch;
		INIT_WORK(&work->work, process_status_event);
		queue_work(ch->wq, &work->work);
		break;
	case SMD_EVENT_REOPEN_READY:
		work = kmalloc(sizeof(*work), GFP_ATOMIC);
		if (!work) {
			SMDXPRT_ERR(ch->edge,
					"%s: unable to process event %d\n",
					__func__, SMD_EVENT_REOPEN_READY);
			return;
		}
		work->ch = ch;
		INIT_WORK(&work->work, process_reopen_event);
		queue_work(ch->wq, &work->work);
		break;
	}
}

/**
 * smd_data_ch_close() - close and cleanup SMD data channel
 * @ch:	Channel to cleanup
 *
 * Must be called with einfo->ssr_sync SRCU locked.
 */
static void smd_data_ch_close(struct channel *ch)
{
	struct intent_info *intent;
	unsigned long flags;
	struct channel_work *ch_work;

	SMDXPRT_INFO(ch->edge, "%s Closing SMD channel lcid %u\n",
			__func__, ch->lcid);

	ch->is_closing = true;
	ch->tx_resume_needed = false;
	flush_workqueue(ch->wq);

	mutex_lock(&ch->ch_probe_lock);
	ch->wait_for_probe = false;
	if (ch->smd_ch) {
		smd_close(ch->smd_ch);
		ch->smd_ch = NULL;
	} else if (ch->local_legacy) {
		ch_work = kzalloc(sizeof(*ch_work), GFP_KERNEL);
		if (ch_work) {
			ch_work->ch = ch;
			INIT_WORK(&ch_work->work, deferred_close_ack);
			queue_work(ch->wq, &ch_work->work);
		}
	}
	mutex_unlock(&ch->ch_probe_lock);

	ch->local_legacy = false;

	spin_lock_irqsave(&ch->intents_lock, flags);
	while (!list_empty(&ch->intents)) {
		intent = list_first_entry(&ch->intents, struct
				intent_info, node);
		list_del(&intent->node);
		kfree(intent);
	}
	while (!list_empty(&ch->used_intents)) {
		intent = list_first_entry(&ch->used_intents,
				struct intent_info, node);
		list_del(&intent->node);
		kfree(intent);
	}
	spin_unlock_irqrestore(&ch->intents_lock, flags);
	ch->is_closing = false;
}

static void data_ch_probe_body(struct channel *ch)
{
	struct edge_info *einfo;
	int ret;

	einfo = ch->edge;
	SMDXPRT_DBG(einfo, "%s Opening SMD channel %d:'%s'\n", __func__,
			einfo->smd_edge, ch->name);

	ret = smd_named_open_on_edge(ch->name, einfo->smd_edge, &ch->smd_ch, ch,
			smd_data_ch_notify);
	if (ret != 0) {
		SMDXPRT_ERR(einfo, "%s Opening failed %d for %d:'%s'\n",
				__func__, ret, einfo->smd_edge, ch->name);
		return;
	}
	smd_disable_read_intr(ch->smd_ch);
}

static int channel_probe(struct platform_device *pdev)
{
	struct channel *ch;
	struct edge_info *einfo;
	int i;
	bool found = false;
	unsigned long flags;

	for (i = 0; i < NUM_EDGES; ++i) {
		if (edge_infos[i].smd_edge == pdev->id) {
			found = true;
			break;
		}
	}

	if (!found)
		return -EPROBE_DEFER;

	einfo = &edge_infos[i];

	found = false;
	spin_lock_irqsave(&einfo->channels_lock, flags);
	list_for_each_entry(ch, &einfo->channels, node) {
		if (!strcmp(pdev->name, ch->name)) {
			found = true;
			break;
		}
	}
	spin_unlock_irqrestore(&einfo->channels_lock, flags);

	if (!found)
		return -EPROBE_DEFER;

	mutex_lock(&ch->ch_probe_lock);
	if (!ch->wait_for_probe) {
		mutex_unlock(&ch->ch_probe_lock);
		return -EPROBE_DEFER;
	}

	ch->wait_for_probe = false;
	ch->had_probed = true;

	data_ch_probe_body(ch);
	mutex_unlock(&ch->ch_probe_lock);

	return 0;
}

static int dummy_probe(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver dummy_driver = {
	.probe = dummy_probe,
	.driver = {
		.name = "dummydriver12345",
		.owner = THIS_MODULE,
	},
};

static struct platform_device dummy_device = {
	.name = "dummydriver12345",
};

/**
 * add_platform_driver() - register the needed platform driver for a channel
 * @ch:	The channel that needs a platform driver registered.
 *
 * SMD channels are unique by name/edge tuples, but the platform driver can
 * only specify the name of the channel, so multiple unique SMD channels can
 * be covered under one platform driver.  Therfore we need to smartly manage
 * the muxing of channels on platform drivers.
 *
 * Return: Success or standard linux error code.
 */
static int add_platform_driver(struct channel *ch)
{
	struct pdrvs *pdrv;
	bool found = false;
	int ret = 0;
	static bool first = true;

	mutex_lock(&pdrv_list_mutex);
	mutex_lock(&ch->ch_probe_lock);
	ch->wait_for_probe = true;
	list_for_each_entry(pdrv, &pdrv_list, node) {
		if (!strcmp(ch->name, pdrv->pdrv.driver.name)) {
			found = true;
			break;
		}
	}

	if (!found) {
		mutex_unlock(&ch->ch_probe_lock);
		pdrv = kzalloc(sizeof(*pdrv), GFP_KERNEL);
		if (!pdrv) {
			ret = -ENOMEM;
			mutex_lock(&ch->ch_probe_lock);
			ch->wait_for_probe = false;
			mutex_unlock(&ch->ch_probe_lock);
			goto out;
		}
		pdrv->pdrv.driver.name = ch->name;
		pdrv->pdrv.driver.owner = THIS_MODULE;
		pdrv->pdrv.probe = channel_probe;
		list_add_tail(&pdrv->node, &pdrv_list);
		ret = platform_driver_register(&pdrv->pdrv);
		if (ret) {
			list_del(&pdrv->node);
			kfree(pdrv);
			mutex_lock(&ch->ch_probe_lock);
			ch->wait_for_probe = false;
			mutex_unlock(&ch->ch_probe_lock);
		}
	} else {
		if (ch->had_probed)
			data_ch_probe_body(ch);
		mutex_unlock(&ch->ch_probe_lock);
		/*
		 * channel_probe might have seen the device we want, but
		 * returned EPROBE_DEFER so we need to kick the deferred list
		 */
		platform_driver_register(&dummy_driver);
		if (first) {
			platform_device_register(&dummy_device);
			first = false;
		}
		platform_driver_unregister(&dummy_driver);
	}

out:
	mutex_unlock(&pdrv_list_mutex);
	return ret;
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
	uint32_t capabilities = GCAP_SIGNALS | GCAP_AUTO_QUEUE_RX_INT;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	return einfo->intentless ?
				GCAP_INTENTLESS | capabilities : capabilities;
}

/**
 * tx_cmd_ch_open() - convert a channel open cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @name:	The channel name to encode.
 * @req_xprt:	The transport the core would like to migrate this channel to.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_ch_open(struct glink_transport_if *if_ptr, uint32_t lcid,
			  const char *name, uint16_t req_xprt)
{
	struct command {
		uint32_t cmd;
		uint32_t id;
		uint32_t priority;
	};
	struct command cmd;
	struct edge_info *einfo;
	struct channel *ch;
	struct channel *temp_ch;
	bool found = false;
	int rcu_id;
	int ret = 0;
	int len;
	unsigned long flags;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->ssr_sync);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->ssr_sync, rcu_id);
		return -EFAULT;
	}

	spin_lock_irqsave(&einfo->channels_lock, flags);
	list_for_each_entry(ch, &einfo->channels, node) {
		if (!strcmp(name, ch->name)) {
			found = true;
			break;
		}
	}
	spin_unlock_irqrestore(&einfo->channels_lock, flags);

	if (!found) {
		ch = kzalloc(sizeof(*ch), GFP_KERNEL);
		if (!ch) {
			SMDXPRT_ERR(einfo,
				"%s: channel struct allocation failed\n",
				__func__);
			srcu_read_unlock(&einfo->ssr_sync, rcu_id);
			return -ENOMEM;
		}
		strlcpy(ch->name, name, GLINK_NAME_SIZE);
		ch->edge = einfo;
		mutex_init(&ch->ch_probe_lock);
		INIT_LIST_HEAD(&ch->intents);
		INIT_LIST_HEAD(&ch->used_intents);
		spin_lock_init(&ch->intents_lock);
		spin_lock_init(&ch->rx_data_lock);
		INIT_WORK(&ch->work, process_data_event);
		ch->wq = create_singlethread_workqueue(ch->name);
		if (!ch->wq) {
			SMDXPRT_ERR(einfo,
					"%s: channel workqueue create failed\n",
					__func__);
			kfree(ch);
			srcu_read_unlock(&einfo->ssr_sync, rcu_id);
			return -ENOMEM;
		}

		/*
		 * Channel could have been added to the list by someone else
		 * so scan again.  Channel creation is non-atomic, so unlock
		 * and recheck is necessary
		 */
		temp_ch = ch;
		spin_lock_irqsave(&einfo->channels_lock, flags);
		list_for_each_entry(ch, &einfo->channels, node)
			if (!strcmp(name, ch->name)) {
				found = true;
				break;
			}

		if (!found) {
			ch = temp_ch;
			list_add_tail(&ch->node, &einfo->channels);
			spin_unlock_irqrestore(&einfo->channels_lock, flags);
		} else {
			spin_unlock_irqrestore(&einfo->channels_lock, flags);
			destroy_workqueue(temp_ch->wq);
			kfree(temp_ch);
		}
	}

	ch->tx_resume_needed = false;
	ch->lcid = lcid;

	if (einfo->smd_ctl_ch_open) {
		SMDXPRT_INFO(einfo, "%s TX OPEN '%s' lcid %u reqxprt %u\n",
				__func__, name, lcid, req_xprt);
		cmd.cmd = CMD_OPEN;
		cmd.id = lcid;
		cmd.priority = req_xprt;
		len = strlen(name) + 1;
		len += sizeof(cmd);
		mutex_lock(&einfo->smd_lock);
		while (smd_write_avail(einfo->smd_ch) < len)
			msleep(20);
		smd_write_start(einfo->smd_ch, len);
		smd_write_segment(einfo->smd_ch, &cmd, sizeof(cmd));
		smd_write_segment(einfo->smd_ch, name, strlen(name) + 1);
		smd_write_end(einfo->smd_ch);
		mutex_unlock(&einfo->smd_lock);
	} else {
		SMDXPRT_INFO(einfo, "%s Legacy Open '%s' lcid %u reqxprt %u\n",
				__func__, name, lcid, req_xprt);
		ch->rcid = lcid + LEGACY_RCID_CHANNEL_OFFSET;
		ch->local_legacy = true;
		ch->remote_legacy = true;
		ret = add_platform_driver(ch);
		if (!ret) {
			mutex_lock(&einfo->rx_cmd_lock);
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_open_ack(
						&einfo->xprt_if,
						ch->lcid, SMD_TRANS_XPRT_ID);
			mutex_unlock(&einfo->rx_cmd_lock);
		}
	}

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
	struct command {
		uint32_t cmd;
		uint32_t id;
		uint32_t reserved;
	};
	struct command cmd;
	struct edge_info *einfo;
	struct channel *ch;
	int rcu_id;
	bool found = false;
	unsigned long flags;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->ssr_sync);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->ssr_sync, rcu_id);
		return -EFAULT;
	}

	spin_lock_irqsave(&einfo->channels_lock, flags);
	list_for_each_entry(ch, &einfo->channels, node)
		if (lcid == ch->lcid) {
			found = true;
			break;
		}
	spin_unlock_irqrestore(&einfo->channels_lock, flags);

	if (!found) {
		SMDXPRT_ERR(einfo, "%s LCID not found %u\n",
				__func__, lcid);
		srcu_read_unlock(&einfo->ssr_sync, rcu_id);
		return -ENODEV;
	}

	if (!ch->local_legacy) {
		SMDXPRT_INFO(einfo, "%s TX CLOSE lcid %u\n", __func__, lcid);
		cmd.cmd = CMD_CLOSE;
		cmd.id = lcid;
		cmd.reserved = 0;
		mutex_lock(&einfo->smd_lock);
		while (smd_write_avail(einfo->smd_ch) < sizeof(cmd))
			msleep(20);
		smd_write(einfo->smd_ch, &cmd, sizeof(cmd));
		mutex_unlock(&einfo->smd_lock);
	} else {
		smd_data_ch_close(ch);
	}
	srcu_read_unlock(&einfo->ssr_sync, rcu_id);
	return 0;
}

/**
 * tx_cmd_ch_remote_open_ack() - convert a channel open ack cmd to wire format
 *				 and transmit
 * @if_ptr:	The transport to transmit on.
 * @rcid:	The remote channel id to encode.
 * @xprt_resp:	The response to a transport migration request.
 */
static void tx_cmd_ch_remote_open_ack(struct glink_transport_if *if_ptr,
				      uint32_t rcid, uint16_t xprt_resp)
{
	struct command {
		uint32_t cmd;
		uint32_t id;
		uint32_t priority;
	};
	struct command cmd;
	struct edge_info *einfo;
	struct channel *ch;
	bool found = false;
	unsigned long flags;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	if (!einfo->smd_ctl_ch_open)
		return;

	spin_lock_irqsave(&einfo->channels_lock, flags);
	list_for_each_entry(ch, &einfo->channels, node)
		if (ch->rcid == rcid) {
			found = true;
			break;
		}
	spin_unlock_irqrestore(&einfo->channels_lock, flags);

	if (!found) {
		SMDXPRT_ERR(einfo, "%s No matching SMD channel for rcid %u\n",
				__func__, rcid);
		return;
	}

	if (ch->remote_legacy) {
		SMDXPRT_INFO(einfo, "%s Legacy ch rcid %u xprt_resp %u\n",
				__func__, rcid, xprt_resp);
		return;
	}

	SMDXPRT_INFO(einfo, "%s TX OPEN ACK rcid %u xprt_resp %u\n",
			__func__, rcid, xprt_resp);

	cmd.cmd = CMD_OPEN_ACK;
	cmd.id = ch->rcid;
	cmd.priority = xprt_resp;

	mutex_lock(&einfo->smd_lock);
	while (smd_write_avail(einfo->smd_ch) < sizeof(cmd))
		msleep(20);

	smd_write(einfo->smd_ch, &cmd, sizeof(cmd));
	mutex_unlock(&einfo->smd_lock);
}

/**
 * tx_cmd_ch_remote_close_ack() - convert a channel close ack cmd to wire format
 *				  and transmit
 * @if_ptr:	The transport to transmit on.
 * @rcid:	The remote channel id to encode.
 */
static void tx_cmd_ch_remote_close_ack(struct glink_transport_if *if_ptr,
				       uint32_t rcid)
{
	struct command {
		uint32_t cmd;
		uint32_t id;
		uint32_t reserved;
	};
	struct command cmd;
	struct edge_info *einfo;
	struct channel *ch;
	bool found = false;
	unsigned long flags;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	spin_lock_irqsave(&einfo->channels_lock, flags);
	list_for_each_entry(ch, &einfo->channels, node)
		if (rcid == ch->rcid) {
			found = true;
			break;
		}
	spin_unlock_irqrestore(&einfo->channels_lock, flags);

	if (!found) {
		SMDXPRT_ERR(einfo,
			"%s No matching SMD channel for rcid %u\n",
			__func__, rcid);
		return;
	}

	if (!ch->remote_legacy) {
		SMDXPRT_INFO(einfo, "%s TX CLOSE ACK rcid %u\n",
				__func__, rcid);
		cmd.cmd = CMD_CLOSE_ACK;
		cmd.id = rcid;
		cmd.reserved = 0;
		mutex_lock(&einfo->smd_lock);
		while (smd_write_avail(einfo->smd_ch) < sizeof(cmd))
			msleep(20);
		smd_write(einfo->smd_ch, &cmd, sizeof(cmd));
		mutex_unlock(&einfo->smd_lock);
	}
	ch->remote_legacy = false;
	ch->rcid = 0;
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
	unsigned long flags;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	einfo->in_ssr = true;
	synchronize_srcu(&einfo->ssr_sync);

	einfo->smd_ctl_ch_open = false;

	spin_lock_irqsave(&einfo->channels_lock, flags);
	list_for_each_entry(ch, &einfo->channels, node) {
		spin_unlock_irqrestore(&einfo->channels_lock, flags);
		ch->is_closing = true;
		flush_workqueue(ch->wq);
		mutex_lock(&ch->ch_probe_lock);
		ch->wait_for_probe = false;
		if (ch->smd_ch) {
			smd_close(ch->smd_ch);
			ch->smd_ch = NULL;
		}
		mutex_unlock(&ch->ch_probe_lock);
		ch->local_legacy = false;
		ch->remote_legacy = false;
		ch->rcid = 0;
		ch->tx_resume_needed = false;

		spin_lock_irqsave(&ch->intents_lock, flags);
		while (!list_empty(&ch->intents)) {
			intent = list_first_entry(&ch->intents,
							struct intent_info,
							node);
			list_del(&intent->node);
			kfree(intent);
		}
		while (!list_empty(&ch->used_intents)) {
			intent = list_first_entry(&ch->used_intents,
							struct intent_info,
							node);
			list_del(&intent->node);
			kfree(intent);
		}
		kfree(ch->cur_intent);
		ch->cur_intent = NULL;
		spin_unlock_irqrestore(&ch->intents_lock, flags);
		ch->is_closing = false;
		spin_lock_irqsave(&einfo->channels_lock, flags);
	}
	spin_unlock_irqrestore(&einfo->channels_lock, flags);

	einfo->xprt_if.glink_core_if_ptr->link_down(&einfo->xprt_if);
	schedule_delayed_work(&einfo->ssr_work, 5 * HZ);
	return 0;
}

/**
 * allocate_rx_intent() - allocate/reserve space for RX Intent
 * @if_ptr:	The transport the intent is associated with.
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
static int allocate_rx_intent(struct glink_transport_if *if_ptr, size_t size,
			      struct glink_core_rx_intent *intent)
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
 * @if_ptr:	The transport the intent is associated with.
 * @intent:	Pointer to the intent structure.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int deallocate_rx_intent(struct glink_transport_if *if_ptr,
				struct glink_core_rx_intent *intent)
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
 * check_and_resume_rx() - Check the RX state and resume it
 * @ch:		Channel which needs to be checked.
 * @intent_size:	Intent size being queued.
 *
 * This function checks if a receive intent is requested in the
 * channel and resumes the RX if the queued receive intent satisifes
 * the requested receive intent. This function must be called with
 * ch->intents_lock locked.
 */
static void check_and_resume_rx(struct channel *ch, size_t intent_size)
{
	if (ch->intent_req && ch->intent_req_size <= intent_size) {
		ch->intent_req = false;
		queue_work(ch->wq, &ch->work);
	}
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
	unsigned long flags;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->ssr_sync);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->ssr_sync, rcu_id);
		return -EFAULT;
	}

	spin_lock_irqsave(&einfo->channels_lock, flags);
	list_for_each_entry(ch, &einfo->channels, node) {
		if (lcid == ch->lcid)
			break;
	}
	spin_unlock_irqrestore(&einfo->channels_lock, flags);

	intent = kmalloc(sizeof(*intent), GFP_KERNEL);
	if (!intent) {
		SMDXPRT_ERR(einfo, "%s: no memory for intent\n", __func__);
		srcu_read_unlock(&einfo->ssr_sync, rcu_id);
		return -ENOMEM;
	}

	intent->liid = liid;
	intent->size = size;
	spin_lock_irqsave(&ch->intents_lock, flags);
	list_add_tail(&intent->node, &ch->intents);
	check_and_resume_rx(ch, size);
	spin_unlock_irqrestore(&ch->intents_lock, flags);

	srcu_read_unlock(&einfo->ssr_sync, rcu_id);
	return 0;
}

/**
 * tx_cmd_local_rx_done() - convert an rx done cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @liid:	The local intent id to encode.
 * @reuse:	Reuse the consumed intent.
 */
static void tx_cmd_local_rx_done(struct glink_transport_if *if_ptr,
				 uint32_t lcid, uint32_t liid, bool reuse)
{
	struct edge_info *einfo;
	struct channel *ch;
	struct intent_info *i;
	unsigned long flags;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);
	spin_lock_irqsave(&einfo->channels_lock, flags);
	list_for_each_entry(ch, &einfo->channels, node) {
		if (lcid == ch->lcid)
			break;
	}
	spin_unlock_irqrestore(&einfo->channels_lock, flags);
	spin_lock_irqsave(&ch->intents_lock, flags);
	list_for_each_entry(i, &ch->used_intents, node) {
		if (i->liid == liid) {
			list_del(&i->node);
			if (reuse) {
				list_add_tail(&i->node, &ch->intents);
				check_and_resume_rx(ch, i->size);
			} else {
				kfree(i);
			}
			break;
		}
	}
	spin_unlock_irqrestore(&ch->intents_lock, flags);
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
	unsigned long flags;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->ssr_sync);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->ssr_sync, rcu_id);
		return -EFAULT;
	}

	spin_lock_irqsave(&einfo->channels_lock, flags);
	list_for_each_entry(ch, &einfo->channels, node) {
		if (lcid == ch->lcid)
			break;
	}
	spin_unlock_irqrestore(&einfo->channels_lock, flags);

	data_start = get_tx_vaddr(pctx, pctx->size - pctx->size_remaining,
				  &tx_size);
	if (!data_start) {
		srcu_read_unlock(&einfo->ssr_sync, rcu_id);
		return -EINVAL;
	}

	if (!ch->streaming_ch) {
		if (pctx->size == pctx->size_remaining) {
			rc = check_write_avail(smd_write_avail, ch);
			if (rc <= 0) {
				srcu_read_unlock(&einfo->ssr_sync, rcu_id);
				return rc;
			}
			rc = smd_write_start(ch->smd_ch, pctx->size);
			if (rc) {
				srcu_read_unlock(&einfo->ssr_sync, rcu_id);
				return rc;
			}
		}

		rc = check_write_avail(smd_write_segment_avail, ch);
		if (rc <= 0) {
			srcu_read_unlock(&einfo->ssr_sync, rcu_id);
			return rc;
		}
		if (rc > tx_size)
			rc = tx_size;
		rc = smd_write_segment(ch->smd_ch, data_start, rc);
		if (rc < 0) {
			SMDXPRT_ERR(einfo, "%s: write segment failed %d\n",
					__func__, rc);
			srcu_read_unlock(&einfo->ssr_sync, rcu_id);
			return rc;
		}
	} else {
		rc = check_write_avail(smd_write_avail, ch);
		if (rc <= 0) {
			srcu_read_unlock(&einfo->ssr_sync, rcu_id);
			return rc;
		}
		if (rc > tx_size)
			rc = tx_size;
		rc = smd_write(ch->smd_ch, data_start, rc);
		if (rc < 0) {
			SMDXPRT_ERR(einfo, "%s: write failed %d\n",
					__func__, rc);
			srcu_read_unlock(&einfo->ssr_sync, rcu_id);
			return rc;
		}
	}

	pctx->size_remaining -= rc;
	if (!pctx->size_remaining) {
		if (!ch->streaming_ch)
			smd_write_end(ch->smd_ch);
		tx_done = kmalloc(sizeof(*tx_done), GFP_ATOMIC);
		if (!tx_done) {
			SMDXPRT_ERR(einfo, "%s: failed allocation of tx_done\n",
					__func__);
			srcu_read_unlock(&einfo->ssr_sync, rcu_id);
			return -ENOMEM;
		}
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
	unsigned long flags;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);
	spin_lock_irqsave(&einfo->channels_lock, flags);
	list_for_each_entry(ch, &einfo->channels, node) {
		if (lcid == ch->lcid)
			break;
	}
	spin_unlock_irqrestore(&einfo->channels_lock, flags);
	einfo->xprt_if.glink_core_if_ptr->rx_cmd_rx_intent_req_ack(
								&einfo->xprt_if,
								ch->rcid,
								true);
	einfo->xprt_if.glink_core_if_ptr->rx_cmd_remote_rx_intent_put(
							&einfo->xprt_if,
							ch->rcid,
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
	unsigned long flags;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);
	spin_lock_irqsave(&einfo->channels_lock, flags);
	list_for_each_entry(ch, &einfo->channels, node) {
		if (lcid == ch->lcid)
			break;
	}
	spin_unlock_irqrestore(&einfo->channels_lock, flags);

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
	unsigned long flags;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);
	spin_lock_irqsave(&einfo->channels_lock, flags);
	list_for_each_entry(ch, &einfo->channels, node) {
		if (lcid == ch->lcid)
			break;
	}
	spin_unlock_irqrestore(&einfo->channels_lock, flags);
	rc = smd_is_pkt_avail(ch->smd_ch);
	if (rc == 1)
		process_data_event(&ch->work);
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
	int ret = 0;
	unsigned long flags;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);
	spin_lock_irqsave(&einfo->channels_lock, flags);
	list_for_each_entry(ch, &einfo->channels, node) {
		if (lcid == ch->lcid)
			break;
	}
	spin_unlock_irqrestore(&einfo->channels_lock, flags);
	ret = smd_mask_receive_interrupt(ch->smd_ch, mask, pstruct);

	if (ret == 0)
		einfo->irq_disabled = mask;

	return ret;
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
	einfo->xprt_if.tx_cmd_version = tx_cmd_version;
	einfo->xprt_if.tx_cmd_version_ack = tx_cmd_version_ack;
	einfo->xprt_if.set_version = set_version;
	einfo->xprt_if.tx_cmd_ch_open = tx_cmd_ch_open;
	einfo->xprt_if.tx_cmd_ch_close = tx_cmd_ch_close;
	einfo->xprt_if.tx_cmd_ch_remote_open_ack = tx_cmd_ch_remote_open_ack;
	einfo->xprt_if.tx_cmd_ch_remote_close_ack = tx_cmd_ch_remote_close_ack;
	einfo->xprt_if.ssr = ssr;
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
	einfo->xprt_cfg.max_iid = SZ_128;
}

static struct platform_driver migration_driver = {
	.probe		= ctl_ch_probe,
	.driver		= {
		.name	= "GLINK_CTRL",
		.owner	= THIS_MODULE,
	},
};

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
		spin_lock_init(&einfo->channels_lock);
		init_srcu_struct(&einfo->ssr_sync);
		mutex_init(&einfo->smd_lock);
		mutex_init(&einfo->in_ssr_lock);
		mutex_init(&einfo->rx_cmd_lock);
		INIT_DELAYED_WORK(&einfo->ssr_work, ssr_work_func);
		INIT_WORK(&einfo->work, process_ctl_event);
		rc = glink_core_register_transport(&einfo->xprt_if,
							&einfo->xprt_cfg);
		if (rc)
			SMDXPRT_ERR(einfo,
				"%s: %s glink register xprt failed %d\n",
				__func__, einfo->xprt_cfg.edge, rc);
		else
			einfo->xprt_if.glink_core_if_ptr->link_up(
							&einfo->xprt_if);
	}

	platform_driver_register(&migration_driver);

	return 0;
}
arch_initcall(glink_smd_xprt_init);

MODULE_DESCRIPTION("MSM G-Link SMD Transport");
MODULE_LICENSE("GPL v2");
