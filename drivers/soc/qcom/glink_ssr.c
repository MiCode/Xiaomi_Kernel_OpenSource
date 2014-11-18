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
#include <linux/err.h>
#include <linux/ipc_logging.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/random.h>
#include <soc/qcom/glink.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>
#include "glink_private.h"

#define GLINK_SSR_REPLY_TIMEOUT	(HZ / 2)

/* Global restart counter */
static uint32_t sequence_number;

/**
 * enum ssr_command - G-Link SSR protocol commands
 */
enum ssr_command {
	GLINK_SSR_DO_CLEANUP,
	GLINK_SSR_CLEANUP_DONE,
};

/**
 * struct restart_notifier_block - restart notifier wrapper structure
 * subsystem:	the name of the subsystem as recognized by the SSR framework
 * nb:		notifier block structure used by the SSR framework
 */
struct restart_notifier_block {
	const char *subsystem;
	struct notifier_block nb;
};

/**
 * struct subsys_info - Subsystem info structure
 * ssr_name:	name of the subsystem recognized by the SSR framework
 * edge:	name of the G-Link edge
 * xprt:	name of the G-Link transport
 * handle:	glink_ssr channel used for this subsystem
 * link_info:	Transport info used in link state callback registration
 * cb_data:	Private callback data structure for notification functions
 * subsystem_list_node:	used to chain this structure in a list of subsystem
 *			info structures
 * notify_list:	list of subsys_info_leaf structures, containing the subsystems
 *		to notify if this subsystem undergoes SSR
 * notify_list_len:	length of notify_list
 */
struct subsys_info {
	const char *ssr_name;
	const char *edge;
	const char *xprt;
	void *handle;
	struct glink_link_info *link_info;
	struct ssr_notify_data *cb_data;
	struct list_head subsystem_list_node;
	struct list_head notify_list;
	int notify_list_len;
};

/**
 * struct subsys_info_leaf - Subsystem info leaf structure (a subsystem on the
 *                           notify list of a subsys_info structure)
 * ssr_name:	Name of the subsystem recognized by the SSR framework
 * edge:	name of the G-Link edge
 * xprt:	name of the G-Link transport
 * cb_data:	Private callback data structure for notification functions
 * notify_list_node:	used to chain this structure in the notify list
 */
struct subsys_info_leaf {
	const char *ssr_name;
	const char *edge;
	const char *xprt;
	struct ssr_notify_data *cb_data;
	struct list_head notify_list_node;
};

/**
 * struct do_cleanup_msg - The data structure for an SSR do_cleanup message
 * version:	The G-Link SSR protocol version
 * command:	The G-Link SSR command - do_cleanup
 * seq_num:	Sequence number
 * name_len:	Length of the name of the subsystem being restarted
 * name:	G-Link edge name of the subsystem being restarted
 */
struct do_cleanup_msg {
	uint32_t version;
	uint32_t command;
	uint32_t seq_num;
	uint32_t name_len;
	char name[GLINK_NAME_SIZE];
};

/**
 * struct cleanup_done_msg - The data structure for an SSR cleanup_done message
 * version:	The G-Link SSR protocol version
 * response:	The G-Link SSR response to a do_cleanup command, cleanup_done
 * seq_num:	Sequence number
 */
struct cleanup_done_msg {
	uint32_t version;
	uint32_t response;
	uint32_t seq_num;
};

/**
 * struct ssr_notify_data - Contains private data used for client notifications
 *                          from G-Link.
 * tx_done:	Indicates whether or not the tx_done notification has been
 *		received.
 * event:	The state notification event received.
 * responded:	Indicates whether or not a cleanup_done response was received.
 * version:	G-Link SSR protocol version
 * seq_num:	G-Link SSR protocol sequence number
 */
struct ssr_notify_data {
	bool tx_done;
	unsigned event;
	bool responded;
	uint32_t version;
	uint32_t seq_num;
};

static int restart_notifier_cb(struct notifier_block *this,
				  unsigned long code,
				  void *data);
static void print_subsystem_list(void);
static void delete_ss_info_notify_list(struct subsys_info *ss_info);
static struct subsys_info *get_info_for_subsystem(const char *subsystem);
static struct subsys_info *get_info_for_edge(const char *edge);
static int notify_for_subsystem(struct subsys_info *ss_info);
static int configure_and_open_channel(struct subsys_info *ss_info);

static LIST_HEAD(subsystem_list);
static atomic_t responses_remaining = ATOMIC_INIT(0);
static wait_queue_head_t waitqueue;

/**
 * glink_lbsrv_link_state_cb() - Callback to receive link state updates
 * @cb_info:	Information containing link & its state.
 * @priv:	Private data passed during the link state registration.
 *
 * This function is called by the GLINK core to notify the glink_ssr module
 * regarding the link state updates. This function is registered with the
 * G-Link core by the loopback server during glink_register_link_state_cb().
 */
static void glink_ssr_link_state_cb(struct glink_link_state_cb_info *cb_info,
				      void *priv)
{
	struct subsys_info *ss_info;

	if (!cb_info) {
		GLINK_ERR("<SSR> %s: Missing cb_data\n", __func__);
		return;
	}

	GLINK_INFO("<SSR> %s: %s:%s\n", __func__, cb_info->edge,
			cb_info->transport);

	ss_info = get_info_for_edge(cb_info->edge);

	if (ss_info && cb_info->link_state == GLINK_LINK_STATE_UP)
		configure_and_open_channel(ss_info);
}

/**
 * glink_ssr_notify_rx() - RX Notification callback
 * @handle:	G-Link channel handle
 * @priv:	Private callback data
 * @pkt_priv:	Private packet data
 * @ptr:	Pointer to the data received
 * @size:	Size of the data received
 *
 * This function is a notification callback from the G-Link core that data
 * has been received from the remote side. This data is validate to make
 * sure it is a cleanup_done message and is processed accordingly if it is.
 */
void glink_ssr_notify_rx(void *handle, const void *priv, const void *pkt_priv,
		const void *ptr, size_t size)
{
	struct ssr_notify_data *cb_data = (struct ssr_notify_data *)priv;
	struct cleanup_done_msg *resp = (struct cleanup_done_msg *)ptr;

	if (!cb_data)
		goto missing_cb_data;
	if (!resp)
		goto missing_response;
	if (resp->version != cb_data->version)
		goto version_mismatch;
	if (resp->seq_num != cb_data->seq_num)
		goto invalid_seq_number;
	if (resp->response != GLINK_SSR_CLEANUP_DONE)
		goto wrong_response;

	cb_data->responded = true;
	atomic_dec(&responses_remaining);

	GLINK_DBG("<SSR> %s: responses remaining after dec[%d]\n",
			__func__, atomic_read(&responses_remaining));
	GLINK_INFO("<SSR> %s: %s resp[%d] version[%d] seq_num[%d]\n",
			__func__, "Response received.", resp->response,
			resp->version, resp->seq_num);

	wake_up(&waitqueue);
	return;

missing_cb_data:
	GLINK_ERR("<SSR> %s: Missing cb_data\n", __func__);
	return;
missing_response:
	GLINK_ERR("<SSR> %s: Missing response data\n", __func__);
	return;
version_mismatch:
	GLINK_ERR("<SSR> %s: Version mismatch. %s[%d], %s[%d]\n", __func__,
			"do_cleanup version", cb_data->version,
			"cleanup_done version", resp->version);
	return;
invalid_seq_number:
	GLINK_ERR("<SSR> %s: Invalid seq. number. %s[%d], %s[%d]\n", __func__,
			"do_cleanup seq num", cb_data->seq_num,
			"cleanup_done seq_num", resp->seq_num);
	return;
wrong_response:
	GLINK_ERR("<SSR> %s: Not a cleaup_done message. %s[%d]\n", __func__,
			"cleanup_done response", resp->response);
	return;
}

/**
 * glink_ssr_notify_tx_done() - Transmit finished notification callback
 * @handle:	G-Link channel handle
 * @priv:	Private callback data
 * @pkt_priv:	Private packet data
 * @ptr:	Pointer to the data received
 *
 * This function is a notification callback from the G-Link core that data
 * we sent has finished transmitting.
 */
void glink_ssr_notify_tx_done(void *handle, const void *priv,
		const void *pkt_priv, const void *ptr)
{
	struct ssr_notify_data *cb_data = (struct ssr_notify_data *)priv;

	if (!cb_data) {
		GLINK_ERR("<SSR> %s: Could not allocate data for cb_data\n",
				__func__);
	} else {
		GLINK_INFO("<SSR> %s: tx_done notification\n", __func__);
		cb_data->tx_done = true;
	}
}

/**
 * glink_ssr_notify_state() - Channel state notification callback
 * @handle:	G-Link channel handle
 * @priv:	Private callback data
 * @event:	The state that has been transitioned to
 *
 * This function is a notification callback from the G-Link core that the
 * channel state has changed.
 */
void glink_ssr_notify_state(void *handle, const void *priv, unsigned event)
{
	struct ssr_notify_data *cb_data = (struct ssr_notify_data *)priv;

	if (!cb_data) {
		GLINK_ERR("<SSR> %s: Could not allocate data for cb_data\n",
				__func__);
	} else {
		GLINK_INFO("<SSR> %s: event[%d]\n",
				__func__, event);
		cb_data->event = event;
	}
}

/**
 * glink_ssr_notify_rx_intent_req() - RX intent request notification callback
 * @handle:	G-Link channel handle
 * @priv:	Private callback data
 * @req_size:	The size of the requested intent
 *
 * This function is a notification callback from the G-Link core of the remote
 * side's request for an RX intent to be queued.
 *
 * Return: Boolean indicating whether or not the request was successfully
 *         received
 */
bool glink_ssr_notify_rx_intent_req(void *handle, const void *priv,
		size_t req_size)
{
	struct ssr_notify_data *cb_data = (struct ssr_notify_data *)priv;

	if (!cb_data) {
		GLINK_ERR("<SSR> %s: Could not allocate data for cb_data\n",
				__func__);
		return false;
	} else {
		GLINK_INFO("<SSR> %s: rx_intent_req of size %zu\n", __func__,
				req_size);
		return true;
	}
}

/**
 * restart_notifier_cb() - SSR restart notifier callback function
 * @this:	Notifier block used by the SSR framework
 * @code:	The SSR code for which stage of restart is occurring
 * @data:	Structure containing private data - not used here.
 *
 * This function is a callback for the SSR framework. From here we initiate
 * our handling of SSR.
 *
 * Return: Status of SSR handling
 */
static int restart_notifier_cb(struct notifier_block *this,
				  unsigned long code,
				  void *data)
{
	struct subsys_info *ss_info = NULL;
	struct restart_notifier_block *notifier =
		container_of(this, struct restart_notifier_block, nb);
	int ret;

	if (code == SUBSYS_AFTER_SHUTDOWN) {
		GLINK_INFO("<SSR> %s: %s: subsystem restart for %s\n", __func__,
				"SUBSYS_AFTER_SHUTDOWN",
				notifier->subsystem);
		ss_info = get_info_for_subsystem(notifier->subsystem);
		if (ss_info == NULL) {
			GLINK_ERR("<SSR> %s: ss_info is NULL\n", __func__);
			return -EINVAL;
		}

		glink_ssr(ss_info->edge);
		ret = notify_for_subsystem(ss_info);

		if (ret) {
			GLINK_ERR("<SSR>: %s: %s, ret[%d]\n",
					__func__,
					"Subsystem notification failed",
					ret);
			return ret;
		}
	}

	return NOTIFY_DONE;
}

/**
 * notify for subsystem() - Notify other subsystems that a subsystem is being
 *                          restarted
 * @ss_info:	Subsystem info structure for the subsystem being restarted
 *
 * This function sends notifications to affected subsystems that the subsystem
 * in ss_info is being restarted, and waits for the cleanup done response from
 * all of those subsystems. It also initiates any local cleanup that is
 * necessary.
 *
 * Return: 0 on success, standard error codes otherwise
 */
static int notify_for_subsystem(struct subsys_info *ss_info)
{
	struct subsys_info *ss_info_channel;
	struct subsys_info_leaf *ss_leaf_entry;
	struct do_cleanup_msg *do_cleanup_data;
	void *handle;
	int wait_ret;

	if (!ss_info) {
		GLINK_ERR("<SSR> %s: ss_info structure invalid\n", __func__);
		return -EINVAL;
	}

	atomic_set(&responses_remaining, ss_info->notify_list_len);
	init_waitqueue_head(&waitqueue);

	list_for_each_entry(ss_leaf_entry, &ss_info->notify_list,
			notify_list_node) {
		GLINK_INFO("<SSR> %s: Notifying: %s -> %s:%s:%s\n",
				__func__, "subsys:edge:xprt",
				ss_leaf_entry->ssr_name,
				ss_leaf_entry->edge,
				ss_leaf_entry->xprt);

		ss_info_channel =
			get_info_for_subsystem(ss_leaf_entry->ssr_name);
		if (ss_info_channel == NULL) {
			GLINK_ERR("<SSR> %s: unable to find subsystem name\n",
					__func__);
			return -ENODEV;
		}
		handle = ss_info_channel->handle;
		ss_leaf_entry->cb_data = ss_info_channel->cb_data;

		do_cleanup_data = kmalloc(sizeof(struct do_cleanup_msg),
				GFP_KERNEL);
		if (!do_cleanup_data) {
			GLINK_ERR("%s %s: Could not allocate do_cleanup_msg\n",
					"<SSR>", __func__);
			return -ENOMEM;
		}

		do_cleanup_data->version = 0;
		do_cleanup_data->command = GLINK_SSR_DO_CLEANUP;
		do_cleanup_data->seq_num = sequence_number;
		do_cleanup_data->name_len = strlen(ss_info->edge);
		strlcpy(do_cleanup_data->name, ss_info->edge,
				do_cleanup_data->name_len + 1);
		ss_leaf_entry->cb_data->version = 0;
		ss_leaf_entry->cb_data->seq_num = sequence_number;
		sequence_number++;

		glink_queue_rx_intent(handle, (void *)ss_leaf_entry->cb_data,
				sizeof(struct cleanup_done_msg));
		GLINK_INFO("<SSR> %s: Queued intent of size %zu\n",
				__func__, sizeof(struct cleanup_done_msg));

		glink_tx(handle, (void *)ss_leaf_entry->cb_data,
				(void *)do_cleanup_data,
				sizeof(struct do_cleanup_msg), true);
	}

	wait_ret = wait_event_timeout(waitqueue,
			atomic_read(&responses_remaining) == 0,
			GLINK_SSR_REPLY_TIMEOUT);

	list_for_each_entry(ss_leaf_entry, &ss_info->notify_list,
			notify_list_node) {
		if (!wait_ret && !ss_leaf_entry->cb_data->responded) {
			GLINK_ERR("%s %s: Subsystem %s %s\n",
				"<SSR>", __func__, ss_leaf_entry->edge,
				"failed to respond. Restarting.");
			subsystem_restart(ss_leaf_entry->ssr_name);
		}
	}
	return 0;
}

/**
 * configure_and_open_channel() - configure and open a G-Link channel for
 *                                the given subsystem
 * @ss_info:	The subsys_info structure where the channel will be stored
 *
 * Return: 0 on success, standard error codes otherwise
 */
static int configure_and_open_channel(struct subsys_info *ss_info)
{
	struct glink_open_config open_cfg;
	struct ssr_notify_data *cb_data = NULL;
	void *handle = NULL;

	if (!ss_info) {
		GLINK_ERR("<SSR> %s: ss_info structure invalid\n", __func__);
		return -EINVAL;
	}

	cb_data = kmalloc(sizeof(struct ssr_notify_data), GFP_KERNEL);
	if (!cb_data) {
		GLINK_ERR("<SSR> %s: Could not allocate cb_data\n", __func__);
		return -ENOMEM;
	}
	cb_data->responded = false;
	ss_info->cb_data = cb_data;

	memset(&open_cfg, 0, sizeof(struct glink_open_config));

	if (ss_info->xprt)
		open_cfg.transport = ss_info->xprt;
	else
		open_cfg.transport = NULL;
	open_cfg.edge = ss_info->edge;
	open_cfg.name = "glink_ssr";
	open_cfg.notify_rx = glink_ssr_notify_rx;
	open_cfg.notify_tx_done = glink_ssr_notify_tx_done;
	open_cfg.notify_state = glink_ssr_notify_state;
	open_cfg.notify_rx_intent_req = glink_ssr_notify_rx_intent_req;
	open_cfg.priv = ss_info->cb_data;

	handle = glink_open(&open_cfg);
	if (IS_ERR_OR_NULL(handle)) {
		GLINK_ERR("<SSR> %s:%s %s: unable to open channel\n",
				 open_cfg.edge, open_cfg.name, __func__);
		kfree(cb_data);
		return -ENOMEM;
	}
	ss_info->handle = handle;
	return 0;
}

/**
 * get_info_for_subsystem() - Retrieve information about a subsystem from the
 *                            global subsystem_info_list
 * @subsystem:	The name of the subsystem recognized by the SSR
 *		framework
 *
 * Return: subsys_info structure containing info for the requested subsystem;
 *         NULL if no structure can be found for the requested subsystem
 */
static struct subsys_info *get_info_for_subsystem(const char *subsystem)
{
	struct subsys_info *ss_info_entry;

	list_for_each_entry(ss_info_entry, &subsystem_list,
			subsystem_list_node) {
		if (!strcmp(subsystem, ss_info_entry->ssr_name))
			return ss_info_entry;
	}

	return NULL;
}

/**
 * get_info_for_edge() - Retrieve information about a subsystem from the
 *                       global subsystem_info_list
 * @edge:	The name of the edge recognized by G-Link
 *
 * Return: subsys_info structure containing info for the requested subsystem;
 *         NULL if no structure can be found for the requested subsystem
 */
static struct subsys_info *get_info_for_edge(const char *edge)
{
	struct subsys_info *ss_info_entry;

	list_for_each_entry(ss_info_entry, &subsystem_list,
			subsystem_list_node) {
		if (!strcmp(edge, ss_info_entry->edge))
			return ss_info_entry;
	}

	return NULL;
}

/**
 * delete_ss_info_notify_list() - Delete the notify list for a subsystem
 * @ss_info:	The subsystem info structure
 */
static void delete_ss_info_notify_list(struct subsys_info *ss_info)
{
	struct subsys_info_leaf *leaf, *temp;

	list_for_each_entry_safe(leaf, temp, &ss_info->notify_list,
			notify_list_node) {
		list_del(&leaf->notify_list_node);
		kfree(leaf);
	}
}

/**
 * print_subsystem_list() - Display the global subsystem information list
 */
static void print_subsystem_list(void)
{
	struct subsys_info *ss_info_entry;
	struct subsys_info_leaf *ss_leaf_entry;

	list_for_each_entry(ss_info_entry, &subsystem_list,
			subsystem_list_node) {
		GLINK_INFO("%s %s: info node: subsys:edge:xprt -> %s:%s:%s\n",
			"<SSR>", __func__, ss_info_entry->ssr_name,
			ss_info_entry->edge, ss_info_entry->xprt);

		list_for_each_entry(ss_leaf_entry, &ss_info_entry->notify_list,
				notify_list_node) {
			GLINK_INFO("%s: %s: leaf/notify node: %s -> %s:%s:%s\n",
					"<SSR>", __func__, "subsys:edge:xprt",
					ss_leaf_entry->ssr_name,
					ss_leaf_entry->edge,
					ss_leaf_entry->xprt);
		}
	}
}

/**
 * glink_ssr_probe() - G-Link SSR platform device probe function
 * @pdev:	Pointer to the platform device structure
 *
 * This function parses DT for information on which subsystems should be
 * notified when each subsystem undergoes SSR. The global subsystem information
 * list is built from this information. In addition, SSR notifier callback
 * functions are registered here for the necessary subsystems.
 *
 * Return: 0 on success, standard error codes otherwise
 */
static int glink_ssr_probe(struct platform_device *pdev)
{
	struct device_node *node;
	struct device_node *phandle_node;
	struct restart_notifier_block *nb;
	struct subsys_info *ss_info;
	struct subsys_info_leaf *ss_info_leaf;
	struct glink_link_info *link_info;
	char *key;
	const char *edge;
	const char *subsys_name;
	const char *xprt;
	void *handle;
	void *link_state_handle;
	int phandle_index = 0;
	int ret = 0;

	if (!pdev) {
		GLINK_ERR("<SSR> %s: pdev is NULL\n", __func__);
		ret = -EINVAL;
		goto pdev_null_or_ss_info_alloc_failed;
	}

	node = pdev->dev.of_node;

	ss_info = kmalloc(sizeof(*ss_info), GFP_KERNEL);
	if (!ss_info) {
		GLINK_ERR("<SSR> %s: %s\n", __func__,
			"Could not allocate subsystem info structure\n");
		ret = -ENOMEM;
		goto pdev_null_or_ss_info_alloc_failed;
	}
	INIT_LIST_HEAD(&ss_info->notify_list);

	link_info = kmalloc(sizeof(struct glink_link_info),
			GFP_KERNEL);
	if (!link_info) {
		GLINK_ERR("<SSR> %s: %s\n", __func__,
			"Could not allocate link info structure\n");
		ret = -ENOMEM;
		goto link_info_alloc_failed;
	}
	ss_info->link_info = link_info;

	key = "label";
	subsys_name = of_get_property(node, key, NULL);
	if (!subsys_name) {
		GLINK_ERR("<SSR> %s: missing key %s\n", __func__, key);
		ret = -ENODEV;
		goto label_or_edge_missing;
	}

	key = "qcom,edge";
	edge = of_get_property(node, key, NULL);
	if (!edge) {
		GLINK_ERR("<SSR> %s: missing key %s\n", __func__, key);
		ret = -ENODEV;
		goto label_or_edge_missing;
	}

	key = "qcom,xprt";
	xprt = of_get_property(node, key, NULL);
	if (!xprt)
		GLINK_INFO("%s %s: no transport present for subys/edge %s/%s\n",
				"<SSR>", __func__, subsys_name, edge);

	ss_info->ssr_name = subsys_name;
	ss_info->edge = edge;
	ss_info->xprt = xprt;
	ss_info->notify_list_len = 0;
	ss_info->link_info->transport = xprt;
	ss_info->link_info->edge = edge;
	ss_info->link_info->glink_link_state_notif_cb = glink_ssr_link_state_cb;

	link_state_handle = glink_register_link_state_cb(ss_info->link_info,
			NULL);
	if (IS_ERR_OR_NULL(link_state_handle)) {
		GLINK_ERR("<SSR> %s: Could not register link state cb\n",
				__func__);
		ret = PTR_ERR(link_state_handle);
		goto label_or_edge_missing;
	}

	nb = kmalloc(sizeof(struct restart_notifier_block), GFP_KERNEL);
	if (!nb) {
		GLINK_ERR("<SSR> %s: Could not allocate notifier block\n",
				__func__);
		ret = -ENOMEM;
		goto restart_notifier_fail;
	}

	nb->subsystem = subsys_name;
	nb->nb.notifier_call = restart_notifier_cb;

	handle = subsys_notif_register_notifier(nb->subsystem, &nb->nb);
	if (IS_ERR_OR_NULL(handle)) {
		GLINK_ERR("<SSR> %s: Could not register SSR notifier cb\n",
				__func__);
		ret = -EINVAL;
		goto nb_registration_fail;
	}

	key = "qcom,notify-edges";
	while (true) {
		phandle_node = of_parse_phandle(node, key, phandle_index++);
		if (!phandle_node && phandle_index == 0) {
			GLINK_ERR("<SSR> %s: %s", __func__,
					"qcom,notify-edges is not present but is required");
			ret = -ENODEV;
			goto notify_edges_not_present;
		}

		if (!phandle_node)
			break;

		ss_info_leaf = kmalloc(sizeof(struct subsys_info_leaf),
				GFP_KERNEL);
		if (!ss_info_leaf) {
			GLINK_ERR("<SSR> %s: Could not allocate %s\n",
					"subsys_info_leaf structure",
					__func__);
			ret = -ENOMEM;
			goto notify_edges_not_present;
		}

		subsys_name = of_get_property(phandle_node, "label", NULL);
		edge = of_get_property(phandle_node, "qcom,edge", NULL);
		xprt = of_get_property(phandle_node, "qcom,xprt", NULL);

		of_node_put(phandle_node);

		if (!subsys_name || !edge) {
			GLINK_ERR("%s, %s: Found DT node with invalid data!\n",
					"<SSR>", __func__);
			ret = -EINVAL;
			goto invalid_dt_node;
		}

		ss_info_leaf->ssr_name = subsys_name;
		ss_info_leaf->edge = edge;
		ss_info_leaf->xprt = xprt;
		list_add_tail(&ss_info_leaf->notify_list_node,
				&ss_info->notify_list);
		ss_info->notify_list_len++;
	}

	list_add_tail(&ss_info->subsystem_list_node, &subsystem_list);
	return 0;

invalid_dt_node:
	kfree(ss_info_leaf);
notify_edges_not_present:
	subsys_notif_unregister_notifier(handle, &nb->nb);
	delete_ss_info_notify_list(ss_info);
nb_registration_fail:
	kfree(nb);
restart_notifier_fail:
	glink_unregister_link_state_cb(link_state_handle);
label_or_edge_missing:
	kfree(link_info);
link_info_alloc_failed:
	kfree(ss_info);
pdev_null_or_ss_info_alloc_failed:
	return ret;
}

static struct of_device_id match_table[] = {
	{ .compatible = "qcom,glink_ssr" },
	{},
};

static struct platform_driver glink_ssr_driver = {
	.probe = glink_ssr_probe,
	.driver = {
		.name = "msm_glink_ssr",
		.owner = THIS_MODULE,
		.of_match_table = match_table,
	},
};

static int glink_ssr_init(void)
{
	int ret;

	ret = platform_driver_register(&glink_ssr_driver);
	if (ret)
		GLINK_ERR("<SSR> %s: %s ret: %d\n", __func__,
				"glink_ssr driver registration failed", ret);

	print_subsystem_list();
	return 0;
}

module_init(glink_ssr_init);

MODULE_DESCRIPTION("MSM Generic Link (G-Link) SSR Module");
MODULE_LICENSE("GPL v2");
