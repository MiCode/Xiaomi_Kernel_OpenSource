/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

/*
 * RPCROUTER SMD XPRT module.
 */

#include <linux/platform_device.h>
#include <linux/types.h>

#include <mach/msm_smd.h>
#include "smd_rpcrouter.h"
#include "smd_private.h"

struct rpcrouter_smd_xprt {
	struct rpcrouter_xprt xprt;

	smd_channel_t *channel;
};

static struct rpcrouter_smd_xprt smd_remote_xprt;
#ifdef CONFIG_ARCH_FSM9XXX
static struct rpcrouter_smd_xprt smd_remote_qdsp_xprt;
#endif

static int rpcrouter_smd_remote_read_avail(void)
{
	return smd_read_avail(smd_remote_xprt.channel);
}

static int rpcrouter_smd_remote_read(void *data, uint32_t len)
{
	return smd_read(smd_remote_xprt.channel, data, len);
}

static int rpcrouter_smd_remote_write_avail(void)
{
	return smd_write_avail(smd_remote_xprt.channel);
}

static int rpcrouter_smd_remote_write(void *data, uint32_t len, uint32_t type)
{
	return smd_write(smd_remote_xprt.channel, data, len);
}

static int rpcrouter_smd_remote_close(void)
{
	smsm_change_state(SMSM_APPS_STATE, SMSM_RPCINIT, 0);
	return smd_close(smd_remote_xprt.channel);
}

static void rpcrouter_smd_remote_notify(void *_dev, unsigned event)
{
	switch (event) {
	case SMD_EVENT_DATA:
		msm_rpcrouter_xprt_notify(&smd_remote_xprt.xprt,
					  RPCROUTER_XPRT_EVENT_DATA);
		break;

	case SMD_EVENT_OPEN:
		pr_info("%s: smd opened 0x%p\n", __func__, _dev);

		msm_rpcrouter_xprt_notify(&smd_remote_xprt.xprt,
			RPCROUTER_XPRT_EVENT_OPEN);
		break;

	case SMD_EVENT_CLOSE:
		pr_info("%s: smd closed 0x%p\n", __func__, _dev);

		msm_rpcrouter_xprt_notify(&smd_remote_xprt.xprt,
				RPCROUTER_XPRT_EVENT_CLOSE);
		break;
    }
}

#ifdef CONFIG_ARCH_FSM9XXX
static int rpcrouter_smd_remote_qdsp_read_avail(void)
{
	return smd_read_avail(smd_remote_qdsp_xprt.channel);
}

static int rpcrouter_smd_remote_qdsp_read(void *data, uint32_t len)
{
	return smd_read(smd_remote_qdsp_xprt.channel, data, len);
}

static int rpcrouter_smd_remote_qdsp_write_avail(void)
{
	return smd_write_avail(smd_remote_qdsp_xprt.channel);
}

static int rpcrouter_smd_remote_qdsp_write(void *data,
		uint32_t len, uint32_t type)
{
	return smd_write(smd_remote_qdsp_xprt.channel, data, len);
}

static int rpcrouter_smd_remote_qdsp_close(void)
{
	/*
	 * TBD: Implement when we have N way SMSM ported
	 * smsm_change_state(SMSM_APPS_STATE, SMSM_RPCINIT, 0);
	 */
	return smd_close(smd_remote_qdsp_xprt.channel);
}

static void rpcrouter_smd_remote_qdsp_notify(void *_dev, unsigned event)
{
	switch (event) {
	case SMD_EVENT_DATA:
		msm_rpcrouter_xprt_notify(&smd_remote_qdsp_xprt.xprt,
			RPCROUTER_XPRT_EVENT_DATA);
		break;

	case SMD_EVENT_OPEN:
		/* Print log info */
		pr_debug("%s: smd opened\n", __func__);

		msm_rpcrouter_xprt_notify(&smd_remote_qdsp_xprt.xprt,
			RPCROUTER_XPRT_EVENT_OPEN);
		break;

	case SMD_EVENT_CLOSE:
		/* Print log info */
		pr_debug("%s: smd closed\n", __func__);

		msm_rpcrouter_xprt_notify(&smd_remote_qdsp_xprt.xprt,
				RPCROUTER_XPRT_EVENT_CLOSE);
		break;
	}
}

static int rpcrouter_smd_remote_qdsp_probe(struct platform_device *pdev)
{
	int rc;

	smd_remote_qdsp_xprt.xprt.name = "rpcrotuer_smd_qdsp_xprt";
	smd_remote_qdsp_xprt.xprt.read_avail =
			rpcrouter_smd_remote_qdsp_read_avail;
	smd_remote_qdsp_xprt.xprt.read = rpcrouter_smd_remote_qdsp_read;
	smd_remote_qdsp_xprt.xprt.write_avail =
			rpcrouter_smd_remote_qdsp_write_avail;
	smd_remote_qdsp_xprt.xprt.write = rpcrouter_smd_remote_qdsp_write;
	smd_remote_qdsp_xprt.xprt.close = rpcrouter_smd_remote_qdsp_close;
	smd_remote_qdsp_xprt.xprt.priv = NULL;

	/* Open up SMD channel */
	rc = smd_named_open_on_edge("RPCCALL_QDSP", SMD_APPS_QDSP,
			&smd_remote_qdsp_xprt.channel, NULL,
			rpcrouter_smd_remote_qdsp_notify);
	if (rc < 0)
		return rc;

	smd_disable_read_intr(smd_remote_qdsp_xprt.channel);

	return 0;
}

static struct platform_driver rpcrouter_smd_remote_qdsp_driver = {
	.probe		= rpcrouter_smd_remote_qdsp_probe,
	.driver		= {
			.name	= "RPCCALL_QDSP",
			.owner	= THIS_MODULE,
	},
};

static inline int register_smd_remote_qpsp_driver(void)
{
	return platform_driver_register(&rpcrouter_smd_remote_qdsp_driver);
}
#else /* CONFIG_ARCH_FSM9XXX */
static inline int register_smd_remote_qpsp_driver(void)
{
	return 0;
}
#endif

#if defined(CONFIG_MSM_RPC_LOOPBACK_XPRT)

static struct rpcrouter_smd_xprt smd_loopback_xprt;

static int rpcrouter_smd_loopback_read_avail(void)
{
	return smd_read_avail(smd_loopback_xprt.channel);
}

static int rpcrouter_smd_loopback_read(void *data, uint32_t len)
{
	return smd_read(smd_loopback_xprt.channel, data, len);
}

static int rpcrouter_smd_loopback_write_avail(void)
{
	return smd_write_avail(smd_loopback_xprt.channel);
}

static int rpcrouter_smd_loopback_write(void *data, uint32_t len, uint32 type)
{
	return smd_write(smd_loopback_xprt.channel, data, len);
}

static int rpcrouter_smd_loopback_close(void)
{
	return smd_close(smd_loopback_xprt.channel);
}

static void rpcrouter_smd_loopback_notify(void *_dev, unsigned event)
{
	switch (event) {
	case SMD_EVENT_DATA:
		msm_rpcrouter_xprt_notify(&smd_loopback_xprt.xprt,
					  RPCROUTER_XPRT_EVENT_DATA);
		break;

	case SMD_EVENT_OPEN:
		pr_debug("%s: smd loopback opened 0x%p\n", __func__, _dev);

		msm_rpcrouter_xprt_notify(&smd_loopback_xprt.xprt,
			RPCROUTER_XPRT_EVENT_OPEN);
		break;

	case SMD_EVENT_CLOSE:
		pr_debug("%s: smd loopback closed 0x%p\n", __func__, _dev);

		msm_rpcrouter_xprt_notify(&smd_loopback_xprt.xprt,
				RPCROUTER_XPRT_EVENT_CLOSE);
		break;
    }
}

static int rpcrouter_smd_loopback_probe(struct platform_device *pdev)
{
	int rc;

	smd_loopback_xprt.xprt.name = "rpcrouter_loopback_xprt";
	smd_loopback_xprt.xprt.read_avail = rpcrouter_smd_loopback_read_avail;
	smd_loopback_xprt.xprt.read = rpcrouter_smd_loopback_read;
	smd_loopback_xprt.xprt.write_avail = rpcrouter_smd_loopback_write_avail;
	smd_loopback_xprt.xprt.write = rpcrouter_smd_loopback_write;
	smd_loopback_xprt.xprt.close = rpcrouter_smd_loopback_close;
	smd_loopback_xprt.xprt.priv = NULL;

	/* Open up SMD LOOPBACK channel */
	rc = smd_named_open_on_edge("local_loopback", SMD_LOOPBACK_TYPE,
				    &smd_loopback_xprt.channel, NULL,
				    rpcrouter_smd_loopback_notify);
	if (rc < 0)
		return rc;

	smd_disable_read_intr(smd_remote_xprt.channel);
	return 0;
}

static struct platform_driver rpcrouter_smd_loopback_driver = {
	.probe		= rpcrouter_smd_loopback_probe,
	.driver		= {
			.name	= "local_loopback",
			.owner	= THIS_MODULE,
	},
};

static inline int register_smd_loopback_driver(void)
{
	return platform_driver_register(&rpcrouter_smd_loopback_driver);
}
#else /* CONFIG_MSM_RPC_LOOPBACK_XPRT */
static inline int register_smd_loopback_driver(void)
{
	return 0;
}
#endif

static int rpcrouter_smd_remote_probe(struct platform_device *pdev)
{
	int rc;

	smd_remote_xprt.xprt.name = "rpcrotuer_smd_xprt";
	smd_remote_xprt.xprt.read_avail = rpcrouter_smd_remote_read_avail;
	smd_remote_xprt.xprt.read = rpcrouter_smd_remote_read;
	smd_remote_xprt.xprt.write_avail = rpcrouter_smd_remote_write_avail;
	smd_remote_xprt.xprt.write = rpcrouter_smd_remote_write;
	smd_remote_xprt.xprt.close = rpcrouter_smd_remote_close;
	smd_remote_xprt.xprt.priv = NULL;

	/* Open up SMD channel */
	rc = smd_open("RPCCALL", &smd_remote_xprt.channel, NULL,
		      rpcrouter_smd_remote_notify);
	if (rc < 0)
		return rc;

	smd_disable_read_intr(smd_remote_xprt.channel);

	return 0;
}

static struct platform_driver rpcrouter_smd_remote_driver = {
	.probe		= rpcrouter_smd_remote_probe,
	.driver		= {
			.name	= "RPCCALL",
			.owner	= THIS_MODULE,
	},
};

static int __init rpcrouter_smd_init(void)
{
	int rc;

	rc = register_smd_loopback_driver();
	if (rc < 0)
		return rc;

	rc = register_smd_remote_qpsp_driver();
	if (rc < 0)
		return rc;

	return platform_driver_register(&rpcrouter_smd_remote_driver);
}

module_init(rpcrouter_smd_init);
MODULE_DESCRIPTION("RPC Router SMD XPRT");
MODULE_LICENSE("GPL v2");
