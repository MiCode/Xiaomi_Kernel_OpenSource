// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/scmi_protocol.h>
#include <linux/scmi_plh.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/platform_device.h>

extern int cpucp_plh_init(struct scmi_device *sdev);

static int scmi_plh_probe(struct scmi_device *sdev)
{
	if (!sdev)
		return -ENODEV;

	return cpucp_plh_init(sdev);
}

static const struct scmi_device_id scmi_id_table[] = {
	{ .protocol_id = SCMI_PROTOCOL_PLH, .name = "scmi_protocol_plh" },
	{ },
};
MODULE_DEVICE_TABLE(scmi, scmi_id_table);

static struct scmi_driver scmi_plh_drv = {
	.name		= "scmi-plh-driver",
	.probe		= scmi_plh_probe,
	.id_table	= scmi_id_table,
};
module_scmi_driver(scmi_plh_drv);

MODULE_SOFTDEP("pre: plh_vendor");
MODULE_DESCRIPTION("ARM SCMI PLH driver");
MODULE_LICENSE("GPL v2");
