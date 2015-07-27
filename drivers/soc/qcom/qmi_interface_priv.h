/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#ifndef _QMI_INTERFACE_PRIV_H_
#define _QMI_INTERFACE_PRIV_H_

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/socket.h>
#include <linux/gfp.h>
#include <linux/platform_device.h>
#include <linux/qmi_encdec.h>

#include <soc/qcom/msm_qmi_interface.h>

enum txn_type {
	QMI_SYNC_TXN = 1,
	QMI_ASYNC_TXN,
};

/**
 * handle_type - Enum to identify QMI handle type
 */
enum handle_type {
	QMI_CLIENT_HANDLE = 1,
	QMI_SERVICE_HANDLE,
};

struct qmi_txn {
	struct list_head list;
	uint16_t txn_id;
	enum txn_type type;
	struct qmi_handle *handle;
	void *enc_data;
	unsigned int enc_data_len;
	struct msg_desc *resp_desc;
	void *resp;
	unsigned int resp_len;
	int resp_received;
	int send_stat;
	void (*resp_cb)(struct qmi_handle *handle, unsigned int msg_id,
			void *msg, void *resp_cb_data, int stat);
	void *resp_cb_data;
	wait_queue_head_t wait_q;
};

/**
 * svc_addr - Data structure to maintain a list of service addresses.
 * @list_node: Service address list node used by "svc_addr_list"
 * @port_addr: Service address in <node_id:port_id>.
 */
struct svc_addr {
	struct list_head list_node;
	struct msm_ipc_port_addr port_addr;
};

/**
 * svc_event_nb - Service event notification structure.
 * @nb_lock: Spinlock for the notifier block lists.
 * @service_id: Service id for which list of notifier blocks are maintained.
 * @instance_id: Instance id for which list of notifier blocks are maintained.
 * @svc_event_rcvr_list: List of notifier blocks which clients have registered.
 * @list: Used to chain this structure in a global list.
 * @svc_addr_list_lock: Lock to protect @svc_addr_list.
 * @svc_addr_list: List for mantaining all the address for a specific
 *			<service_id:instance_id>.
 */
struct svc_event_nb {
	spinlock_t nb_lock;
	uint32_t service_id;
	uint32_t instance_id;
	struct raw_notifier_head svc_event_rcvr_list;
	struct list_head list;
	struct mutex svc_addr_list_lock;
	struct list_head svc_addr_list;
};

/**
 * req_handle - Data structure to store request information
 * @list: Points to req_handle_list maintained per connection.
 * @conn_h: Connection handle on which the concerned request is received.
 * @msg_id: Message ID of the request.
 * @txn_id: Transaction ID of the request.
 */
struct req_handle {
	struct list_head list;
	struct qmi_svc_clnt_conn *conn_h;
	uint16_t msg_id;
	uint16_t txn_id;
};

/**
 * qmi_svc_clnt_conn - Data structure to identify client service connection
 * @list: List to chain up the client conncection to the connection list.
 * @svc_handle: Service side information of the connection.
 * @clnt_addr: Client side information of the connection.
 * @clnt_addr_len: Length of the client address.
 * @req_handle_list: Pending requests in this connection.
 * @pending_tx_list: Pending response/indications awaiting flow control.
 */
struct qmi_svc_clnt_conn {
	struct list_head list;
	void *svc_handle;
	void *clnt_addr;
	size_t clnt_addr_len;
	struct list_head req_handle_list;
	struct delayed_work resume_tx_work;
	struct list_head pending_txn_list;
	struct mutex pending_txn_lock;
};

#endif
