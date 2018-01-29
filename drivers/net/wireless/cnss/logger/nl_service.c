/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "cnss_logger: %s: "fmt, __func__

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/export.h>
#include <net/cnss_logger.h>
#include "logger.h"

static DEFINE_MUTEX(logger_sem);

/**
 * logger_get_radio_idx() - to get the radio index
 * @ctx: the logger context pointer
 *
 * Return: the first available radio index, otherwise failure code
 */
static int logger_get_radio_idx(struct logger_context *ctx)
{
	int i;

	for (i = 0; i < sizeof(ctx->radio_mask); i++) {
		if (!test_and_set_bit(i, &ctx->radio_mask))
			return i;
	}
	return -EINVAL;
}

/**
 * logger_put_radio_idx() - to release the radio index
 * @radio: the radio index to release
 *
 * Return: None
 */
static void logger_put_radio_idx(struct logger_context *ctx, int radio)
{
	clear_bit(radio, &ctx->radio_mask);
}

/**
 * logger_get_device() - to get the logger device per radio index
 * @radio: the radio index
 *
 * Return: the logger_device pointer, otherwise return NULL.
 */
static struct logger_device *logger_get_device(int radio)
{
	struct logger_device *dev;
	struct logger_context *ctx;

	ctx = logger_get_ctx();
	if (!ctx)
		return NULL;

	list_for_each_entry(dev, &ctx->dev_list, list) {
		if (dev->radio_idx == radio)
			return dev;
	}
	return NULL;
}

/**
 * logger_device_is_registered() - to check if device has been registered
 * @dev: pointer to logger device
 * @wiphy: the wiphy pointer of the device to register
 *
 * This helper function is to check if this device has been registered.
 *
 * Return: NULL if it has not, otherwise return the logger_device pointer.
 */
static struct logger_device *logger_device_is_registered(
						 struct logger_context *ctx,
						 struct wiphy *wiphy)
{
	struct logger_device *dev;

	list_for_each_entry(dev, &ctx->dev_list, list) {
		if (dev->wiphy == wiphy)
			return dev;
	}
	return NULL;
}

/**
 * logger_dispatch_skb() - to dispatch the skb to devices
 * @skb: the socket buffer received and to dispatch
 *
 * The function will look up the header of the skb, and dispatch the skb
 * to the associated event and device that registered.
 *
 * Return: 0 if successfully dispatch, otherwise failure code
 */
static int logger_dispatch_skb(struct sk_buff *skb)
{
	pr_info("skb_len: %d, skb_data_len: %d\n",
		skb->len, skb->data_len);

	return 0;
}

/**
 * logger_flush_event_handle() - to flush the event handle associate to device
 * @dev: pointer to logger device
 *
 * The function will clean up all the event handle's resource, take it out
 * from the list, and free the memory allocated.
 *
 * Return: None
 */
static void logger_flush_event_handle(struct logger_device *dev)
{
	struct list_head *pos, *temp;
	struct logger_event_handler *cur;

	list_for_each_safe(pos, temp, &dev->event_list) {
		cur = container_of(pos, struct logger_event_handler, list);
		pr_info("radio: %d, event: %d unregistered!\n",
			dev->radio_idx, cur->event);
		list_del(&cur->list);
		kfree(cur);
	}
}

/**
 * logger_flush_devices() - to flush the devices infomration
 * @dev: pointer to logger device
 *
 * The helper function to flush the device information, all the device clean
 * up prcoess should be starting from here.
 *
 * Return: None
 */
static void logger_flush_devices(struct logger_device *dev)
{
	pr_info("driver: [%s] and radio-%d is unregistered\n",
		dev->name, dev->radio_idx);
	logger_flush_event_handle(dev);
	logger_put_radio_idx(dev->ctx, dev->radio_idx);
	list_del(&dev->list);
	kfree(dev);
}

/**
 * logger_register_device_event() - register the evet to device
 * @dev: pointer to logger device
 * @event: the event to register
 * @cb: the callback associated to the device and event
 *
 * Return: 0 if register successfully, otherwise the failure code
 */
static int logger_register_device_event(struct logger_device *dev, int event,
					int (*cb)(struct sk_buff *skb))
{
	struct logger_event_handler *cur;

	list_for_each_entry(cur, &dev->event_list, list) {
		if (cur->event == event) {
			pr_info("event %d, is already added\n", event);
			return 0;
		}
	}

	cur = kmalloc(sizeof(*cur), GFP_KERNEL);
	if (!cur)
		return -ENOMEM;

	cur->event = event;
	cur->cb = cb;

	pr_info("radio: %d, event: %d\n", dev->radio_idx, cur->event);
	list_add_tail(&cur->list, &dev->event_list);

	return 0;
}

/**
 * logger_unregister_device_event() - unregister the evet from device
 * @dev: pointer to logger device
 * @event: the event to unregister
 * @cb: the callback associated to the device and event
 *
 * Return: 0 if unregister successfully, otherwise the failure code
 */
static int logger_unregister_device_event(struct logger_device *dev, int event,
					  int (*cb)(struct sk_buff *skb))
{
	struct list_head *pos, *temp;
	struct logger_event_handler *cur;

	list_for_each_safe(pos, temp, &dev->event_list) {
		cur = container_of(pos, struct logger_event_handler, list);
		if (cur->event == event && cur->cb == cb) {
			pr_info("radio: %d, event: %d\n",
				dev->radio_idx, cur->event);
			list_del(&cur->list);
			kfree(cur);
			return 0;
		}
	}
	return -ENOENT;
}

/**
 * logger_skb_input() - the callback to receive the skb
 * @skb: the receive socket buffer
 *
 * Return: None
 */
static void logger_skb_input(struct sk_buff *skb)
{
	mutex_lock(&logger_sem);
	logger_dispatch_skb(skb);
	mutex_unlock(&logger_sem);
}

/**
 * cnss_logger_event_register() - register the event
 * @radio: the radio index to register
 * @event: the event to register
 * @cb: the callback
 *
 * This function is used to register event associated to the radio index.
 *
 * Return: 0 if register success, otherwise failure code
 */
int cnss_logger_event_register(int radio, int event,
			       int (*cb)(struct sk_buff *skb))
{
	int ret = -ENOENT;
	struct logger_device *dev;

	dev = logger_get_device(radio);
	if (dev)
		ret = logger_register_device_event(dev, event, cb);

	return ret;
}
EXPORT_SYMBOL(cnss_logger_event_register);

/**
 * cnss_logger_event_unregister() - unregister the event
 * @radio: the radio index to unregister
 * @event: the event to unregister
 * @cb: the callback
 *
 * This function is used to unregister the event from cnss logger module.
 *
 * Return: 0 if unregister success, otherwise failure code
 */
int cnss_logger_event_unregister(int radio, int event,
				 int (*cb)(struct sk_buff *skb))
{
	int ret = -ENOENT;
	struct logger_device *dev;

	dev = logger_get_device(radio);
	if (dev)
		ret = logger_unregister_device_event(dev, event, cb);

	return ret;
}
EXPORT_SYMBOL(cnss_logger_event_unregister);

/**
 * cnss_logger_device_register() - register the driver
 * @wiphy: the wiphy device to unregister
 * @name: the module name of the driver
 *
 * This function is used to register the driver to cnss logger module,
 * this will indicate the existence of the driver, and also assign the
 * radio index for further operation.
 *
 * Return: the radio index if register successful, otherwise failure code
 */
int cnss_logger_device_register(struct wiphy *wiphy, const char *name)
{
	int radio;
	struct logger_context *ctx;
	struct logger_device *new;

	ctx = logger_get_ctx();
	if (!ctx)
		return -ENOENT;

	/* sanity check, already registered? */
	new = logger_device_is_registered(ctx, wiphy);
	if (new)
		return new->radio_idx;

	radio = logger_get_radio_idx(ctx);
	if (radio < 0) {
		pr_err("driver registration is full!\n");
		return -ENOMEM;
	}

	new = kmalloc(sizeof(*new), GFP_KERNEL);
	if (!new) {
		logger_put_radio_idx(ctx, radio);
		return -ENOMEM;
	}

	new->radio_idx = radio;
	new->wiphy = wiphy;
	new->ctx = ctx;
	strlcpy(new->name, name, sizeof(new->name));
	INIT_LIST_HEAD(&new->event_list);

	list_add(&new->list, &ctx->dev_list);

	pr_info("driver: [%s] is registered as radio-%d\n",
		new->name, new->radio_idx);

	return new->radio_idx;
}
EXPORT_SYMBOL(cnss_logger_device_register);

/**
 * cnss_logger_device_unregister() - unregister the driver
 * @radio: the radio to unregister
 * @wiphy: the wiphy device to unregister
 *
 * This function is used to unregister the driver from cnss logger module.
 * This will disable the driver to access the interface in cnss logger,
 * and also all the related events that registered will be reset.
 *
 * Return: 0 if success, otherwise failure code
 */
int cnss_logger_device_unregister(int radio, struct wiphy *wiphy)
{
	struct logger_context *ctx;
	struct logger_device *cur;
	struct list_head *pos, *temp;

	ctx = logger_get_ctx();
	if (!ctx)
		return -ENOENT;

	list_for_each_safe(pos, temp, &ctx->dev_list) {
		cur = list_entry(pos, struct logger_device, list);
		if (cur->radio_idx == radio && cur->wiphy == wiphy) {
			logger_flush_devices(cur);
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL(cnss_logger_device_unregister);

/**
 * cnss_logger_nl_ucast() - nl interface to unicast the buffer
 * @skb: the socket buffer to transmit
 * @portid: netlink portid of the destination socket
 * @flag: the flag to indicate if this is a nonblock call
 *
 * Return: 0 if success, otherwise failure code
 */
int cnss_logger_nl_ucast(struct sk_buff *skb, int portid, int flag)
{
	struct logger_context *ctx;

	ctx = logger_get_ctx();
	if (!ctx) {
		dev_kfree_skb(skb);
		return -ENOENT;
	}

	return netlink_unicast(ctx->nl_sock, skb, portid, flag);
}
EXPORT_SYMBOL(cnss_logger_nl_ucast);

/**
 * cnss_logger_nl_bcast() - nl interface to broadcast the buffer
 * @skb: the socket buffer to transmit
 * @portid: netlink portid of the destination socket
 * @flag: the gfp_t flag
 *
 * Return: 0 if success, otherwise failure code
 */
int cnss_logger_nl_bcast(struct sk_buff *skb, int portid, int flag)
{
	struct logger_context *ctx;

	ctx = logger_get_ctx();
	if (!ctx) {
		dev_kfree_skb(skb);
		return -ENOENT;
	}

	return netlink_broadcast(ctx->nl_sock, skb, 0, portid, flag);
}
EXPORT_SYMBOL(cnss_logger_nl_bcast);

/**
 * logger_netlink_init() - initialize the netlink socket
 * @ctx: the cnss logger context pointer
 *
 * Return: the netlink handle if success, otherwise failure code
 */
int logger_netlink_init(struct logger_context *ctx)
{
	struct netlink_kernel_cfg cfg = {
		.groups	= CNSS_LOGGER_NL_MCAST_GRP_ID,
		.input	= logger_skb_input,
	};

	ctx->nl_sock = netlink_kernel_create(&init_net, NETLINK_USERSOCK, &cfg);
	if (ctx->nl_sock == NULL) {
		pr_err("cnss_logger: Cannot create netlink socket");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&ctx->dev_list);

	return 0;
}

/**
 * logger_netlink_deinit() - release the netlink socket and other resource
 * @ctx: the cnss logger context pointer
 *
 * Return: 0 if success, otherwise failure code
 */
int logger_netlink_deinit(struct logger_context *ctx)
{
	struct list_head *pos, *temp;
	struct logger_device *dev;

	netlink_kernel_release(ctx->nl_sock);
	list_for_each_safe(pos, temp, &ctx->dev_list) {
		dev = container_of(pos, struct logger_device, list);
		logger_flush_devices(dev);
	}
	return 0;
}
