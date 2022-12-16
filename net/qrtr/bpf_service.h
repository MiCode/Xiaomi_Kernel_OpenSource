/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2021 The Linux Foundation. All rights reserved. */

#ifndef __BPF_SERVICE_H_
#define __BPF_SERVICE_H_

#include <linux/types.h>
#include <linux/version.h>
#include <linux/qrtr.h>

/* structure to use with service add/lookup/remove */
struct service_info {
	u32 service_id;
	u32 instance_id;
};

void qrtr_service_add(struct qrtr_ctrl_pkt *pkt);
void qrtr_service_remove(struct qrtr_ctrl_pkt *pkt);
void qrtr_service_node_remove(u32 src_node);
int qrtr_service_lookup(u32 node, u32 port, struct service_info **info);
#endif /* __BPF_SERVICE_H_ */
