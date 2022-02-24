/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * SCMI Vendor Protocols header
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _SCMI_GPLAF_VENDOR_H
#define _SCMI_GPLAF_VENDOR_H

#include <linux/bitfield.h>
#include <linux/device.h>
#include <linux/types.h>


#define SCMI_PROTOCOL_GPLAF      0x85

struct scmi_protocol_handle;

/**
 * struct scmi_gplaf_vendor_ops - represents the various operations provided
 *	by SCMI GPLAF Protocol
 *
 * @start_gplaf: starts hw gplaf in cpucp
 * @stop_gplaf: stops hw gplaf in cpucp
 * @set_gplaf_log_level: configure the supported log_level in gplaf module of cpucp
 * @send_frame_retire_event: pass frame retire info to cpucp
 * @send_gfx_data_notify: notify cpucp once gfx data is passed
 */
struct scmi_gplaf_vendor_ops {
	int (*start_gplaf)(const struct scmi_protocol_handle *handle, u16 pid);
	int (*stop_gplaf)(const struct scmi_protocol_handle *handle);
	int (*set_gplaf_log_level)(const struct scmi_protocol_handle *handle,
				u16 log_level);
	int (*send_frame_retire_event)(const struct scmi_protocol_handle *handle);
	int (*send_gfx_data_notify)(const struct scmi_protocol_handle *handle);
	int (*pass_gplaf_data)(const struct scmi_protocol_handle *handle, u16 data);
	int (*update_gplaf_health)(const struct scmi_protocol_handle *handle, u16 data);
};

#endif
