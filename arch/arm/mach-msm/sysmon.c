/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <mach/msm_smd.h>
#include <mach/subsystem_notif.h>

#include "sysmon.h"

#define MAX_MSG_LENGTH	50
#define TIMEOUT_MS	5000

struct sysmon_subsys {
	struct mutex		lock;
	struct smd_channel	*chan;
	bool			chan_open;
	struct completion	resp_ready;
	char			rx_buf[MAX_MSG_LENGTH];
};

static struct sysmon_subsys subsys[SYSMON_NUM_SS];

static const char *notif_name[SUBSYS_NOTIF_TYPE_COUNT] = {
	[SUBSYS_BEFORE_SHUTDOWN] = "before_shutdown",
	[SUBSYS_AFTER_SHUTDOWN]  = "after_shutdown",
	[SUBSYS_BEFORE_POWERUP]  = "before_powerup",
	[SUBSYS_AFTER_POWERUP]   = "after_powerup",
};

int sysmon_send_event(enum subsys_id dest_ss, const char *event_ss,
		      enum subsys_notif_type notif)
{
	struct sysmon_subsys *ss = &subsys[dest_ss];
	char tx_buf[MAX_MSG_LENGTH];
	int ret;

	if (dest_ss < 0 || dest_ss >= SYSMON_NUM_SS ||
	    notif < 0 || notif >= SUBSYS_NOTIF_TYPE_COUNT ||
	    event_ss == NULL)
		return -EINVAL;

	if (!ss->chan_open)
		return -ENODEV;

	mutex_lock(&ss->lock);
	init_completion(&ss->resp_ready);
	snprintf(tx_buf, ARRAY_SIZE(tx_buf), "ssr:%s:%s", event_ss,
		 notif_name[notif]);
	pr_debug("Sending message: %s\n", tx_buf);
	smd_write(ss->chan, tx_buf, ARRAY_SIZE(tx_buf));
	ret = wait_for_completion_timeout(&ss->resp_ready,
					  msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		ret = -ETIMEDOUT;
	} else if (strncmp(ss->rx_buf, "ssr:ack", ARRAY_SIZE(ss->rx_buf))) {
		pr_debug("Received response: %s\n", ss->rx_buf);
		ret = -ENOSYS;
	} else {
		ret = 0;
	}
	mutex_unlock(&ss->lock);

	return ret;
}

static void sysmon_notify(void *priv, unsigned int smd_event)
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
	static const uint32_t ss_map[SMD_NUM_TYPE] = {
		[SMD_APPS_MODEM]	= SYSMON_SS_MODEM,
		[SMD_APPS_QDSP]		= SYSMON_SS_LPASS,
		[SMD_APPS_WCNSS]	= SYSMON_SS_WCNSS,
		[SMD_APPS_DSPS]		= SYSMON_SS_DSPS,
		[SMD_APPS_Q6FW]		= SYSMON_SS_Q6FW,
	};
	struct sysmon_subsys *ss;
	int ret;

	if (pdev == NULL)
		return -EINVAL;

	if (pdev->id < 0 || pdev->id >= SMD_NUM_TYPE ||
	    ss_map[pdev->id] < 0 || ss_map[pdev->id] >= SYSMON_NUM_SS)
		return -ENODEV;

	ss = &subsys[ss_map[pdev->id]];
	mutex_init(&ss->lock);

	/* Open and configure the SMD channel */
	ret = smd_named_open_on_edge("sys_mon", pdev->id, &ss->chan,
				     ss, sysmon_notify);
	if (ret) {
		pr_err("SMD open failed\n");
		return -ENOSYS;
	}
	smd_disable_read_intr(ss->chan);

	return 0;
}

static int __devexit sysmon_remove(struct platform_device *pdev)
{
	smd_close(subsys[pdev->id].chan);
	return 0;
}

static struct platform_driver sysmon_driver = {
	.probe		= sysmon_probe,
	.remove		= __devexit_p(sysmon_remove),
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
