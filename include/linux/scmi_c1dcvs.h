/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * scmi c1dcvs protocols header
 *
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _SCMI_C1DCVS_H
#define _SCMI_C1DCVS_H

#include <linux/bitfield.h>
#include <linux/device.h>
#include <linux/types.h>


#define SCMI_C1DCVS_PROTOCOL    0x87

struct scmi_protocol_handle;

/**
 * struct scmi_c1dcvs_vendor_ops - represents the various operations provided
 *      by scmi c1dcvs protocol
 *
 * @set_enable_c1dcvs: enable/disable c1dcvs
 * @get_enable_c1dcvs: retrieve c1dcvs enable/disable status
 * @set_enable_trace: enable/disable tracing
 * @get_enable_trace: retrieve tracing enable/disable status
 * @set_ipc_thresh: set IPC thresholds
 * @get_ipc_thresh: retrieve IPC thresholds
 * @set_efreq_thresh: set effective frequency threshold
 * @get_efreq_thresh: retrieve effective frequency threshold
 * @set_hysteresis: set hysteresis settings
 * @get_hysteresis: get hysteresis settings
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

#endif /* _SCMI_C1DCVS_H */
