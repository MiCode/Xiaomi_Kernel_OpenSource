/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 * Copyright (c) 2017, Linaro Ltd.
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
#ifndef __QMI_HELPERS_H__
#define __QMI_HELPERS_H__

#include <linux/qrtr.h>
#include <linux/types.h>

/**
 * qmi_header - wireformat header of QMI messages
 * @type:	type of message
 * @txn_id:	transaction id
 * @msg_id:	message id
 * @msg_len:	length of message payload following header
 */
struct qmi_header {
	uint8_t type;
	uint16_t txn_id;
	uint16_t msg_id;
	uint16_t msg_len;
} __packed;

#define QMI_REQUEST	0
#define QMI_RESPONSE	2
#define QMI_INDICATION	4

#define QMI_COMMON_TLV_TYPE 0

enum qmi_elem_type {
	QMI_EOTI,
	QMI_OPT_FLAG,
	QMI_DATA_LEN,
	QMI_UNSIGNED_1_BYTE,
	QMI_UNSIGNED_2_BYTE,
	QMI_UNSIGNED_4_BYTE,
	QMI_UNSIGNED_8_BYTE,
	QMI_SIGNED_2_BYTE_ENUM,
	QMI_SIGNED_4_BYTE_ENUM,
	QMI_STRUCT,
	QMI_STRING,
};

enum qmi_array_type {
	NO_ARRAY,
	STATIC_ARRAY,
	VAR_LEN_ARRAY,
};

/**
 * struct qmi_elem_info - describes how to encode a single QMI element
 * @data_type:	Data type of this element.
 * @elem_len:	Array length of this element, if an array.
 * @elem_size:	Size of a single instance of this data type.
 * @is_array:	Array type of this element.
 * @tlv_type:	QMI message specific type to identify which element
 *		is present in an incoming message.
 * @offset:	Specifies the offset of the first instance of this
 *		element in the data structure.
 * @ei_array:	Null-terminated array of @qmi_elem_info to describe nested
 *		structures.
 */
struct qmi_elem_info {
	enum qmi_elem_type data_type;
	uint32_t elem_len;
	uint32_t elem_size;
	enum qmi_array_type is_array;
	uint8_t tlv_type;
	uint32_t offset;
	struct qmi_elem_info *ei_array;
};

#define QMI_RESULT_SUCCESS_V01			0
#define QMI_RESULT_FAILURE_V01			1

#define QMI_ERR_NONE_V01			0
#define QMI_ERR_MALFORMED_MSG_V01		1
#define QMI_ERR_NO_MEMORY_V01			2
#define QMI_ERR_INTERNAL_V01			3
#define QMI_ERR_CLIENT_IDS_EXHAUSTED_V01	5
#define QMI_ERR_INVALID_ID_V01			41
#define QMI_ERR_ENCODING_V01			58
#define QMI_ERR_INCOMPATIBLE_STATE_V01		90
#define QMI_ERR_NOT_SUPPORTED_V01		94

/**
 * qmi_response_type_v01 - common response header (decoded)
 * @result:	result of the transaction
 * @error:	error value, when @result is QMI_RESULT_FAILURE_V01
 */
struct qmi_response_type_v01 {
	u32 result;
	u32 error;
};

extern struct qmi_elem_info qmi_response_type_v01_ei[];

/**
 * struct qrtr_service - context to track lookup-results
 * @node:	node of the service
 * @port:	port of the service
 * @cookie:	handle for client's use
 * @list_node:	list_head for house keeping
 */
struct qrtr_service {
	unsigned int node;
	unsigned int port;

	void *cookie;
	struct list_head list_node;
};

struct qrtr_handle;

/**
 * struct qrtr_handle_ops - callbacks from qrtr_handle
 * @new_server:		invoked as a new_server message arrives
 * @del_server:		invoked as a del_server message arrives
 * @net_reset:		invoked as the name server is restarted
 * @msg_handler:	invoked as a non-control message arrives
 */
struct qrtr_handle_ops {
	int (*new_server)(struct qrtr_handle *, struct qrtr_service *);
	void (*del_server)(struct qrtr_handle *, struct qrtr_service *);
	void (*net_reset)(struct qrtr_handle *);
	void (*msg_handler)(struct qrtr_handle *, struct sockaddr_qrtr *,
			    const void *, size_t);
};

/**
 * struct qrtr_handle - qrtr client context
 * @sock:	socket handle
 * @sq:		sockaddr of @sock
 * @work:	work for handling incoming messages
 * @wq:		workqueue to post @work on
 * @recv_buf:	scratch buffer for handling incoming messages
 * @recv_buf_size:	size of @recv_buf
 * @services:	list of services advertised to the client
 * @ops:	reference to callbacks
 */
struct qrtr_handle {
	struct socket *sock;
	struct sockaddr_qrtr sq;

	struct work_struct work;
	struct workqueue_struct *wq;

	void *recv_buf;
	size_t recv_buf_size;

	struct list_head services;

	struct qrtr_handle_ops ops;
};

/**
 * struct qmi_txn - transaction context
 * @qmi:	QMI handle this transaction is associated with
 * @id:		transaction id
 * @completion:	completion object as the transaction receives a response
 * @result:	result code for the completed transaction
 * @ei:		description of the QMI encoded response (optional)
 * @dest:	destination buffer to decode message into (optional)
 */
struct qmi_txn {
	struct qmi_handle *qmi;

	int id;

	struct completion completion;
	int result;

	struct qmi_elem_info *ei;
	void *dest;
};

/**
 * struct qmi_msg_handler - description of QMI message handler
 * @type:	type of message
 * @msg_id:	message id
 * @ei:		description of the QMI encoded message
 * @decoded_size:	size of the decoded object
 * @fn:		function to invoke as the message is decoded
 */
struct qmi_msg_handler {
	unsigned int type;
	unsigned int msg_id;

	struct qmi_elem_info *ei;

	size_t decoded_size;
	void (*fn)(struct qmi_handle *qmi, struct sockaddr_qrtr *sq,
		   struct qmi_txn *txn, const void *decoded);
};

/**
 * struct qmi_handle - QMI client handle
 * @qrtr:	qrtr handle backing the QMI client
 * @txns:	outstanding transactions
 * @txn_lock:	lock for modifications of @txns
 * @handlers:	list of handlers for incoming messages
 */
struct qmi_handle {
	struct qrtr_handle qrtr;

	struct idr txns;
	struct mutex txn_lock;

	struct qmi_msg_handler *handlers;
};

int qrtr_client_init(struct qrtr_handle *qrtr, size_t recv_buf_size,
		     struct qrtr_handle_ops *ops);
void qrtr_client_release(struct qrtr_handle *qrtr);
int qrtr_client_new_lookup(struct qrtr_handle *qrtr,
			   unsigned int service, unsigned int instance);

int qmi_client_init(struct qmi_handle *qmi, size_t max_msg_len,
		    struct qmi_msg_handler *handlers);
void qmi_client_release(struct qmi_handle *qmi);

ssize_t qmi_send_message(struct qmi_handle *qmi,
			 struct sockaddr_qrtr *sq, struct qmi_txn *txn,
			 int type, int msg_id, size_t len,
			 struct qmi_elem_info *ei, const void *c_struct);

void *qmi_encode_message(int type, unsigned int msg_id, size_t *len,
			 unsigned int txn_id, struct qmi_elem_info *ei,
			 const void *c_struct);

int qmi_decode_message(const void *buf, size_t len,
		       struct qmi_elem_info *ei, void *c_struct);

int qmi_txn_init(struct qmi_handle *qmi, struct qmi_txn *txn,
		 struct qmi_elem_info *ei, void *c_struct);
int qmi_txn_wait(struct qmi_txn *txn, unsigned long timeout);
void qmi_txn_cancel(struct qmi_txn *txn);

#endif
