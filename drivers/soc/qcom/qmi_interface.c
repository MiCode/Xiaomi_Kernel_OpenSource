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

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/socket.h>
#include <linux/gfp.h>
#include <linux/qmi_encdec.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/hashtable.h>
#include <linux/ipc_router.h>
#include <linux/ipc_logging.h>

#include <soc/qcom/msm_qmi_interface.h>

#include "qmi_interface_priv.h"

#define BUILD_INSTANCE_ID(vers, ins) (((vers) & 0xFF) | (((ins) & 0xFF) << 8))
#define LOOKUP_MASK 0xFFFFFFFF
#define MAX_WQ_NAME_LEN 20
#define QMI_REQ_RESP_LOG_PAGES 3
#define QMI_IND_LOG_PAGES 2
#define QMI_REQ_RESP_LOG(buf...) \
do { \
	if (qmi_req_resp_log_ctx) { \
		ipc_log_string(qmi_req_resp_log_ctx, buf); \
	} \
} while (0) \

#define QMI_IND_LOG(buf...) \
do { \
	if (qmi_ind_log_ctx) { \
		ipc_log_string(qmi_ind_log_ctx, buf); \
	} \
} while (0) \

static LIST_HEAD(svc_event_nb_list);
static DEFINE_MUTEX(svc_event_nb_list_lock);

struct qmi_notify_event_work {
	unsigned event;
	void *oob_data;
	size_t oob_data_len;
	void *priv;
	struct work_struct work;
};
static void qmi_notify_event_worker(struct work_struct *work);

#define HANDLE_HASH_TBL_SZ 1
static DEFINE_HASHTABLE(handle_hash_tbl, HANDLE_HASH_TBL_SZ);
static DEFINE_MUTEX(handle_hash_tbl_lock);

struct elem_info qmi_response_type_v01_ei[] = {
	{
		.data_type	= QMI_SIGNED_2_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint16_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct qmi_response_type_v01,
					   result),
		.ei_array	= NULL,
	},
	{
		.data_type      = QMI_SIGNED_2_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(uint16_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
		.offset         = offsetof(struct qmi_response_type_v01,
					   error),
		.ei_array       = NULL,
	},
	{
		.data_type	= QMI_EOTI,
		.elem_len	= 0,
		.elem_size	= 0,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= 0,
		.ei_array	= NULL,
	},
};

struct elem_info qmi_error_resp_type_v01_ei[] = {
	{
		.data_type = QMI_STRUCT,
		.elem_len  = 1,
		.elem_size = sizeof(struct qmi_response_type_v01),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x02,
		.offset    = 0,
		.ei_array  = qmi_response_type_v01_ei,
	},
	{
		.data_type = QMI_EOTI,
		.elem_len  = 0,
		.elem_size = 0,
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x00,
		.offset    = 0,
		.ei_array  = NULL,
	},
};

struct msg_desc err_resp_desc = {
	.max_msg_len = 7,
	.msg_id = 0,
	.ei_array = qmi_error_resp_type_v01_ei,
};

static DEFINE_MUTEX(qmi_svc_event_notifier_lock);
static struct msm_ipc_port *qmi_svc_event_notifier_port;
static struct workqueue_struct *qmi_svc_event_notifier_wq;
static void qmi_svc_event_notifier_init(void);
static void qmi_svc_event_worker(struct work_struct *work);
static struct svc_event_nb *find_svc_event_nb(uint32_t service_id,
					      uint32_t instance_id);
DECLARE_WORK(qmi_svc_event_work, qmi_svc_event_worker);
static void svc_resume_tx_worker(struct work_struct *work);
static void clean_txn_info(struct qmi_handle *handle);
static void *qmi_req_resp_log_ctx;
static void *qmi_ind_log_ctx;

/**
 * qmi_log() - Pass log data to IPC logging framework
 * @handle:	The pointer to the qmi_handle
 * @cntl_flg:	Indicates the type(request/response/indications) of the message
 * @txn_id:	Transaction ID of the message.
 * @msg_id:	Message ID of the incoming/outgoing message.
 * @msg_len:	Total size of the message.
 *
 * This function builds the data the would be passed on to the IPC logging
 * framework. The data that would be passed corresponds to the information
 * that is exchanged between the IPC Router and kernel modules during
 * request/response/indication transactions.
 */

static void qmi_log(struct qmi_handle *handle,
			unsigned char cntl_flag, uint16_t txn_id,
			uint16_t msg_id, uint16_t msg_len)
{
	uint32_t service_id = 0;
	const char *ops_type = NULL;

	if (handle->handle_type == QMI_CLIENT_HANDLE) {
		service_id = handle->dest_service_id;
		if (cntl_flag == QMI_REQUEST_CONTROL_FLAG)
			ops_type = "TX";
		else if (cntl_flag == QMI_INDICATION_CONTROL_FLAG ||
			cntl_flag == QMI_RESPONSE_CONTROL_FLAG)
			ops_type = "RX";
	} else if (handle->handle_type == QMI_SERVICE_HANDLE) {
		service_id = handle->svc_ops_options->service_id;
		if (cntl_flag == QMI_REQUEST_CONTROL_FLAG)
			ops_type = "RX";
		else if (cntl_flag == QMI_INDICATION_CONTROL_FLAG ||
			cntl_flag == QMI_RESPONSE_CONTROL_FLAG)
			ops_type = "TX";
	}

	/*
	 * IPC Logging format is as below:-
	 * <Type of module>(CLNT or  SERV)	:
	 * <Opertaion Type> (Transmit/ RECV)	:
	 * <Control Flag> (Req/Resp/Ind)	:
	 * <Transaction ID>			:
	 * <Message ID>				:
	 * <Message Length>			:
	 * <Service ID>				:
	 */
	if (qmi_req_resp_log_ctx &&
		((cntl_flag == QMI_REQUEST_CONTROL_FLAG) ||
		(cntl_flag == QMI_RESPONSE_CONTROL_FLAG))) {
		QMI_REQ_RESP_LOG("%s %s CF:%x TI:%x MI:%x ML:%x SvcId: %x",
		(handle->handle_type == QMI_CLIENT_HANDLE ? "QCCI" : "QCSI"),
		ops_type, cntl_flag, txn_id, msg_id, msg_len, service_id);
	} else if (qmi_ind_log_ctx &&
		(cntl_flag == QMI_INDICATION_CONTROL_FLAG)) {
		QMI_IND_LOG("%s %s CF:%x TI:%x MI:%x ML:%x SvcId: %x",
		(handle->handle_type == QMI_CLIENT_HANDLE ? "QCCI" : "QCSI"),
		ops_type, cntl_flag, txn_id, msg_id, msg_len, service_id);
	}
}

/**
 * add_req_handle() - Create and Add a request handle to the connection
 * @conn_h: Connection handle over which the request has arrived.
 * @msg_id: Message ID of the request.
 * @txn_id: Transaction ID of the request.
 *
 * @return: Pointer to request handle on success, NULL on error.
 *
 * This function creates a request handle to track the request that arrives
 * on a connection. This function then adds it to the connection's request
 * handle list.
 */
static struct req_handle *add_req_handle(struct qmi_svc_clnt_conn *conn_h,
					 uint16_t msg_id, uint16_t txn_id)
{
	struct req_handle *req_h;

	req_h = kmalloc(sizeof(struct req_handle), GFP_KERNEL);
	if (!req_h) {
		pr_err("%s: Error allocating req_h\n", __func__);
		return NULL;
	}

	req_h->conn_h = conn_h;
	req_h->msg_id = msg_id;
	req_h->txn_id = txn_id;
	list_add_tail(&req_h->list, &conn_h->req_handle_list);
	return req_h;
}

/**
 * verify_req_handle() - Verify the validity of a request handle
 * @conn_h: Connection handle over which the request has arrived.
 * @req_h: Request handle to be verified.
 *
 * @return: true on success, false on failure.
 *
 * This function is used to check if the request handle is present in
 * the connection handle.
 */
static bool verify_req_handle(struct qmi_svc_clnt_conn *conn_h,
			      struct req_handle *req_h)
{
	struct req_handle *temp_req_h;

	list_for_each_entry(temp_req_h, &conn_h->req_handle_list, list) {
		if (temp_req_h == req_h)
			return true;
	}
	return false;
}

/**
 * rmv_req_handle() - Remove and destroy the request handle
 * @req_h: Request handle to be removed and destroyed.
 *
 * @return: 0.
 */
static int rmv_req_handle(struct req_handle *req_h)
{
	list_del(&req_h->list);
	kfree(req_h);
	return 0;
}

/**
 * add_svc_clnt_conn() - Create and add a connection handle to a service
 * @handle: QMI handle in which the service is hosted.
 * @clnt_addr: Address of the client connecting with the service.
 * @clnt_addr_len: Length of the client address.
 *
 * @return: Pointer to connection handle on success, NULL on error.
 *
 * This function is used to create a connection handle that binds the service
 * with a client. This function is called on a service's QMI handle when a
 * client sends its first message to the service.
 *
 * This function must be called with handle->handle_lock locked.
 */
static struct qmi_svc_clnt_conn *add_svc_clnt_conn(
	struct qmi_handle *handle, void *clnt_addr, size_t clnt_addr_len)
{
	struct qmi_svc_clnt_conn *conn_h;

	conn_h = kmalloc(sizeof(struct qmi_svc_clnt_conn), GFP_KERNEL);
	if (!conn_h) {
		pr_err("%s: Error allocating conn_h\n", __func__);
		return NULL;
	}

	conn_h->clnt_addr = kmalloc(clnt_addr_len, GFP_KERNEL);
	if (!conn_h->clnt_addr) {
		pr_err("%s: Error allocating clnt_addr\n", __func__);
		return NULL;
	}

	INIT_LIST_HEAD(&conn_h->list);
	conn_h->svc_handle = handle;
	memcpy(conn_h->clnt_addr, clnt_addr, clnt_addr_len);
	conn_h->clnt_addr_len = clnt_addr_len;
	INIT_LIST_HEAD(&conn_h->req_handle_list);
	INIT_DELAYED_WORK(&conn_h->resume_tx_work, svc_resume_tx_worker);
	INIT_LIST_HEAD(&conn_h->pending_txn_list);
	mutex_init(&conn_h->pending_txn_lock);
	list_add_tail(&conn_h->list, &handle->conn_list);
	return conn_h;
}

/**
 * find_svc_clnt_conn() - Find the existence of a client<->service connection
 * @handle: Service's QMI handle.
 * @clnt_addr: Address of the client to be present in the connection.
 * @clnt_addr_len: Length of the client address.
 *
 * @return: Pointer to connection handle if the matching connection is found,
 *          NULL if the connection is not found.
 *
 * This function is used to find the existence of a client<->service connection
 * handle in a service's QMI handle. This function tries to match the client
 * address in the existing connections.
 *
 * This function must be called with handle->handle_lock locked.
 */
static struct qmi_svc_clnt_conn *find_svc_clnt_conn(
	struct qmi_handle *handle, void *clnt_addr, size_t clnt_addr_len)
{
	struct qmi_svc_clnt_conn *conn_h;

	list_for_each_entry(conn_h, &handle->conn_list, list) {
		if (!memcmp(conn_h->clnt_addr, clnt_addr, clnt_addr_len))
			return conn_h;
	}
	return NULL;
}

/**
 * verify_svc_clnt_conn() - Verify the existence of a connection handle
 * @handle: Service's QMI handle.
 * @conn_h: Connection handle to be verified.
 *
 * @return: true on success, false on failure.
 *
 * This function is used to verify the existence of a connection in the
 * connection list maintained by the service.
 *
 * This function must be called with handle->handle_lock locked.
 */
static bool verify_svc_clnt_conn(struct qmi_handle *handle,
				 struct qmi_svc_clnt_conn *conn_h)
{
	struct qmi_svc_clnt_conn *temp_conn_h;

	list_for_each_entry(temp_conn_h, &handle->conn_list, list) {
		if (temp_conn_h == conn_h)
			return true;
	}
	return false;
}

/**
 * rmv_svc_clnt_conn() - Remove the connection handle info from the service
 * @conn_h: Connection handle to be removed.
 *
 * This function removes a connection handle from a service's QMI handle.
 *
 * This function must be called with handle->handle_lock locked.
 */
static void rmv_svc_clnt_conn(struct qmi_svc_clnt_conn *conn_h)
{
	struct req_handle *req_h, *temp_req_h;
	struct qmi_txn *txn_h, *temp_txn_h;

	list_del(&conn_h->list);
	list_for_each_entry_safe(req_h, temp_req_h,
				 &conn_h->req_handle_list, list)
		rmv_req_handle(req_h);

	mutex_lock(&conn_h->pending_txn_lock);
	list_for_each_entry_safe(txn_h, temp_txn_h,
				 &conn_h->pending_txn_list, list) {
		list_del(&txn_h->list);
		kfree(txn_h->enc_data);
		kfree(txn_h);
	}
	mutex_unlock(&conn_h->pending_txn_lock);
	flush_delayed_work(&conn_h->resume_tx_work);
	kfree(conn_h->clnt_addr);
	kfree(conn_h);
}

/**
 * qmi_event_notify() - Notification function to QMI client/service interface
 * @event: Type of event that gets notified.
 * @oob_data: Any out-of-band data associated with event.
 * @oob_data_len: Length of the out-of-band data, if any.
 * @priv: Private data.
 *
 * This function is called by the underlying transport to notify the QMI
 * interface regarding any incoming event. This function is registered by
 * QMI interface when it opens a port/handle with the underlying transport.
 */
static void qmi_event_notify(unsigned event, void *oob_data,
			     size_t oob_data_len, void *priv)
{
	struct qmi_notify_event_work *notify_work;
	struct qmi_handle *handle;
	uint32_t key = 0;

	notify_work = kmalloc(sizeof(struct qmi_notify_event_work),
			      GFP_KERNEL);
	if (!notify_work) {
		pr_err("%s: Couldn't notify %d event to %p\n",
			__func__, event, priv);
		return;
	}
	notify_work->event = event;
	if (oob_data) {
		notify_work->oob_data = kmalloc(oob_data_len, GFP_KERNEL);
		if (!notify_work->oob_data) {
			pr_err("%s: Couldn't allocate oob_data @ %d to %p\n",
				__func__, event, priv);
			kfree(notify_work);
			return;
		}
		memcpy(notify_work->oob_data, oob_data, oob_data_len);
	} else {
		notify_work->oob_data = NULL;
	}
	notify_work->oob_data_len = oob_data_len;
	notify_work->priv = priv;
	INIT_WORK(&notify_work->work, qmi_notify_event_worker);

	mutex_lock(&handle_hash_tbl_lock);
	hash_for_each_possible(handle_hash_tbl, handle, handle_hash, key) {
		if (handle == (struct qmi_handle *)priv) {
			queue_work(handle->handle_wq,
				   &notify_work->work);
			mutex_unlock(&handle_hash_tbl_lock);
			return;
		}
	}
	mutex_unlock(&handle_hash_tbl_lock);
	kfree(notify_work->oob_data);
	kfree(notify_work);
}

static void qmi_notify_event_worker(struct work_struct *work)
{
	struct qmi_notify_event_work *notify_work =
		container_of(work, struct qmi_notify_event_work, work);
	struct qmi_handle *handle = (struct qmi_handle *)notify_work->priv;
	unsigned long flags;

	if (!handle)
		return;

	mutex_lock(&handle->handle_lock);
	if (handle->handle_reset) {
		mutex_unlock(&handle->handle_lock);
		kfree(notify_work->oob_data);
		kfree(notify_work);
		return;
	}

	switch (notify_work->event) {
	case IPC_ROUTER_CTRL_CMD_DATA:
		spin_lock_irqsave(&handle->notify_lock, flags);
		handle->notify(handle, QMI_RECV_MSG, handle->notify_priv);
		spin_unlock_irqrestore(&handle->notify_lock, flags);
		break;

	case IPC_ROUTER_CTRL_CMD_RESUME_TX:
		if (handle->handle_type == QMI_CLIENT_HANDLE) {
			queue_delayed_work(handle->handle_wq,
					   &handle->resume_tx_work,
					   msecs_to_jiffies(0));
		} else if (handle->handle_type == QMI_SERVICE_HANDLE) {
			struct msm_ipc_addr rtx_addr = {0};
			struct qmi_svc_clnt_conn *conn_h;
			union rr_control_msg *msg;

			msg = (union rr_control_msg *)notify_work->oob_data;
			rtx_addr.addrtype = MSM_IPC_ADDR_ID;
			rtx_addr.addr.port_addr.node_id = msg->cli.node_id;
			rtx_addr.addr.port_addr.port_id = msg->cli.port_id;
			conn_h = find_svc_clnt_conn(handle, &rtx_addr,
						    sizeof(rtx_addr));
			if (conn_h)
				queue_delayed_work(handle->handle_wq,
						   &conn_h->resume_tx_work,
						   msecs_to_jiffies(0));
		}
		break;

	case IPC_ROUTER_CTRL_CMD_NEW_SERVER:
	case IPC_ROUTER_CTRL_CMD_REMOVE_SERVER:
	case IPC_ROUTER_CTRL_CMD_REMOVE_CLIENT:
		queue_delayed_work(handle->handle_wq,
				   &handle->ctl_work, msecs_to_jiffies(0));
		break;
	default:
		break;
	}
	mutex_unlock(&handle->handle_lock);
	kfree(notify_work->oob_data);
	kfree(notify_work);
}

/**
 * clnt_resume_tx_worker() - Handle the Resume_Tx event
 * @work : Pointer to the work strcuture.
 *
 * This function handles the resume_tx event for any QMI client that
 * exists in the kernel space. This function parses the pending_txn_list of
 * the handle and attempts a send for each transaction in that list.
 */
static void clnt_resume_tx_worker(struct work_struct *work)
{
	struct delayed_work *rtx_work = to_delayed_work(work);
	struct qmi_handle *handle =
		container_of(rtx_work, struct qmi_handle, resume_tx_work);
	struct qmi_txn *pend_txn, *temp_txn;
	int ret;
	uint16_t msg_id;

	mutex_lock(&handle->handle_lock);
	if (handle->handle_reset)
		goto out_clnt_handle_rtx;

	list_for_each_entry_safe(pend_txn, temp_txn,
				&handle->pending_txn_list, list) {
		ret = msm_ipc_router_send_msg(
				(struct msm_ipc_port *)handle->src_port,
				(struct msm_ipc_addr *)handle->dest_info,
				pend_txn->enc_data, pend_txn->enc_data_len);

		if (ret == -EAGAIN)
			break;
		msg_id = ((struct qmi_header *)pend_txn->enc_data)->msg_id;
		kfree(pend_txn->enc_data);
		if (ret < 0) {
			pr_err("%s: Sending transaction %d from port %d failed",
				__func__, pend_txn->txn_id,
				((struct msm_ipc_port *)handle->src_port)->
							this_port.port_id);
			if (pend_txn->type == QMI_ASYNC_TXN) {
				pend_txn->resp_cb(pend_txn->handle,
						msg_id, pend_txn->resp,
						pend_txn->resp_cb_data,
						ret);
				list_del(&pend_txn->list);
				kfree(pend_txn);
			} else if (pend_txn->type == QMI_SYNC_TXN) {
				pend_txn->send_stat = ret;
				wake_up(&pend_txn->wait_q);
			}
		} else {
			list_del(&pend_txn->list);
			list_add_tail(&pend_txn->list, &handle->txn_list);
		}
	}
out_clnt_handle_rtx:
	mutex_unlock(&handle->handle_lock);
}

/**
 * svc_resume_tx_worker() - Handle the Resume_Tx event
 * @work : Pointer to the work strcuture.
 *
 * This function handles the resume_tx event for any QMI service that
 * exists in the kernel space. This function parses the pending_txn_list of
 * the connection handle and attempts a send for each transaction in that list.
 */
static void svc_resume_tx_worker(struct work_struct *work)
{
	struct delayed_work *rtx_work = to_delayed_work(work);
	struct qmi_svc_clnt_conn *conn_h =
		container_of(rtx_work, struct qmi_svc_clnt_conn,
			     resume_tx_work);
	struct qmi_handle *handle = (struct qmi_handle *)conn_h->svc_handle;
	struct qmi_txn *pend_txn, *temp_txn;
	int ret;

	mutex_lock(&conn_h->pending_txn_lock);
	if (handle->handle_reset)
		goto out_svc_handle_rtx;

	list_for_each_entry_safe(pend_txn, temp_txn,
				&conn_h->pending_txn_list, list) {
		ret = msm_ipc_router_send_msg(
				(struct msm_ipc_port *)handle->src_port,
				(struct msm_ipc_addr *)conn_h->clnt_addr,
				pend_txn->enc_data, pend_txn->enc_data_len);

		if (ret == -EAGAIN)
			break;
		if (ret < 0)
			pr_err("%s: Sending transaction %d from port %d failed",
				__func__, pend_txn->txn_id,
				((struct msm_ipc_port *)handle->src_port)->
							this_port.port_id);
		list_del(&pend_txn->list);
		kfree(pend_txn->enc_data);
		kfree(pend_txn);
	}
out_svc_handle_rtx:
	mutex_unlock(&conn_h->pending_txn_lock);
}

/**
 * handle_rmv_server() - Handle the server exit event
 * @handle: Client handle on which the server exit event is received.
 * @ctl_msg: Information about the server that is exiting.
 *
 * @return: 0 on success, standard Linux error codes on failure.
 *
 * This function must be called with handle->handle_lock locked.
 */
static int handle_rmv_server(struct qmi_handle *handle,
			     union rr_control_msg *ctl_msg)
{
	struct msm_ipc_addr *svc_addr;
	unsigned long flags;

	if (unlikely(!handle->dest_info))
		return 0;

	svc_addr = (struct msm_ipc_addr *)(handle->dest_info);
	if (svc_addr->addr.port_addr.node_id == ctl_msg->srv.node_id &&
	    svc_addr->addr.port_addr.port_id == ctl_msg->srv.port_id) {
		/* Wakeup any threads waiting for the response */
		handle->handle_reset = 1;
		clean_txn_info(handle);

		spin_lock_irqsave(&handle->notify_lock, flags);
		handle->notify(handle, QMI_SERVER_EXIT, handle->notify_priv);
		spin_unlock_irqrestore(&handle->notify_lock, flags);
	}
	return 0;
}

/**
 * handle_rmv_client() - Handle the client exit event
 * @handle: Service handle on which the client exit event is received.
 * @ctl_msg: Information about the client that is exiting.
 *
 * @return: 0 on success, standard Linux error codes on failure.
 *
 * This function must be called with handle->handle_lock locked.
 */
static int handle_rmv_client(struct qmi_handle *handle,
			     union rr_control_msg *ctl_msg)
{
	struct qmi_svc_clnt_conn *conn_h;
	struct msm_ipc_addr clnt_addr = {0};
	unsigned long flags;

	clnt_addr.addrtype = MSM_IPC_ADDR_ID;
	clnt_addr.addr.port_addr.node_id = ctl_msg->cli.node_id;
	clnt_addr.addr.port_addr.port_id = ctl_msg->cli.port_id;
	conn_h = find_svc_clnt_conn(handle, &clnt_addr, sizeof(clnt_addr));
	if (conn_h) {
		spin_lock_irqsave(&handle->notify_lock, flags);
		handle->svc_ops_options->disconnect_cb(handle, conn_h);
		spin_unlock_irqrestore(&handle->notify_lock, flags);
		rmv_svc_clnt_conn(conn_h);
	}
	return 0;
}

/**
 * handle_ctl_msg: Worker function to handle the control events
 * @work: Work item to map the QMI handle.
 *
 * This function is a worker function to handle the incoming control
 * events like REMOVE_SERVER/REMOVE_CLIENT. The work item is unique
 * to a handle and the workker function handles the control events on
 * a specific handle.
 */
static void handle_ctl_msg(struct work_struct *work)
{
	struct delayed_work *ctl_work = to_delayed_work(work);
	struct qmi_handle *handle =
		container_of(ctl_work, struct qmi_handle, ctl_work);
	unsigned int ctl_msg_len;
	union rr_control_msg *ctl_msg = NULL;
	struct msm_ipc_addr src_addr;
	int rc;

	mutex_lock(&handle->handle_lock);
	while (1) {
		if (handle->handle_reset)
			break;

		/* Read the messages */
		rc = msm_ipc_router_read_msg(
			(struct msm_ipc_port *)(handle->ctl_port),
			&src_addr, (unsigned char **)&ctl_msg, &ctl_msg_len);
		if (rc == -ENOMSG)
			break;
		if (rc < 0) {
			pr_err("%s: Read failed %d\n", __func__, rc);
			break;
		}
		if (ctl_msg->cmd == IPC_ROUTER_CTRL_CMD_REMOVE_SERVER &&
		    handle->handle_type == QMI_CLIENT_HANDLE)
			handle_rmv_server(handle, ctl_msg);
		else if (ctl_msg->cmd == IPC_ROUTER_CTRL_CMD_REMOVE_CLIENT &&
			 handle->handle_type == QMI_SERVICE_HANDLE)
			handle_rmv_client(handle, ctl_msg);
		kfree(ctl_msg);
	}
	mutex_unlock(&handle->handle_lock);
	return;
}

struct qmi_handle *qmi_handle_create(
	void (*notify)(struct qmi_handle *handle,
		       enum qmi_event_type event, void *notify_priv),
	void *notify_priv)
{
	struct qmi_handle *temp_handle;
	struct msm_ipc_port *port_ptr, *ctl_port_ptr;
	static uint32_t handle_count;
	char wq_name[MAX_WQ_NAME_LEN];

	temp_handle = kzalloc(sizeof(struct qmi_handle), GFP_KERNEL);
	if (!temp_handle) {
		pr_err("%s: Failure allocating client handle\n", __func__);
		return NULL;
	}
	mutex_lock(&handle_hash_tbl_lock);
	handle_count++;
	scnprintf(wq_name, MAX_WQ_NAME_LEN, "qmi_hndl%08x", handle_count);
	hash_add(handle_hash_tbl, &temp_handle->handle_hash, 0);
	temp_handle->handle_wq = create_singlethread_workqueue(wq_name);
	mutex_unlock(&handle_hash_tbl_lock);
	if (!temp_handle->handle_wq) {
		pr_err("%s: Couldn't create workqueue for handle\n", __func__);
		goto handle_create_err1;
	}

	/* Initialize common elements */
	temp_handle->handle_type = QMI_CLIENT_HANDLE;
	temp_handle->next_txn_id = 1;
	mutex_init(&temp_handle->handle_lock);
	spin_lock_init(&temp_handle->notify_lock);
	temp_handle->notify = notify;
	temp_handle->notify_priv = notify_priv;
	init_waitqueue_head(&temp_handle->reset_waitq);
	INIT_DELAYED_WORK(&temp_handle->resume_tx_work, clnt_resume_tx_worker);
	INIT_DELAYED_WORK(&temp_handle->ctl_work, handle_ctl_msg);

	/* Initialize client specific elements */
	INIT_LIST_HEAD(&temp_handle->txn_list);
	INIT_LIST_HEAD(&temp_handle->pending_txn_list);

	/* Initialize service specific elements */
	INIT_LIST_HEAD(&temp_handle->conn_list);

	port_ptr = msm_ipc_router_create_port(qmi_event_notify,
					      (void *)temp_handle);
	if (!port_ptr) {
		pr_err("%s: IPC router port creation failed\n", __func__);
		goto handle_create_err2;
	}

	ctl_port_ptr = msm_ipc_router_create_port(qmi_event_notify,
						  (void *)temp_handle);
	if (!ctl_port_ptr) {
		pr_err("%s: IPC router ctl port creation failed\n", __func__);
		goto handle_create_err3;
	}
	msm_ipc_router_bind_control_port(ctl_port_ptr);

	temp_handle->src_port = port_ptr;
	temp_handle->ctl_port = ctl_port_ptr;
	return temp_handle;

handle_create_err3:
	msm_ipc_router_close_port(port_ptr);
handle_create_err2:
	destroy_workqueue(temp_handle->handle_wq);
handle_create_err1:
	mutex_lock(&handle_hash_tbl_lock);
	hash_del(&temp_handle->handle_hash);
	mutex_unlock(&handle_hash_tbl_lock);
	kfree(temp_handle);
	return NULL;
}
EXPORT_SYMBOL(qmi_handle_create);

static void clean_txn_info(struct qmi_handle *handle)
{
	struct qmi_txn *txn_handle, *temp_txn_handle, *pend_txn;

	list_for_each_entry_safe(pend_txn, temp_txn_handle,
				&handle->pending_txn_list, list) {
		if (pend_txn->type == QMI_ASYNC_TXN) {
			list_del(&pend_txn->list);
			pend_txn->resp_cb(pend_txn->handle,
					((struct qmi_header *)
					pend_txn->enc_data)->msg_id,
					pend_txn->resp, pend_txn->resp_cb_data,
					-ENETRESET);
			kfree(pend_txn->enc_data);
			kfree(pend_txn);
		} else if (pend_txn->type == QMI_SYNC_TXN) {
			kfree(pend_txn->enc_data);
			wake_up(&pend_txn->wait_q);
		}
	}
	list_for_each_entry_safe(txn_handle, temp_txn_handle,
				 &handle->txn_list, list) {
		if (txn_handle->type == QMI_ASYNC_TXN) {
			list_del(&txn_handle->list);
			kfree(txn_handle);
		} else if (txn_handle->type == QMI_SYNC_TXN) {
			wake_up(&txn_handle->wait_q);
		}
	}
}

int qmi_handle_destroy(struct qmi_handle *handle)
{
	DEFINE_WAIT(wait);

	if (!handle)
		return -EINVAL;

	mutex_lock(&handle_hash_tbl_lock);
	hash_del(&handle->handle_hash);
	mutex_unlock(&handle_hash_tbl_lock);

	mutex_lock(&handle->handle_lock);
	handle->handle_reset = 1;
	clean_txn_info(handle);
	msm_ipc_router_close_port((struct msm_ipc_port *)(handle->ctl_port));
	msm_ipc_router_close_port((struct msm_ipc_port *)(handle->src_port));
	mutex_unlock(&handle->handle_lock);
	flush_workqueue(handle->handle_wq);
	destroy_workqueue(handle->handle_wq);

	mutex_lock(&handle->handle_lock);
	while (!list_empty(&handle->txn_list) ||
		    !list_empty(&handle->pending_txn_list)) {
		prepare_to_wait(&handle->reset_waitq, &wait,
				TASK_UNINTERRUPTIBLE);
		mutex_unlock(&handle->handle_lock);
		schedule();
		mutex_lock(&handle->handle_lock);
		finish_wait(&handle->reset_waitq, &wait);
	}
	mutex_unlock(&handle->handle_lock);
	kfree(handle->dest_info);
	kfree(handle);
	return 0;
}
EXPORT_SYMBOL(qmi_handle_destroy);

int qmi_register_ind_cb(struct qmi_handle *handle,
	void (*ind_cb)(struct qmi_handle *handle,
		       unsigned int msg_id, void *msg,
		       unsigned int msg_len, void *ind_cb_priv),
	void *ind_cb_priv)
{
	if (!handle)
		return -EINVAL;

	mutex_lock(&handle->handle_lock);
	if (handle->handle_reset) {
		mutex_unlock(&handle->handle_lock);
		return -ENETRESET;
	}

	handle->ind_cb = ind_cb;
	handle->ind_cb_priv = ind_cb_priv;
	mutex_unlock(&handle->handle_lock);
	return 0;
}
EXPORT_SYMBOL(qmi_register_ind_cb);

static int qmi_encode_and_send_req(struct qmi_txn **ret_txn_handle,
	struct qmi_handle *handle, enum txn_type type,
	struct msg_desc *req_desc, void *req, unsigned int req_len,
	struct msg_desc *resp_desc, void *resp, unsigned int resp_len,
	void (*resp_cb)(struct qmi_handle *handle,
			unsigned int msg_id, void *msg,
			void *resp_cb_data, int stat),
	void *resp_cb_data)
{
	struct qmi_txn *txn_handle;
	int rc, encoded_req_len;
	void *encoded_req;

	if (!handle || !handle->dest_info ||
	    !req_desc || !resp_desc || !resp)
		return -EINVAL;

	if ((!req && req_len) || (!req_len && req))
		return -EINVAL;

	mutex_lock(&handle->handle_lock);
	if (handle->handle_reset) {
		mutex_unlock(&handle->handle_lock);
		return -ENETRESET;
	}

	/* Allocate Transaction Info */
	txn_handle = kzalloc(sizeof(struct qmi_txn), GFP_KERNEL);
	if (!txn_handle) {
		pr_err("%s: Failed to allocate txn handle\n", __func__);
		mutex_unlock(&handle->handle_lock);
		return -ENOMEM;
	}
	txn_handle->type = type;
	INIT_LIST_HEAD(&txn_handle->list);
	init_waitqueue_head(&txn_handle->wait_q);

	/* Cache the parameters passed & mark it as sync*/
	txn_handle->handle = handle;
	txn_handle->resp_desc = resp_desc;
	txn_handle->resp = resp;
	txn_handle->resp_len = resp_len;
	txn_handle->resp_received = 0;
	txn_handle->resp_cb = resp_cb;
	txn_handle->resp_cb_data = resp_cb_data;
	txn_handle->enc_data = NULL;
	txn_handle->enc_data_len = 0;

	/* Encode the request msg */
	encoded_req_len = req_desc->max_msg_len + QMI_HEADER_SIZE;
	encoded_req = kmalloc(encoded_req_len, GFP_KERNEL);
	if (!encoded_req) {
		pr_err("%s: Failed to allocate req_msg_buf\n", __func__);
		rc = -ENOMEM;
		goto encode_and_send_req_err1;
	}
	rc = qmi_kernel_encode(req_desc,
		(void *)(encoded_req + QMI_HEADER_SIZE),
		req_desc->max_msg_len, req);
	if (rc < 0) {
		pr_err("%s: Encode Failure %d\n", __func__, rc);
		goto encode_and_send_req_err2;
	}
	encoded_req_len = rc;

	/* Encode the header & Add to the txn_list */
	if (!handle->next_txn_id)
		handle->next_txn_id++;
	txn_handle->txn_id = handle->next_txn_id++;
	encode_qmi_header(encoded_req, QMI_REQUEST_CONTROL_FLAG,
			  txn_handle->txn_id, req_desc->msg_id,
			  encoded_req_len);
	encoded_req_len += QMI_HEADER_SIZE;

	/*
	 * Check if this port has transactions queued to its pending list
	 * and if there are any pending transactions then add the current
	 * transaction to the pending list rather than sending it. This avoids
	 * out-of-order message transfers.
	 */
	if (!list_empty(&handle->pending_txn_list)) {
		rc = -EAGAIN;
		goto append_pend_txn;
	}

	list_add_tail(&txn_handle->list, &handle->txn_list);
	qmi_log(handle, QMI_REQUEST_CONTROL_FLAG, txn_handle->txn_id,
			req_desc->msg_id, encoded_req_len);
	/* Send the request */
	rc = msm_ipc_router_send_msg((struct msm_ipc_port *)(handle->src_port),
		(struct msm_ipc_addr *)handle->dest_info,
		encoded_req, encoded_req_len);
append_pend_txn:
	if (rc == -EAGAIN) {
		txn_handle->enc_data = encoded_req;
		txn_handle->enc_data_len = encoded_req_len;
		if (list_empty(&handle->pending_txn_list))
			list_del(&txn_handle->list);
		list_add_tail(&txn_handle->list, &handle->pending_txn_list);
		if (ret_txn_handle)
			*ret_txn_handle = txn_handle;
		mutex_unlock(&handle->handle_lock);
		return 0;
	}
	if (rc < 0) {
		pr_err("%s: send_msg failed %d\n", __func__, rc);
		goto encode_and_send_req_err3;
	}
	mutex_unlock(&handle->handle_lock);

	kfree(encoded_req);
	if (ret_txn_handle)
		*ret_txn_handle = txn_handle;
	return 0;

encode_and_send_req_err3:
	list_del(&txn_handle->list);
encode_and_send_req_err2:
	kfree(encoded_req);
encode_and_send_req_err1:
	kfree(txn_handle);
	mutex_unlock(&handle->handle_lock);
	return rc;
}

int qmi_send_req_wait(struct qmi_handle *handle,
		      struct msg_desc *req_desc,
		      void *req, unsigned int req_len,
		      struct msg_desc *resp_desc,
		      void *resp, unsigned int resp_len,
		      unsigned long timeout_ms)
{
	struct qmi_txn *txn_handle = NULL;
	int rc;

	/* Encode and send the request */
	rc = qmi_encode_and_send_req(&txn_handle, handle, QMI_SYNC_TXN,
				     req_desc, req, req_len,
				     resp_desc, resp, resp_len,
				     NULL, NULL);
	if (rc < 0) {
		pr_err("%s: Error encode & send req: %d\n", __func__, rc);
		return rc;
	}

	/* Wait for the response */
	if (!timeout_ms) {
		wait_event(txn_handle->wait_q,
			   (txn_handle->resp_received ||
			    handle->handle_reset ||
			   (txn_handle->send_stat < 0)));
	} else {
		rc = wait_event_timeout(txn_handle->wait_q,
				(txn_handle->resp_received ||
				handle->handle_reset ||
				(txn_handle->send_stat < 0)),
				msecs_to_jiffies(timeout_ms));
		if (rc == 0)
			rc = -ETIMEDOUT;
	}

	mutex_lock(&handle->handle_lock);
	if (!txn_handle->resp_received) {
		pr_err("%s: Response Wait Error %d\n", __func__, rc);
		if (handle->handle_reset)
			rc = -ENETRESET;
		if (rc >= 0)
			rc = -EFAULT;
		if (txn_handle->send_stat < 0)
			rc = txn_handle->send_stat;
		goto send_req_wait_err;
	}
	rc = 0;

send_req_wait_err:
	list_del(&txn_handle->list);
	kfree(txn_handle);
	wake_up(&handle->reset_waitq);
	mutex_unlock(&handle->handle_lock);
	return rc;
}
EXPORT_SYMBOL(qmi_send_req_wait);

int qmi_send_req_nowait(struct qmi_handle *handle,
			struct msg_desc *req_desc,
			void *req, unsigned int req_len,
			struct msg_desc *resp_desc,
			void *resp, unsigned int resp_len,
			void (*resp_cb)(struct qmi_handle *handle,
					unsigned int msg_id, void *msg,
					void *resp_cb_data, int stat),
			void *resp_cb_data)
{
	return qmi_encode_and_send_req(NULL, handle, QMI_ASYNC_TXN,
				       req_desc, req, req_len,
				       resp_desc, resp, resp_len,
				       resp_cb, resp_cb_data);
}
EXPORT_SYMBOL(qmi_send_req_nowait);

/**
 * qmi_encode_and_send_resp() - Encode and send QMI response
 * @handle: QMI service handle sending the response.
 * @conn_h: Connection handle to which the response is sent.
 * @req_h: Request handle for which the response is sent.
 * @resp_desc: Message Descriptor describing the response structure.
 * @resp: Response structure.
 * @resp_len: Length of the response structure.
 *
 * @return: 0 on success, standard Linux error codes on failure.
 *
 * This function encodes and sends a response message from a service to
 * a client identified from the connection handle. The request for which
 * the response is sent is identified from the connection handle.
 *
 * This function must be called with handle->handle_lock locked.
 */
static int qmi_encode_and_send_resp(struct qmi_handle *handle,
	struct qmi_svc_clnt_conn *conn_h, struct req_handle *req_h,
	struct msg_desc *resp_desc, void *resp, unsigned int resp_len)
{
	struct qmi_txn *txn_handle;
	uint16_t cntl_flag;
	int rc;
	int encoded_resp_len;
	void *encoded_resp;

	if (handle->handle_reset) {
		rc = -ENETRESET;
		goto encode_and_send_resp_err0;
	}

	if (handle->handle_type != QMI_SERVICE_HANDLE ||
	    !verify_svc_clnt_conn(handle, conn_h) ||
	    (req_h && !verify_req_handle(conn_h, req_h))) {
		rc = -EINVAL;
		goto encode_and_send_resp_err0;
	}

	/* Allocate Transaction Info */
	txn_handle = kzalloc(sizeof(struct qmi_txn), GFP_KERNEL);
	if (!txn_handle) {
		pr_err("%s: Failed to allocate txn handle\n", __func__);
		rc = -ENOMEM;
		goto encode_and_send_resp_err0;
	}
	INIT_LIST_HEAD(&txn_handle->list);
	init_waitqueue_head(&txn_handle->wait_q);
	txn_handle->handle = handle;
	txn_handle->enc_data = NULL;
	txn_handle->enc_data_len = 0;

	/* Encode the response msg */
	encoded_resp_len = resp_desc->max_msg_len + QMI_HEADER_SIZE;
	encoded_resp = kmalloc(encoded_resp_len, GFP_KERNEL);
	if (!encoded_resp) {
		pr_err("%s: Failed to allocate resp_msg_buf\n", __func__);
		rc = -ENOMEM;
		goto encode_and_send_resp_err1;
	}
	rc = qmi_kernel_encode(resp_desc,
		(void *)(encoded_resp + QMI_HEADER_SIZE),
		resp_desc->max_msg_len, resp);
	if (rc < 0) {
		pr_err("%s: Encode Failure %d\n", __func__, rc);
		goto encode_and_send_resp_err2;
	}
	encoded_resp_len = rc;

	/* Encode the header & Add to the txn_list */
	if (req_h) {
		txn_handle->txn_id = req_h->txn_id;
		cntl_flag = QMI_RESPONSE_CONTROL_FLAG;
	} else {
		if (!handle->next_txn_id)
			handle->next_txn_id++;
		txn_handle->txn_id = handle->next_txn_id++;
		cntl_flag = QMI_INDICATION_CONTROL_FLAG;
	}
	encode_qmi_header(encoded_resp, cntl_flag,
			  txn_handle->txn_id, resp_desc->msg_id,
			  encoded_resp_len);
	encoded_resp_len += QMI_HEADER_SIZE;

	qmi_log(handle, cntl_flag, txn_handle->txn_id,
			resp_desc->msg_id, encoded_resp_len);
	/*
	 * Check if this svc_clnt has transactions queued to its pending list
	 * and if there are any pending transactions then add the current
	 * transaction to the pending list rather than sending it. This avoids
	 * out-of-order message transfers.
	 */
	mutex_lock(&conn_h->pending_txn_lock);
	if (list_empty(&conn_h->pending_txn_list))
		rc = msm_ipc_router_send_msg(
			(struct msm_ipc_port *)(handle->src_port),
			(struct msm_ipc_addr *)conn_h->clnt_addr,
			encoded_resp, encoded_resp_len);
	else
		rc = -EAGAIN;

	if (req_h)
		rmv_req_handle(req_h);
	if (rc == -EAGAIN) {
		txn_handle->enc_data = encoded_resp;
		txn_handle->enc_data_len = encoded_resp_len;
		list_add_tail(&txn_handle->list, &conn_h->pending_txn_list);
		mutex_unlock(&conn_h->pending_txn_lock);
		return 0;
	}
	mutex_unlock(&conn_h->pending_txn_lock);
	if (rc < 0)
		pr_err("%s: send_msg failed %d\n", __func__, rc);
encode_and_send_resp_err2:
	kfree(encoded_resp);
encode_and_send_resp_err1:
	kfree(txn_handle);
encode_and_send_resp_err0:
	return rc;
}

/**
 * qmi_send_resp() - Send response to a request
 * @handle: QMI handle from which the response is sent.
 * @clnt: Client to which the response is sent.
 * @req_handle: Request for which the response is sent.
 * @resp_desc: Descriptor explaining the response structure.
 * @resp: Pointer to the response structure.
 * @resp_len: Length of the response structure.
 *
 * @return: 0 on success, < 0 on error.
 */
int qmi_send_resp(struct qmi_handle *handle, void *conn_handle,
		  void *req_handle, struct msg_desc *resp_desc,
		  void *resp, unsigned int resp_len)
{
	int rc;
	struct qmi_svc_clnt_conn *conn_h;
	struct req_handle *req_h;

	if (!handle || !conn_handle || !req_handle ||
	    !resp_desc || !resp || !resp_len)
		return -EINVAL;

	conn_h = (struct qmi_svc_clnt_conn *)conn_handle;
	req_h = (struct req_handle *)req_handle;
	mutex_lock(&handle->handle_lock);
	rc = qmi_encode_and_send_resp(handle, conn_h, req_h,
				      resp_desc, resp, resp_len);
	if (rc < 0)
		pr_err("%s: Error encoding and sending response\n", __func__);
	mutex_unlock(&handle->handle_lock);
	return rc;
}
EXPORT_SYMBOL(qmi_send_resp);

/**
 * qmi_send_resp_from_cb() - Send response to a request from request_cb
 * @handle: QMI handle from which the response is sent.
 * @clnt: Client to which the response is sent.
 * @req_handle: Request for which the response is sent.
 * @resp_desc: Descriptor explaining the response structure.
 * @resp: Pointer to the response structure.
 * @resp_len: Length of the response structure.
 *
 * @return: 0 on success, < 0 on error.
 */
int qmi_send_resp_from_cb(struct qmi_handle *handle, void *conn_handle,
			  void *req_handle, struct msg_desc *resp_desc,
			  void *resp, unsigned int resp_len)
{
	int rc;
	struct qmi_svc_clnt_conn *conn_h;
	struct req_handle *req_h;

	if (!handle || !conn_handle || !req_handle ||
	    !resp_desc || !resp || !resp_len)
		return -EINVAL;

	conn_h = (struct qmi_svc_clnt_conn *)conn_handle;
	req_h = (struct req_handle *)req_handle;
	rc = qmi_encode_and_send_resp(handle, conn_h, req_h,
				      resp_desc, resp, resp_len);
	if (rc < 0)
		pr_err("%s: Error encoding and sending response\n", __func__);
	return rc;
}
EXPORT_SYMBOL(qmi_send_resp_from_cb);

/**
 * qmi_send_ind() - Send unsolicited event/indication to a client
 * @handle: QMI handle from which the indication is sent.
 * @clnt: Client to which the indication is sent.
 * @ind_desc: Descriptor explaining the indication structure.
 * @ind: Pointer to the indication structure.
 * @ind_len: Length of the indication structure.
 *
 * @return: 0 on success, < 0 on error.
 */
int qmi_send_ind(struct qmi_handle *handle, void *conn_handle,
		 struct msg_desc *ind_desc, void *ind, unsigned int ind_len)
{
	int rc = 0;
	struct qmi_svc_clnt_conn *conn_h;

	if (!handle || !conn_handle || !ind_desc)
		return -EINVAL;

	if ((!ind && ind_len) || (ind && !ind_len))
		return -EINVAL;

	conn_h = (struct qmi_svc_clnt_conn *)conn_handle;
	mutex_lock(&handle->handle_lock);
	rc = qmi_encode_and_send_resp(handle, conn_h, NULL,
				      ind_desc, ind, ind_len);
	if (rc < 0)
		pr_err("%s: Error encoding and sending ind.\n", __func__);
	mutex_unlock(&handle->handle_lock);
	return rc;
}
EXPORT_SYMBOL(qmi_send_ind);

/**
 * qmi_send_ind_from_cb() - Send indication to a client from registration_cb
 * @handle: QMI handle from which the indication is sent.
 * @clnt: Client to which the indication is sent.
 * @ind_desc: Descriptor explaining the indication structure.
 * @ind: Pointer to the indication structure.
 * @ind_len: Length of the indication structure.
 *
 * @return: 0 on success, < 0 on error.
 */
int qmi_send_ind_from_cb(struct qmi_handle *handle, void *conn_handle,
		struct msg_desc *ind_desc, void *ind, unsigned int ind_len)
{
	int rc = 0;
	struct qmi_svc_clnt_conn *conn_h;

	if (!handle || !conn_handle || !ind_desc)
		return -EINVAL;

	if ((!ind && ind_len) || (ind && !ind_len))
		return -EINVAL;

	conn_h = (struct qmi_svc_clnt_conn *)conn_handle;
	rc = qmi_encode_and_send_resp(handle, conn_h, NULL,
				      ind_desc, ind, ind_len);
	if (rc < 0)
		pr_err("%s: Error encoding and sending ind.\n", __func__);
	return rc;
}
EXPORT_SYMBOL(qmi_send_ind_from_cb);

/**
 * translate_err_code() - Translate Linux error codes into QMI error codes
 * @err: Standard Linux error codes to be translated.
 *
 * @return: Return QMI error code.
 */
static int translate_err_code(int err)
{
	int rc;

	switch (err) {
	case -ECONNREFUSED:
		rc = QMI_ERR_CLIENT_IDS_EXHAUSTED_V01;
		break;
	case -EBADMSG:
		rc = QMI_ERR_ENCODING_V01;
		break;
	case -ENOMEM:
		rc = QMI_ERR_NO_MEMORY_V01;
		break;
	case -EOPNOTSUPP:
		rc = QMI_ERR_MALFORMED_MSG_V01;
		break;
	case -ENOTSUPP:
		rc = QMI_ERR_NOT_SUPPORTED_V01;
		break;
	default:
		rc = QMI_ERR_INTERNAL_V01;
		break;
	}
	return rc;
}

/**
 * send_err_resp() - Send the error response
 * @handle: Service handle from which the response is sent.
 * @conn_h: Client<->Service connection on which the response is sent.
 * @addr: Client address to which the error response is sent.
 * @msg_id: Request message id for which the error response is sent.
 * @txn_id: Request Transaction ID for which the error response is sent.
 * @err: Error code to be sent.
 *
 * @return: 0 on success, standard Linux error codes on failure.
 *
 * This function is used to send an error response from within the QMI
 * service interface. This function is called when the service returns
 * an error to the QMI interface while handling a request.
 */
static int send_err_resp(struct qmi_handle *handle,
			 struct qmi_svc_clnt_conn *conn_h, void *addr,
			 uint16_t msg_id, uint16_t txn_id, int err)
{
	struct qmi_response_type_v01 err_resp;
	struct qmi_txn *txn_handle;
	struct msm_ipc_addr *dest_addr;
	int rc;
	int encoded_resp_len;
	void *encoded_resp;

	if (handle->handle_reset)
		return -ENETRESET;

	err_resp.result = QMI_RESULT_FAILURE_V01;
	err_resp.error = translate_err_code(err);

	/* Allocate Transaction Info */
	txn_handle = kzalloc(sizeof(struct qmi_txn), GFP_KERNEL);
	if (!txn_handle) {
		pr_err("%s: Failed to allocate txn handle\n", __func__);
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&txn_handle->list);
	init_waitqueue_head(&txn_handle->wait_q);
	txn_handle->handle = handle;
	txn_handle->enc_data = NULL;
	txn_handle->enc_data_len = 0;

	/* Encode the response msg */
	encoded_resp_len = err_resp_desc.max_msg_len + QMI_HEADER_SIZE;
	encoded_resp = kmalloc(encoded_resp_len, GFP_KERNEL);
	if (!encoded_resp) {
		pr_err("%s: Failed to allocate resp_msg_buf\n", __func__);
		rc = -ENOMEM;
		goto encode_and_send_err_resp_err0;
	}
	rc = qmi_kernel_encode(&err_resp_desc,
		(void *)(encoded_resp + QMI_HEADER_SIZE),
		err_resp_desc.max_msg_len, &err_resp);
	if (rc < 0) {
		pr_err("%s: Encode Failure %d\n", __func__, rc);
		goto encode_and_send_err_resp_err1;
	}
	encoded_resp_len = rc;

	/* Encode the header & Add to the txn_list */
	txn_handle->txn_id = txn_id;
	encode_qmi_header(encoded_resp, QMI_RESPONSE_CONTROL_FLAG,
			  txn_handle->txn_id, msg_id,
			  encoded_resp_len);
	encoded_resp_len += QMI_HEADER_SIZE;

	qmi_log(handle, QMI_RESPONSE_CONTROL_FLAG, txn_id,
			msg_id, encoded_resp_len);
	/*
	 * Check if this svc_clnt has transactions queued to its pending list
	 * and if there are any pending transactions then add the current
	 * transaction to the pending list rather than sending it. This avoids
	 * out-of-order message transfers.
	 */
	if (!conn_h) {
		dest_addr = (struct msm_ipc_addr *)addr;
		goto tx_err_resp;
	}

	mutex_lock(&conn_h->pending_txn_lock);
	dest_addr = (struct msm_ipc_addr *)conn_h->clnt_addr;
	if (!list_empty(&conn_h->pending_txn_list)) {
		rc = -EAGAIN;
		goto queue_err_resp;
	}
tx_err_resp:
	rc = msm_ipc_router_send_msg(
			(struct msm_ipc_port *)(handle->src_port),
			dest_addr, encoded_resp, encoded_resp_len);
queue_err_resp:
	if (rc == -EAGAIN && conn_h) {
		txn_handle->enc_data = encoded_resp;
		txn_handle->enc_data_len = encoded_resp_len;
		list_add_tail(&txn_handle->list, &conn_h->pending_txn_list);
		mutex_unlock(&conn_h->pending_txn_lock);
		return 0;
	}
	if (conn_h)
		mutex_unlock(&conn_h->pending_txn_lock);
	if (rc < 0)
		pr_err("%s: send_msg failed %d\n", __func__, rc);
encode_and_send_err_resp_err1:
	kfree(encoded_resp);
encode_and_send_err_resp_err0:
	kfree(txn_handle);
	return rc;
}

/**
 * handle_qmi_request() - Handle the QMI request
 * @handle: QMI service handle on which the request has arrived.
 * @req_msg: Request message to be handled.
 * @txn_id: Transaction ID of the request message.
 * @msg_id: Message ID of the request message.
 * @msg_len: Message Length of the request message.
 * @src_addr: Address of the source which sent the request.
 * @src_addr_len: Length of the source address.
 *
 * @return: 0 on success, standard Linux error codes on failure.
 */
static int handle_qmi_request(struct qmi_handle *handle,
			      unsigned char *req_msg, uint16_t txn_id,
			      uint16_t msg_id, uint16_t msg_len,
			      void *src_addr, size_t src_addr_len)
{
	struct qmi_svc_clnt_conn *conn_h;
	struct msg_desc *req_desc = NULL;
	void *req_struct = NULL;
	unsigned int req_struct_len = 0;
	struct req_handle *req_h = NULL;
	int rc = 0;

	if (handle->handle_type != QMI_SERVICE_HANDLE)
		return -EOPNOTSUPP;

	conn_h = find_svc_clnt_conn(handle, src_addr, src_addr_len);
	if (conn_h)
		goto decode_req;

	/* New client, establish a connection */
	conn_h = add_svc_clnt_conn(handle, src_addr, src_addr_len);
	if (!conn_h) {
		pr_err("%s: Error adding a new conn_h\n", __func__);
		rc = -ENOMEM;
		goto out_handle_req;
	}
	rc = handle->svc_ops_options->connect_cb(handle, conn_h);
	if (rc < 0) {
		pr_err("%s: Error accepting new client\n", __func__);
		rmv_svc_clnt_conn(conn_h);
		conn_h = NULL;
		goto out_handle_req;
	}

decode_req:
	if (!msg_len)
		goto process_req;

	req_struct_len = handle->svc_ops_options->req_desc_cb(msg_id,
							      &req_desc);
	if (!req_desc || req_struct_len <= 0) {
		pr_err("%s: Error getting req_desc for msg_id %d\n",
			__func__, msg_id);
		rc = -ENOTSUPP;
		goto out_handle_req;
	}

	req_struct = kzalloc(req_struct_len, GFP_KERNEL);
	if (!req_struct) {
		pr_err("%s: Error allocating request struct\n", __func__);
		rc = -ENOMEM;
		goto out_handle_req;
	}

	rc = qmi_kernel_decode(req_desc, req_struct,
				(void *)(req_msg + QMI_HEADER_SIZE), msg_len);
	if (rc < 0) {
		pr_err("%s: Error decoding msg_id %d\n", __func__, msg_id);
		rc = -EBADMSG;
		goto out_handle_req;
	}

process_req:
	req_h = add_req_handle(conn_h, msg_id, txn_id);
	if (!req_h) {
		pr_err("%s: Error adding new request handle\n", __func__);
		rc = -ENOMEM;
		goto out_handle_req;
	}
	rc = handle->svc_ops_options->req_cb(handle, conn_h, req_h,
					      msg_id, req_struct);
	if (rc < 0) {
		pr_err("%s: Error while req_cb\n", __func__);
		/* Check if the error is before or after sending a response */
		if (verify_req_handle(conn_h, req_h))
			rmv_req_handle(req_h);
		else
			rc = 0;
	}

out_handle_req:
	kfree(req_struct);
	if (rc < 0)
		send_err_resp(handle, conn_h, src_addr, msg_id, txn_id, rc);
	return rc;
}

static struct qmi_txn *find_txn_handle(struct qmi_handle *handle,
				       uint16_t txn_id)
{
	struct qmi_txn *txn_handle;

	list_for_each_entry(txn_handle, &handle->txn_list, list) {
		if (txn_handle->txn_id == txn_id)
			return txn_handle;
	}
	return NULL;
}

static int handle_qmi_response(struct qmi_handle *handle,
			       unsigned char *resp_msg, uint16_t txn_id,
			       uint16_t msg_id, uint16_t msg_len)
{
	struct qmi_txn *txn_handle;
	int rc;

	/* Find the transaction handle */
	txn_handle = find_txn_handle(handle, txn_id);
	if (!txn_handle) {
		pr_err("%s Response received for non-existent txn_id %d\n",
			__func__, txn_id);
		return 0;
	}

	/* Decode the message */
	rc = qmi_kernel_decode(txn_handle->resp_desc, txn_handle->resp,
			       (void *)(resp_msg + QMI_HEADER_SIZE), msg_len);
	if (rc < 0) {
		pr_err("%s: Response Decode Failure <%d: %d: %d> rc: %d\n",
			__func__, txn_id, msg_id, msg_len, rc);
		wake_up(&txn_handle->wait_q);
		if (txn_handle->type == QMI_ASYNC_TXN) {
			list_del(&txn_handle->list);
			kfree(txn_handle);
		}
		return rc;
	}

	/* Handle async or sync resp */
	switch (txn_handle->type) {
	case QMI_SYNC_TXN:
		txn_handle->resp_received = 1;
		wake_up(&txn_handle->wait_q);
		rc = 0;
		break;

	case QMI_ASYNC_TXN:
		if (txn_handle->resp_cb)
			txn_handle->resp_cb(txn_handle->handle, msg_id,
					    txn_handle->resp,
					    txn_handle->resp_cb_data, 0);
		list_del(&txn_handle->list);
		kfree(txn_handle);
		rc = 0;
		break;

	default:
		pr_err("%s: Unrecognized transaction type\n", __func__);
		return -EFAULT;
	}
	return rc;
}

static int handle_qmi_indication(struct qmi_handle *handle, void *msg,
				 unsigned int msg_id, unsigned int msg_len)
{
	if (handle->ind_cb)
		handle->ind_cb(handle, msg_id, msg + QMI_HEADER_SIZE,
				msg_len, handle->ind_cb_priv);
	return 0;
}

int qmi_recv_msg(struct qmi_handle *handle)
{
	unsigned int recv_msg_len;
	unsigned char *recv_msg = NULL;
	struct msm_ipc_addr src_addr = {0};
	unsigned char cntl_flag;
	uint16_t txn_id, msg_id, msg_len;
	int rc;

	if (!handle)
		return -EINVAL;

	mutex_lock(&handle->handle_lock);
	if (handle->handle_reset) {
		mutex_unlock(&handle->handle_lock);
		return -ENETRESET;
	}

	/* Read the messages */
	rc = msm_ipc_router_read_msg((struct msm_ipc_port *)(handle->src_port),
				     &src_addr, &recv_msg, &recv_msg_len);
	if (rc == -ENOMSG) {
		mutex_unlock(&handle->handle_lock);
		return rc;
	}

	if (rc < 0) {
		pr_err("%s: Read failed %d\n", __func__, rc);
		mutex_unlock(&handle->handle_lock);
		return rc;
	}

	/* Decode the header & Handle the req, resp, indication message */
	decode_qmi_header(recv_msg, &cntl_flag, &txn_id, &msg_id, &msg_len);

	qmi_log(handle, cntl_flag, txn_id, msg_id, msg_len);
	switch (cntl_flag) {
	case QMI_REQUEST_CONTROL_FLAG:
		rc = handle_qmi_request(handle, recv_msg, txn_id, msg_id,
					msg_len, &src_addr, sizeof(src_addr));
		break;

	case QMI_RESPONSE_CONTROL_FLAG:
		rc = handle_qmi_response(handle, recv_msg,
					 txn_id, msg_id, msg_len);
		break;

	case QMI_INDICATION_CONTROL_FLAG:
		rc = handle_qmi_indication(handle, recv_msg, msg_id, msg_len);
		break;

	default:
		rc = -EFAULT;
		pr_err("%s: Unsupported message type %d\n",
			__func__, cntl_flag);
		break;
	}
	kfree(recv_msg);
	mutex_unlock(&handle->handle_lock);
	return rc;
}
EXPORT_SYMBOL(qmi_recv_msg);

int qmi_connect_to_service(struct qmi_handle *handle,
			   uint32_t service_id,
			   uint32_t service_vers,
			   uint32_t service_ins)
{
	struct msm_ipc_port_name svc_name;
	struct msm_ipc_server_info svc_info;
	struct msm_ipc_addr *svc_dest_addr;
	int rc;
	uint32_t instance_id;

	if (!handle)
		return -EINVAL;

	svc_dest_addr = kzalloc(sizeof(struct msm_ipc_addr),
				GFP_KERNEL);
	if (!svc_dest_addr) {
		pr_err("%s: Failure allocating memory\n", __func__);
		return -ENOMEM;
	}

	instance_id = BUILD_INSTANCE_ID(service_vers, service_ins);
	svc_name.service = service_id;
	svc_name.instance = instance_id;

	rc = msm_ipc_router_lookup_server_name(&svc_name, &svc_info,
						1, LOOKUP_MASK);
	if (rc <= 0) {
		pr_err("%s: Server %08x:%08x not found\n",
			__func__, service_id, instance_id);
		return -ENODEV;
	}
	svc_dest_addr->addrtype = MSM_IPC_ADDR_ID;
	svc_dest_addr->addr.port_addr.node_id = svc_info.node_id;
	svc_dest_addr->addr.port_addr.port_id = svc_info.port_id;
	mutex_lock(&handle->handle_lock);
	if (handle->handle_reset) {
		mutex_unlock(&handle->handle_lock);
		return -ENETRESET;
	}
	handle->dest_info = svc_dest_addr;
	handle->dest_service_id = service_id;
	mutex_unlock(&handle->handle_lock);

	return 0;
}
EXPORT_SYMBOL(qmi_connect_to_service);

/**
 * svc_event_add_svc_addr() - Add a specific service address to the list
 * @event_nb:	Reference to the service event structure.
 * @node_id:	Node id of the service address.
 * @port_id:	Port id of the service address.
 *
 * Return: 0 on success, standard error code otheriwse.
 *
 * This function should be called with svc_addr_list_lock locked.
 */
static int svc_event_add_svc_addr(struct svc_event_nb *event_nb,
				uint32_t node_id, uint32_t port_id)
{

	struct svc_addr *addr;

	if (!event_nb)
		return -EINVAL;
	addr = kmalloc(sizeof(*addr), GFP_KERNEL);
	if (!addr) {
		pr_err("%s: Memory allocation failed for address list\n",
			__func__);
		return -ENOMEM;
	}
	addr->port_addr.node_id = node_id;
	addr->port_addr.port_id = port_id;
	list_add_tail(&addr->list_node, &event_nb->svc_addr_list);
	return 0;
}

/**
 * qmi_notify_svc_event_arrive() - Notify the clients about service arrival
 * @service:	Service id for the specific service.
 * @instance:	Instance id for the specific service.
 * @node_id:	Node id of the processor where the service is hosted.
 * @port_id:	Port id of the service port created by IPC Router.
 *
 * Return:	0 on Success or standard error code.
 */
static int qmi_notify_svc_event_arrive(uint32_t service,
					uint32_t instance,
					uint32_t node_id,
					uint32_t port_id)
{
	struct svc_event_nb *temp;
	unsigned long flags;
	struct svc_addr *addr;
	bool already_notified = false;

	mutex_lock(&svc_event_nb_list_lock);
	temp = find_svc_event_nb(service, instance);
	if (!temp) {
		mutex_unlock(&svc_event_nb_list_lock);
		return -EINVAL;
	}
	mutex_unlock(&svc_event_nb_list_lock);

	mutex_lock(&temp->svc_addr_list_lock);
	list_for_each_entry(addr, &temp->svc_addr_list, list_node)
		if (addr->port_addr.node_id == node_id &&
			addr->port_addr.port_id == port_id)
				already_notified = true;
	if (!already_notified) {
		/*
		 * Notify only if the clients are not notified about the
		 * service during registration.
		 */
		svc_event_add_svc_addr(temp, node_id, port_id);
		spin_lock_irqsave(&temp->nb_lock, flags);
		raw_notifier_call_chain(&temp->svc_event_rcvr_list,
				QMI_SERVER_ARRIVE, NULL);
		spin_unlock_irqrestore(&temp->nb_lock, flags);
	}
	mutex_unlock(&temp->svc_addr_list_lock);

	return 0;
}

/**
 * qmi_notify_svc_event_exit() - Notify the clients about service exit
 * @service:	Service id for the specific service.
 * @instance:	Instance id for the specific service.
 * @node_id:	Node id of the processor where the service is hosted.
 * @port_id:	Port id of the service port created by IPC Router.
 *
 * Return:	0 on Success or standard error code.
 */
static int qmi_notify_svc_event_exit(uint32_t service,
					uint32_t instance,
					uint32_t node_id,
					uint32_t port_id)
{
	struct svc_event_nb *temp;
	unsigned long flags;
	struct svc_addr *addr;
	struct svc_addr *temp_addr;

	mutex_lock(&svc_event_nb_list_lock);
	temp = find_svc_event_nb(service, instance);
	if (!temp) {
		mutex_unlock(&svc_event_nb_list_lock);
		return -EINVAL;
	}
	mutex_unlock(&svc_event_nb_list_lock);

	mutex_lock(&temp->svc_addr_list_lock);
	list_for_each_entry_safe(addr, temp_addr, &temp->svc_addr_list,
					list_node) {
		if (addr->port_addr.node_id == node_id &&
			addr->port_addr.port_id == port_id) {
			/*
			 * Notify only if an already notified service has
			 * gone down.
			 */
			spin_lock_irqsave(&temp->nb_lock, flags);
			raw_notifier_call_chain(&temp->svc_event_rcvr_list,
						QMI_SERVER_EXIT, NULL);
			spin_unlock_irqrestore(&temp->nb_lock, flags);
			list_del(&addr->list_node);
			kfree(addr);
		}
	}

	mutex_unlock(&temp->svc_addr_list_lock);

	return 0;
}

static struct svc_event_nb *find_svc_event_nb(uint32_t service_id,
					      uint32_t instance_id)
{
	struct svc_event_nb *temp;

	list_for_each_entry(temp, &svc_event_nb_list, list) {
		if (temp->service_id == service_id &&
		    temp->instance_id == instance_id)
			return temp;
	}
	return NULL;
}

/**
 * find_and_add_svc_event_nb() - Find/Add a notifier block for specific service
 * @service_id:	Service Id of the service
 * @instance_id:Instance Id of the service
 *
 * Return:	Pointer to svc_event_nb structure for the specified service
 *
 * This function should only be called after acquiring svc_event_nb_list_lock.
 */
static struct svc_event_nb *find_and_add_svc_event_nb(uint32_t service_id,
						      uint32_t instance_id)
{
	struct svc_event_nb *temp;

	temp = find_svc_event_nb(service_id, instance_id);
	if (temp)
		return temp;

	temp = kzalloc(sizeof(struct svc_event_nb), GFP_KERNEL);
	if (!temp) {
		pr_err("%s: Failed to alloc notifier block\n", __func__);
		return temp;
	}

	spin_lock_init(&temp->nb_lock);
	temp->service_id = service_id;
	temp->instance_id = instance_id;
	INIT_LIST_HEAD(&temp->list);
	INIT_LIST_HEAD(&temp->svc_addr_list);
	RAW_INIT_NOTIFIER_HEAD(&temp->svc_event_rcvr_list);
	mutex_init(&temp->svc_addr_list_lock);
	list_add_tail(&temp->list, &svc_event_nb_list);

	return temp;
}

int qmi_svc_event_notifier_register(uint32_t service_id,
				    uint32_t service_vers,
				    uint32_t service_ins,
				    struct notifier_block *nb)
{
	struct svc_event_nb *temp;
	unsigned long flags;
	int ret;
	int i;
	int num_servers;
	uint32_t instance_id;
	struct msm_ipc_port_name svc_name;
	struct msm_ipc_server_info *svc_info_arr = NULL;

	mutex_lock(&qmi_svc_event_notifier_lock);
	if (!qmi_svc_event_notifier_port && !qmi_svc_event_notifier_wq)
		qmi_svc_event_notifier_init();
	mutex_unlock(&qmi_svc_event_notifier_lock);

	instance_id = BUILD_INSTANCE_ID(service_vers, service_ins);
	mutex_lock(&svc_event_nb_list_lock);
	temp = find_and_add_svc_event_nb(service_id, instance_id);
	if (!temp) {
		mutex_unlock(&svc_event_nb_list_lock);
		return -EFAULT;
	}
	mutex_unlock(&svc_event_nb_list_lock);

	mutex_lock(&temp->svc_addr_list_lock);
	spin_lock_irqsave(&temp->nb_lock, flags);
	ret = raw_notifier_chain_register(&temp->svc_event_rcvr_list, nb);
	spin_unlock_irqrestore(&temp->nb_lock, flags);
	if (!list_empty(&temp->svc_addr_list)) {
		/* Notify this client only if Some services already exist. */
		spin_lock_irqsave(&temp->nb_lock, flags);
		nb->notifier_call(nb, QMI_SERVER_ARRIVE, NULL);
		spin_unlock_irqrestore(&temp->nb_lock, flags);
	} else {
		/*
		 * Check if we have missed a new server event that happened
		 * earlier.
		 */
		svc_name.service = service_id;
		svc_name.instance = instance_id;
		num_servers = msm_ipc_router_lookup_server_name(&svc_name,
								NULL,
								0, LOOKUP_MASK);
		if (num_servers > 0) {
			svc_info_arr = kmalloc_array(num_servers,
						sizeof(*svc_info_arr),
						GFP_KERNEL);
			if (!svc_info_arr)
				return -ENOMEM;
			num_servers = msm_ipc_router_lookup_server_name(
								&svc_name,
								svc_info_arr,
								num_servers,
								LOOKUP_MASK);
			for (i = 0; i < num_servers; i++)
				svc_event_add_svc_addr(temp,
						svc_info_arr[i].node_id,
						svc_info_arr[i].port_id);
			kfree(svc_info_arr);

			spin_lock_irqsave(&temp->nb_lock, flags);
			raw_notifier_call_chain(&temp->svc_event_rcvr_list,
						QMI_SERVER_ARRIVE, NULL);
			spin_unlock_irqrestore(&temp->nb_lock, flags);
		}
	}
	mutex_unlock(&temp->svc_addr_list_lock);

	return ret;
}
EXPORT_SYMBOL(qmi_svc_event_notifier_register);

int qmi_svc_event_notifier_unregister(uint32_t service_id,
				      uint32_t service_vers,
				      uint32_t service_ins,
				      struct notifier_block *nb)
{
	int ret;
	struct svc_event_nb *temp;
	unsigned long flags;
	uint32_t instance_id;

	instance_id = BUILD_INSTANCE_ID(service_vers, service_ins);
	mutex_lock(&svc_event_nb_list_lock);
	temp = find_svc_event_nb(service_id, instance_id);
	if (!temp) {
		mutex_unlock(&svc_event_nb_list_lock);
		return -EINVAL;
	}

	spin_lock_irqsave(&temp->nb_lock, flags);
	ret = raw_notifier_chain_unregister(&temp->svc_event_rcvr_list, nb);
	spin_unlock_irqrestore(&temp->nb_lock, flags);
	mutex_unlock(&svc_event_nb_list_lock);

	return ret;
}
EXPORT_SYMBOL(qmi_svc_event_notifier_unregister);

/**
 * qmi_svc_event_worker() - Read control messages over service event port
 * @work:	Reference to the work structure queued.
 *
 */
static void qmi_svc_event_worker(struct work_struct *work)
{
	union rr_control_msg *ctl_msg = NULL;
	unsigned int ctl_msg_len;
	struct msm_ipc_addr src_addr;
	int ret;

	while (1) {
		ret = msm_ipc_router_read_msg(qmi_svc_event_notifier_port,
			&src_addr, (unsigned char **)&ctl_msg, &ctl_msg_len);
		if (ret == -ENOMSG)
			break;
		if (ret < 0) {
			pr_err("%s:Error receiving control message\n",
					__func__);
			break;
		}
		if (ctl_msg->cmd == IPC_ROUTER_CTRL_CMD_NEW_SERVER)
			qmi_notify_svc_event_arrive(ctl_msg->srv.service,
							ctl_msg->srv.instance,
							ctl_msg->srv.node_id,
							ctl_msg->srv.port_id);
		else if (ctl_msg->cmd == IPC_ROUTER_CTRL_CMD_REMOVE_SERVER)
			qmi_notify_svc_event_exit(ctl_msg->srv.service,
							ctl_msg->srv.instance,
							ctl_msg->srv.node_id,
							ctl_msg->srv.port_id);
		kfree(ctl_msg);
	}
}

/**
 * qmi_svc_event_notify() - Callback for any service event posted on the control port
 * @event:	The event posted on the control port.
 * @data:	Any out-of-band data associated with event.
 * @odata_len:	Length of the out-of-band data, if any.
 * @priv:	Private Data.
 *
 * This function is called by the underlying transport to notify the QMI
 * interface regarding any incoming service related events. It is registered
 * during service event control port creation.
 */
static void qmi_svc_event_notify(unsigned event, void *data,
				size_t odata_len, void *priv)
{
	if (event == IPC_ROUTER_CTRL_CMD_NEW_SERVER
		|| event == IPC_ROUTER_CTRL_CMD_REMOVE_CLIENT
		|| event == IPC_ROUTER_CTRL_CMD_REMOVE_SERVER)
		queue_work(qmi_svc_event_notifier_wq, &qmi_svc_event_work);
}

/**
 * qmi_svc_event_notifier_init() - Create a control port to get service events
 *
 * This function is called during first service notifier registration. It
 * creates a control port to get notification about server events so that
 * respective clients can be notified about the events.
 */
static void qmi_svc_event_notifier_init(void)
{
	qmi_svc_event_notifier_wq = create_singlethread_workqueue(
					"qmi_svc_event_wq");
	if (!qmi_svc_event_notifier_wq) {
		pr_err("%s: ctrl workqueue allocation failed\n", __func__);
		return;
	}
	qmi_svc_event_notifier_port = msm_ipc_router_create_port(
				qmi_svc_event_notify, NULL);
	if (!qmi_svc_event_notifier_port) {
		destroy_workqueue(qmi_svc_event_notifier_wq);
		pr_err("%s: IPC Router Port creation failed\n", __func__);
		return;
	}
	msm_ipc_router_bind_control_port(qmi_svc_event_notifier_port);

	return;
}

/**
 * qmi_log_init() - Init function for IPC Logging
 *
 * Initialize log contexts for QMI request/response/indications.
 */
void qmi_log_init(void)
{
	qmi_req_resp_log_ctx =
		ipc_log_context_create(QMI_REQ_RESP_LOG_PAGES,
			"kqmi_req_resp", 0);
	if (!qmi_req_resp_log_ctx)
		pr_err("%s: Unable to create QMI IPC logging for Req/Resp",
			__func__);
	qmi_ind_log_ctx =
		ipc_log_context_create(QMI_IND_LOG_PAGES, "kqmi_ind", 0);
	if (!qmi_ind_log_ctx)
		pr_err("%s: Unable to create QMI IPC %s",
				"logging for Indications", __func__);
}

/**
 * qmi_svc_register() - Register a QMI service with a QMI handle
 * @handle: QMI handle on which the service has to be registered.
 * @ops_options: Service specific operations and options.
 *
 * @return: 0 if successfully registered, < 0 on error.
 */
int qmi_svc_register(struct qmi_handle *handle, void *ops_options)
{
	struct qmi_svc_ops_options *svc_ops_options;
	struct msm_ipc_addr svc_name;
	int rc;
	uint32_t instance_id;

	svc_ops_options = (struct qmi_svc_ops_options *)ops_options;
	if (!handle || !svc_ops_options)
		return -EINVAL;

	/* Check if the required elements of opts_options are filled */
	if (!svc_ops_options->service_id || !svc_ops_options->service_vers ||
	    !svc_ops_options->connect_cb || !svc_ops_options->disconnect_cb ||
	    !svc_ops_options->req_desc_cb || !svc_ops_options->req_cb)
		return -EINVAL;

	mutex_lock(&handle->handle_lock);
	/* Check if another service/client is registered in that handle */
	if (handle->handle_type == QMI_SERVICE_HANDLE || handle->dest_info) {
		mutex_unlock(&handle->handle_lock);
		return -EBUSY;
	}
	INIT_LIST_HEAD(&handle->conn_list);
	mutex_unlock(&handle->handle_lock);

	/*
	 * Unlocked the handle_lock, because NEW_SERVER message will end up
	 * in this handle's control port, which requires holding the same
	 * mutex. Also it is safe to call register_server unlocked.
	 */
	/* Register the service */
	instance_id = ((svc_ops_options->service_vers & 0xFF) |
		       ((svc_ops_options->service_ins & 0xFF) << 8));
	svc_name.addrtype = MSM_IPC_ADDR_NAME;
	svc_name.addr.port_name.service = svc_ops_options->service_id;
	svc_name.addr.port_name.instance = instance_id;
	rc = msm_ipc_router_register_server(
		(struct msm_ipc_port *)handle->src_port, &svc_name);
	if (rc < 0) {
		pr_err("%s: Error %d registering QMI service %08x:%08x\n",
			__func__, rc, svc_ops_options->service_id,
			instance_id);
		return rc;
	}
	mutex_lock(&handle->handle_lock);
	handle->svc_ops_options = svc_ops_options;
	handle->handle_type = QMI_SERVICE_HANDLE;
	mutex_unlock(&handle->handle_lock);
	return rc;
}
EXPORT_SYMBOL(qmi_svc_register);


/**
 * qmi_svc_unregister() - Unregister the service from a QMI handle
 * @handle: QMI handle from which the service has to be unregistered.
 *
 * return: 0 on success, < 0 on error.
 */
int qmi_svc_unregister(struct qmi_handle *handle)
{
	struct qmi_svc_clnt_conn *conn_h, *temp_conn_h;

	if (!handle || handle->handle_type != QMI_SERVICE_HANDLE)
		return -EINVAL;

	mutex_lock(&handle->handle_lock);
	handle->handle_type = QMI_CLIENT_HANDLE;
	mutex_unlock(&handle->handle_lock);
	/*
	 * Unlocked the handle_lock, because REMOVE_SERVER message will end up
	 * in this handle's control port, which requires holding the same
	 * mutex. Also it is safe to call register_server unlocked.
	 */
	msm_ipc_router_unregister_server(
		(struct msm_ipc_port *)handle->src_port);

	mutex_lock(&handle->handle_lock);
	list_for_each_entry_safe(conn_h, temp_conn_h,
				 &handle->conn_list, list)
		rmv_svc_clnt_conn(conn_h);
	mutex_unlock(&handle->handle_lock);
	return 0;
}
EXPORT_SYMBOL(qmi_svc_unregister);

static int __init qmi_interface_init(void)
{
	qmi_log_init();
	return 0;
}
module_init(qmi_interface_init);

MODULE_DESCRIPTION("MSM QMI Interface");
MODULE_LICENSE("GPL v2");
