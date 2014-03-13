/*
 * Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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
#include <soc/qcom/hsic_sysmon.h>
#include <soc/qcom/sysmon.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/smd.h>

#define TX_BUF_SIZE	50
#define RX_BUF_SIZE	500
#define TIMEOUT_MS	5000

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
};

static struct sysmon_subsys subsys[SYSMON_NUM_SS] = {
	[SYSMON_SS_MODEM].transport     = TRANSPORT_SMD,
	[SYSMON_SS_LPASS].transport     = TRANSPORT_SMD,
	[SYSMON_SS_WCNSS].transport     = TRANSPORT_SMD,
	[SYSMON_SS_DSPS].transport      = TRANSPORT_SMD,
	[SYSMON_SS_Q6FW].transport      = TRANSPORT_SMD,
	[SYSMON_SS_EXT_MODEM].transport = TRANSPORT_HSIC,
};

static const char *notif_name[SUBSYS_NOTIF_TYPE_COUNT] = {
	[SUBSYS_BEFORE_SHUTDOWN] = "before_shutdown",
	[SUBSYS_AFTER_SHUTDOWN]  = "after_shutdown",
	[SUBSYS_BEFORE_POWERUP]  = "before_powerup",
	[SUBSYS_AFTER_POWERUP]   = "after_powerup",
};

struct enum_name_map {
	int id;
	const char name[50];
};

static struct enum_name_map map[SYSMON_NUM_SS] = {
	{SYSMON_SS_WCNSS, "wcnss"},
	{SYSMON_SS_MODEM, "modem"},
	{SYSMON_SS_LPASS, "adsp"},
	{SYSMON_SS_Q6FW, "modem_fw"},
	{SYSMON_SS_EXT_MODEM, "external_modem"},
	{SYSMON_SS_DSPS, "dsps"},
};

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
 * sysmon_send_event() - Notify a subsystem of another's state change
 * @dest_ss:	ID of subsystem the notification should be sent to
 * @event_ss:	String name of the subsystem that generated the notification
 * @notif:	ID of the notification type (ex. SUBSYS_BEFORE_SHUTDOWN)
 *
 * Returns 0 for success, -EINVAL for invalid destination or notification IDs,
 * -ENODEV if the transport channel is not open, -ETIMEDOUT if the destination
 * subsystem does not respond, and -ENOSYS if the destination subsystem
 * responds, but with something other than an acknowledgement.
 *
 * If CONFIG_MSM_SYSMON_COMM is not defined, always return success (0).
 */
int sysmon_send_event(const char *dest_ss, const char *event_ss,
		      enum subsys_notif_type notif)
{

	char tx_buf[TX_BUF_SIZE];
	int ret, i;
	struct sysmon_subsys *ss = NULL;

	for (i = 0; i < ARRAY_SIZE(map); i++) {
		if (!strcmp(map[i].name, dest_ss)) {
			ss = &subsys[map[i].id];
			break;
		}
	}

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
	if (ret)
		goto out;

	if (strncmp(ss->rx_buf, "ssr:ack", ARRAY_SIZE(ss->rx_buf)))
		ret = -ENOSYS;
out:
	mutex_unlock(&ss->lock);
	return ret;
}

/**
 * sysmon_send_shutdown() - send shutdown command to a
 * subsystem.
 * @dest_ss:	ID of subsystem to send to.
 *
 * Returns 0 for success, -EINVAL for an invalid destination, -ENODEV if
 * the SMD transport channel is not open, -ETIMEDOUT if the destination
 * subsystem does not respond, and -ENOSYS if the destination subsystem
 * responds with something unexpected.
 *
 * If CONFIG_MSM_SYSMON_COMM is not defined, always return success (0).
 */
int sysmon_send_shutdown(enum subsys_id dest_ss)
{
	struct sysmon_subsys *ss = &subsys[dest_ss];
	const char tx_buf[] = "system:shutdown";
	const char expect[] = "system:ack";
	size_t prefix_len = ARRAY_SIZE(expect) - 1;
	int ret;

	if (ss->dev == NULL)
		return -ENODEV;

	if (dest_ss < 0 || dest_ss >= SYSMON_NUM_SS)
		return -EINVAL;

	mutex_lock(&ss->lock);
	ret = sysmon_send_msg(ss, tx_buf, ARRAY_SIZE(tx_buf));
	if (ret)
		goto out;

	if (strncmp(ss->rx_buf, expect, prefix_len))
		ret = -ENOSYS;
out:
	mutex_unlock(&ss->lock);
	return ret;
}

/**
 * sysmon_get_reason() - Retrieve failure reason from a subsystem.
 * @dest_ss:	ID of subsystem to query
 * @buf:	Caller-allocated buffer for the returned NUL-terminated reason
 * @len:	Length of @buf
 *
 * Returns 0 for success, -EINVAL for an invalid destination, -ENODEV if
 * the SMD transport channel is not open, -ETIMEDOUT if the destination
 * subsystem does not respond, and -ENOSYS if the destination subsystem
 * responds with something unexpected.
 *
 * If CONFIG_MSM_SYSMON_COMM is not defined, always return success (0).
 */
int sysmon_get_reason(enum subsys_id dest_ss, char *buf, size_t len)
{
	struct sysmon_subsys *ss = &subsys[dest_ss];
	const char tx_buf[] = "ssr:retrieve:sfr";
	const char expect[] = "ssr:return:";
	size_t prefix_len = ARRAY_SIZE(expect) - 1;
	int ret;

	if (ss->dev == NULL)
		return -ENODEV;

	if (dest_ss < 0 || dest_ss >= SYSMON_NUM_SS ||
	    buf == NULL || len == 0)
		return -EINVAL;

	mutex_lock(&ss->lock);
	ret = sysmon_send_msg(ss, tx_buf, ARRAY_SIZE(tx_buf));
	if (ret)
		goto out;

	if (strncmp(ss->rx_buf, expect, prefix_len)) {
		ret = -ENOSYS;
		goto out;
	}
	strlcpy(buf, ss->rx_buf + prefix_len, len);
out:
	mutex_unlock(&ss->lock);
	return ret;
}

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

	ss = &subsys[pdev->id];
	mutex_init(&ss->lock);

	switch (ss->transport) {
	case TRANSPORT_SMD:
		if (pdev->id >= SMD_NUM_TYPE)
			return -EINVAL;

		ret = smd_named_open_on_edge("sys_mon", pdev->id, &ss->chan, ss,
					     sysmon_smd_notify);
		if (ret) {
			pr_err("SMD open failed\n");
			return ret;
		}

		smd_disable_read_intr(ss->chan);
		break;
	case TRANSPORT_HSIC:
		if (pdev->id < SMD_NUM_TYPE)
			return -EINVAL;

		ret = hsic_sysmon_open(HSIC_SYSMON_DEV_EXT_MODEM);
		if (ret) {
			pr_err("HSIC open failed\n");
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}
	ss->dev = &pdev->dev;

	return 0;
}

static int sysmon_remove(struct platform_device *pdev)
{
	struct sysmon_subsys *ss = &subsys[pdev->id];

	ss->dev = NULL;

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
