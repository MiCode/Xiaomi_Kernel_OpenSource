// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
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
 * struct ipa3_teth_bridge_ctx - Tethering bridge driver context information
 * @class: kernel class pointer
 * @dev_num: kernel device number
 * @dev: kernel device struct pointer
 * @cdev: kernel character device struct
 */
struct ipa3_teth_bridge_ctx {
	struct class *class;
	dev_t dev_num;
	struct device *dev;
	struct cdev cdev;
	u32 modem_pm_hdl;
};
static struct ipa3_teth_bridge_ctx *ipa3_teth_ctx;

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
 * ipa3_teth_bridge_init() - Initialize the Tethering bridge driver
 * @params - in/out params for USB initialization API (please look at struct
 *  definition for more info)
 *
 * USB driver gets a pointer to a callback function (usb_notify_cb) and an
 * associated data.
 *
 * Builds IPA resource manager dependency graph.
 *
 * Return codes: 0: success,
 *		-EINVAL - Bad parameter
 *		Other negative value - Failure
 */
int ipa3_teth_bridge_init(struct teth_bridge_init_params *params)
{
	TETH_DBG_FUNC_ENTRY();

	if (!params) {
		TETH_ERR("Bad parameter\n");
		TETH_DBG_FUNC_EXIT();
		return -EINVAL;
	}

	params->usb_notify_cb = teth_bridge_ipa_cb;
	params->private_data = NULL;
	params->skip_ep_cfg = true;

	TETH_DBG_FUNC_EXIT();
	return 0;
}

/**
 * ipa3_teth_bridge_get_pm_hdl() - Get the Tethering bridge Driver pm hdl
 *
 *
 * Return codes: handle
 *		-EINVAL - Bad parameter
 */
int ipa3_teth_bridge_get_pm_hdl(void)
{
	TETH_DBG_FUNC_ENTRY();

	if (ipa3_teth_ctx->modem_pm_hdl == ~0) {
		TETH_ERR("Bad parameter\n");
		TETH_DBG_FUNC_EXIT();
		return -EINVAL;
	}

	TETH_DBG("Return pm-handle %d\n", ipa3_teth_ctx->modem_pm_hdl);
	TETH_DBG_FUNC_EXIT();
	return ipa3_teth_ctx->modem_pm_hdl;
}

/**
 * ipa3_teth_bridge_disconnect() - Disconnect tethering bridge module
 */
int ipa3_teth_bridge_disconnect(enum ipa_client_type client)
{
	int res = 0;

	TETH_DBG_FUNC_ENTRY();
	res = ipa_pm_deactivate_sync(ipa3_teth_ctx->modem_pm_hdl);

	if (res) {
		TETH_ERR("fail to deactivate modem %d\n", res);
		return res;
	}

	res = ipa_pm_deregister(ipa3_teth_ctx->modem_pm_hdl);
	ipa3_teth_ctx->modem_pm_hdl = ~0;
	TETH_DBG_FUNC_EXIT();

	return res;
}

/**
 * ipa3_teth_bridge_connect() - Connect bridge for a tethered Rmnet / MBIM call
 * @connect_params:	Connection info
 *
 * Return codes: 0: success
 *		-EINVAL: invalid parameters
 *		-EPERM: Operation not permitted as the bridge is already
 *		connected
 */
int ipa3_teth_bridge_connect(struct teth_bridge_connect_params *connect_params)
{
	int res = 0;
	struct ipa_pm_register_params reg_params;

	memset(&reg_params, 0, sizeof(reg_params));

	TETH_DBG_FUNC_ENTRY();

	reg_params.name = "MODEM (USB RMNET)";
	reg_params.group = IPA_PM_GROUP_MODEM;
	reg_params.skip_clk_vote = true;
	res = ipa_pm_register(&reg_params,
		&ipa3_teth_ctx->modem_pm_hdl);
	if (res) {
		TETH_ERR("fail to register with PM %d\n", res);
		return res;
	}
	res = ipa_pm_activate_sync(ipa3_teth_ctx->modem_pm_hdl);

	TETH_DBG_FUNC_EXIT();
	return res;
}

static long ipa3_teth_bridge_ioctl(struct file *filp,
			      unsigned int cmd,
			      unsigned long arg)
{
	IPAERR("No ioctls are supported!\n");
	return -ENOIOCTLCMD;
}

static const struct file_operations ipa3_teth_bridge_drv_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ipa3_teth_bridge_ioctl,
};

/**
 * ipa3_teth_bridge_driver_init() - Initialize tethering bridge driver
 *
 */
int ipa3_teth_bridge_driver_init(void)
{
	int res;

	TETH_DBG("Tethering bridge driver init\n");
	ipa3_teth_ctx = kzalloc(sizeof(*ipa3_teth_ctx), GFP_KERNEL);
	if (!ipa3_teth_ctx)
		return -ENOMEM;

	ipa3_teth_ctx->class = class_create(THIS_MODULE, TETH_BRIDGE_DRV_NAME);

	res = alloc_chrdev_region(&ipa3_teth_ctx->dev_num, 0, 1,
				  TETH_BRIDGE_DRV_NAME);
	if (res) {
		TETH_ERR("alloc_chrdev_region err.\n");
		res = -ENODEV;
		goto fail_alloc_chrdev_region;
	}

	ipa3_teth_ctx->dev = device_create(ipa3_teth_ctx->class,
			NULL,
			ipa3_teth_ctx->dev_num,
			ipa3_teth_ctx,
			TETH_BRIDGE_DRV_NAME);
	if (IS_ERR(ipa3_teth_ctx->dev)) {
		TETH_ERR(":device_create err.\n");
		res = -ENODEV;
		goto fail_device_create;
	}

	cdev_init(&ipa3_teth_ctx->cdev, &ipa3_teth_bridge_drv_fops);
	ipa3_teth_ctx->cdev.owner = THIS_MODULE;
	ipa3_teth_ctx->cdev.ops = &ipa3_teth_bridge_drv_fops;

	res = cdev_add(&ipa3_teth_ctx->cdev, ipa3_teth_ctx->dev_num, 1);
	if (res) {
		TETH_ERR(":cdev_add err=%d\n", -res);
		res = -ENODEV;
		goto fail_cdev_add;
	}

	ipa3_teth_ctx->modem_pm_hdl = ~0;
	TETH_DBG("Tethering bridge driver init OK\n");

	return 0;
fail_cdev_add:
	device_destroy(ipa3_teth_ctx->class, ipa3_teth_ctx->dev_num);
fail_device_create:
	unregister_chrdev_region(ipa3_teth_ctx->dev_num, 1);
fail_alloc_chrdev_region:
	kfree(ipa3_teth_ctx);
	ipa3_teth_ctx = NULL;

	return res;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Tethering bridge driver");
