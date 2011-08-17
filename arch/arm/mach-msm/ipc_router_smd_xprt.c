/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/types.h>

#include <mach/msm_smd.h>

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

struct msm_ipc_router_smd_xprt {
	struct msm_ipc_router_xprt xprt;

	smd_channel_t *channel;
};

static struct msm_ipc_router_smd_xprt smd_remote_xprt;

static void smd_xprt_read_data(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_read_data, smd_xprt_read_data);
static struct workqueue_struct *smd_xprt_workqueue;

static wait_queue_head_t write_avail_wait_q;
static struct rr_packet *in_pkt;
static int is_partial_in_pkt;

static int msm_ipc_router_smd_remote_write_avail(void)
{
	return smd_write_avail(smd_remote_xprt.channel);
}

static int msm_ipc_router_smd_remote_write(void *data,
					   uint32_t len,
					   uint32_t type)
{
	struct rr_packet *pkt = (struct rr_packet *)data;
	struct sk_buff *ipc_rtr_pkt;
	int align_sz, align_data = 0;
	int offset, sz_written = 0;

	if (!pkt)
		return -EINVAL;

	if (!len || pkt->length != len)
		return -EINVAL;

	align_sz = ALIGN_SIZE(pkt->length);
	while (smd_write_start(smd_remote_xprt.channel, (len + align_sz)) < 0)
		msleep(50);

	D("%s: Ready to write\n", __func__);
	skb_queue_walk(pkt->pkt_fragment_q, ipc_rtr_pkt) {
		offset = 0;
		while (offset < ipc_rtr_pkt->len) {
			if (!smd_write_avail(smd_remote_xprt.channel))
				smd_enable_read_intr(smd_remote_xprt.channel);

			wait_event_interruptible_timeout(write_avail_wait_q,
				smd_write_avail(smd_remote_xprt.channel),
				msecs_to_jiffies(50));
			smd_disable_read_intr(smd_remote_xprt.channel);

			sz_written = smd_write_segment(smd_remote_xprt.channel,
					  ipc_rtr_pkt->data + offset,
					  (ipc_rtr_pkt->len - offset), 0);
			offset += sz_written;
			sz_written = 0;
		}
		D("%s: Wrote %d bytes\n", __func__, offset);
	}

	if (align_sz) {
		if (smd_write_avail(smd_remote_xprt.channel) < align_sz)
			smd_enable_read_intr(smd_remote_xprt.channel);

		wait_event_interruptible_timeout(write_avail_wait_q,
			(smd_write_avail(smd_remote_xprt.channel) >=
			 align_sz), msecs_to_jiffies(50));
		smd_disable_read_intr(smd_remote_xprt.channel);

		smd_write_segment(smd_remote_xprt.channel,
				  &align_data, align_sz, 0);
		D("%s: Wrote %d align bytes\n", __func__, align_sz);
	}
	if (!smd_write_end(smd_remote_xprt.channel))
		D("%s: Finished writing\n", __func__);
	return len;
}

static int msm_ipc_router_smd_remote_close(void)
{
	smsm_change_state(SMSM_APPS_STATE, SMSM_RPCINIT, 0);
	return smd_close(smd_remote_xprt.channel);
}

static void smd_xprt_read_data(struct work_struct *work)
{
	int pkt_size, sz_read, sz;
	struct sk_buff *ipc_rtr_pkt;
	void *data;

	D("%s pkt_size: %d, read_avail: %d\n", __func__,
		smd_cur_packet_size(smd_remote_xprt.channel),
		smd_read_avail(smd_remote_xprt.channel));
	while ((pkt_size = smd_cur_packet_size(smd_remote_xprt.channel)) &&
		smd_read_avail(smd_remote_xprt.channel)) {
		if (!is_partial_in_pkt) {
			in_pkt = kzalloc(sizeof(struct rr_packet), GFP_KERNEL);
			if (!in_pkt) {
				pr_err("%s: Couldn't alloc rr_packet\n",
					__func__);
				return;
			}

			in_pkt->pkt_fragment_q = kmalloc(
						  sizeof(struct sk_buff_head),
						  GFP_KERNEL);
			if (!in_pkt->pkt_fragment_q) {
				pr_err("%s: Couldn't alloc pkt_fragment_q\n",
					__func__);
				kfree(in_pkt);
				return;
			}
			skb_queue_head_init(in_pkt->pkt_fragment_q);
			is_partial_in_pkt = 1;
			D("%s: Allocated rr_packet\n", __func__);
		}

		if ((pkt_size >= MIN_FRAG_SZ) &&
		    (smd_read_avail(smd_remote_xprt.channel) < MIN_FRAG_SZ))
			return;

		sz = smd_read_avail(smd_remote_xprt.channel);
		do {
			ipc_rtr_pkt = alloc_skb(sz, GFP_KERNEL);
			if (!ipc_rtr_pkt) {
				if (sz <= (PAGE_SIZE/2)) {
					queue_delayed_work(smd_xprt_workqueue,
						   &work_read_data,
						   msecs_to_jiffies(100));
					return;
				}
				sz = sz / 2;
			}
		} while (!ipc_rtr_pkt);

		D("%s: Allocated the sk_buff of size %d\n",
			__func__, sz);
		data = skb_put(ipc_rtr_pkt, sz);
		sz_read = smd_read(smd_remote_xprt.channel, data, sz);
		if (sz_read != sz) {
			pr_err("%s: Couldn't read completely\n", __func__);
			kfree_skb(ipc_rtr_pkt);
			release_pkt(in_pkt);
			is_partial_in_pkt = 0;
			return;
		}
		skb_queue_tail(in_pkt->pkt_fragment_q, ipc_rtr_pkt);
		in_pkt->length += sz_read;
		if (sz_read != pkt_size)
			is_partial_in_pkt = 1;
		else
			is_partial_in_pkt = 0;

		if (!is_partial_in_pkt) {
			D("%s: Packet size read %d\n",
				__func__, in_pkt->length);
			msm_ipc_router_xprt_notify(&smd_remote_xprt.xprt,
					   IPC_ROUTER_XPRT_EVENT_DATA,
					   (void *)in_pkt);
			release_pkt(in_pkt);
			in_pkt = NULL;
		}
	}
}

static void msm_ipc_router_smd_remote_notify(void *_dev, unsigned event)
{
	if (event == SMD_EVENT_DATA) {
		if (smd_read_avail(smd_remote_xprt.channel))
			queue_delayed_work(smd_xprt_workqueue,
					   &work_read_data, 0);
		if (smd_write_avail(smd_remote_xprt.channel))
			wake_up(&write_avail_wait_q);
	}
}

static int msm_ipc_router_smd_remote_probe(struct platform_device *pdev)
{
	int rc;

	smd_xprt_workqueue = create_singlethread_workqueue("smd_xprt");
	if (!smd_xprt_workqueue)
		return -ENOMEM;

	smd_remote_xprt.xprt.name = "msm_ipc_router_smd_xprt";
	smd_remote_xprt.xprt.link_id = 1;
	smd_remote_xprt.xprt.read_avail = NULL;
	smd_remote_xprt.xprt.read = NULL;
	smd_remote_xprt.xprt.write_avail =
		msm_ipc_router_smd_remote_write_avail;
	smd_remote_xprt.xprt.write = msm_ipc_router_smd_remote_write;
	smd_remote_xprt.xprt.close = msm_ipc_router_smd_remote_close;
	smd_remote_xprt.xprt.priv = NULL;

	init_waitqueue_head(&write_avail_wait_q);

	rc = smd_open("RPCRPY_CNTL", &smd_remote_xprt.channel, NULL,
		      msm_ipc_router_smd_remote_notify);
	if (rc < 0) {
		destroy_workqueue(smd_xprt_workqueue);
		return rc;
	}

	smd_disable_read_intr(smd_remote_xprt.channel);

	msm_ipc_router_xprt_notify(&smd_remote_xprt.xprt,
				  IPC_ROUTER_XPRT_EVENT_OPEN,
				  NULL);
	D("%s: Notified IPC Router of OPEN Event\n", __func__);

	smsm_change_state(SMSM_APPS_STATE, 0, SMSM_RPCINIT);

	return 0;
}

static struct platform_driver msm_ipc_router_smd_remote_driver = {
	.probe		= msm_ipc_router_smd_remote_probe,
	.driver		= {
			.name	= "RPCRPY_CNTL",
			.owner	= THIS_MODULE,
	},
};

static int __init msm_ipc_router_smd_init(void)
{
	return platform_driver_register(&msm_ipc_router_smd_remote_driver);
}

module_init(msm_ipc_router_smd_init);
MODULE_DESCRIPTION("RPC Router SMD XPRT");
MODULE_LICENSE("GPL v2");
