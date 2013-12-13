/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <linux/of.h>
#include <linux/ipc_router_xprt.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <soc/qcom/subsystem_restart.h>

#include <mach/ipc_bridge.h>

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
 * msm_ipc_router_hsic_xprt - IPC Router's HSIC XPRT structure
 * @list: IPC router's HSIC XPRTs list.
 * @ch_name: Name of the HSIC endpoint exported by ipc_bridge driver.
 * @xprt_name: Name of the XPRT to be registered with IPC Router.
 * @driver: Platform drivers register by this XPRT.
 * @xprt: IPC Router XPRT structure to contain HSIC XPRT specific info.
 * @pdev: Platform device registered by IPC Bridge function driver.
 * @hsic_xprt_wq: Workqueue to queue read & other XPRT related works.
 * @read_work: Read Work to perform read operation from HSIC's ipc_bridge.
 * @in_pkt: Pointer to any partially read packet.
 * @ss_reset_lock: Lock to protect access to the ss_reset flag.
 * @ss_reset: flag used to check SSR state.
 * @sft_close_complete: Variable to indicate completion of SSR handling
 *                      by IPC Router.
 * @xprt_version: IPC Router header version supported by this XPRT.
 * @xprt_option: XPRT specific options to be handled by IPC Router.
 */
struct msm_ipc_router_hsic_xprt {
	struct list_head list;
	char ch_name[XPRT_NAME_LEN];
	char xprt_name[XPRT_NAME_LEN];
	struct platform_driver driver;
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

#define MODULE_NAME "ipc_router_hsic_xprt"
#define IPC_ROUTER_HSIC_XPRT_WAIT_TIMEOUT 3000
static int ipc_router_hsic_xprt_probe_done;
static struct delayed_work ipc_router_hsic_xprt_probe_work;
static DEFINE_MUTEX(hsic_remote_xprt_list_lock_lha1);
static LIST_HEAD(hsic_remote_xprt_list);

/**
 * find_hsic_xprt_list() - Find xprt item specific to an HSIC endpoint
 * @name: Name of the platform device to find in list
 *
 * @return: pointer to msm_ipc_router_hsic_xprt if matching endpoint is found,
 *		else NULL.
 *
 * This function is used to find specific xprt item from the global xprt list
 */
static struct msm_ipc_router_hsic_xprt *
		find_hsic_xprt_list(const char *name)
{
	struct msm_ipc_router_hsic_xprt *hsic_xprtp;

	mutex_lock(&hsic_remote_xprt_list_lock_lha1);
	list_for_each_entry(hsic_xprtp, &hsic_remote_xprt_list, list) {
		if (!strcmp(name, hsic_xprtp->ch_name)) {
			mutex_unlock(&hsic_remote_xprt_list_lock_lha1);
			return hsic_xprtp;
		}
	}
	mutex_unlock(&hsic_remote_xprt_list_lock_lha1);
	return NULL;
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
	struct ipc_bridge_platform_data *pdata;
	struct msm_ipc_router_hsic_xprt *hsic_xprtp;

	hsic_xprtp = find_hsic_xprt_list(pdev->name);
	if (!hsic_xprtp) {
		pr_err("%s No device with name %s\n", __func__, pdev->name);
		return -ENODEV;
	}

	mutex_lock(&hsic_xprtp->ss_reset_lock);
	hsic_xprtp->ss_reset = 1;
	mutex_unlock(&hsic_xprtp->ss_reset_lock);
	flush_workqueue(hsic_xprtp->hsic_xprt_wq);
	destroy_workqueue(hsic_xprtp->hsic_xprt_wq);
	init_completion(&hsic_xprtp->sft_close_complete);
	msm_ipc_router_xprt_notify(&hsic_xprtp->xprt,
				   IPC_ROUTER_XPRT_EVENT_CLOSE, NULL);
	D("%s: Notified IPC Router of %s CLOSE\n",
	  __func__, hsic_xprtp->xprt.name);
	wait_for_completion(&hsic_xprtp->sft_close_complete);
	hsic_xprtp->pdev = NULL;
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
	struct ipc_bridge_platform_data *pdata;
	struct msm_ipc_router_hsic_xprt *hsic_xprtp;

	pdata = pdev->dev.platform_data;
	if (!pdata || !pdata->open || !pdata->read ||
	    !pdata->write || !pdata->close) {
		pr_err("%s: pdata or pdata->operations is NULL\n", __func__);
		return -EINVAL;
	}

	hsic_xprtp = find_hsic_xprt_list(pdev->name);
	if (!hsic_xprtp) {
		pr_err("%s No device with name %s\n", __func__, pdev->name);
		return -ENODEV;
	}

	hsic_xprtp->hsic_xprt_wq =
		create_singlethread_workqueue(pdev->name);
	if (!hsic_xprtp->hsic_xprt_wq) {
		pr_err("%s: WQ creation failed for %s\n",
			__func__, pdev->name);
		return -EFAULT;
	}

	rc = pdata->open(pdev);
	if (rc < 0) {
		pr_err("%s: Channel open failed for %s.%d\n",
			__func__, pdev->name, pdev->id);
		destroy_workqueue(hsic_xprtp->hsic_xprt_wq);
		return rc;
	}
	hsic_xprtp->pdev = pdev;
	msm_ipc_router_xprt_notify(&hsic_xprtp->xprt,
				   IPC_ROUTER_XPRT_EVENT_OPEN, NULL);
	D("%s: Notified IPC Router of %s OPEN\n",
	  __func__, hsic_xprtp->xprt.name);
	queue_delayed_work(hsic_xprtp->hsic_xprt_wq,
			   &hsic_xprtp->read_work, 0);
	return 0;
}

/**
 * msm_ipc_router_hsic_driver_register() - register HSIC XPRT drivers
 *
 * @hsic_xprtp: pointer to IPC router hsic xprt structure.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called when a new XPRT is added to register platform
 * drivers for new XPRT.
 */
static int msm_ipc_router_hsic_driver_register(
			struct msm_ipc_router_hsic_xprt *hsic_xprtp)
{
	int ret;
	struct msm_ipc_router_hsic_xprt *hsic_xprtp_item;

	hsic_xprtp_item = find_hsic_xprt_list(hsic_xprtp->ch_name);

	mutex_lock(&hsic_remote_xprt_list_lock_lha1);
	list_add(&hsic_xprtp->list, &hsic_remote_xprt_list);
	mutex_unlock(&hsic_remote_xprt_list_lock_lha1);

	if (!hsic_xprtp_item) {
		hsic_xprtp->driver.driver.name = hsic_xprtp->ch_name;
		hsic_xprtp->driver.driver.owner = THIS_MODULE;
		hsic_xprtp->driver.probe = msm_ipc_router_hsic_remote_probe;
		hsic_xprtp->driver.remove = msm_ipc_router_hsic_remote_remove;

		ret = platform_driver_register(&hsic_xprtp->driver);
		if (ret) {
			pr_err("%s: Failed to register platform driver[%s]\n",
					__func__, hsic_xprtp->ch_name);
			return ret;
		}
	} else {
		pr_err("%s Already driver registered %s\n",
					__func__, hsic_xprtp->ch_name);
	}

	return 0;
}

/**
 * msm_ipc_router_hsic_config_init() - init HSIC xprt configs
 *
 * @hsic_xprt_config: pointer to HSIC xprt configurations.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called to initialize the HSIC XPRT pointer with
 * the HSIC XPRT configurations either from device tree or static arrays.
 */
static int msm_ipc_router_hsic_config_init(
		struct msm_ipc_router_hsic_xprt_config *hsic_xprt_config)
{
	struct msm_ipc_router_hsic_xprt *hsic_xprtp;

	hsic_xprtp = kzalloc(sizeof(struct msm_ipc_router_hsic_xprt),
							GFP_KERNEL);
	if (IS_ERR_OR_NULL(hsic_xprtp)) {
		pr_err("%s: kzalloc() failed for hsic_xprtp id:%s\n",
				__func__, hsic_xprt_config->ch_name);
		return -ENOMEM;
	}

	hsic_xprtp->xprt.link_id = hsic_xprt_config->link_id;
	hsic_xprtp->xprt_version = hsic_xprt_config->xprt_version;

	strlcpy(hsic_xprtp->ch_name, hsic_xprt_config->ch_name,
					XPRT_NAME_LEN);

	strlcpy(hsic_xprtp->xprt_name, hsic_xprt_config->xprt_name,
						XPRT_NAME_LEN);
	hsic_xprtp->xprt.name = hsic_xprtp->xprt_name;

	hsic_xprtp->xprt.get_version =
		msm_ipc_router_hsic_get_xprt_version;
	hsic_xprtp->xprt.get_option =
		 msm_ipc_router_hsic_get_xprt_option;
	hsic_xprtp->xprt.read_avail = NULL;
	hsic_xprtp->xprt.read = NULL;
	hsic_xprtp->xprt.write_avail =
		msm_ipc_router_hsic_remote_write_avail;
	hsic_xprtp->xprt.write = msm_ipc_router_hsic_remote_write;
	hsic_xprtp->xprt.close = msm_ipc_router_hsic_remote_close;
	hsic_xprtp->xprt.sft_close_done = hsic_xprt_sft_close_done;
	hsic_xprtp->xprt.priv = NULL;

	hsic_xprtp->in_pkt = NULL;
	INIT_DELAYED_WORK(&hsic_xprtp->read_work, hsic_xprt_read_data);
	mutex_init(&hsic_xprtp->ss_reset_lock);
	hsic_xprtp->ss_reset = 0;
	hsic_xprtp->xprt_option = 0;

	msm_ipc_router_hsic_driver_register(hsic_xprtp);
	return 0;

}

/**
 * parse_devicetree() - parse device tree binding
 *
 * @node: pointer to device tree node
 * @hsic_xprt_config: pointer to HSIC XPRT configurations
 *
 * @return: 0 on success, -ENODEV on failure.
 */
static int parse_devicetree(struct device_node *node,
		struct msm_ipc_router_hsic_xprt_config *hsic_xprt_config)
{
	int ret;
	int link_id;
	int version;
	char *key;
	const char *ch_name;
	const char *remote_ss;

	key = "qcom,ch-name";
	ch_name = of_get_property(node, key, NULL);
	if (!ch_name)
		goto error;
	strlcpy(hsic_xprt_config->ch_name, ch_name, XPRT_NAME_LEN);

	key = "qcom,xprt-remote";
	remote_ss = of_get_property(node, key, NULL);
	if (!remote_ss)
		goto error;

	key = "qcom,xprt-linkid";
	ret = of_property_read_u32(node, key, &link_id);
	if (ret)
		goto error;
	hsic_xprt_config->link_id = link_id;

	key = "qcom,xprt-version";
	ret = of_property_read_u32(node, key, &version);
	if (ret)
		goto error;
	hsic_xprt_config->xprt_version = version;

	scnprintf(hsic_xprt_config->xprt_name, XPRT_NAME_LEN, "%s_%s",
			remote_ss, hsic_xprt_config->ch_name);

	return 0;

error:
	pr_err("%s: missing key: %s\n", __func__, key);
	return -ENODEV;
}

/**
 * msm_ipc_router_hsic_xprt_probe() - Probe an HSIC xprt
 * @pdev: Platform device corresponding to HSIC xprt.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called when the underlying device tree driver registers
 * a platform device, mapped to an HSIC transport.
 */
static int msm_ipc_router_hsic_xprt_probe(
				struct platform_device *pdev)
{
	int ret;
	struct msm_ipc_router_hsic_xprt_config hsic_xprt_config;

	if (pdev && pdev->dev.of_node) {
		mutex_lock(&hsic_remote_xprt_list_lock_lha1);
		ipc_router_hsic_xprt_probe_done = 1;
		mutex_unlock(&hsic_remote_xprt_list_lock_lha1);

		ret = parse_devicetree(pdev->dev.of_node,
						&hsic_xprt_config);
		if (ret) {
			pr_err(" failed to parse device tree\n");
			return ret;
		}

		ret = msm_ipc_router_hsic_config_init(
						&hsic_xprt_config);
		if (ret) {
			pr_err(" %s init failed\n", __func__);
			return ret;
		}
	}
	return ret;
}

/**
 * ipc_router_hsic_xprt_probe_worker() - probe worker for non DT configurations
 *
 * @work: work item to process
 *
 * This function is called by schedule_delay_work after 3sec and check if
 * device tree probe is done or not. If device tree probe fails the default
 * configurations read from static array.
 */
static void ipc_router_hsic_xprt_probe_worker(struct work_struct *work)
{
	int i, ret;

	BUG_ON(ARRAY_SIZE(hsic_xprt_cfg) != NUM_HSIC_XPRTS);

	mutex_lock(&hsic_remote_xprt_list_lock_lha1);
	if (!ipc_router_hsic_xprt_probe_done) {
		mutex_unlock(&hsic_remote_xprt_list_lock_lha1);
		for (i = 0; i < ARRAY_SIZE(hsic_xprt_cfg); i++) {
			ret = msm_ipc_router_hsic_config_init(
							&hsic_xprt_cfg[i]);
			if (ret)
				pr_err(" %s init failed config idx %d\n",
								__func__, i);
		}
		mutex_lock(&hsic_remote_xprt_list_lock_lha1);
	}
	mutex_unlock(&hsic_remote_xprt_list_lock_lha1);
}

static struct of_device_id msm_ipc_router_hsic_xprt_match_table[] = {
	{ .compatible = "qcom,ipc_router_hsic_xprt" },
	{},
};

static struct platform_driver msm_ipc_router_hsic_xprt_driver = {
	.probe = msm_ipc_router_hsic_xprt_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_ipc_router_hsic_xprt_match_table,
	 },
};

static int __init msm_ipc_router_hsic_xprt_init(void)
{
	int rc;

	rc = platform_driver_register(&msm_ipc_router_hsic_xprt_driver);
	if (rc) {
		pr_err("%s: msm_ipc_router_hsic_xprt_driver register failed %d\n",
								__func__, rc);
		return rc;
	}

	INIT_DELAYED_WORK(&ipc_router_hsic_xprt_probe_work,
					ipc_router_hsic_xprt_probe_worker);
	schedule_delayed_work(&ipc_router_hsic_xprt_probe_work,
			msecs_to_jiffies(IPC_ROUTER_HSIC_XPRT_WAIT_TIMEOUT));
	return 0;
}

module_init(msm_ipc_router_hsic_xprt_init);
MODULE_DESCRIPTION("IPC Router HSIC XPRT");
MODULE_LICENSE("GPL v2");
