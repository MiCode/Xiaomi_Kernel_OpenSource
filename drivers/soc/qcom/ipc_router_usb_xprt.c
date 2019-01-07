/*
 * Copyright (c) 2013-2016, 2018 The Linux Foundation. All rights reserved.
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
 * IPC ROUTER USB XPRT module.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/ipc_router_xprt.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <soc/qcom/subsystem_restart.h>

#include <linux/usb/ipc_bridge.h>

static int msm_ipc_router_usb_xprt_debug_mask;
module_param_named(debug_mask, msm_ipc_router_usb_xprt_debug_mask,
		   int, 0664);

#define D(x...) do { \
if (msm_ipc_router_usb_xprt_debug_mask) \
	pr_info(x); \
} while (0)

#define NUM_USB_XPRTS 1
#define XPRT_NAME_LEN 32

/**
 * msm_ipc_router_usb_xprt - IPC Router's USB XPRT structure
 * @list: IPC router's USB XPRTs list.
 * @ch_name: Name of the USB endpoint exported by ipc_bridge driver.
 * @xprt_name: Name of the XPRT to be registered with IPC Router.
 * @driver: Platform drivers register by this XPRT.
 * @xprt: IPC Router XPRT structure to contain USB XPRT specific info.
 * @pdev: Platform device registered by IPC Bridge function driver.
 * @usb_xprt_wq: Workqueue to queue read & other XPRT related works.
 * @read_work: Read Work to perform read operation from USB's ipc_bridge.
 * @in_pkt: Pointer to any partially read packet.
 * @ss_reset_lock: Lock to protect access to the ss_reset flag.
 * @ss_reset: flag used to check SSR state.
 * @sft_close_complete: Variable to indicate completion of SSR handling
 *                      by IPC Router.
 * @xprt_version: IPC Router header version supported by this XPRT.
 * @xprt_option: XPRT specific options to be handled by IPC Router.
 */
struct msm_ipc_router_usb_xprt {
	struct list_head list;
	char ch_name[XPRT_NAME_LEN];
	char xprt_name[XPRT_NAME_LEN];
	struct platform_driver driver;
	struct msm_ipc_router_xprt xprt;
	struct platform_device *pdev;
	struct workqueue_struct *usb_xprt_wq;
	struct delayed_work read_work;
	struct rr_packet *in_pkt;
	struct mutex ss_reset_lock;
	int ss_reset;
	struct completion sft_close_complete;
	unsigned int xprt_version;
	unsigned int xprt_option;
};

struct msm_ipc_router_usb_xprt_work {
	struct msm_ipc_router_xprt *xprt;
	struct work_struct work;
};

static void usb_xprt_read_data(struct work_struct *work);

/**
 * msm_ipc_router_usb_xprt_config - Config. Info. of each USB XPRT
 * @ch_name: Name of the USB endpoint exported by ipc_bridge driver.
 * @xprt_name: Name of the XPRT to be registered with IPC Router.
 * @usb_pdev_id: ID to differentiate among multiple ipc_bridge endpoints.
 * @link_id: Network Cluster ID to which this XPRT belongs to.
 * @xprt_version: IPC Router header version supported by this XPRT.
 */
struct msm_ipc_router_usb_xprt_config {
	char ch_name[XPRT_NAME_LEN];
	char xprt_name[XPRT_NAME_LEN];
	int usb_pdev_id;
	uint32_t link_id;
	unsigned int xprt_version;
};

static struct msm_ipc_router_usb_xprt_config usb_xprt_cfg[] = {
	{"ipc_bridge", "ipc_rtr_ipc_bridge1", 1, 2, 3},
};

#define MODULE_NAME "ipc_router_usb_xprt"
#define IPC_ROUTER_USB_XPRT_WAIT_TIMEOUT 3000
static int ipc_router_usb_xprt_probe_done;
static struct delayed_work ipc_router_usb_xprt_probe_work;
static DEFINE_MUTEX(usb_remote_xprt_list_lock_lha1);
static LIST_HEAD(usb_remote_xprt_list);

/**
 * find_usb_xprt_list() - Find xprt item specific to an USB endpoint
 * @name: Name of the platform device to find in list
 *
 * @return: pointer to msm_ipc_router_usb_xprt if matching endpoint is found,
 *		else NULL.
 *
 * This function is used to find specific xprt item from the global xprt list
 */
static struct msm_ipc_router_usb_xprt *
		find_usb_xprt_list(const char *name)
{
	struct msm_ipc_router_usb_xprt *usb_xprtp;

	mutex_lock(&usb_remote_xprt_list_lock_lha1);
	list_for_each_entry(usb_xprtp, &usb_remote_xprt_list, list) {
		if (!strcmp(name, usb_xprtp->ch_name)) {
			mutex_unlock(&usb_remote_xprt_list_lock_lha1);
			return usb_xprtp;
		}
	}
	mutex_unlock(&usb_remote_xprt_list_lock_lha1);
	return NULL;
}

/**
 * ipc_router_usb_set_xprt_version() - Set IPC Router header version
 *                                          in the transport
 * @xprt: Reference to the transport structure.
 * @version: The version to be set in transport.
 */
static void ipc_router_usb_set_xprt_version(
	struct msm_ipc_router_xprt *xprt, unsigned int version)
{
	struct msm_ipc_router_usb_xprt *usb_xprtp;

	if (!xprt)
		return;
	usb_xprtp = container_of(xprt, struct msm_ipc_router_usb_xprt, xprt);
	usb_xprtp->xprt_version = version;
}

/**
 * msm_ipc_router_usb_get_xprt_version() - Get IPC Router header version
 *                                          supported by the XPRT
 * @xprt: XPRT for which the version information is required.
 *
 * @return: IPC Router header version supported by the XPRT.
 */
static int msm_ipc_router_usb_get_xprt_version(
	struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_usb_xprt *usb_xprtp;

	if (!xprt)
		return -EINVAL;
	usb_xprtp = container_of(xprt, struct msm_ipc_router_usb_xprt, xprt);

	return (int)usb_xprtp->xprt_version;
}

/**
 * msm_ipc_router_usb_get_xprt_option() - Get XPRT options
 * @xprt: XPRT for which the option information is required.
 *
 * @return: Options supported by the XPRT.
 */
static int msm_ipc_router_usb_get_xprt_option(
	struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_usb_xprt *usb_xprtp;

	if (!xprt)
		return -EINVAL;
	usb_xprtp = container_of(xprt, struct msm_ipc_router_usb_xprt, xprt);

	return (int)usb_xprtp->xprt_option;
}

/**
 * msm_ipc_router_usb_remote_write_avail() - Get available write space
 * @xprt: XPRT for which the available write space info. is required.
 *
 * @return: Write space in bytes on success, 0 on SSR.
 */
static int msm_ipc_router_usb_remote_write_avail(
	struct msm_ipc_router_xprt *xprt)
{
	struct ipc_bridge_platform_data *pdata;
	int write_avail;
	struct msm_ipc_router_usb_xprt *usb_xprtp =
		container_of(xprt, struct msm_ipc_router_usb_xprt, xprt);

	mutex_lock(&usb_xprtp->ss_reset_lock);
	if (usb_xprtp->ss_reset || !usb_xprtp->pdev) {
		write_avail = 0;
	} else {
		pdata = usb_xprtp->pdev->dev.platform_data;
		write_avail = pdata->max_write_size;
	}
	mutex_unlock(&usb_xprtp->ss_reset_lock);
	return write_avail;
}

/**
 * msm_ipc_router_usb_remote_write() - Write to XPRT
 * @data: Data to be written to the XPRT.
 * @len: Length of the data to be written.
 * @xprt: XPRT to which the data has to be written.
 *
 * @return: Data Length on success, standard Linux error codes on failure.
 */
static int msm_ipc_router_usb_remote_write(void *data,
		uint32_t len, struct msm_ipc_router_xprt *xprt)
{
	struct rr_packet *pkt = (struct rr_packet *)data;
	struct sk_buff *skb;
	struct ipc_bridge_platform_data *pdata;
	struct msm_ipc_router_usb_xprt *usb_xprtp;
	int ret;
	uint32_t bytes_written = 0;
	uint32_t bytes_to_write;
	unsigned char *tx_data;

	if (!pkt || pkt->length != len || !xprt) {
		IPC_RTR_ERR("%s: Invalid input parameters\n", __func__);
		return -EINVAL;
	}

	usb_xprtp = container_of(xprt, struct msm_ipc_router_usb_xprt, xprt);
	mutex_lock(&usb_xprtp->ss_reset_lock);
	if (usb_xprtp->ss_reset) {
		IPC_RTR_ERR("%s: Trying to write on a reset link\n", __func__);
		mutex_unlock(&usb_xprtp->ss_reset_lock);
		return -ENETRESET;
	}

	if (!usb_xprtp->pdev) {
		IPC_RTR_ERR("%s: Trying to write on a closed link\n", __func__);
		mutex_unlock(&usb_xprtp->ss_reset_lock);
		return -ENODEV;
	}

	pdata = usb_xprtp->pdev->dev.platform_data;
	if (!pdata || !pdata->write) {
		IPC_RTR_ERR("%s on a uninitialized link\n", __func__);
		mutex_unlock(&usb_xprtp->ss_reset_lock);
		return -EFAULT;
	}

	skb = skb_peek(pkt->pkt_fragment_q);
	if (!skb) {
		IPC_RTR_ERR("%s SKB is NULL\n", __func__);
		mutex_unlock(&usb_xprtp->ss_reset_lock);
		return -EINVAL;
	}

	if (len > pdata->max_write_size)
		pr_warn("%s: Data size exceeds max write size %d\n",
					__func__, pdata->max_write_size);

	D("%s: About to write %d bytes\n", __func__, len);

	while (bytes_written < len) {
		bytes_to_write = min_t(uint32_t, (skb->len - bytes_written),
				       pdata->max_write_size);
		tx_data = skb->data + bytes_written;
		ret = pdata->write(usb_xprtp->pdev, tx_data, bytes_to_write);
		if (ret < 0) {
			IPC_RTR_ERR("%s: Error writing data %d\n",
				    __func__, ret);
			break;
		}
		if (ret != bytes_to_write)
			IPC_RTR_ERR("%s: Partial write %d < %d, retrying...\n",
				    __func__, ret, bytes_to_write);
		bytes_written += bytes_to_write;
	}
	if (bytes_written == len) {
		ret = bytes_written;
	} else if (ret > 0 && bytes_written != len) {
		IPC_RTR_ERR("%s: Fault writing data %d != %d\n",
			    __func__, bytes_written, len);
		ret = -EFAULT;
	}
	D("%s: Finished writing %d bytes\n", __func__, len);
	mutex_unlock(&usb_xprtp->ss_reset_lock);
	return ret;
}

/**
 * msm_ipc_router_usb_remote_close() - Close the XPRT
 * @xprt: XPRT which needs to be closed.
 *
 * @return: 0 on success, standard Linux error codes on failure.
 */
static int msm_ipc_router_usb_remote_close(
	struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_usb_xprt *usb_xprtp;
	struct ipc_bridge_platform_data *pdata;

	if (!xprt)
		return -EINVAL;
	usb_xprtp = container_of(xprt, struct msm_ipc_router_usb_xprt, xprt);

	mutex_lock(&usb_xprtp->ss_reset_lock);
	usb_xprtp->ss_reset = 1;
	mutex_unlock(&usb_xprtp->ss_reset_lock);
	flush_workqueue(usb_xprtp->usb_xprt_wq);
	destroy_workqueue(usb_xprtp->usb_xprt_wq);
	pdata = usb_xprtp->pdev->dev.platform_data;
	if (pdata && pdata->close)
		pdata->close(usb_xprtp->pdev);
	usb_xprtp->pdev = NULL;
	return 0;
}

/**
 * usb_xprt_read_data() - Read work to read from the XPRT
 * @work: Read work to be executed.
 *
 * This function is a read work item queued on a XPRT specific workqueue.
 * The work parameter contains information regarding the XPRT on which this
 * read work has to be performed. The work item keeps reading from the USB
 * endpoint, until the endpoint returns an error.
 */
static void usb_xprt_read_data(struct work_struct *work)
{
	int bytes_to_read;
	int bytes_read;
	int skb_size;
	struct sk_buff *skb = NULL;
	struct ipc_bridge_platform_data *pdata;
	struct delayed_work *rwork = to_delayed_work(work);
	struct msm_ipc_router_usb_xprt *usb_xprtp =
		container_of(rwork, struct msm_ipc_router_usb_xprt, read_work);

	while (1) {
		mutex_lock(&usb_xprtp->ss_reset_lock);
		if (usb_xprtp->ss_reset) {
			mutex_unlock(&usb_xprtp->ss_reset_lock);
			break;
		}
		pdata = usb_xprtp->pdev->dev.platform_data;
		mutex_unlock(&usb_xprtp->ss_reset_lock);
		while (!usb_xprtp->in_pkt) {
			usb_xprtp->in_pkt = create_pkt(NULL);
			if (usb_xprtp->in_pkt)
				break;
			IPC_RTR_ERR("%s: packet allocation failure\n",
								__func__);
			msleep(100);
		}
		D("%s: Allocated rr_packet\n", __func__);

		bytes_to_read = 0;
		skb_size = pdata->max_read_size;
		do {
			do {
				skb = alloc_skb(skb_size, GFP_KERNEL);
				if (skb)
					break;
				IPC_RTR_ERR("%s: Couldn't alloc SKB\n",
					    __func__);
				msleep(100);
			} while (!skb);
			bytes_read = pdata->read(usb_xprtp->pdev, skb->data,
						 pdata->max_read_size);
			if (bytes_read < 0) {
				IPC_RTR_ERR("%s: Error %d @ read operation\n",
					    __func__, bytes_read);
				kfree_skb(skb);
				goto out_read_data;
			}
			if (!bytes_to_read) {
				bytes_to_read = ipc_router_peek_pkt_size(
						skb->data);
				if (bytes_to_read < 0) {
					IPC_RTR_ERR("%s: Invalid size %d\n",
						__func__, bytes_to_read);
					kfree_skb(skb);
					goto out_read_data;
				}
			}
			bytes_to_read -= bytes_read;
			skb_put(skb, bytes_read);
			skb_queue_tail(usb_xprtp->in_pkt->pkt_fragment_q, skb);
			usb_xprtp->in_pkt->length += bytes_read;
			skb_size = min_t(uint32_t, pdata->max_read_size,
					 (uint32_t)bytes_to_read);
		} while (bytes_to_read > 0);

		D("%s: Packet size read %d\n",
		  __func__, usb_xprtp->in_pkt->length);
		msm_ipc_router_xprt_notify(&usb_xprtp->xprt,
			IPC_ROUTER_XPRT_EVENT_DATA, (void *)usb_xprtp->in_pkt);
		release_pkt(usb_xprtp->in_pkt);
		usb_xprtp->in_pkt = NULL;
	}
out_read_data:
	release_pkt(usb_xprtp->in_pkt);
	usb_xprtp->in_pkt = NULL;
}

/**
 * usb_xprt_sft_close_done() - Completion of XPRT reset
 * @xprt: XPRT on which the reset operation is complete.
 *
 * This function is used by IPC Router to signal this USB XPRT Abstraction
 * Layer(XAL) that the reset of XPRT is completely handled by IPC Router.
 */
static void usb_xprt_sft_close_done(struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_usb_xprt *usb_xprtp =
		container_of(xprt, struct msm_ipc_router_usb_xprt, xprt);

	complete_all(&usb_xprtp->sft_close_complete);
}

/**
 * msm_ipc_router_usb_remote_remove() - Remove an USB endpoint
 * @pdev: Platform device corresponding to USB endpoint.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called when the underlying ipc_bridge driver unregisters
 * a platform device, mapped to an USB endpoint, during SSR.
 */
static int msm_ipc_router_usb_remote_remove(struct platform_device *pdev)
{
	struct ipc_bridge_platform_data *pdata;
	struct msm_ipc_router_usb_xprt *usb_xprtp;

	usb_xprtp = find_usb_xprt_list(pdev->name);
	if (!usb_xprtp) {
		IPC_RTR_ERR("%s No device with name %s\n",
					__func__, pdev->name);
		return -ENODEV;
	}

	mutex_lock(&usb_xprtp->ss_reset_lock);
	usb_xprtp->ss_reset = 1;
	mutex_unlock(&usb_xprtp->ss_reset_lock);
	flush_workqueue(usb_xprtp->usb_xprt_wq);
	destroy_workqueue(usb_xprtp->usb_xprt_wq);
	init_completion(&usb_xprtp->sft_close_complete);
	msm_ipc_router_xprt_notify(&usb_xprtp->xprt,
				   IPC_ROUTER_XPRT_EVENT_CLOSE, NULL);
	D("%s: Notified IPC Router of %s CLOSE\n",
	  __func__, usb_xprtp->xprt.name);
	wait_for_completion(&usb_xprtp->sft_close_complete);
	usb_xprtp->pdev = NULL;
	pdata = pdev->dev.platform_data;
	if (pdata && pdata->close)
		pdata->close(pdev);
	return 0;
}

/**
 * msm_ipc_router_usb_remote_probe() - Probe an USB endpoint
 * @pdev: Platform device corresponding to USB endpoint.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called when the underlying ipc_bridge driver registers
 * a platform device, mapped to an USB endpoint.
 */
static int msm_ipc_router_usb_remote_probe(struct platform_device *pdev)
{
	int rc;
	struct ipc_bridge_platform_data *pdata;
	struct msm_ipc_router_usb_xprt *usb_xprtp;

	pdata = pdev->dev.platform_data;
	if (!pdata || !pdata->open || !pdata->read ||
	    !pdata->write || !pdata->close) {
		IPC_RTR_ERR("%s: pdata or pdata->operations is NULL\n",
								__func__);
		return -EINVAL;
	}

	usb_xprtp = find_usb_xprt_list(pdev->name);
	if (!usb_xprtp) {
		IPC_RTR_ERR("%s No device with name %s\n",
						__func__, pdev->name);
		return -ENODEV;
	}

	usb_xprtp->usb_xprt_wq =
		create_singlethread_workqueue(pdev->name);
	if (!usb_xprtp->usb_xprt_wq) {
		IPC_RTR_ERR("%s: WQ creation failed for %s\n",
			__func__, pdev->name);
		return -EFAULT;
	}

	rc = pdata->open(pdev);
	if (rc < 0) {
		IPC_RTR_ERR("%s: Channel open failed for %s.%d\n",
			__func__, pdev->name, pdev->id);
		destroy_workqueue(usb_xprtp->usb_xprt_wq);
		return rc;
	}
	usb_xprtp->pdev = pdev;
	mutex_lock(&usb_xprtp->ss_reset_lock);
	usb_xprtp->ss_reset = 0;
	mutex_unlock(&usb_xprtp->ss_reset_lock);
	msm_ipc_router_xprt_notify(&usb_xprtp->xprt,
				   IPC_ROUTER_XPRT_EVENT_OPEN, NULL);
	D("%s: Notified IPC Router of %s OPEN\n",
	  __func__, usb_xprtp->xprt.name);
	queue_delayed_work(usb_xprtp->usb_xprt_wq,
			   &usb_xprtp->read_work, 0);
	return 0;
}

/**
 * msm_ipc_router_usb_driver_register() - register USB XPRT drivers
 *
 * @usb_xprtp: pointer to IPC router usb xprt structure.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called when a new XPRT is added to register platform
 * drivers for new XPRT.
 */
static int msm_ipc_router_usb_driver_register(
			struct msm_ipc_router_usb_xprt *usb_xprtp)
{
	int ret;
	struct msm_ipc_router_usb_xprt *usb_xprtp_item;

	usb_xprtp_item = find_usb_xprt_list(usb_xprtp->ch_name);

	mutex_lock(&usb_remote_xprt_list_lock_lha1);
	list_add(&usb_xprtp->list, &usb_remote_xprt_list);
	mutex_unlock(&usb_remote_xprt_list_lock_lha1);

	if (!usb_xprtp_item) {
		usb_xprtp->driver.driver.name = usb_xprtp->ch_name;
		usb_xprtp->driver.driver.owner = THIS_MODULE;
		usb_xprtp->driver.probe = msm_ipc_router_usb_remote_probe;
		usb_xprtp->driver.remove = msm_ipc_router_usb_remote_remove;

		ret = platform_driver_register(&usb_xprtp->driver);
		if (ret) {
			IPC_RTR_ERR(
			"%s: Failed to register platform driver[%s]\n",
					__func__, usb_xprtp->ch_name);
			return ret;
		}
	} else {
		IPC_RTR_ERR("%s Already driver registered %s\n",
					__func__, usb_xprtp->ch_name);
	}

	return 0;
}

/**
 * msm_ipc_router_usb_config_init() - init USB xprt configs
 *
 * @usb_xprt_config: pointer to USB xprt configurations.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called to initialize the USB XPRT pointer with
 * the USB XPRT configurations either from device tree or static arrays.
 */
static int msm_ipc_router_usb_config_init(
		struct msm_ipc_router_usb_xprt_config *usb_xprt_config)
{
	struct msm_ipc_router_usb_xprt *usb_xprtp;

	usb_xprtp = kzalloc(sizeof(struct msm_ipc_router_usb_xprt),
							GFP_KERNEL);
	if (IS_ERR_OR_NULL(usb_xprtp)) {
		IPC_RTR_ERR("%s: kzalloc() failed for usb_xprtp id:%s\n",
				__func__, usb_xprt_config->ch_name);
		return -ENOMEM;
	}

	usb_xprtp->xprt.link_id = usb_xprt_config->link_id;
	usb_xprtp->xprt_version = usb_xprt_config->xprt_version;

	strlcpy(usb_xprtp->ch_name, usb_xprt_config->ch_name,
					XPRT_NAME_LEN);

	strlcpy(usb_xprtp->xprt_name, usb_xprt_config->xprt_name,
						XPRT_NAME_LEN);
	usb_xprtp->xprt.name = usb_xprtp->xprt_name;

	usb_xprtp->xprt.set_version =
		ipc_router_usb_set_xprt_version;
	usb_xprtp->xprt.get_version =
		msm_ipc_router_usb_get_xprt_version;
	usb_xprtp->xprt.get_option =
		 msm_ipc_router_usb_get_xprt_option;
	usb_xprtp->xprt.read_avail = NULL;
	usb_xprtp->xprt.read = NULL;
	usb_xprtp->xprt.write_avail =
		msm_ipc_router_usb_remote_write_avail;
	usb_xprtp->xprt.write = msm_ipc_router_usb_remote_write;
	usb_xprtp->xprt.close = msm_ipc_router_usb_remote_close;
	usb_xprtp->xprt.sft_close_done = usb_xprt_sft_close_done;
	usb_xprtp->xprt.priv = NULL;

	usb_xprtp->in_pkt = NULL;
	INIT_DELAYED_WORK(&usb_xprtp->read_work, usb_xprt_read_data);
	mutex_init(&usb_xprtp->ss_reset_lock);
	usb_xprtp->ss_reset = 0;
	usb_xprtp->xprt_option = 0;

	msm_ipc_router_usb_driver_register(usb_xprtp);
	return 0;

}

/**
 * parse_devicetree() - parse device tree binding
 *
 * @node: pointer to device tree node
 * @usb_xprt_config: pointer to USB XPRT configurations
 *
 * @return: 0 on success, -ENODEV on failure.
 */
static int parse_devicetree(struct device_node *node,
		struct msm_ipc_router_usb_xprt_config *usb_xprt_config)
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
	strlcpy(usb_xprt_config->ch_name, ch_name, XPRT_NAME_LEN);

	key = "qcom,xprt-remote";
	remote_ss = of_get_property(node, key, NULL);
	if (!remote_ss)
		goto error;

	key = "qcom,xprt-linkid";
	ret = of_property_read_u32(node, key, &link_id);
	if (ret)
		goto error;
	usb_xprt_config->link_id = link_id;

	key = "qcom,xprt-version";
	ret = of_property_read_u32(node, key, &version);
	if (ret)
		goto error;
	usb_xprt_config->xprt_version = version;

	scnprintf(usb_xprt_config->xprt_name, XPRT_NAME_LEN, "%s_%s",
			remote_ss, usb_xprt_config->ch_name);

	return 0;

error:
	IPC_RTR_ERR("%s: missing key: %s\n", __func__, key);
	return -ENODEV;
}

/**
 * msm_ipc_router_usb_xprt_probe() - Probe an USB xprt
 * @pdev: Platform device corresponding to USB xprt.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called when the underlying device tree driver registers
 * a platform device, mapped to an USB transport.
 */
static int msm_ipc_router_usb_xprt_probe(
				struct platform_device *pdev)
{
	int ret;
	struct msm_ipc_router_usb_xprt_config usb_xprt_config;

	if (pdev && pdev->dev.of_node) {
		mutex_lock(&usb_remote_xprt_list_lock_lha1);
		ipc_router_usb_xprt_probe_done = 1;
		mutex_unlock(&usb_remote_xprt_list_lock_lha1);

		ret = parse_devicetree(pdev->dev.of_node,
						&usb_xprt_config);
		if (ret) {
			IPC_RTR_ERR("%s: Failed to parse device tree\n",
								__func__);
			return ret;
		}

		ret = msm_ipc_router_usb_config_init(
						&usb_xprt_config);
		if (ret) {
			IPC_RTR_ERR(" %s init failed\n", __func__);
			return ret;
		}
	}

	return 0;
}

/**
 * ipc_router_usb_xprt_probe_worker() - probe worker for non DT configurations
 *
 * @work: work item to process
 *
 * This function is called by schedule_delay_work after 3sec and check if
 * device tree probe is done or not. If device tree probe fails the default
 * configurations read from static array.
 */
static void ipc_router_usb_xprt_probe_worker(struct work_struct *work)
{
	int i, ret;

	if (WARN_ON(ARRAY_SIZE(usb_xprt_cfg) != NUM_USB_XPRTS))
		return;

	mutex_lock(&usb_remote_xprt_list_lock_lha1);
	if (!ipc_router_usb_xprt_probe_done) {
		mutex_unlock(&usb_remote_xprt_list_lock_lha1);
		for (i = 0; i < ARRAY_SIZE(usb_xprt_cfg); i++) {
			ret = msm_ipc_router_usb_config_init(
							&usb_xprt_cfg[i]);
			if (ret)
				IPC_RTR_ERR(" %s init failed config idx %d\n",
								__func__, i);
		}
		mutex_lock(&usb_remote_xprt_list_lock_lha1);
	}
	mutex_unlock(&usb_remote_xprt_list_lock_lha1);
}

static const struct of_device_id msm_ipc_router_usb_xprt_match_table[] = {
	{ .compatible = "qcom,ipc-router-usb-xprt" },
	{},
};

static struct platform_driver msm_ipc_router_usb_xprt_driver = {
	.probe = msm_ipc_router_usb_xprt_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_ipc_router_usb_xprt_match_table,
	 },
};

static int __init msm_ipc_router_usb_xprt_init(void)
{
	int rc;

	rc = platform_driver_register(&msm_ipc_router_usb_xprt_driver);
	if (rc) {
		IPC_RTR_ERR(
		"%s: msm_ipc_router_usb_xprt_driver register failed %d\n",
								__func__, rc);
		return rc;
	}

	INIT_DELAYED_WORK(&ipc_router_usb_xprt_probe_work,
					ipc_router_usb_xprt_probe_worker);
	schedule_delayed_work(&ipc_router_usb_xprt_probe_work,
			msecs_to_jiffies(IPC_ROUTER_USB_XPRT_WAIT_TIMEOUT));
	return 0;
}

static void __exit msm_ipc_router_usb_xprt_exit(void)
{
	struct msm_ipc_router_usb_xprt *usb_xprtp;
	struct msm_ipc_router_usb_xprt *temp_usb_xprtp;

	list_for_each_entry_safe(usb_xprtp, temp_usb_xprtp,
					&usb_remote_xprt_list, list) {
		kfree(usb_xprtp);
	}

	platform_driver_unregister(&msm_ipc_router_usb_xprt_driver);
}

module_init(msm_ipc_router_usb_xprt_init);
module_exit(msm_ipc_router_usb_xprt_exit);
MODULE_DESCRIPTION("IPC Router USB XPRT");
MODULE_LICENSE("GPL v2");
