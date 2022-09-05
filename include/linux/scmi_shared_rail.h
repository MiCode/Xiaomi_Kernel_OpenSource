/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * SCMI Vendor Protocols header
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _SCMI_SHARED_RAIL_H
#define _SCMI_SHARED_RAIL_H

#include <linux/bitfield.h>
#include <linux/device.h>
#include <linux/types.h>

#define SCMI_PROTOCOL_SHARED_RAIL	(0x88)

enum srb_features {
	L3_BOOST,
	SILVER_CORE_BOOST,
};

struct scmi_protocol_handle;

struct scmi_shared_rail_vendor_ops {
	int (*set_shared_rail_boost)(const struct scmi_protocol_handle *ph,
				u16 level, enum srb_features feature);
};

#endif
