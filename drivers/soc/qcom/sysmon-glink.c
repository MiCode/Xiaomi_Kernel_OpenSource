/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/workqueue.h>

#include <soc/qcom/sysmon.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/glink.h>

#define TX_BUF_SIZE	50
#define RX_BUF_SIZE	500
#define TIMEOUT_MS	500

/**
 * struct sysmon_subsys - Sysmon info structure for subsystem
 * name:	subsys_desc name
 * edge:	name of the G-Link edge.
 * handle:	glink_ssr channel used for this subsystem.
 * rx_buf:	Buffer used to store received message.
 * chan_open:	Set when GLINK_CONNECTED. Reset otherwise.
 * event:	Last stored glink state event.
 * glink_handle:	Notifier handle reference.
 * resp_ready:	Completion struct for event response.
 */
struct sysmon_subsys {
	const char		*name;
	const char		*edge;
	void			*handle;
	struct glink_link_info	*link_info;
	char			rx_buf[RX_BUF_SIZE];
	bool			chan_open;
	unsigned		event;
	void			*glink_handle;
	int			intent_count;
	struct completion	resp_ready;
	struct mutex		lock;
	struct workqueue_struct *glink_event_wq;
	struct work_struct	work;
	struct list_head	list;
};

static const char *notif_name[SUBSYS_NOTIF_TYPE_COUNT] = {
	[SUBSYS_BEFORE_SHUTDOWN] = "before_shutdown",
	[SUBSYS_AFTER_SHUTDOWN]  = "after_shutdown",
	[SUBSYS_BEFORE_POWERUP]  = "before_powerup",
	[SUBSYS_AFTER_POWERUP]   = "after_powerup",
};

static LIST_HEAD(sysmon_glink_list);
static DEFINE_MUTEX(sysmon_glink_list_lock);

static struct sysmon_subsys *_find_subsys(struct subsys_desc *desc)
{
	struct sysmon_subsys *ss;

	if (desc == NULL)
		return NULL;

	mutex_lock(&sysmon_glink_list_lock);
	list_for_each_entry(ss, &sysmon_glink_list, list) {
		if (!strcmp(ss->name, desc->name)) {
			mutex_unlock(&sysmon_glink_list_lock);
			return ss;
		}
	}
	mutex_unlock(&sysmon_glink_list_lock);

	return NULL;
}

static int sysmon_send_msg(struct sysmon_subsys *ss, const char *tx_buf,
			   size_t len)
{
	int ret;
	void *handle;

	if (!ss->chan_open)
		return -ENODEV;

	if (!ss->handle)
		return -EINVAL;

	init_completion(&ss->resp_ready);
	handle = ss->handle;

	/* Register an intent to receive data */
	if (!ss->intent_count) {
		ret = glink_queue_rx_intent(handle, (void *)ss,
						sizeof(ss->rx_buf));
		if (ret) {
			pr_err("Failed to register receive intent\n");
			return ret;
		}
		ss->intent_count++;
	}

	pr_debug("Sending sysmon message: %s\n", tx_buf);
	ret = glink_tx(handle, (void *)ss, (void *)tx_buf, len,
						GLINK_TX_REQ_INTENT);
	if (ret) {
		pr_err("Failed to send sysmon message!\n");
		return ret;
	}

	ret = wait_for_completion_timeout(&ss->resp_ready,
				  msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("Timed out waiting for response\n");
		return -ETIMEDOUT;
	}
	pr_debug("Received response: %s\n", ss->rx_buf);
	return ret;
}

/**
 * sysmon_send_event_no_qmi() - Notify a subsystem of another's state change
 * @dest_desc:	Subsystem descriptor of the subsystem the notification
 * should be sent to
 * @event_desc:	Subsystem descriptor of the subsystem that generated the
 * notification
 * @notif:	ID of the notification type (ex. SUBSYS_BEFORE_SHUTDOWN)
 *
 * Returns 0 for success, -EINVAL for invalid destination or notification IDs,
 * -ENODEV if the transport channel is not open, -ETIMEDOUT if the destination
 * subsystem does not respond, and -ENOSYS if the destination subsystem
 * responds, but with something other than an acknowledgement.
 *
 * If CONFIG_MSM_SYSMON_GLINK_COMM is not defined, always return success (0).
 */
int sysmon_send_event_no_qmi(struct subsys_desc *dest_desc,
			struct subsys_desc *event_desc,
			enum subsys_notif_type notif)
{

	char tx_buf[TX_BUF_SIZE];
	int ret;
	struct sysmon_subsys *ss = NULL;

	ss = _find_subsys(dest_desc);
	if (ss == NULL)
		return -EINVAL;

	if (event_desc == NULL || notif < 0 || notif >= SUBSYS_NOTIF_TYPE_COUNT
			|| notif_name[notif] == NULL)
		return -EINVAL;

	snprintf(tx_buf, sizeof(tx_buf), "ssr:%s:%s", event_desc->name,
		 notif_name[notif]);

	mutex_lock(&ss->lock);
	ret = sysmon_send_msg(ss, tx_buf, strlen(tx_buf));
	if (ret < 0) {
		mutex_unlock(&ss->lock);
		pr_err("Message sending failed %d\n", ret);
		goto out;
	}

	if (strcmp(ss->rx_buf, "ssr:ack")) {
		mutex_unlock(&ss->lock);
		pr_debug("Unexpected response %s\n", ss->rx_buf);
		ret = -ENOSYS;
		goto out;
	}
	mutex_unlock(&ss->lock);
out:
	return ret;
}
EXPORT_SYMBOL(sysmon_send_event_no_qmi);

/**
 * sysmon_send_shutdown_no_qmi() - send shutdown command to a subsystem.
 * @dest_desc:	Subsystem descriptor of the subsystem to send to
 *
 * Returns 0 for success, -EINVAL for an invalid destination, -ENODEV if
 * the SMD transport channel is not open, -ETIMEDOUT if the destination
 * subsystem does not respond, and -ENOSYS if the destination subsystem
 * responds with something unexpected.
 *
 * If CONFIG_MSM_SYSMON_GLINK_COMM is not defined, always return success (0).
 */
int sysmon_send_shutdown_no_qmi(struct subsys_desc *dest_desc)
{
	struct sysmon_subsys *ss = NULL;
	const char tx_buf[] = "system:shutdown";
	const char expect[] = "system:ack";
	int ret;

	ss = _find_subsys(dest_desc);
	if (ss == NULL)
		return -EINVAL;

	mutex_lock(&ss->lock);
	ret = sysmon_send_msg(ss, tx_buf, sizeof(tx_buf));
	if (ret < 0) {
		mutex_unlock(&ss->lock);
		pr_err("Message sending failed %d\n", ret);
		goto out;
	}

	if (strcmp(ss->rx_buf, expect)) {
		mutex_unlock(&ss->lock);
		pr_err("Unexpected response %s\n", ss->rx_buf);
		ret = -ENOSYS;
		goto out;
	}
	mutex_unlock(&ss->lock);
out:
	return ret;
}
EXPORT_SYMBOL(sysmon_send_shutdown_no_qmi);

/**
 * sysmon_get_reason_no_qmi() - Retrieve failure reason from a subsystem.
 * @dest_desc:	Subsystem descriptor of the subsystem to query
 * @buf:	Caller-allocated buffer for the returned NULL-terminated reason
 * @len:	Length of @buf
 *
 * Returns 0 for success, -EINVAL for an invalid destination, -ENODEV if
 * the SMD transport channel is not open, -ETIMEDOUT if the destination
 * subsystem does not respond, and -ENOSYS if the destination subsystem
 * responds with something unexpected.
 *
 * If CONFIG_MSM_SYSMON_GLINK_COMM is not defined, always return success (0).
 */
int sysmon_get_reason_no_qmi(struct subsys_desc *dest_desc,
				char *buf, size_t len)
{
	struct sysmon_subsys *ss = NULL;
	const char tx_buf[] = "ssr:retrieve:sfr";
	const char expect[] = "ssr:return:";
	size_t prefix_len = ARRAY_SIZE(expect) - 1;
	int ret;

	ss = _find_subsys(dest_desc);
	if (ss == NULL || buf == NULL || len == 0)
		return -EINVAL;

	mutex_lock(&ss->lock);
	ret = sysmon_send_msg(ss, tx_buf, sizeof(tx_buf));
	if (ret < 0) {
		mutex_unlock(&ss->lock);
		pr_err("Message sending failed %d\n", ret);
		goto out;
	}

	if (strncmp(ss->rx_buf, expect, prefix_len)) {
		mutex_unlock(&ss->lock);
		pr_err("Unexpected response %s\n", ss->rx_buf);
		ret = -ENOSYS;
		goto out;
	}
	strlcpy(buf, ss->rx_buf + prefix_len, len);
	mutex_unlock(&ss->lock);
out:
	return ret;
}
EXPORT_SYMBOL(sysmon_get_reason_no_qmi);

static void glink_notify_rx(void *handle, const void *priv,
		const void *pkt_priv, const void *ptr, size_t size)
{
	struct sysmon_subsys *ss = (struct sysmon_subsys *)priv;

	if (!ss) {
		pr_err("sysmon_subsys mapping failed\n");
		return;
	}

	memset(ss->rx_buf, 0, sizeof(ss->rx_buf));
	ss->intent_count--;
	if (sizeof(ss->rx_buf) > size)
		strlcpy(ss->rx_buf, ptr, size);
	else
		pr_warn("Invalid recv message size\n");
	glink_rx_done(ss->handle, ptr, false);
	complete(&ss->resp_ready);
}

static void glink_notify_tx_done(void *handle, const void *priv,
		const void *pkt_priv, const void *ptr)
{
	struct sysmon_subsys *cb_data = (struct sysmon_subsys *)priv;

	if (!cb_data)
		pr_err("sysmon_subsys mapping failed\n");
	else
		pr_debug("tx_done notification!\n");
}

static void glink_notify_state(void *handle, const void *priv, unsigned event)
{
	struct sysmon_subsys *ss = (struct sysmon_subsys *)priv;

	if (!ss) {
		pr_err("sysmon_subsys mapping failed\n");
		return;
	}

	mutex_lock(&ss->lock);
	ss->event = event;
	switch (event) {
	case GLINK_CONNECTED:
		ss->chan_open = true;
		break;
	case GLINK_REMOTE_DISCONNECTED:
		ss->chan_open = false;
		break;
	default:
		break;
	}
	mutex_unlock(&ss->lock);
}

static void glink_state_up_work_hdlr(struct work_struct *work)
{
	struct glink_open_config open_cfg;
	struct sysmon_subsys *ss = container_of(work, struct sysmon_subsys,
							work);
	void *handle = NULL;

	if (!ss) {
		pr_err("Invalid sysmon_subsys struct parameter\n");
		return;
	}

	memset(&open_cfg, 0, sizeof(struct glink_open_config));
	open_cfg.priv = (void *)ss;
	open_cfg.notify_rx = glink_notify_rx;
	open_cfg.notify_tx_done = glink_notify_tx_done;
	open_cfg.notify_state = glink_notify_state;
	open_cfg.edge = ss->edge;
	open_cfg.transport = "smd_trans";
	open_cfg.name = "sys_mon";

	handle = glink_open(&open_cfg);
	if (IS_ERR_OR_NULL(handle)) {
		pr_err("%s: %s: unable to open channel\n",
					open_cfg.edge, open_cfg.name);
		return;
	}
	ss->handle = handle;
}

static void glink_state_down_work_hdlr(struct work_struct *work)
{
	struct sysmon_subsys *ss = container_of(work, struct sysmon_subsys,
							work);

	if (ss->handle)
		glink_close(ss->handle);
	ss->handle = NULL;
}

static void sysmon_glink_cb(struct glink_link_state_cb_info *cb_info,
					void *priv)
{
	struct sysmon_subsys *ss = (struct sysmon_subsys *)priv;

	if (!cb_info || !ss) {
		pr_err("Invalid parameters\n");
		return;
	}

	mutex_lock(&ss->lock);
	switch (cb_info->link_state) {
	case GLINK_LINK_STATE_UP:
		pr_debug("LINK UP %s\n", ss->edge);
		INIT_WORK(&ss->work, glink_state_up_work_hdlr);
		queue_work(ss->glink_event_wq, &ss->work);
		break;
	case GLINK_LINK_STATE_DOWN:
		pr_debug("LINK DOWN %s\n", ss->edge);
		INIT_WORK(&ss->work, glink_state_down_work_hdlr);
		queue_work(ss->glink_event_wq, &ss->work);
		break;
	default:
		pr_warn("Invalid event notification\n");
		break;
	}
	mutex_unlock(&ss->lock);
}

int sysmon_glink_register(struct subsys_desc *desc)
{
	struct sysmon_subsys *ss;
	struct glink_link_info *link_info;
	int ret;

	if (!desc)
		return -EINVAL;

	ss = kzalloc(sizeof(*ss), GFP_KERNEL);
	if (!ss)
		return -ENOMEM;

	link_info = kzalloc(sizeof(struct glink_link_info), GFP_KERNEL);
	if (!link_info) {
		pr_err("Could not allocate link info structure\n");
		kfree(ss);
		return -ENOMEM;
	}

	ss->glink_event_wq = create_singlethread_workqueue(desc->name);
	if (ss->glink_event_wq == NULL) {
		ret = -ENOMEM;
		goto err_wq;
	}
	mutex_init(&ss->lock);

	ss->name = desc->name;
	ss->handle = NULL;
	ss->intent_count = 0;
	ss->link_info = link_info;
	ss->link_info->edge = ss->edge = desc->edge;
	ss->link_info->transport = "smd_trans";
	ss->link_info->glink_link_state_notif_cb = sysmon_glink_cb;

	ss->glink_handle = glink_register_link_state_cb(ss->link_info,
								(void *)ss);
	if (IS_ERR_OR_NULL(ss->glink_handle)) {
		pr_err("Could not register link state cb\n");
		ret = PTR_ERR(ss->glink_handle);
		goto err;
	}

	mutex_lock(&sysmon_glink_list_lock);
	INIT_LIST_HEAD(&ss->list);
	list_add_tail(&ss->list, &sysmon_glink_list);
	mutex_unlock(&sysmon_glink_list_lock);
	return 0;
err:
	destroy_workqueue(ss->glink_event_wq);
err_wq:
	kfree(link_info);
	kfree(ss);
	return ret;
}
EXPORT_SYMBOL(sysmon_glink_register);

void sysmon_glink_unregister(struct subsys_desc *desc)
{
	struct sysmon_subsys *ss = NULL;

	if (!desc)
		return;

	ss = _find_subsys(desc);
	if (ss == NULL)
		return;

	list_del(&ss->list);
	if (ss->handle)
		glink_close(ss->handle);
	destroy_workqueue(ss->glink_event_wq);
	glink_unregister_link_state_cb(ss->glink_handle);
	kfree(ss->link_info);
	kfree(ss);
}
EXPORT_SYMBOL(sysmon_glink_unregister);
