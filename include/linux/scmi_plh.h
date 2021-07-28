/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * SCMI Vendor Protocols header
 *
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _SCMI_PLH_VENDOR_H
#define _SCMI_PLH_VENDOR_H

#include <linux/bitfield.h>
#include <linux/device.h>
#include <linux/types.h>


#define SCMI_PROTOCOL_PLH      0x81

struct scmi_protocol_handle;

/**
 * struct scmi_plh_vendor_ops - represents the various operations provided
 *	by SCMI PLH Protocol
 *
 * @init_splh_ipc_freq_tbl: initialize scroll plh ipc freq voting table in rimps
 * @start_splh: starts scroll plh in rimps
 * @stop_splh: stops scroll plh in rimps
 * @set_splh_sample_ms: configure the sampling duration of scroll plh in rimps
 * @set_splh_log_level: configure the supported log_level of scroll plh in rimps
 */
struct scmi_plh_vendor_ops {
	int (*init_splh_ipc_freq_tbl)(const struct scmi_protocol_handle *ph,
				u16 *p_init_args, u16 init_len);
	int (*start_splh)(const struct scmi_protocol_handle *ph, u16 fps);
	int (*stop_splh)(const struct scmi_protocol_handle *ph);
	int (*set_splh_sample_ms)(const struct scmi_protocol_handle *ph,
				u16 sample_ms);
	int (*set_splh_log_level)(const struct scmi_protocol_handle *ph,
				u16 log_level);
};

#endif
