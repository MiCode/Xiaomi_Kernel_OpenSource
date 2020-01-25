/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DFC_DEFS_H
#define _DFC_DEFS_H

#include <linux/soc/qcom/qmi.h>
#include "qmi_rmnet_i.h"

#define DFC_ACK_TYPE_DISABLE 1
#define DFC_ACK_TYPE_THRESHOLD 2

#define DFC_MASK_TCP_BIDIR 0x1
#define DFC_MASK_RAT_SWITCH 0x2
#define DFC_IS_TCP_BIDIR(r) (bool)((r) & DFC_MASK_TCP_BIDIR)
#define DFC_IS_RAT_SWITCH(r) (bool)((r) & DFC_MASK_RAT_SWITCH)

#define DFC_MAX_QOS_ID_V01 2

struct dfc_qmi_data {
	void *rmnet_port;
	struct workqueue_struct *dfc_wq;
	struct work_struct svc_arrive;
	struct qmi_handle handle;
	struct sockaddr_qrtr ssctl;
	struct svc_info svc;
	struct work_struct qmi_ind_work;
	struct list_head qmi_ind_q;
	spinlock_t qmi_ind_lock;
	int index;
	int restart_state;
};

enum dfc_ip_type_enum_v01 {
	DFC_IP_TYPE_ENUM_MIN_ENUM_VAL_V01 = -2147483647,
	DFC_IPV4_TYPE_V01 = 0x4,
	DFC_IPV6_TYPE_V01 = 0x6,
	DFC_IP_TYPE_ENUM_MAX_ENUM_VAL_V01 = 2147483647
};

struct dfc_qos_id_type_v01 {
	u32 qos_id;
	enum dfc_ip_type_enum_v01 ip_type;
};

struct dfc_flow_status_info_type_v01 {
	u8 subs_id;
	u8 mux_id;
	u8 bearer_id;
	u32 num_bytes;
	u16 seq_num;
	u8 qos_ids_len;
	struct dfc_qos_id_type_v01 qos_ids[DFC_MAX_QOS_ID_V01];
	u8 rx_bytes_valid;
	u32 rx_bytes;
};

struct dfc_ancillary_info_type_v01 {
	u8 subs_id;
	u8 mux_id;
	u8 bearer_id;
	u32 reserved;
};

struct dfc_flow_status_ind_msg_v01 {
	u8 flow_status_valid;
	u8 flow_status_len;
	struct dfc_flow_status_info_type_v01 flow_status[DFC_MAX_BEARERS_V01];
	u8 eod_ack_reqd_valid;
	u8 eod_ack_reqd;
	u8 ancillary_info_valid;
	u8 ancillary_info_len;
	struct dfc_ancillary_info_type_v01 ancillary_info[DFC_MAX_BEARERS_V01];
};

struct dfc_bearer_info_type_v01 {
	u8 subs_id;
	u8 mux_id;
	u8 bearer_id;
	enum dfc_ip_type_enum_v01 ip_type;
};

struct dfc_tx_link_status_ind_msg_v01 {
	u8 tx_status;
	u8 bearer_info_valid;
	u8 bearer_info_len;
	struct dfc_bearer_info_type_v01 bearer_info[DFC_MAX_BEARERS_V01];
};

void dfc_do_burst_flow_control(struct dfc_qmi_data *dfc,
			       struct dfc_flow_status_ind_msg_v01 *ind,
			       bool is_query);

void dfc_handle_tx_link_status_ind(struct dfc_qmi_data *dfc,
				   struct dfc_tx_link_status_ind_msg_v01 *ind);

#endif /* _DFC_DEFS_H */
