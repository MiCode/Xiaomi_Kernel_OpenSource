/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include "protocol.h"


#define SCMI_TINYSYS_CB_MAX		20 /* must large/equal than tinysys side define */


struct scmi_tinysys_info_st {
	struct scmi_device *sdev;
	struct scmi_protocol_handle *ph;
};

typedef struct scmi_tinysys_report_st {
	u32 feature_id;
	u32 p1; u32 p2;
	u32 p3; u32 p4;
}scmi_tinysys_report;

struct scmi_tinysys_info_st *get_scmi_tinysys_info(void);

int scmi_tinysys_common_set(const struct scmi_protocol_handle *ph, u32 feature_id,
	u32 p1, u32 p2, u32 p3, u32 p4, u32 p5);

int scmi_tinysys_common_get(const struct scmi_protocol_handle *ph, u32 feature_id,
	u32 p1, struct scmi_tinysys_status *rvalue);

int scmi_tinysys_event_notify(u32 feature_id, u32 notify_enable);

typedef void (*f_handler_t)(u32 feature_id, scmi_tinysys_report* report);

void scmi_tinysys_register_event_notifier(u32 feature_id, f_handler_t hand);
