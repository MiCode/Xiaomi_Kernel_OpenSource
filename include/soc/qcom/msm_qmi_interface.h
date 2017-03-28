/* Copyright (c) 2012-2015, 2017 The Linux Foundation. All rights reserved.
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

#ifndef _MSM_QMI_INTERFACE_H_
#define _MSM_QMI_INTERFACE_H_

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/socket.h>
#include <linux/gfp.h>
#include <linux/qmi_encdec.h>
#include <linux/workqueue.h>

#define QMI_COMMON_TLV_TYPE 0

enum qmi_event_type {
	QMI_RECV_MSG = 1,
	QMI_SERVER_ARRIVE,
	QMI_SERVER_EXIT,
};

/**
 * struct qmi_handle - QMI Handle Data Structure
 * @handle_hash: Hash Table Node in which this handle is present.
 * @src_port: Pointer to port used for message exchange.
 * @ctl_port: Pointer to port used for out-of-band event exchange.
 * @handle_type: Type of handle(Service/Client).
 * @next_txn_id: Transaction ID of the next outgoing request.
 * @handle_wq: Workqueue to handle any handle-specific events.
 * @handle_lock: Lock to protect access to elements in the handle.
 * @notify_lock: Lock to protect and generate notification atomically.
 * @notify: Function to notify the handle owner of an event.
 * @notify_priv: Private info to be passed during the notifcation.
 * @handle_reset: Flag to hold the reset state of the handle.
 * @reset_waitq: Wait queue to wait for any reset events.
 * @ctl_work: Work to handle the out-of-band events for this handle.
 * @dest_info: Destination to which this handle is connected to.
 * @dest_service_id: service id of the service that client connected to.
 * @txn_list: List of transactions waiting for the response.
 * @ind_cb: Function to notify the handle owner of an indication message.
 * @ind_cb_priv: Private info to be passed during an indication notification.
 * @resume_tx_work: Work to resume the tx when the transport is not busy.
 * @pending_txn_list: List of requests pending tx due to busy transport.
 * @conn_list: List of connections handled by the service.
 * @svc_ops_options: Service specific operations and options.
 */
struct qmi_handle {
	struct hlist_node handle_hash;
	void *src_port;
	void *ctl_port;
	unsigned handle_type;
	uint16_t next_txn_id;
	struct workqueue_struct *handle_wq;
	struct mutex handle_lock;
	spinlock_t notify_lock;
	void (*notify)(struct qmi_handle *handle, enum qmi_event_type event,
			void *notify_priv);
	void *notify_priv;
	int handle_reset;
	wait_queue_head_t reset_waitq;
	struct delayed_work ctl_work;

	/* Client specific elements */
	void *dest_info;
	uint32_t dest_service_id;
	struct list_head txn_list;
	void (*ind_cb)(struct qmi_handle *handle,
			unsigned int msg_id, void *msg,
			unsigned int msg_len, void *ind_cb_priv);
	void *ind_cb_priv;
	struct delayed_work resume_tx_work;
	struct list_head pending_txn_list;

	/* Service specific elements */
	struct list_head conn_list;
	struct qmi_svc_ops_options *svc_ops_options;
};

enum qmi_result_type_v01 {
	/* To force a 32 bit signed enum. Do not change or use*/
	QMI_RESULT_TYPE_MIN_ENUM_VAL_V01 = INT_MIN,
	QMI_RESULT_SUCCESS_V01 = 0,
	QMI_RESULT_FAILURE_V01 = 1,
	QMI_ERR_DISABLED_V01 = 0x45,
	QMI_RESULT_TYPE_MAX_ENUM_VAL_V01 = INT_MAX,
};

enum qmi_error_type_v01 {
	/* To force a 32 bit signed enum. Do not change or use*/
	QMI_ERR_TYPE_MIN_ENUM_VAL_V01 = INT_MIN,
	QMI_ERR_NONE_V01 = 0x0000,
	QMI_ERR_MALFORMED_MSG_V01 = 0x0001,
	QMI_ERR_NO_MEMORY_V01 = 0x0002,
	QMI_ERR_INTERNAL_V01 = 0x0003,
	QMI_ERR_CLIENT_IDS_EXHAUSTED_V01 = 0x0005,
	QMI_ERR_INVALID_ID_V01 = 0x0029,
	QMI_ERR_ENCODING_V01 = 0x003A,
	QMI_ERR_INCOMPATIBLE_STATE_V01 = 0x005A,
	QMI_ERR_NOT_SUPPORTED_V01 = 0x005E,
	QMI_ERR_TYPE_MAX_ENUM_VAL_V01 = INT_MAX,
};

struct qmi_response_type_v01 {
	enum qmi_result_type_v01 result;
	enum qmi_error_type_v01 error;
};

/**
 * qmi_svc_ops_options - Operations and options to be specified when
 *                       a service registers.
 * @version: Version field to identify the ops_options structure.
 * @service_id: Service ID of the service.
 * @service_vers: Version to identify the client-service compatibility.
 * @service_ins: Instance ID registered by the service.
 * @connect_cb: Callback when a new client connects with the service.
 * @disconnect_cb: Callback when the client exits the connection.
 * @req_desc_cb: Callback to get request structure and its descriptor
 *               for a message id.
 * @req_cb: Callback to process the request.
 */
struct qmi_svc_ops_options {
	unsigned version;
	uint32_t service_id;
	uint32_t service_vers;
	uint32_t service_ins;
	int (*connect_cb)(struct qmi_handle *handle,
			  void *conn_handle);
	int (*disconnect_cb)(struct qmi_handle *handle,
			     void *conn_handle);
	int (*req_desc_cb)(unsigned int msg_id,
			   struct msg_desc **req_desc);
	int (*req_cb)(struct qmi_handle *handle,
		      void *conn_handle,
		      void *req_handle,
		      unsigned int msg_id,
		      void *req);
};

#ifdef CONFIG_MSM_QMI_INTERFACE

/* Element info array describing common qmi response structure */
extern struct elem_info qmi_response_type_v01_ei[];
#define get_qmi_response_type_v01_ei() qmi_response_type_v01_ei

/**
 * qmi_handle_create() - Create a QMI handle
 * @notify: Callback to notify events on the handle created.
 * @notify_priv: Private information to be passed along with the notification.
 *
 * @return: Valid QMI handle on success, NULL on error.
 */
struct qmi_handle *qmi_handle_create(
	void (*notify)(struct qmi_handle *handle,
		       enum qmi_event_type event, void *notify_priv),
	void *notify_priv);

/**
 * qmi_handle_destroy() - Destroy the QMI handle
 * @handle: QMI handle to be destroyed.
 *
 * @return: 0 on success, < 0 on error.
 */
int qmi_handle_destroy(struct qmi_handle *handle);

/**
 * qmi_register_ind_cb() - Register the indication callback function
 * @handle: QMI handle with which the function is registered.
 * @ind_cb: Callback function to be registered.
 * @ind_cb_priv: Private data to be passed with the indication callback.
 *
 * @return: 0 on success, < 0 on error.
 */
int qmi_register_ind_cb(struct qmi_handle *handle,
	void (*ind_cb)(struct qmi_handle *handle,
		       unsigned int msg_id, void *msg,
		       unsigned int msg_len, void *ind_cb_priv),
	void *ind_cb_priv);

/**
 * qmi_send_req_wait() - Send a synchronous QMI request
 * @handle: QMI handle through which the QMI request is sent.
 * @request_desc: Structure describing the request data structure.
 * @req: Buffer containing the request data structure.
 * @req_len: Length of the request data structure.
 * @resp_desc: Structure describing the response data structure.
 * @resp: Buffer to hold the response data structure.
 * @resp_len: Length of the response data structure.
 * @timeout_ms: Timeout before a response is received.
 *
 * @return: 0 on success, < 0 on error.
 */
int qmi_send_req_wait(struct qmi_handle *handle,
		      struct msg_desc *req_desc,
		      void *req, unsigned int req_len,
		      struct msg_desc *resp_desc,
		      void *resp, unsigned int resp_len,
		      unsigned long timeout_ms);

/**
 * qmi_send_req_nowait() - Send an asynchronous QMI request
 * @handle: QMI handle through which the QMI request is sent.
 * @request_desc: Structure describing the request data structure.
 * @req: Buffer containing the request data structure.
 * @req_len: Length of the request data structure.
 * @resp_desc: Structure describing the response data structure.
 * @resp: Buffer to hold the response data structure.
 * @resp_len: Length of the response data structure.
 * @resp_cb: Callback function to be invoked when the response arrives.
 * @resp_cb_data: Private information to be passed along with the callback.
 *
 * @return: 0 on success, < 0 on error.
 */
int qmi_send_req_nowait(struct qmi_handle *handle,
			struct msg_desc *req_desc,
			void *req, unsigned int req_len,
			struct msg_desc *resp_desc,
			void *resp, unsigned int resp_len,
			void (*resp_cb)(struct qmi_handle *handle,
					unsigned int msg_id, void *msg,
					void *resp_cb_data,
					int stat),
			void *resp_cb_data);

/**
 * qmi_recv_msg() - Receive the QMI message
 * @handle: Handle for which the QMI message has to be received.
 *
 * @return: 0 on success, < 0 on error.
 */
int qmi_recv_msg(struct qmi_handle *handle);

/**
 * qmi_connect_to_service() - Connect the QMI handle with a QMI service
 * @handle: QMI handle to be connected with the QMI service.
 * @service_id: Service id to identify the QMI service.
 * @service_vers: Version to identify the compatibility.
 * @service_ins: Instance id to identify the instance of the QMI service.
 *
 * @return: 0 on success, < 0 on error.
 */
int qmi_connect_to_service(struct qmi_handle *handle,
			   uint32_t service_id,
			   uint32_t service_vers,
			   uint32_t service_ins);

/**
 * qmi_svc_event_notifier_register() - Register a notifier block to receive
 *                                     events regarding a QMI service
 * @service_id: Service ID to identify the QMI service.
 * @service_vers: Version to identify the compatibility.
 * @service_ins: Instance ID to identify the instance of the QMI service.
 * @nb: Notifier block used to receive the event.
 *
 * @return: 0 if successfully registered, < 0 on error.
 */
int qmi_svc_event_notifier_register(uint32_t service_id,
				    uint32_t service_vers,
				    uint32_t service_ins,
				    struct notifier_block *nb);

/**
 * qmi_svc_event_notifier_unregister() - Unregister service event
 *                                       notifier block
 * @service_id: Service ID to identify the QMI service.
 * @service_vers: Version to identify the compatibility.
 * @service_ins: Instance ID to identify the instance of the QMI service.
 * @nb: Notifier block registered to receive the events.
 *
 * @return: 0 if successfully registered, < 0 on error.
 */
int qmi_svc_event_notifier_unregister(uint32_t service_id,
				      uint32_t service_vers,
				      uint32_t service_ins,
				      struct notifier_block *nb);

/**
 * qmi_svc_register() - Register a QMI service with a QMI handle
 * @handle: QMI handle on which the service has to be registered.
 * @ops_options: Service specific operations and options.
 *
 * @return: 0 if successfully registered, < 0 on error.
 */
int qmi_svc_register(struct qmi_handle *handle,
		     void *ops_options);

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
int qmi_send_resp(struct qmi_handle *handle,
		  void *conn_handle,
		  void *req_handle,
		  struct msg_desc *resp_desc,
		  void *resp,
		  unsigned int resp_len);

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
int qmi_send_resp_from_cb(struct qmi_handle *handle,
			  void *conn_handle,
			  void *req_handle,
			  struct msg_desc *resp_desc,
			  void *resp,
			  unsigned int resp_len);

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
int qmi_send_ind(struct qmi_handle *handle,
		 void *conn_handle,
		 struct msg_desc *ind_desc,
		 void *ind,
		 unsigned int ind_len);

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
int qmi_send_ind_from_cb(struct qmi_handle *handle,
			 void *conn_handle,
			 struct msg_desc *ind_desc,
			 void *ind,
			 unsigned int ind_len);

/**
 * qmi_svc_unregister() - Unregister the service from a QMI handle
 * @handle: QMI handle from which the service has to be unregistered.
 *
 * return: 0 on success, < 0 on error.
 */
int qmi_svc_unregister(struct qmi_handle *handle);

#else

#define get_qmi_response_type_v01_ei() NULL

static inline struct qmi_handle *qmi_handle_create(
	void (*notify)(struct qmi_handle *handle,
		       enum qmi_event_type event, void *notify_priv),
	void *notify_priv)
{
	return NULL;
}

static inline int qmi_handle_destroy(struct qmi_handle *handle)
{
	return -ENODEV;
}

static inline int qmi_register_ind_cb(struct qmi_handle *handle,
	void (*ind_cb)(struct qmi_handle *handle,
		       unsigned int msg_id, void *msg,
		       unsigned int msg_len, void *ind_cb_priv),
	void *ind_cb_priv)
{
	return -ENODEV;
}

static inline int qmi_send_req_wait(struct qmi_handle *handle,
				    struct msg_desc *req_desc,
				    void *req, unsigned int req_len,
				    struct msg_desc *resp_desc,
				    void *resp, unsigned int resp_len,
				    unsigned long timeout_ms)
{
	return -ENODEV;
}

static inline int qmi_send_req_nowait(struct qmi_handle *handle,
				struct msg_desc *req_desc,
				void *req, unsigned int req_len,
				struct msg_desc *resp_desc,
				void *resp, unsigned int resp_len,
				void (*resp_cb)(struct qmi_handle *handle,
						unsigned int msg_id, void *msg,
						void *resp_cb_data),
				void *resp_cb_data)
{
	return -ENODEV;
}

static inline int qmi_recv_msg(struct qmi_handle *handle)
{
	return -ENODEV;
}

static inline int qmi_connect_to_service(struct qmi_handle *handle,
					 uint32_t service_id,
					 uint32_t service_vers,
					 uint32_t service_ins)
{
	return -ENODEV;
}

static inline int qmi_svc_event_notifier_register(uint32_t service_id,
						  uint32_t service_vers,
						  uint32_t service_ins,
						  struct notifier_block *nb)
{
	return -ENODEV;
}

static inline int qmi_svc_event_notifier_unregister(uint32_t service_id,
						    uint32_t service_vers,
						    uint32_t service_ins,
						    struct notifier_block *nb)
{
	return -ENODEV;
}

static inline int qmi_svc_register(struct qmi_handle *handle,
				   void *ops_options)
{
	return -ENODEV;
}

static inline int qmi_send_resp(struct qmi_handle *handle,
				void *conn_handle,
				void *req_handle,
				struct msg_desc *resp_desc,
				void *resp,
				unsigned int resp_len)
{
	return -ENODEV;
}

static inline int qmi_send_resp_from_cb(struct qmi_handle *handle,
					void *conn_handle,
					void *req_handle,
					struct msg_desc *resp_desc,
					void *resp,
					unsigned int resp_len)
{
	return -ENODEV;
}

static inline int qmi_send_ind(struct qmi_handle *handle,
			       void *conn_handle,
			       struct msg_desc *ind_desc,
			       void *ind,
			       unsigned int ind_len)
{
	return -ENODEV;
}

static inline int qmi_send_ind_from_cb(struct qmi_handle *handle,
				       void *conn_handle,
				       struct msg_desc *ind_desc,
				       void *ind,
				       unsigned int ind_len)
{
	return -ENODEV;
}

static inline int qmi_svc_unregister(struct qmi_handle *handle)
{
	return -ENODEV;
}

#endif

#endif
