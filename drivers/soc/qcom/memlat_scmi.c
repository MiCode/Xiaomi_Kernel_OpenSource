// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 - 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/scmi_protocol.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/platform_device.h>

extern void rimps_memlat_init(struct scmi_handle *handle);

static int scmi_memlat_probe(struct scmi_device *sdev)
{
	struct scmi_handle *handle = sdev->handle;

	if (!handle || !handle->memlat_ops)
		return -ENODEV;

	rimps_memlat_init(handle);
	return 0;
}

static const struct scmi_device_id scmi_id_table[] = {
	{ SCMI_PROTOCOL_MEMLAT },
	{ },
};
MODULE_DEVICE_TABLE(scmi, scmi_id_table);

static struct scmi_driver scmi_memlat_drv = {
	.name		= "scmi-memlat-driver",
	.probe		= scmi_memlat_probe,
	.id_table	= scmi_id_table,
};
module_scmi_driver(scmi_memlat_drv);

MODULE_DESCRIPTION("ARM SCMI Memlat driver");
MODULE_LICENSE("GPL v2");
