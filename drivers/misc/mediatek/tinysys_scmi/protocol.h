// SPDX-License-Identifier: GPL-2.0
/*
 * System Control and Management Interface (SCMI) MTK Tinysys Protocol
 *
 * Copyright (C) 2021 Mediatek Inc.
 */

#ifndef _MTK_TINYSYS_PROTOCOL_H
#define _MTK_TINYSYS_PROTOCOL_H

#include <linux/bitfield.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/types.h>


struct scmi_tinysys_status {
	u32 r1;
	u32 r2;
	u32 r3;
};

/**
 * struct scmi_tinysys_ops - represents the various operations provided
 *	by MTK Tinysys Protocol
 */
struct scmi_tinysys_proto_ops {
	int (*common_set)(const struct scmi_protocol_handle *ph,\
		u32 p0, u32 p1, u32 p2, u32 p3, u32 p4, u32 p5);
	int (*common_get)(const struct scmi_protocol_handle *ph, \
		u32 p0, u32 p1, struct scmi_tinysys_status *rvalue);
};

enum scmi_mtk_protocol {
	SCMI_PROTOCOL_TINYSYS = 0x80,
};

enum scmi_tinysys_notification_events {
	SCMI_EVENT_TINYSYS_NOTIFIER = 0x0,
};

struct scmi_tinysys_notifier_report {
	ktime_t		timestamp;
	unsigned int	f_id;
	unsigned int	p1_status;
	unsigned int	p2_status;
	unsigned int	p3_status;
	unsigned int	p4_status;
};

int scmi_tinysys_register(void);

#endif /* _MTK_TINYSYS_PROTOCOL_H */
