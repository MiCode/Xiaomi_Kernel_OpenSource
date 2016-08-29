/*
 * Copyright (c) 2011-2014, 2016 The Linux Foundation. All rights reserved.
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
#undef DEBUG

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <soc/qcom/hsic_sysmon.h>
#include <soc/qcom/sysmon.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/smd.h>

#define TX_BUF_SIZE	50
#define RX_BUF_SIZE	500
#define TIMEOUT_MS	500

enum transports {
	TRANSPORT_SMD,
	TRANSPORT_HSIC,
};

struct sysmon_subsys {
	struct mutex		lock;
	struct smd_channel	*chan;
	bool			chan_open;
	struct completion	resp_ready;
	char			rx_buf[RX_BUF_SIZE];
	enum transports		transport;
	struct device		*dev;
	u32			pid;
	struct list_head	list;
};

static const char *notif_name[SUBSYS_NOTIF_TYPE_COUNT] = {
	[SUBSYS_BEFORE_SHUTDOWN] = "before_shutdown",
	[SUBSYS_AFTER_SHUTDOWN]  = "after_shutdown",
	[SUBSYS_BEFORE_POWERUP]  = "before_powerup",
	[SUBSYS_AFTER_POWERUP]   = "after_powerup",
};

static LIST_HEAD(sysmon_list);
static DEFINE_MUTEX(sysmon_list_lock);

static int sysmon_send_smd(struct sysmon_subsys *ss, const char *tx_buf,
			   size_t len)
{
	int ret;

	if (!ss->chan_open)
		return -ENODEV;

	init_completion(&ss->resp_ready);
	pr_debug("Sending SMD message: %s\n", tx_buf);
	smd_write(ss->chan, tx_buf, len);
	ret = wait_for_completion_timeout(&ss->resp_ready,
				  msecs_to_jiffies(TIMEOUT_MS));
	if (!ret)
		return -ETIMEDOUT;

	return 0;
}

static int sysmon_send_hsic(struct sysmon_subsys *ss, const char *tx_buf,
			    size_t len)
{
	int ret;
	size_t actual_len;

	pr_debug("Sending HSIC message: %s\n", tx_buf);
	ret = hsic_sysmon_write(HSIC_SYSMON_DEV_EXT_MODEM,
				tx_buf, len, TIMEOUT_MS);
	if (ret)
		return ret;
	ret = hsic_sysmon_read(HSIC_SYSMON_DEV_EXT_MODEM, ss->rx_buf,
			       ARRAY_SIZE(ss->rx_buf), &actual_len, TIMEOUT_MS);
	return ret;
}

static int sysmon_send_msg(struct sysmon_subsys *ss, const char *tx_buf,
			   size_t len)
{
	int ret;

	switch (ss->transport) {
	case TRANSPORT_SMD:
		ret = sysmon_send_smd(ss, tx_buf, len);
		break;
	case TRANSPORT_HSIC:
		ret = sysmon_send_hsic(ss, tx_buf, len);
		break;
	default:
		ret = -EINVAL;
	}

	if (!ret)
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
 * subsystem does not respond, and -EPROTO if the destination subsystem
 * responds, but with something other than an acknowledgment.
 *
 * If CONFIG_MSM_SYSMON_COMM is not defined, always return success (0).
 */
int sysmon_send_event_no_qmi(struct subsys_desc *dest_desc,
			struct subsys_desc *event_desc,
			enum subsys_notif_type notif)
{

	char tx_buf[TX_BUF_SIZE];
	int ret;
	struct sysmon_subsys *tmp, *ss = NULL;
	const char *event_ss = event_desc->name;

	mutex_lock(&sysmon_list_lock);
	list_for_each_entry(tmp, &sysmon_list, list)
		if (tmp->pid == dest_desc->sysmon_pid)
			ss = tmp;
	mutex_unlock(&sysmon_list_lock);

	if (ss == NULL)
		return -EINVAL;

	if (ss->dev == NULL)
		return -ENODEV;

	if (notif < 0 || notif >= SUBSYS_NOTIF_TYPE_COUNT || event_ss == NULL ||
						notif_name[notif] == NULL)
		return -EINVAL;

	snprintf(tx_buf, ARRAY_SIZE(tx_buf), "ssr:%s:%s", event_ss,
		 notif_name[notif]);

	mutex_lock(&ss->lock);
	ret = sysmon_send_msg(ss, tx_buf, strlen(tx_buf));
	if (ret) {
		pr_err("Message sending failed %d\n", ret);
		goto out;
	}

	if (strcmp(ss->rx_buf, "ssr:ack")) {
		pr_debug("Unexpected response %s\n", ss->rx_buf);
		ret = -EPROTO;
	}
out:
	mutex_unlock(&ss->lock);
	return ret;
}
EXPORT_SYMBOL(sysmon_send_event_no_qmi);

/**
 * sysmon_send_shutdown_no_qmi() - send shutdown command to a subsystem.
 * @dest_desc:	Subsystem descriptor of the subsystem to send to
 *
 * Returns 0 for success, -EINVAL for an invalid destination, -ENODEV if
 * the SMD transport channel is not open, -ETIMEDOUT if the destination
 * subsystem does not respond, and -EPROTO if the destination subsystem
 * responds with something unexpected.
 *
 * If CONFIG_MSM_SYSMON_COMM is not defined, always return success (0).
 */
int sysmon_send_shutdown_no_qmi(struct subsys_desc *dest_desc)
{
	struct sysmon_subsys *tmp, *ss = NULL;
	const char tx_buf[] = "system:shutdown";
	const char expect[] = "system:ack";
	int ret;

	mutex_lock(&sysmon_list_lock);
	list_for_each_entry(tmp, &sysmon_list, list)
		if (tmp->pid == dest_desc->sysmon_pid)
			ss = tmp;
	mutex_unlock(&sysmon_list_lock);

	if (ss == NULL)
		return -EINVAL;

	if (ss->dev == NULL)
		return -ENODEV;

	mutex_lock(&ss->lock);
	ret = sysmon_send_msg(ss, tx_buf, ARRAY_SIZE(tx_buf));
	if (ret) {
		pr_err("Message sending failed %d\n", ret);
		goto out;
	}

	if (strcmp(ss->rx_buf, expect)) {
		pr_err("Unexpected response %s\n", ss->rx_buf);
		ret = -EPROTO;
	}
out:
	mutex_unlock(&ss->lock);
	return ret;
}
EXPORT_SYMBOL(sysmon_send_shutdown_no_qmi);

/**
 * sysmon_get_reason_no_qmi() - Retrieve failure reason from a subsystem.
 * @dest_desc:	Subsystem descriptor of the subsystem to query
 * @buf:	Caller-allocated buffer for the returned NUL-terminated reason
 * @len:	Length of @buf
 *
 * Returns 0 for success, -EINVAL for an invalid destination, -ENODEV if
 * the SMD transport channel is not open, -ETIMEDOUT if the destination
 * subsystem does not respond, and -EPROTO if the destination subsystem
 * responds with something unexpected.
 *
 * If CONFIG_MSM_SYSMON_COMM is not defined, always return success (0).
 */
int sysmon_get_reason_no_qmi(struct subsys_desc *dest_desc,
				char *buf, size_t len)
{
	struct sysmon_subsys *tmp, *ss = NULL;
	const char tx_buf[] = "ssr:retrieve:sfr";
	const char expect[] = "ssr:return:";
	size_t prefix_len = ARRAY_SIZE(expect) - 1;
	int ret;

	mutex_lock(&sysmon_list_lock);
	list_for_each_entry(tmp, &sysmon_list, list)
		if (tmp->pid == dest_desc->sysmon_pid)
			ss = tmp;
	mutex_unlock(&sysmon_list_lock);

	if (ss == NULL || buf == NULL || len == 0)
		return -EINVAL;

	if (ss->dev == NULL)
		return -ENODEV;

	mutex_lock(&ss->lock);
	ret = sysmon_send_msg(ss, tx_buf, ARRAY_SIZE(tx_buf));
	if (ret) {
		pr_err("Message sending failed %d\n", ret);
		goto out;
	}

	if (strncmp(ss->rx_buf, expect, prefix_len)) {
		pr_err("Unexpected response %s\n", ss->rx_buf);
		ret = -EPROTO;
		goto out;
	}
	strlcpy(buf, ss->rx_buf + prefix_len, len);
out:
	mutex_unlock(&ss->lock);
	return ret;
}
EXPORT_SYMBOL(sysmon_get_reason_no_qmi);

static void sysmon_smd_notify(void *priv, unsigned int smd_event)
{
	struct sysmon_subsys *ss = priv;

	switch (smd_event) {
	case SMD_EVENT_DATA: {
		if (smd_read_avail(ss->chan) > 0) {
			smd_read_from_cb(ss->chan, ss->rx_buf,
					 ARRAY_SIZE(ss->rx_buf));
			complete(&ss->resp_ready);
		}
		break;
	}
	case SMD_EVENT_OPEN:
		ss->chan_open = true;
		break;
	case SMD_EVENT_CLOSE:
		ss->chan_open = false;
		break;
	}
}

static int sysmon_probe(struct platform_device *pdev)
{
	struct sysmon_subsys *ss;
	int ret;

	if (pdev->id < 0 || pdev->id >= SYSMON_NUM_SS)
		return -ENODEV;

	ss = devm_kzalloc(&pdev->dev, sizeof(*ss), GFP_KERNEL);
	if (!ss)
		return -ENOMEM;

	mutex_init(&ss->lock);
	if (pdev->id == SYSMON_SS_EXT_MODEM) {
		ss->transport = TRANSPORT_HSIC;
		ret = hsic_sysmon_open(HSIC_SYSMON_DEV_EXT_MODEM);
		if (ret) {
			pr_err("HSIC open failed\n");
			return ret;
		}
	} else if (pdev->id < SMD_NUM_TYPE) {
		ss->transport = TRANSPORT_SMD;
		ret = smd_named_open_on_edge("sys_mon", pdev->id, &ss->chan,
						ss, sysmon_smd_notify);
		if (ret) {
			pr_err("SMD open failed\n");
			return ret;
		}
		smd_disable_read_intr(ss->chan);
	} else
		return -EINVAL;

	ss->dev = &pdev->dev;
	ss->pid = pdev->id;

	mutex_lock(&sysmon_list_lock);
	INIT_LIST_HEAD(&ss->list);
	list_add_tail(&ss->list, &sysmon_list);
	mutex_unlock(&sysmon_list_lock);
	return 0;
}

static int sysmon_remove(struct platform_device *pdev)
{
	struct sysmon_subsys *sysmon, *tmp, *ss = NULL;

	mutex_lock(&sysmon_list_lock);
	list_for_each_entry_safe(sysmon, tmp, &sysmon_list, list) {
		if (sysmon->pid == pdev->id) {
			ss = sysmon;
			list_del(&ss->list);
		}
	}
	mutex_unlock(&sysmon_list_lock);

	if (ss == NULL)
		return -EINVAL;

	mutex_lock(&ss->lock);
	switch (ss->transport) {
	case TRANSPORT_SMD:
		smd_close(ss->chan);
		break;
	case TRANSPORT_HSIC:
		hsic_sysmon_close(HSIC_SYSMON_DEV_EXT_MODEM);
		break;
	}
	mutex_unlock(&ss->lock);

	return 0;
}

static struct platform_driver sysmon_driver = {
	.probe		= sysmon_probe,
	.remove		= sysmon_remove,
	.driver		= {
		.name		= "sys_mon",
		.owner		= THIS_MODULE,
	},
};

static int __init sysmon_init(void)
{
	return platform_driver_register(&sysmon_driver);
}
subsys_initcall(sysmon_init);

static void __exit sysmon_exit(void)
{
	platform_driver_unregister(&sysmon_driver);
}
module_exit(sysmon_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("system monitor communication library");
MODULE_ALIAS("platform:sys_mon");
