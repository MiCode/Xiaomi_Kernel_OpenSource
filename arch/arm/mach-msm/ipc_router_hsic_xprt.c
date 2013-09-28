/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
 * IPC ROUTER HSIC XPRT module.
 */
#define DEBUG

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#include <mach/ipc_bridge.h>
#include <mach/subsystem_restart.h>

#include "ipc_router.h"

static int msm_ipc_router_hsic_xprt_debug_mask;
module_param_named(debug_mask, msm_ipc_router_hsic_xprt_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#if defined(DEBUG)
#define D(x...) do { \
if (msm_ipc_router_hsic_xprt_debug_mask) \
	pr_info(x); \
} while (0)
#else
#define D(x...) do { } while (0)
#endif

#define NUM_HSIC_XPRTS 1
#define XPRT_NAME_LEN 32

/**
 * msm_ipc_router_hsic_xprt - IPC Router's HSIC XPRT strucutre
 * @xprt: IPC Router XPRT structure to contain HSIC XPRT specific info.
 * @pdev: Platform device registered by IPC Bridge function driver.
 * @hsic_xprt_wq: Workqueue to queue read & other XPRT related works.
 * @read_work: Read Work to perform read operation from HSIC's ipc_bridge.
 * @in_pkt: Pointer to any partially read packet.
 * @ss_reset_lock: Lock to protect access to the ss_reset flag.
 * @sft_close_complete: Variable to indicate completion of SSR handling
 *                      by IPC Router.
 * @xprt_version: IPC Router header version supported by this XPRT.
 * @xprt_option: XPRT specific options to be handled by IPC Router.
 */
struct msm_ipc_router_hsic_xprt {
	struct msm_ipc_router_xprt xprt;
	struct platform_device *pdev;
	struct workqueue_struct *hsic_xprt_wq;
	struct delayed_work read_work;
	struct rr_packet *in_pkt;
	struct mutex ss_reset_lock;
	int ss_reset;
	struct completion sft_close_complete;
	unsigned xprt_version;
	unsigned xprt_option;
};

struct msm_ipc_router_hsic_xprt_work {
	struct msm_ipc_router_xprt *xprt;
	struct work_struct work;
};

static void hsic_xprt_read_data(struct work_struct *work);

/**
 * msm_ipc_router_hsic_xprt_config - Config. Info. of each HSIC XPRT
 * @ch_name: Name of the HSIC endpoint exported by ipc_bridge driver.
 * @xprt_name: Name of the XPRT to be registered with IPC Router.
 * @hsic_pdev_id: ID to differentiate among multiple ipc_bridge endpoints.
 * @link_id: Network Cluster ID to which this XPRT belongs to.
 * @xprt_version: IPC Router header version supported by this XPRT.
 */
struct msm_ipc_router_hsic_xprt_config {
	char ch_name[XPRT_NAME_LEN];
	char xprt_name[XPRT_NAME_LEN];
	int hsic_pdev_id;
	uint32_t link_id;
	unsigned xprt_version;
};

struct msm_ipc_router_hsic_xprt_config hsic_xprt_cfg[] = {
	{"ipc_bridge", "ipc_rtr_ipc_bridge1", 1, 1, 3},
};

static struct msm_ipc_router_hsic_xprt hsic_remote_xprt[NUM_HSIC_XPRTS];

/**
 * find_hsic_xprt_cfg() - Find the config info specific to an HSIC endpoint
 * @pdev: Platform device registered by HSIC's ipc_bridge driver
 *
 * @return: Index to the entry in the hsic_remote_xprt table if matching
 *          endpoint is found, < 0 on error.
 *
 * This function is used to find the configuration information specific to
 * an HSIC endpoint from the hsic_remote_xprt table.
 */
static int find_hsic_xprt_cfg(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < NUM_HSIC_XPRTS; i++) {
		/* TODO: Update the condition for multiple hsic links */
		if (!strncmp(pdev->name, hsic_xprt_cfg[i].ch_name, 32))
			return i;
	}

	return -ENODEV;
}

/**
 * msm_ipc_router_hsic_get_xprt_version() - Get IPC Router header version
 *                                          supported by the XPRT
 * @xprt: XPRT for which the version information is required.
 *
 * @return: IPC Router header version supported by the XPRT.
 */
static int msm_ipc_router_hsic_get_xprt_version(
	struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_hsic_xprt *hsic_xprtp;
	if (!xprt)
		return -EINVAL;
	hsic_xprtp = container_of(xprt, struct msm_ipc_router_hsic_xprt, xprt);

	return (int)hsic_xprtp->xprt_version;
}

/**
 * msm_ipc_router_hsic_get_xprt_option() - Get XPRT options
 * @xprt: XPRT for which the option information is required.
 *
 * @return: Options supported by the XPRT.
 */
static int msm_ipc_router_hsic_get_xprt_option(
	struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_hsic_xprt *hsic_xprtp;
	if (!xprt)
		return -EINVAL;
	hsic_xprtp = container_of(xprt, struct msm_ipc_router_hsic_xprt, xprt);

	return (int)hsic_xprtp->xprt_option;
}

/**
 * msm_ipc_router_hsic_remote_write_avail() - Get available write space
 * @xprt: XPRT for which the available write space info. is required.
 *
 * @return: Write space in bytes on success, 0 on SSR.
 */
static int msm_ipc_router_hsic_remote_write_avail(
	struct msm_ipc_router_xprt *xprt)
{
	struct ipc_bridge_platform_data *pdata;
	int write_avail;
	struct msm_ipc_router_hsic_xprt *hsic_xprtp =
		container_of(xprt, struct msm_ipc_router_hsic_xprt, xprt);

	mutex_lock(&hsic_xprtp->ss_reset_lock);
	if (hsic_xprtp->ss_reset || !hsic_xprtp->pdev) {
		write_avail = 0;
	} else {
		pdata = hsic_xprtp->pdev->dev.platform_data;
		write_avail = pdata->max_write_size;
	}
	mutex_unlock(&hsic_xprtp->ss_reset_lock);
	return write_avail;
}

/**
 * msm_ipc_router_hsic_remote_write() - Write to XPRT
 * @data: Data to be written to the XPRT.
 * @len: Length of the data to be written.
 * @xprt: XPRT to which the data has to be written.
 *
 * @return: Data Length on success, standard Linux error codes on failure.
 */
static int msm_ipc_router_hsic_remote_write(void *data,
		uint32_t len, struct msm_ipc_router_xprt *xprt)
{
	struct rr_packet *pkt = (struct rr_packet *)data;
	struct sk_buff *skb;
	struct ipc_bridge_platform_data *pdata;
	struct msm_ipc_router_hsic_xprt *hsic_xprtp;
	int ret;

	if (!pkt || pkt->length != len || !xprt) {
		pr_err("%s: Invalid input parameters\n", __func__);
		return -EINVAL;
	}

	hsic_xprtp = container_of(xprt, struct msm_ipc_router_hsic_xprt, xprt);
	mutex_lock(&hsic_xprtp->ss_reset_lock);
	if (hsic_xprtp->ss_reset) {
		pr_err("%s: Trying to write on a reset link\n", __func__);
		mutex_unlock(&hsic_xprtp->ss_reset_lock);
		return -ENETRESET;
	}

	if (!hsic_xprtp->pdev) {
		pr_err("%s: Trying to write on a closed link\n", __func__);
		mutex_unlock(&hsic_xprtp->ss_reset_lock);
		return -ENODEV;
	}

	pdata = hsic_xprtp->pdev->dev.platform_data;
	if (!pdata || !pdata->write) {
		pr_err("%s on a uninitialized link\n", __func__);
		mutex_unlock(&hsic_xprtp->ss_reset_lock);
		return -EFAULT;
	}

	skb = skb_peek(pkt->pkt_fragment_q);
	if (!skb) {
		pr_err("%s SKB is NULL\n", __func__);
		mutex_unlock(&hsic_xprtp->ss_reset_lock);
		return -EINVAL;
	}
	D("%s: About to write %d bytes\n", __func__, len);
	ret = pdata->write(hsic_xprtp->pdev, skb->data, skb->len);
	if (ret == skb->len)
		ret = len;
	D("%s: Finished writing %d bytes\n", __func__, len);
	mutex_unlock(&hsic_xprtp->ss_reset_lock);
	return ret;
}

/**
 * msm_ipc_router_hsic_remote_close() - Close the XPRT
 * @xprt: XPRT which needs to be closed.
 *
 * @return: 0 on success, standard Linux error codes on failure.
 */
static int msm_ipc_router_hsic_remote_close(
	struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_hsic_xprt *hsic_xprtp;
	struct ipc_bridge_platform_data *pdata;

	if (!xprt)
		return -EINVAL;
	hsic_xprtp = container_of(xprt, struct msm_ipc_router_hsic_xprt, xprt);

	mutex_lock(&hsic_xprtp->ss_reset_lock);
	hsic_xprtp->ss_reset = 1;
	mutex_unlock(&hsic_xprtp->ss_reset_lock);
	flush_workqueue(hsic_xprtp->hsic_xprt_wq);
	destroy_workqueue(hsic_xprtp->hsic_xprt_wq);
	pdata = hsic_xprtp->pdev->dev.platform_data;
	if (pdata && pdata->close)
		pdata->close(hsic_xprtp->pdev);
	hsic_xprtp->pdev = NULL;
	return 0;
}

/**
 * hsic_xprt_read_data() - Read work to read from the XPRT
 * @work: Read work to be executed.
 *
 * This function is a read work item queued on a XPRT specific workqueue.
 * The work parameter contains information regarding the XPRT on which this
 * read work has to be performed. The work item keeps reading from the HSIC
 * endpoint, until the endpoint returns an error.
 */
static void hsic_xprt_read_data(struct work_struct *work)
{
	int pkt_size;
	struct sk_buff *skb = NULL;
	void *data;
	struct ipc_bridge_platform_data *pdata;
	struct delayed_work *rwork = to_delayed_work(work);
	struct msm_ipc_router_hsic_xprt *hsic_xprtp =
		container_of(rwork, struct msm_ipc_router_hsic_xprt, read_work);

	while (1) {
		mutex_lock(&hsic_xprtp->ss_reset_lock);
		if (hsic_xprtp->ss_reset) {
			mutex_unlock(&hsic_xprtp->ss_reset_lock);
			break;
		}
		pdata = hsic_xprtp->pdev->dev.platform_data;
		mutex_unlock(&hsic_xprtp->ss_reset_lock);
		while (!hsic_xprtp->in_pkt) {
			hsic_xprtp->in_pkt = kzalloc(sizeof(struct rr_packet),
						     GFP_KERNEL);
			if (hsic_xprtp->in_pkt)
				break;
			pr_err("%s: packet allocation failure\n", __func__);
			msleep(100);
		}
		while (!hsic_xprtp->in_pkt->pkt_fragment_q) {
			hsic_xprtp->in_pkt->pkt_fragment_q =
				kmalloc(sizeof(struct sk_buff_head),
					GFP_KERNEL);
			if (hsic_xprtp->in_pkt->pkt_fragment_q)
				break;
			pr_err("%s: Couldn't alloc pkt_fragment_q\n",
				__func__);
			msleep(100);
		}
		skb_queue_head_init(hsic_xprtp->in_pkt->pkt_fragment_q);
		D("%s: Allocated rr_packet\n", __func__);

		while (!skb) {
			skb = alloc_skb(pdata->max_read_size, GFP_KERNEL);
			if (skb)
				break;
			pr_err("%s: Couldn't alloc SKB\n", __func__);
			msleep(100);
		}
		data = skb_put(skb, pdata->max_read_size);
		pkt_size = pdata->read(hsic_xprtp->pdev, data,
					pdata->max_read_size);
		if (pkt_size < 0) {
			pr_err("%s: Error %d @ read operation\n",
				__func__, pkt_size);
			kfree_skb(skb);
			kfree(hsic_xprtp->in_pkt->pkt_fragment_q);
			kfree(hsic_xprtp->in_pkt);
			break;
		}
		skb_queue_tail(hsic_xprtp->in_pkt->pkt_fragment_q, skb);
		hsic_xprtp->in_pkt->length = pkt_size;
		D("%s: Packet size read %d\n", __func__, pkt_size);
		msm_ipc_router_xprt_notify(&hsic_xprtp->xprt,
			IPC_ROUTER_XPRT_EVENT_DATA, (void *)hsic_xprtp->in_pkt);
		release_pkt(hsic_xprtp->in_pkt);
		hsic_xprtp->in_pkt = NULL;
		skb = NULL;
	}
}

/**
 * hsic_xprt_sft_close_done() - Completion of XPRT reset
 * @xprt: XPRT on which the reset operation is complete.
 *
 * This function is used by IPC Router to signal this HSIC XPRT Abstraction
 * Layer(XAL) that the reset of XPRT is completely handled by IPC Router.
 */
static void hsic_xprt_sft_close_done(struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_hsic_xprt *hsic_xprtp =
		container_of(xprt, struct msm_ipc_router_hsic_xprt, xprt);

	complete_all(&hsic_xprtp->sft_close_complete);
}

/**
 * msm_ipc_router_hsic_remote_remove() - Remove an HSIC endpoint
 * @pdev: Platform device corresponding to HSIC endpoint.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called when the underlying ipc_bridge driver unregisters
 * a platform device, mapped to an HSIC endpoint, during SSR.
 */
static int msm_ipc_router_hsic_remote_remove(struct platform_device *pdev)
{
	int id;
	struct ipc_bridge_platform_data *pdata;

	id = find_hsic_xprt_cfg(pdev);
	if (id < 0) {
		pr_err("%s: called for unknown ch %s\n",
			__func__, pdev->name);
		return id;
	}

	mutex_lock(&hsic_remote_xprt[id].ss_reset_lock);
	hsic_remote_xprt[id].ss_reset = 1;
	mutex_unlock(&hsic_remote_xprt[id].ss_reset_lock);
	flush_workqueue(hsic_remote_xprt[id].hsic_xprt_wq);
	destroy_workqueue(hsic_remote_xprt[id].hsic_xprt_wq);
	init_completion(&hsic_remote_xprt[id].sft_close_complete);
	msm_ipc_router_xprt_notify(&hsic_remote_xprt[id].xprt,
				   IPC_ROUTER_XPRT_EVENT_CLOSE, NULL);
	D("%s: Notified IPC Router of %s CLOSE\n",
	  __func__, hsic_remote_xprt[id].xprt.name);
	wait_for_completion(&hsic_remote_xprt[id].sft_close_complete);
	hsic_remote_xprt[id].pdev = NULL;
	pdata = pdev->dev.platform_data;
	if (pdata && pdata->close)
		pdata->close(pdev);
	return 0;
}

/**
 * msm_ipc_router_hsic_remote_probe() - Probe an HSIC endpoint
 * @pdev: Platform device corresponding to HSIC endpoint.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called when the underlying ipc_bridge driver registers
 * a platform device, mapped to an HSIC endpoint.
 */
static int msm_ipc_router_hsic_remote_probe(struct platform_device *pdev)
{
	int rc;
	int id;		/*Index into the hsic_xprt_cfg table*/
	struct ipc_bridge_platform_data *pdata;

	pdata = pdev->dev.platform_data;
	if (!pdata || !pdata->open || !pdata->read ||
	    !pdata->write || !pdata->close) {
		pr_err("%s: pdata or pdata->operations is NULL\n", __func__);
		return -EINVAL;
	}

	id = find_hsic_xprt_cfg(pdev);
	if (id < 0) {
		pr_err("%s: called for unknown ch %s\n",
			__func__, pdev->name);
		return id;
	}

	hsic_remote_xprt[id].hsic_xprt_wq =
		create_singlethread_workqueue(pdev->name);
	if (!hsic_remote_xprt[id].hsic_xprt_wq) {
		pr_err("%s: WQ creation failed for %s\n",
			__func__, pdev->name);
		return -EFAULT;
	}

	hsic_remote_xprt[id].xprt.name = hsic_xprt_cfg[id].xprt_name;
	hsic_remote_xprt[id].xprt.link_id = hsic_xprt_cfg[id].link_id;
	hsic_remote_xprt[id].xprt.get_version =
		msm_ipc_router_hsic_get_xprt_version;
	hsic_remote_xprt[id].xprt.get_option =
		 msm_ipc_router_hsic_get_xprt_option;
	hsic_remote_xprt[id].xprt.read_avail = NULL;
	hsic_remote_xprt[id].xprt.read = NULL;
	hsic_remote_xprt[id].xprt.write_avail =
		msm_ipc_router_hsic_remote_write_avail;
	hsic_remote_xprt[id].xprt.write = msm_ipc_router_hsic_remote_write;
	hsic_remote_xprt[id].xprt.close = msm_ipc_router_hsic_remote_close;
	hsic_remote_xprt[id].xprt.sft_close_done = hsic_xprt_sft_close_done;
	hsic_remote_xprt[id].xprt.priv = NULL;

	hsic_remote_xprt[id].in_pkt = NULL;
	INIT_DELAYED_WORK(&hsic_remote_xprt[id].read_work, hsic_xprt_read_data);
	mutex_init(&hsic_remote_xprt[id].ss_reset_lock);
	hsic_remote_xprt[id].ss_reset = 0;
	hsic_remote_xprt[id].xprt_version = hsic_xprt_cfg[id].xprt_version;
	hsic_remote_xprt[id].xprt_option = 0;

	rc = pdata->open(pdev);
	if (rc < 0) {
		pr_err("%s: Channel open failed for %s.%d\n",
			__func__, pdev->name, pdev->id);
		destroy_workqueue(hsic_remote_xprt[id].hsic_xprt_wq);
		return rc;
	}
	hsic_remote_xprt[id].pdev = pdev;
	msm_ipc_router_xprt_notify(&hsic_remote_xprt[id].xprt,
				   IPC_ROUTER_XPRT_EVENT_OPEN, NULL);
	D("%s: Notified IPC Router of %s OPEN\n",
	  __func__, hsic_remote_xprt[id].xprt.name);
	queue_delayed_work(hsic_remote_xprt[id].hsic_xprt_wq,
			   &hsic_remote_xprt[id].read_work, 0);
	return 0;
}

static struct platform_driver msm_ipc_router_hsic_remote_driver[] = {
	{
		.probe		= msm_ipc_router_hsic_remote_probe,
		.remove		= msm_ipc_router_hsic_remote_remove,
		.driver		= {
				.name	= "ipc_bridge",
				.owner	= THIS_MODULE,
		},
	},
};

static int __init msm_ipc_router_hsic_init(void)
{
	int i, ret, rc = 0;
	BUG_ON(ARRAY_SIZE(hsic_xprt_cfg) != NUM_HSIC_XPRTS);
	for (i = 0; i < ARRAY_SIZE(msm_ipc_router_hsic_remote_driver); i++) {
		ret = platform_driver_register(
				&msm_ipc_router_hsic_remote_driver[i]);
		if (ret) {
			pr_err("%s: Failed to register platform driver for xprt%d. Continuing...\n",
				__func__, i);
			rc = ret;
		}
	}
	return rc;
}

module_init(msm_ipc_router_hsic_init);
MODULE_DESCRIPTION("IPC Router HSIC XPRT");
MODULE_LICENSE("GPL v2");
