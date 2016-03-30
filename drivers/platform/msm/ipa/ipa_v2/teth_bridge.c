/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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

#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/if_ether.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/msm_ipa.h>
#include <linux/mutex.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/ipa.h>
#include <linux/netdevice.h>
#include "ipa_i.h"

#define TETH_BRIDGE_DRV_NAME "ipa_tethering_bridge"

#define TETH_DBG(fmt, args...) \
	pr_debug(TETH_BRIDGE_DRV_NAME " %s:%d " fmt, \
		 __func__, __LINE__, ## args)
#define TETH_DBG_FUNC_ENTRY() \
	pr_debug(TETH_BRIDGE_DRV_NAME " %s:%d ENTRY\n", __func__, __LINE__)
#define TETH_DBG_FUNC_EXIT() \
	pr_debug(TETH_BRIDGE_DRV_NAME " %s:%d EXIT\n", __func__, __LINE__)
#define TETH_ERR(fmt, args...) \
	pr_err(TETH_BRIDGE_DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)

/**
 * struct teth_bridge_ctx - Tethering bridge driver context information
 * @class: kernel class pointer
 * @dev_num: kernel device number
 * @dev: kernel device struct pointer
 * @cdev: kernel character device struct
 */
struct teth_bridge_ctx {
	struct class *class;
	dev_t dev_num;
	struct device *dev;
	struct cdev cdev;
};
static struct teth_bridge_ctx *teth_ctx;

/**
* teth_bridge_ipa_cb() - Callback to handle IPA data path events
* @priv - private data
* @evt - event type
* @data - event specific data (usually skb)
*
* This callback is called by IPA driver for exception packets from USB.
* All exception packets are handled by Q6 and should not reach this function.
* Packets will arrive to AP exception pipe only in case where packets are
* sent from USB before Q6 has setup the call.
*/
static void teth_bridge_ipa_cb(void *priv, enum ipa_dp_evt_type evt,
	unsigned long data)
{
	struct sk_buff *skb = (struct sk_buff *)data;

	TETH_DBG_FUNC_ENTRY();
	if (evt != IPA_RECEIVE) {
		TETH_ERR("unexpected event %d\n", evt);
		WARN_ON(1);
		return;
	}

	TETH_ERR("Unexpected exception packet from USB, dropping packet\n");
	dev_kfree_skb_any(skb);
	TETH_DBG_FUNC_EXIT();
}

/**
* ipa2_teth_bridge_init() - Initialize the Tethering bridge driver
* @params - in/out params for USB initialization API (please look at struct
*  definition for more info)
*
* USB driver gets a pointer to a callback function (usb_notify_cb) and an
* associated data. USB driver installs this callback function in the call to
* ipa_connect().
*
* Builds IPA resource manager dependency graph.
*
* Return codes: 0: success,
*		-EINVAL - Bad parameter
*		Other negative value - Failure
*/
int ipa2_teth_bridge_init(struct teth_bridge_init_params *params)
{
	int res = 0;

	TETH_DBG_FUNC_ENTRY();

	if (!params) {
		TETH_ERR("Bad parameter\n");
		TETH_DBG_FUNC_EXIT();
		return -EINVAL;
	}

	params->usb_notify_cb = teth_bridge_ipa_cb;
	params->private_data = NULL;
	params->skip_ep_cfg = true;

	/* Build dependency graph */
	res = ipa_rm_add_dependency(IPA_RM_RESOURCE_USB_PROD,
				    IPA_RM_RESOURCE_Q6_CONS);
	if (res < 0 && res != -EINPROGRESS) {
		TETH_ERR("ipa_rm_add_dependency() failed.\n");
		goto bail;
	}
	res = ipa_rm_add_dependency(IPA_RM_RESOURCE_Q6_PROD,
				    IPA_RM_RESOURCE_USB_CONS);
	if (res < 0 && res != -EINPROGRESS) {
		ipa_rm_delete_dependency(IPA_RM_RESOURCE_USB_PROD,
					IPA_RM_RESOURCE_Q6_CONS);
		TETH_ERR("ipa_rm_add_dependency() failed.\n");
		goto bail;
	}

	res = 0;
	goto bail;

bail:
	TETH_DBG_FUNC_EXIT();
	return res;
}

/**
* ipa2_teth_bridge_disconnect() - Disconnect tethering bridge module
*/
int ipa2_teth_bridge_disconnect(enum ipa_client_type client)
{
	TETH_DBG_FUNC_ENTRY();
	ipa_rm_delete_dependency(IPA_RM_RESOURCE_USB_PROD,
				 IPA_RM_RESOURCE_Q6_CONS);
	ipa_rm_delete_dependency(IPA_RM_RESOURCE_Q6_PROD,
				 IPA_RM_RESOURCE_USB_CONS);
	TETH_DBG_FUNC_EXIT();

	return 0;
}

/**
* ipa2_teth_bridge_connect() - Connect bridge for a tethered Rmnet / MBIM call
* @connect_params:	Connection info
*
* Return codes: 0: success
*		-EINVAL: invalid parameters
*		-EPERM: Operation not permitted as the bridge is already
*		connected
*/
int ipa2_teth_bridge_connect(struct teth_bridge_connect_params *connect_params)
{
	return 0;
}

static long teth_bridge_ioctl(struct file *filp,
			      unsigned int cmd,
			      unsigned long arg)
{
	IPAERR("No ioctls are supported for krypton !\n");
	return -ENOIOCTLCMD;
}

static const struct file_operations teth_bridge_drv_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = teth_bridge_ioctl,
};

/**
* teth_bridge_driver_init() - Initialize tethering bridge driver
*
*/
int teth_bridge_driver_init(void)
{
	int res;

	TETH_DBG("Tethering bridge driver init\n");
	teth_ctx = kzalloc(sizeof(*teth_ctx), GFP_KERNEL);
	if (!teth_ctx) {
		TETH_ERR("kzalloc err.\n");
		return -ENOMEM;
	}

	teth_ctx->class = class_create(THIS_MODULE, TETH_BRIDGE_DRV_NAME);

	res = alloc_chrdev_region(&teth_ctx->dev_num, 0, 1,
				  TETH_BRIDGE_DRV_NAME);
	if (res) {
		TETH_ERR("alloc_chrdev_region err.\n");
		res = -ENODEV;
		goto fail_alloc_chrdev_region;
	}

	teth_ctx->dev = device_create(teth_ctx->class, NULL, teth_ctx->dev_num,
				      teth_ctx, TETH_BRIDGE_DRV_NAME);
	if (IS_ERR(teth_ctx->dev)) {
		TETH_ERR(":device_create err.\n");
		res = -ENODEV;
		goto fail_device_create;
	}

	cdev_init(&teth_ctx->cdev, &teth_bridge_drv_fops);
	teth_ctx->cdev.owner = THIS_MODULE;
	teth_ctx->cdev.ops = &teth_bridge_drv_fops;

	res = cdev_add(&teth_ctx->cdev, teth_ctx->dev_num, 1);
	if (res) {
		TETH_ERR(":cdev_add err=%d\n", -res);
		res = -ENODEV;
		goto fail_cdev_add;
	}
	TETH_DBG("Tethering bridge driver init OK\n");

	return 0;
fail_cdev_add:
	device_destroy(teth_ctx->class, teth_ctx->dev_num);
fail_device_create:
	unregister_chrdev_region(teth_ctx->dev_num, 1);
fail_alloc_chrdev_region:
	kfree(teth_ctx);
	teth_ctx = NULL;

	return res;
}
EXPORT_SYMBOL(teth_bridge_driver_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Tethering bridge driver");
