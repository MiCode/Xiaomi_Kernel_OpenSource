// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/scmi_protocol.h>
#include <linux/scmi_gplaf.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/platform_device.h>

extern int cpucp_gplaf_init(struct scmi_device *sdev);

static int scmi_gplaf_probe(struct scmi_device *sdev)
{
	if (!sdev)
		return -ENODEV;

	return cpucp_gplaf_init(sdev);
}

static const struct scmi_device_id scmi_id_table[] = {
	{ .protocol_id = SCMI_PROTOCOL_GPLAF, .name = "scmi_protocol_gplaf" },
	{ },
};
MODULE_DEVICE_TABLE(scmi, scmi_id_table);

static struct scmi_driver scmi_gplaf_drv = {
	.name		= "scmi-gplaf-driver",
	.probe		= scmi_gplaf_probe,
	.id_table	= scmi_id_table,
};
module_scmi_driver(scmi_gplaf_drv);

MODULE_SOFTDEP("pre: gplaf_vendor");
MODULE_DESCRIPTION("ARM SCMI GPLAF driver");
MODULE_LICENSE("GPL v2");
