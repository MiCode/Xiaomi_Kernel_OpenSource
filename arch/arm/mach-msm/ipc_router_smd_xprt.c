/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

/*
 * IPC ROUTER SMD XPRT module.
 */
#define DEBUG

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#include <mach/msm_smd.h>
#include <mach/subsystem_restart.h>

#include "ipc_router.h"
#include "smd_private.h"

static int msm_ipc_router_smd_xprt_debug_mask;
module_param_named(debug_mask, msm_ipc_router_smd_xprt_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#if defined(DEBUG)
#define D(x...) do { \
if (msm_ipc_router_smd_xprt_debug_mask) \
	pr_info(x); \
} while (0)
#else
#define D(x...) do { } while (0)
#endif

#define MIN_FRAG_SZ (IPC_ROUTER_HDR_SIZE + sizeof(union rr_control_msg))

#define NUM_SMD_XPRTS 4
#define XPRT_NAME_LEN (SMD_MAX_CH_NAME_LEN + 12)

struct msm_ipc_router_smd_xprt {
	struct msm_ipc_router_xprt xprt;
	smd_channel_t *channel;
	struct workqueue_struct *smd_xprt_wq;
	wait_queue_head_t write_avail_wait_q;
	struct rr_packet *in_pkt;
	int is_partial_in_pkt;
	struct delayed_work read_work;
	spinlock_t ss_reset_lock;	/*Subsystem reset lock*/
	int ss_reset;
	void *pil;
	struct completion sft_close_complete;
	unsigned xprt_version;
	unsigned xprt_option;
};

struct msm_ipc_router_smd_xprt_work {
	struct msm_ipc_router_xprt *xprt;
	struct work_struct work;
};

static void smd_xprt_read_data(struct work_struct *work);
static void smd_xprt_open_event(struct work_struct *work);
static void smd_xprt_close_event(struct work_struct *work);

struct msm_ipc_router_smd_xprt_config {
	char ch_name[SMD_MAX_CH_NAME_LEN];
	char xprt_name[XPRT_NAME_LEN];
	uint32_t edge;
	uint32_t link_id;
	unsigned xprt_version;
};

struct msm_ipc_router_smd_xprt_config smd_xprt_cfg[] = {
	{"RPCRPY_CNTL", "ipc_rtr_smd_rpcrpy_cntl", SMD_APPS_MODEM, 1, 1},
	{"IPCRTR", "ipc_rtr_smd_ipcrtr", SMD_APPS_MODEM, 1, 1},
	{"IPCRTR", "ipc_rtr_q6_ipcrtr", SMD_APPS_QDSP, 1, 1},
	{"IPCRTR", "ipc_rtr_wcnss_ipcrtr", SMD_APPS_WCNSS, 1, 1},
};

static struct msm_ipc_router_smd_xprt smd_remote_xprt[NUM_SMD_XPRTS];

static int find_smd_xprt_cfg(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < NUM_SMD_XPRTS; i++) {
		if (!strncmp(pdev->name, smd_xprt_cfg[i].ch_name, 20) &&
		    (pdev->id == smd_xprt_cfg[i].edge))
			return i;
	}

	return -ENODEV;
}

static int msm_ipc_router_smd_get_xprt_version(
	struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_smd_xprt *smd_xprtp;
	if (!xprt)
		return -EINVAL;
	smd_xprtp = container_of(xprt, struct msm_ipc_router_smd_xprt, xprt);

	return (int)smd_xprtp->xprt_version;
}

static int msm_ipc_router_smd_get_xprt_option(
	struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_smd_xprt *smd_xprtp;
	if (!xprt)
		return -EINVAL;
	smd_xprtp = container_of(xprt, struct msm_ipc_router_smd_xprt, xprt);

	return (int)smd_xprtp->xprt_option;
}

static int msm_ipc_router_smd_remote_write_avail(
	struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_smd_xprt *smd_xprtp =
		container_of(xprt, struct msm_ipc_router_smd_xprt, xprt);

	return smd_write_avail(smd_xprtp->channel);
}

static int msm_ipc_router_smd_remote_write(void *data,
					   uint32_t len,
					   struct msm_ipc_router_xprt *xprt)
{
	struct rr_packet *pkt = (struct rr_packet *)data;
	struct sk_buff *ipc_rtr_pkt;
	int offset, sz_written = 0;
	int ret, num_retries = 0;
	unsigned long flags;
	struct msm_ipc_router_smd_xprt *smd_xprtp =
		container_of(xprt, struct msm_ipc_router_smd_xprt, xprt);

	if (!pkt)
		return -EINVAL;

	if (!len || pkt->length != len)
		return -EINVAL;

	while ((ret = smd_write_start(smd_xprtp->channel, len)) < 0) {
		spin_lock_irqsave(&smd_xprtp->ss_reset_lock, flags);
		if (smd_xprtp->ss_reset) {
			spin_unlock_irqrestore(&smd_xprtp->ss_reset_lock,
						flags);
			pr_err("%s: %s chnl reset\n", __func__, xprt->name);
			return -ENETRESET;
		}
		spin_unlock_irqrestore(&smd_xprtp->ss_reset_lock, flags);
		if (num_retries >= 5) {
			pr_err("%s: Error %d @smd_write_start for %s\n",
				__func__, ret, xprt->name);
			return ret;
		}
		msleep(50);
		num_retries++;
	}

	D("%s: Ready to write %d bytes\n", __func__, len);
	skb_queue_walk(pkt->pkt_fragment_q, ipc_rtr_pkt) {
		offset = 0;
		while (offset < ipc_rtr_pkt->len) {
			if (!smd_write_segment_avail(smd_xprtp->channel))
				smd_enable_read_intr(smd_xprtp->channel);

			wait_event(smd_xprtp->write_avail_wait_q,
				(smd_write_segment_avail(smd_xprtp->channel) ||
				smd_xprtp->ss_reset));
			smd_disable_read_intr(smd_xprtp->channel);
			spin_lock_irqsave(&smd_xprtp->ss_reset_lock, flags);
			if (smd_xprtp->ss_reset) {
				spin_unlock_irqrestore(
					&smd_xprtp->ss_reset_lock, flags);
				pr_err("%s: %s chnl reset\n",
					__func__, xprt->name);
				return -ENETRESET;
			}
			spin_unlock_irqrestore(&smd_xprtp->ss_reset_lock,
						flags);

			sz_written = smd_write_segment(smd_xprtp->channel,
					ipc_rtr_pkt->data + offset,
					(ipc_rtr_pkt->len - offset), 0);
			offset += sz_written;
			sz_written = 0;
		}
		D("%s: Wrote %d bytes over %s\n",
		  __func__, offset, xprt->name);
	}

	if (!smd_write_end(smd_xprtp->channel))
		D("%s: Finished writing\n", __func__);
	return len;
}

static int msm_ipc_router_smd_remote_close(struct msm_ipc_router_xprt *xprt)
{
	int rc;
	struct msm_ipc_router_smd_xprt *smd_xprtp =
		container_of(xprt, struct msm_ipc_router_smd_xprt, xprt);

	rc = smd_close(smd_xprtp->channel);
	if (smd_xprtp->pil) {
		subsystem_put(smd_xprtp->pil);
		smd_xprtp->pil = NULL;
	}
	return rc;
}

static void smd_xprt_sft_close_done(struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_smd_xprt *smd_xprtp =
		container_of(xprt, struct msm_ipc_router_smd_xprt, xprt);

	complete_all(&smd_xprtp->sft_close_complete);
}

static void smd_xprt_read_data(struct work_struct *work)
{
	int pkt_size, sz_read, sz;
	struct sk_buff *ipc_rtr_pkt;
	void *data;
	unsigned long flags;
	struct delayed_work *rwork = to_delayed_work(work);
	struct msm_ipc_router_smd_xprt *smd_xprtp =
		container_of(rwork, struct msm_ipc_router_smd_xprt, read_work);

	spin_lock_irqsave(&smd_xprtp->ss_reset_lock, flags);
	if (smd_xprtp->ss_reset) {
		spin_unlock_irqrestore(&smd_xprtp->ss_reset_lock, flags);
		if (smd_xprtp->in_pkt)
			release_pkt(smd_xprtp->in_pkt);
		smd_xprtp->is_partial_in_pkt = 0;
		pr_err("%s: %s channel reset\n",
			__func__, smd_xprtp->xprt.name);
		return;
	}
	spin_unlock_irqrestore(&smd_xprtp->ss_reset_lock, flags);

	D("%s pkt_size: %d, read_avail: %d\n", __func__,
		smd_cur_packet_size(smd_xprtp->channel),
		smd_read_avail(smd_xprtp->channel));
	while ((pkt_size = smd_cur_packet_size(smd_xprtp->channel)) &&
		smd_read_avail(smd_xprtp->channel)) {
		if (!smd_xprtp->is_partial_in_pkt) {
			smd_xprtp->in_pkt = kzalloc(sizeof(struct rr_packet),
						    GFP_KERNEL);
			if (!smd_xprtp->in_pkt) {
				pr_err("%s: Couldn't alloc rr_packet\n",
					__func__);
				return;
			}

			smd_xprtp->in_pkt->pkt_fragment_q =
				kmalloc(sizeof(struct sk_buff_head),
					GFP_KERNEL);
			if (!smd_xprtp->in_pkt->pkt_fragment_q) {
				pr_err("%s: Couldn't alloc pkt_fragment_q\n",
					__func__);
				kfree(smd_xprtp->in_pkt);
				return;
			}
			skb_queue_head_init(smd_xprtp->in_pkt->pkt_fragment_q);
			smd_xprtp->is_partial_in_pkt = 1;
			D("%s: Allocated rr_packet\n", __func__);
		}

		if (((pkt_size >= MIN_FRAG_SZ) &&
		     (smd_read_avail(smd_xprtp->channel) < MIN_FRAG_SZ)) ||
		    ((pkt_size < MIN_FRAG_SZ) &&
		     (smd_read_avail(smd_xprtp->channel) < pkt_size)))
			return;

		sz = smd_read_avail(smd_xprtp->channel);
		do {
			ipc_rtr_pkt = alloc_skb(sz, GFP_KERNEL);
			if (!ipc_rtr_pkt) {
				if (sz <= (PAGE_SIZE/2)) {
					queue_delayed_work(
						smd_xprtp->smd_xprt_wq,
						&smd_xprtp->read_work,
						msecs_to_jiffies(100));
					return;
				}
				sz = sz / 2;
			}
		} while (!ipc_rtr_pkt);

		D("%s: Allocated the sk_buff of size %d\n", __func__, sz);
		data = skb_put(ipc_rtr_pkt, sz);
		sz_read = smd_read(smd_xprtp->channel, data, sz);
		if (sz_read != sz) {
			pr_err("%s: Couldn't read %s completely\n",
				__func__, smd_xprtp->xprt.name);
			kfree_skb(ipc_rtr_pkt);
			release_pkt(smd_xprtp->in_pkt);
			smd_xprtp->is_partial_in_pkt = 0;
			return;
		}
		skb_queue_tail(smd_xprtp->in_pkt->pkt_fragment_q, ipc_rtr_pkt);
		smd_xprtp->in_pkt->length += sz_read;
		if (sz_read != pkt_size)
			smd_xprtp->is_partial_in_pkt = 1;
		else
			smd_xprtp->is_partial_in_pkt = 0;

		if (!smd_xprtp->is_partial_in_pkt) {
			D("%s: Packet size read %d\n",
			  __func__, smd_xprtp->in_pkt->length);
			msm_ipc_router_xprt_notify(&smd_xprtp->xprt,
						IPC_ROUTER_XPRT_EVENT_DATA,
						(void *)smd_xprtp->in_pkt);
			release_pkt(smd_xprtp->in_pkt);
			smd_xprtp->in_pkt = NULL;
		}
	}
}

static void smd_xprt_open_event(struct work_struct *work)
{
	struct msm_ipc_router_smd_xprt_work *xprt_work =
		container_of(work, struct msm_ipc_router_smd_xprt_work, work);
	struct msm_ipc_router_smd_xprt *smd_xprtp =
		container_of(xprt_work->xprt,
			     struct msm_ipc_router_smd_xprt, xprt);
	unsigned long flags;

	spin_lock_irqsave(&smd_xprtp->ss_reset_lock, flags);
	smd_xprtp->ss_reset = 0;
	spin_unlock_irqrestore(&smd_xprtp->ss_reset_lock, flags);
	msm_ipc_router_xprt_notify(xprt_work->xprt,
				IPC_ROUTER_XPRT_EVENT_OPEN, NULL);
	D("%s: Notified IPC Router of %s OPEN\n",
	   __func__, xprt_work->xprt->name);
	kfree(xprt_work);
}

static void smd_xprt_close_event(struct work_struct *work)
{
	struct msm_ipc_router_smd_xprt_work *xprt_work =
		container_of(work, struct msm_ipc_router_smd_xprt_work, work);
	struct msm_ipc_router_smd_xprt *smd_xprtp =
		container_of(xprt_work->xprt,
			     struct msm_ipc_router_smd_xprt, xprt);

	init_completion(&smd_xprtp->sft_close_complete);
	msm_ipc_router_xprt_notify(xprt_work->xprt,
				IPC_ROUTER_XPRT_EVENT_CLOSE, NULL);
	D("%s: Notified IPC Router of %s CLOSE\n",
	   __func__, xprt_work->xprt->name);
	wait_for_completion(&smd_xprtp->sft_close_complete);
	kfree(xprt_work);
}

static void msm_ipc_router_smd_remote_notify(void *_dev, unsigned event)
{
	unsigned long flags;
	struct msm_ipc_router_smd_xprt *smd_xprtp;
	struct msm_ipc_router_smd_xprt_work *xprt_work;

	smd_xprtp = (struct msm_ipc_router_smd_xprt *)_dev;
	if (!smd_xprtp)
		return;

	switch (event) {
	case SMD_EVENT_DATA:
		if (smd_read_avail(smd_xprtp->channel))
			queue_delayed_work(smd_xprtp->smd_xprt_wq,
					   &smd_xprtp->read_work, 0);
		if (smd_write_segment_avail(smd_xprtp->channel))
			wake_up(&smd_xprtp->write_avail_wait_q);
		break;

	case SMD_EVENT_OPEN:
		xprt_work = kmalloc(sizeof(struct msm_ipc_router_smd_xprt_work),
				    GFP_ATOMIC);
		if (!xprt_work) {
			pr_err("%s: Couldn't notify %d event to IPC Router\n",
				__func__, event);
			return;
		}
		xprt_work->xprt = &smd_xprtp->xprt;
		INIT_WORK(&xprt_work->work, smd_xprt_open_event);
		queue_work(smd_xprtp->smd_xprt_wq, &xprt_work->work);
		break;

	case SMD_EVENT_CLOSE:
		spin_lock_irqsave(&smd_xprtp->ss_reset_lock, flags);
		smd_xprtp->ss_reset = 1;
		spin_unlock_irqrestore(&smd_xprtp->ss_reset_lock, flags);
		wake_up(&smd_xprtp->write_avail_wait_q);
		xprt_work = kmalloc(sizeof(struct msm_ipc_router_smd_xprt_work),
				    GFP_ATOMIC);
		if (!xprt_work) {
			pr_err("%s: Couldn't notify %d event to IPC Router\n",
				__func__, event);
			return;
		}
		xprt_work->xprt = &smd_xprtp->xprt;
		INIT_WORK(&xprt_work->work, smd_xprt_close_event);
		queue_work(smd_xprtp->smd_xprt_wq, &xprt_work->work);
		break;
	}
}

static void *msm_ipc_load_subsystem(uint32_t edge)
{
	void *pil = NULL;
	const char *peripheral;

	peripheral = smd_edge_to_subsystem(edge);
	if (peripheral) {
		pil = subsystem_get(peripheral);
		if (IS_ERR(pil)) {
			pr_err("%s: Failed to load %s\n",
				__func__, peripheral);
			pil = NULL;
		}
	}
	return pil;
}

static int msm_ipc_router_smd_remote_probe(struct platform_device *pdev)
{
	int rc;
	int id;		/*Index into the smd_xprt_cfg table*/

	id = find_smd_xprt_cfg(pdev);
	if (id < 0) {
		pr_err("%s: called for unknown ch %s\n",
			__func__, pdev->name);
		return id;
	}

	smd_remote_xprt[id].smd_xprt_wq =
		create_singlethread_workqueue(pdev->name);
	if (!smd_remote_xprt[id].smd_xprt_wq) {
		pr_err("%s: WQ creation failed for %s\n",
			__func__, pdev->name);
		return -EFAULT;
	}

	smd_remote_xprt[id].xprt.name = smd_xprt_cfg[id].xprt_name;
	smd_remote_xprt[id].xprt.link_id = smd_xprt_cfg[id].link_id;
	smd_remote_xprt[id].xprt.get_version =
		msm_ipc_router_smd_get_xprt_version;
	smd_remote_xprt[id].xprt.get_option =
		msm_ipc_router_smd_get_xprt_option;
	smd_remote_xprt[id].xprt.read_avail = NULL;
	smd_remote_xprt[id].xprt.read = NULL;
	smd_remote_xprt[id].xprt.write_avail =
		msm_ipc_router_smd_remote_write_avail;
	smd_remote_xprt[id].xprt.write = msm_ipc_router_smd_remote_write;
	smd_remote_xprt[id].xprt.close = msm_ipc_router_smd_remote_close;
	smd_remote_xprt[id].xprt.sft_close_done = smd_xprt_sft_close_done;
	smd_remote_xprt[id].xprt.priv = NULL;

	init_waitqueue_head(&smd_remote_xprt[id].write_avail_wait_q);
	smd_remote_xprt[id].in_pkt = NULL;
	smd_remote_xprt[id].is_partial_in_pkt = 0;
	INIT_DELAYED_WORK(&smd_remote_xprt[id].read_work, smd_xprt_read_data);
	spin_lock_init(&smd_remote_xprt[id].ss_reset_lock);
	smd_remote_xprt[id].ss_reset = 0;
	smd_remote_xprt[id].xprt_version = smd_xprt_cfg[id].xprt_version;
	smd_remote_xprt[id].xprt_option = FRAG_PKT_WRITE_ENABLE;

	smd_remote_xprt[id].pil = msm_ipc_load_subsystem(
					smd_xprt_cfg[id].edge);
	rc = smd_named_open_on_edge(smd_xprt_cfg[id].ch_name,
				    smd_xprt_cfg[id].edge,
				    &smd_remote_xprt[id].channel,
				    &smd_remote_xprt[id],
				    msm_ipc_router_smd_remote_notify);
	if (rc < 0) {
		pr_err("%s: Channel open failed for %s\n",
			__func__, smd_xprt_cfg[id].ch_name);
		if (smd_remote_xprt[id].pil) {
			subsystem_put(smd_remote_xprt[id].pil);
			smd_remote_xprt[id].pil = NULL;
		}
		destroy_workqueue(smd_remote_xprt[id].smd_xprt_wq);
		return rc;
	}

	smd_disable_read_intr(smd_remote_xprt[id].channel);

	smsm_change_state(SMSM_APPS_STATE, 0, SMSM_RPCINIT);

	return 0;
}

void *msm_ipc_load_default_node(void)
{
	void *pil = NULL;
	const char *peripheral;

	peripheral = smd_edge_to_subsystem(SMD_APPS_MODEM);
	if (peripheral && !strncmp(peripheral, "modem", 6)) {
		pil = subsystem_get(peripheral);
		if (IS_ERR(pil)) {
			pr_err("%s: Failed to load %s\n",
				__func__, peripheral);
			pil = NULL;
		}
	}
	return pil;
}
EXPORT_SYMBOL(msm_ipc_load_default_node);

void msm_ipc_unload_default_node(void *pil)
{
	if (pil)
		subsystem_put(pil);
}
EXPORT_SYMBOL(msm_ipc_unload_default_node);

static struct platform_driver msm_ipc_router_smd_remote_driver[] = {
	{
		.probe		= msm_ipc_router_smd_remote_probe,
		.driver		= {
				.name	= "RPCRPY_CNTL",
				.owner	= THIS_MODULE,
		},
	},
	{
		.probe		= msm_ipc_router_smd_remote_probe,
		.driver		= {
				.name	= "IPCRTR",
				.owner	= THIS_MODULE,
		},
	},
};

static int __init msm_ipc_router_smd_init(void)
{
	int i, ret, rc = 0;
	BUG_ON(ARRAY_SIZE(smd_xprt_cfg) != NUM_SMD_XPRTS);
	for (i = 0; i < ARRAY_SIZE(msm_ipc_router_smd_remote_driver); i++) {
		ret = platform_driver_register(
				&msm_ipc_router_smd_remote_driver[i]);
		if (ret) {
			pr_err("%s: Failed to register platform driver for"
			       " xprt%d. Continuing...\n", __func__, i);
			rc = ret;
		}
	}
	return rc;
}

module_init(msm_ipc_router_smd_init);
MODULE_DESCRIPTION("IPC Router SMD XPRT");
MODULE_LICENSE("GPL v2");
