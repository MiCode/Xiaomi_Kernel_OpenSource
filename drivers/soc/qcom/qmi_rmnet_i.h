/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef _RMNET_QMI_I_H
#define _RMNET_QMI_I_H

#include <linux/netdevice.h>
#include <linux/skbuff.h>

#define IP_VER_4 4
#define IP_VER_6 6

#define MAX_CLIENT_NUM 2

struct rmnet_flow_map {
	struct list_head list;
	u8 bearer_id;
	u32 flow_id;
	int ip_type;
	u32 tcm_handle;
};

struct rmnet_bearer_map {
	struct list_head list;
	u8 bearer_id;
	int flow_ref;
	u32 grant_size;
	u16 seq;
	u8  ack_req;
};

struct svc_info {
	u32 instance;
	u32 ep_type;
	u32 iface_id;
};

struct fc_info {
	struct svc_info svc;
	void *dfc_client;
};

struct qos_info {
	u8 mux_id;
	struct net_device *real_dev;
	struct list_head flow_head;
	struct list_head bearer_head;
	u32 default_grant;
	u32 tran_num;
};

struct qmi_info {
	int client_count;
	int flag;
	struct fc_info fc_info[MAX_CLIENT_NUM];
};

enum data_ep_type_enum_v01 {
	DATA_EP_TYPE_ENUM_MIN_ENUM_VAL_V01 = INT_MIN,
	DATA_EP_TYPE_RESERVED_V01 = 0x00,
	DATA_EP_TYPE_HSIC_V01 = 0x01,
	DATA_EP_TYPE_HSUSB_V01 = 0x02,
	DATA_EP_TYPE_PCIE_V01 = 0x03,
	DATA_EP_TYPE_EMBEDDED_V01 = 0x04,
	DATA_EP_TYPE_ENUM_MAX_ENUM_VAL_V01 = INT_MAX
};

struct data_ep_id_type_v01 {

	enum data_ep_type_enum_v01 ep_type;
	u32 iface_id;
};

extern struct qmi_elem_info data_ep_id_type_v01_ei[];

struct rmnet_flow_map *
qmi_rmnet_get_flow_map(struct qos_info *qos_info,
		       u32 flow_id, int ip_type);

struct rmnet_bearer_map *
qmi_rmnet_get_bearer_map(struct qos_info *qos_info, u8 bearer_id);

int dfc_qmi_client_init(void *port, int index, struct qmi_info *qmi);

void dfc_qmi_client_exit(void *dfc_data);

void dfc_reset_port_pt(void *dfc_data);

void dfc_qmi_burst_check(struct net_device *dev,
			 struct qos_info *qos, struct sk_buff *skb);

int dfc_reg_unreg_fc_ind(void *dfc_data, int reg);
#endif /*_RMNET_QMI_I_H*/
