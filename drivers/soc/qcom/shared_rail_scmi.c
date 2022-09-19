// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/scmi_shared_rail.h>
#include <linux/scmi_protocol.h>

extern int cpucp_scmi_shared_rail_boost_init(struct scmi_device *sdev);

static int scmi_shared_rail_probe(struct scmi_device *sdev)
{
	if (!sdev || !sdev->handle)
		return -ENODEV;

	return cpucp_scmi_shared_rail_boost_init(sdev);
}

static const struct scmi_device_id scmi_id_table[] = {
	{ .protocol_id = SCMI_PROTOCOL_SHARED_RAIL, .name = "scmi_protocol_shared_rail" },
	{ },
};
MODULE_DEVICE_TABLE(scmi, scmi_id_table);

static struct scmi_driver scmi_shared_rail_drv = {
	.name		= "scmi-shared-rail-driver",
	.probe		= scmi_shared_rail_probe,
	.id_table	= scmi_id_table,
};
module_scmi_driver(scmi_shared_rail_drv);

MODULE_SOFTDEP("pre: shared_rail_vendor");
MODULE_DESCRIPTION("ARM SCMI Shared Rail Driver");
MODULE_LICENSE("GPL v2");

