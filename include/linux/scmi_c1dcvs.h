/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * scmi c1dcvs protocols header
 *
 * copyright (c) 2021, the linux foundation. all rights reserved.
 */

#ifndef _scmi_c1dcvs_h
#define _scmi_c1dcvs_h

#include <linux/bitfield.h>
#include <linux/device.h>
#include <linux/types.h>


#define SCMI_C1DCVS_PROTOCOL    0x87

struct scmi_protocol_handle;

/**
 * struct scmi_c1dcvs_vendor_ops - represents the various operations provided
 *      by scmi c1dcvs protocol
 *
 * @scmi_enable_c1dcvs: enables/disables c1dcvs algorithm on cpucp.
 */
struct scmi_c1dcvs_vendor_ops {
	int (*set_enable_c1dcvs)(const struct scmi_protocol_handle *ph, void *buf);
	int (*get_enable_c1dcvs)(const struct scmi_protocol_handle *ph, void *buf);
	int (*set_enable_trace)(const struct scmi_protocol_handle *ph, void *buf);
	int (*get_enable_trace)(const struct scmi_protocol_handle *ph, void *buf);
	int (*set_ipc_thresh)(const struct scmi_protocol_handle *ph, void *buf);
	int (*get_ipc_thresh)(const struct scmi_protocol_handle *ph, void *buf);
	int (*set_efreq_thresh)(const struct scmi_protocol_handle *ph, void *buf);
	int (*get_efreq_thresh)(const struct scmi_protocol_handle *ph, void *buf);
	int (*set_hysteresis)(const struct scmi_protocol_handle *ph, void *buf);
	int (*get_hysteresis)(const struct scmi_protocol_handle *ph, void *buf);
};

#if IS_ENABLED(CONFIG_QTI_C1DCVS_SCMI_CLIENT)
int c1dcvs_enable(bool enable);
#else
static inline int c1dcvs_enable(bool enable)
{
	return -ENODEV;
}
#endif

#endif /* _scmi_c1dcvs_h */

