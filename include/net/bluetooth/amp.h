/*
   Copyright (c) 2010-2012 The Linux Foundation.  All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 and
   only version 2 as published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#ifndef __AMP_H
#define __AMP_H

/* AMP defaults */

#define A2MP_RSP_TIMEOUT        (8000)  /*  8 seconds */

/* A2MP Protocol */

/* A2MP command codes */
#define A2MP_COMMAND_REJ         0x01
#define A2MP_DISCOVER_REQ        0x02
#define A2MP_DISCOVER_RSP        0x03
#define A2MP_CHANGE_NOTIFY       0x04
#define A2MP_CHANGE_RSP          0x05
#define A2MP_GETINFO_REQ         0x06
#define A2MP_GETINFO_RSP         0x07
#define A2MP_GETAMPASSOC_REQ     0x08
#define A2MP_GETAMPASSOC_RSP     0x09
#define A2MP_CREATEPHYSLINK_REQ  0x0A
#define A2MP_CREATEPHYSLINK_RSP  0x0B
#define A2MP_DISCONNPHYSLINK_REQ 0x0C
#define A2MP_DISCONNPHYSLINK_RSP 0x0D

struct a2mp_cmd_hdr {
	__u8       code;
	__u8       ident;
	__le16     len;
} __packed;

struct a2mp_cmd_rej {
	__le16     reason;
} __packed;

struct a2mp_discover_req {
	__le16     mtu;
	__le16     ext_feat;
} __packed;

struct a2mp_cl {
	__u8       id;
	__u8       type;
	__u8       status;
} __packed;

struct a2mp_discover_rsp {
	__le16     mtu;
	__le16     ext_feat;
	struct a2mp_cl cl[0];
} __packed;

struct a2mp_getinfo_req {
	__u8       id;
} __packed;

struct a2mp_getinfo_rsp {
	__u8       id;
	__u8       status;
	__le32     total_bw;
	__le32     max_bw;
	__le32     min_latency;
	__le16     pal_cap;
	__le16     assoc_size;
} __packed;

struct a2mp_getampassoc_req {
	__u8       id;
} __packed;

struct a2mp_getampassoc_rsp {
	__u8       id;
	__u8       status;
	__u8       amp_assoc[0];
} __packed;

struct a2mp_createphyslink_req {
	__u8       local_id;
	__u8       remote_id;
	__u8       amp_assoc[0];
} __packed;

struct a2mp_createphyslink_rsp {
	__u8       local_id;
	__u8       remote_id;
	__u8       status;
} __packed;

struct a2mp_disconnphyslink_req {
	__u8       local_id;
	__u8       remote_id;
} __packed;

struct a2mp_disconnphyslink_rsp {
	__u8       local_id;
	__u8       remote_id;
	__u8       status;
} __packed;


/* L2CAP-AMP module interface */
int amp_init(void);
void amp_exit(void);

/* L2CAP-AMP fixed channel interface */
void amp_conn_ind(struct hci_conn *hcon, struct sk_buff *skb);

/* L2CAP-AMP link interface */
void amp_create_physical(struct l2cap_conn *conn, struct sock *sk);
void amp_accept_physical(struct l2cap_conn *conn, u8 id, struct sock *sk);

/* AMP manager internals */
struct amp_ctrl {
	struct  amp_mgr *mgr;
	__u8    id;
	__u8    type;
	__u8    status;
	__u32   total_bw;
	__u32   max_bw;
	__u32   min_latency;
	__u16   pal_cap;
	__u16   max_assoc_size;
};

struct amp_mgr {
	struct list_head list;
	__u8    discovered;
	__u8    next_ident;
	struct l2cap_conn *l2cap_conn;
	struct socket *a2mp_sock;
	struct list_head  ctx_list;
	rwlock_t       ctx_list_lock;
	struct amp_ctrl *ctrls;          /* @@ TODO s.b. list of controllers */
	struct sk_buff *skb;
	__u8   connected;
};

/* AMP Manager signalling contexts */
#define AMP_GETAMPASSOC       1
#define AMP_CREATEPHYSLINK    2
#define AMP_ACCEPTPHYSLINK    3
#define AMP_CREATELOGLINK     4
#define AMP_ACCEPTLOGLINK     5

/* Get AMP Assoc sequence */
#define AMP_GAA_INIT           0
#define AMP_GAA_RLAA_COMPLETE  1
struct amp_gaa_state {
	__u8       req_ident;
	__u16      len_so_far;
	__u8      *assoc;
};

/* Create Physical Link sequence */
#define AMP_CPL_INIT           0
#define AMP_CPL_DISC_RSP       1
#define AMP_CPL_GETINFO_RSP    2
#define AMP_CPL_GAA_RSP        3
#define AMP_CPL_CPL_STATUS     4
#define AMP_CPL_WRA_COMPLETE   5
#define AMP_CPL_CHANNEL_SELECT 6
#define AMP_CPL_RLA_COMPLETE   7
#define AMP_CPL_PL_COMPLETE    8
#define AMP_CPL_PL_CANCEL      9
struct amp_cpl_state {
	__u8       remote_id;
	__u16      max_len;
	__u8      *remote_assoc;
	__u8      *local_assoc;
	__u16      len_so_far;
	__u16      rem_len;
	__u8       phy_handle;
};

/* Accept Physical Link sequence */
#define AMP_APL_INIT           0
#define AMP_APL_APL_STATUS     1
#define AMP_APL_WRA_COMPLETE   2
#define AMP_APL_PL_COMPLETE    3
struct amp_apl_state {
	__u8       remote_id;
	__u8       req_ident;
	__u8      *remote_assoc;
	__u16      len_so_far;
	__u16      rem_len;
	__u8       phy_handle;
};

/* Create/Accept Logical Link sequence */
#define AMP_LOG_INIT         0
#define AMP_LOG_LL_STATUS    1
#define AMP_LOG_LL_COMPLETE  2
struct amp_log_state {
	__u8       remote_id;
};

/* Possible event types a context may wait for */
#define AMP_INIT            0x01
#define AMP_HCI_EVENT       0x02
#define AMP_HCI_CMD_CMPLT   0x04
#define AMP_HCI_CMD_STATUS  0x08
#define AMP_A2MP_RSP        0x10
#define AMP_KILLED          0x20
#define AMP_CANCEL          0x40
struct amp_ctx {
	struct list_head list;
	struct amp_mgr *mgr;
	struct hci_dev *hdev;
	__u8       type;
	__u8       state;
	union {
		struct amp_gaa_state gaa;
		struct amp_cpl_state cpl;
		struct amp_apl_state apl;
	} d;
	__u8 evt_type;
	__u8 evt_code;
	__u16 opcode;
	__u8 id;
	__u8 rsp_ident;

	struct sock *sk;
	struct amp_ctx *deferred;
	struct timer_list timer;
};

/* AMP work */
struct amp_work_pl_timeout {
	struct work_struct work;
	struct amp_ctrl *ctrl;
};
struct amp_work_ctx_timeout {
	struct work_struct work;
	struct amp_ctx *ctx;
};
struct amp_work_data_ready {
	struct work_struct work;
	struct sock *sk;
	int bytes;
};
struct amp_work_state_change {
	struct work_struct work;
	struct sock *sk;
};
struct amp_work_conn_ind {
	struct work_struct work;
	struct hci_conn *hcon;
	struct sk_buff *skb;
};
struct amp_work_create_physical {
	struct work_struct work;
	struct l2cap_conn *conn;
	u8 id;
	struct sock *sk;
};
struct amp_work_accept_physical {
	struct work_struct work;
	struct l2cap_conn *conn;
	u8 id;
	struct sock *sk;
};
struct amp_work_cmd_cmplt {
	struct work_struct work;
	struct hci_dev *hdev;
	u16 opcode;
	struct sk_buff *skb;
};
struct amp_work_cmd_status {
	struct work_struct work;
	struct hci_dev *hdev;
	u16 opcode;
	u8 status;
};
struct amp_work_event {
	struct work_struct work;
	struct hci_dev *hdev;
	u8 event;
	struct sk_buff *skb;
};

#endif /* __AMP_H */
