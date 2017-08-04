/*
 * Sample QRTR client driver
 *
 * Copyright (C) 2017 Linaro Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/qrtr.h>
#include <linux/net.h>
#include <linux/completion.h>
#include <linux/idr.h>
#include <linux/string.h>
#include <net/sock.h>
#include <linux/workqueue.h>
#include <linux/soc/qcom/qmi.h>

/**
 * qrtr_client_new_server() - handler of NEW_SERVER control message
 * @qrtr:	qrtr handle
 * @node:	node of the new server
 * @port:	port of the new server
 *
 * Calls the new_server callback to inform the client about a newly registered
 * server matching the currently registered service lookup.
 */
static void qrtr_client_new_server(struct qrtr_handle *qrtr,
				   unsigned int node, unsigned int port)
{
	struct qrtr_handle_ops *ops = &qrtr->ops;
	struct qrtr_service *service;
	int ret;

	if (!ops->new_server)
		return;

	/* Ignore EOF marker */
	if (!node && !port)
		return;

	service = kzalloc(sizeof(*service), GFP_KERNEL);
	if (!service)
		return;

	service->node = node;
	service->port = port;

	ret = ops->new_server(qrtr, service);
	if (ret < 0)
		kfree(service);
	else
		list_add(&service->list_node, &qrtr->services);
}

/**
 * qrtr_client_del_server() - handler of DEL_SERVER control message
 * @qrtr:	qrtr handle
 * @node:	node of the dying server, a value of -1 matches all nodes
 * @port:	port of the dying server, a value of -1 matches all ports
 *
 * Calls the del_server callback for each previously seen server, allowing the
 * client to react to the disappearing server.
 */
static void qrtr_client_del_server(struct qrtr_handle *qrtr,
				   unsigned int node, unsigned int port)
{
	struct qrtr_handle_ops *ops = &qrtr->ops;
	struct qrtr_service *service;
	struct qrtr_service *tmp;

	list_for_each_entry_safe(service, tmp, &qrtr->services, list_node) {
		if (node != -1 && service->node != node)
			continue;
		if (port != -1 && service->port != port)
			continue;

		if (ops->del_server)
			ops->del_server(qrtr, service);

		list_del(&service->list_node);
		kfree(service);
	}
}

static void qrtr_client_ctrl_pkt(struct qrtr_handle *qrtr,
				 const void *buf, size_t len)
{
	const struct qrtr_ctrl_pkt *pkt = buf;

	if (len < sizeof(struct qrtr_ctrl_pkt)) {
		pr_debug("ignoring short control packet\n");
		return;
	}

	switch (le32_to_cpu(pkt->cmd)) {
	case QRTR_TYPE_NEW_SERVER:
		qrtr_client_new_server(qrtr,
				       le32_to_cpu(pkt->server.node),
				       le32_to_cpu(pkt->server.port));
		break;
	case QRTR_TYPE_DEL_SERVER:
		qrtr_client_del_server(qrtr,
				       le32_to_cpu(pkt->server.node),
				       le32_to_cpu(pkt->server.port));
		break;
	}
}

static void qrtr_client_data_ready_work(struct work_struct *work)
{
	struct qrtr_handle *qrtr = container_of(work, struct qrtr_handle, work);
	struct qrtr_handle_ops *ops = &qrtr->ops;
	struct sockaddr_qrtr sq;
	struct msghdr msg = { .msg_name = &sq, .msg_namelen = sizeof(sq) };
	struct kvec iv;
	ssize_t msglen;

	for (;;) {
		iv.iov_base = qrtr->recv_buf;
		iv.iov_len = qrtr->recv_buf_size;

		msglen = kernel_recvmsg(qrtr->sock, &msg, &iv, 1, iv.iov_len,
					MSG_DONTWAIT);
		if (msglen == -EAGAIN)
			break;

		if (msglen == -ENETRESET) {
			if (ops->net_reset)
				ops->net_reset(qrtr);
			break;
		}

		if (msglen < 0) {
			pr_err("qrtr_handle recvmsg failed: %zd\n", msglen);
			break;
		}

		if (sq.sq_node == qrtr->sq.sq_node &&
		    sq.sq_port == QRTR_PORT_CTRL) {
			qrtr_client_ctrl_pkt(qrtr, qrtr->recv_buf, msglen);
		} else {
			if (!ops->msg_handler)
				continue;

			ops->msg_handler(qrtr, &sq, qrtr->recv_buf, msglen);
		}
	}
}

static void qrtr_client_schedule_worker(struct sock *sk)
{
	struct qrtr_handle *qrtr = sk->sk_user_data;

	/*
	 * This will be NULL if we receive data while being in
	 * qrtr_client_release()
	 */
	if (!qrtr)
		return;

	queue_work(qrtr->wq, &qrtr->work);
}

/**
 * qrtr_client_init() - initialize a qrtr_handle
 * @qrtr:		reference to qrtr handle
 * @recv_buf_size:	maximum size of received messages
 * @ops:		qrtr_handle_ops struct with callbacks
 *
 * Returns 0 on success, negative errno on failure.
 *
 * This function initializes @qrtr to allow sending, receiving and handling
 * QRTR control messages and data packets.
 */
int qrtr_client_init(struct qrtr_handle *qrtr, size_t recv_buf_size,
		     struct qrtr_handle_ops *ops)
{
	struct sockaddr_qrtr sq;
	struct socket *sock;
	int ret;
	int sl = sizeof(sq);

	if (recv_buf_size < sizeof(struct qrtr_ctrl_pkt))
		recv_buf_size = sizeof(struct qrtr_ctrl_pkt);

	INIT_LIST_HEAD(&qrtr->services);
	INIT_WORK(&qrtr->work, qrtr_client_data_ready_work);

	qrtr->wq = alloc_workqueue("qrtr_handle", WQ_UNBOUND, 1);
	if (!qrtr->wq)
		return -ENOMEM;

	qrtr->ops = *ops;
	qrtr->recv_buf_size = recv_buf_size;
	qrtr->recv_buf = kzalloc(recv_buf_size, GFP_KERNEL);
	if (!qrtr->recv_buf)
		return -ENOMEM;

	ret = sock_create_kern(&init_net, AF_QIPCRTR, SOCK_DGRAM,
			       PF_QIPCRTR, &sock);
	if (ret < 0)
		goto err_free_recv_buf;

	ret = kernel_getsockname(sock, (struct sockaddr *)&sq, &sl);
	if (ret < 0)
		goto err_release_sock;

	qrtr->sock = sock;
	qrtr->sq = sq;

	sock->sk->sk_user_data = qrtr;
	sock->sk->sk_data_ready = qrtr_client_schedule_worker;
	sock->sk->sk_error_report = qrtr_client_schedule_worker;

	return 0;
err_release_sock:
	sock_release(sock);

err_free_recv_buf:
	kfree(qrtr->recv_buf);

	return ret;
}
EXPORT_SYMBOL(qrtr_client_init);

/**
 * qrtr_client_release() - tear down a qrtr_handle
 * @qrtr:	qrtr handle
 *
 * This will tear down the qrtr handle, stop handling of any incoming messages
 * and release the underlying socket.
 */
void qrtr_client_release(struct qrtr_handle *qrtr)
{
	struct socket *sock = qrtr->sock;

	sock->sk->sk_user_data = NULL;
	cancel_work_sync(&qrtr->work);

	kfree(qrtr->recv_buf);

	qrtr_client_del_server(qrtr, -1, -1);

	sock_release(sock);
	qrtr->sock = NULL;

	destroy_workqueue(qrtr->wq);
}
EXPORT_SYMBOL(qrtr_client_release);

/**
 * qrtr_client_new_lookup() - register a new lookup with the name service
 * @qrtr:	qrtr handle
 * @service:	service id of the request
 * @instance:	instance id of the request
 *
 * Returns 0 on success, negative errno on failure.
 *
 * Registering a lookup query with the name server will cause the name server
 * to send NEW_SERVER and DEL_SERVER control messages to this socket as
 * matching services are registered.
 */
int qrtr_client_new_lookup(struct qrtr_handle *qrtr,
			   unsigned int service, unsigned int instance)
{
	struct qrtr_ctrl_pkt pkt;
	struct sockaddr_qrtr sq;
	struct msghdr msg = {0};
	struct kvec iv = { &pkt, sizeof(pkt) };
	int ret;

	memset(&pkt, 0, sizeof(pkt));
	pkt.cmd = cpu_to_le32(QRTR_TYPE_NEW_LOOKUP);
	pkt.server.service = cpu_to_le32(service);
	pkt.server.instance = cpu_to_le32(instance);

	sq.sq_family = qrtr->sq.sq_family;
	sq.sq_node = qrtr->sq.sq_node;
	sq.sq_port = QRTR_PORT_CTRL;

	msg.msg_name = &sq;
	msg.msg_namelen = sizeof(sq);

	ret = kernel_sendmsg(qrtr->sock, &msg, &iv, 1, sizeof(pkt));
	if (ret < 0)
		pr_err("failed to send lookup registration: %d\n", ret);

	return ret < 0 ? ret : 0;
}
EXPORT_SYMBOL(qrtr_client_new_lookup);

/**
 * qmi_txn_init() - allocate transaction id within the given QMI handle
 * @qmi:	QMI handle
 * @txn:	transaction context
 * @ei:		description of how to decode a matching response (optional)
 * @c_struct:	pointer to the object to decode the response into (optional)
 *
 * Returns transaction id on success, negative errno on failure.
 *
 * This allocates a transaction id within the QMI handle. If @ei and @c_struct
 * are specified any responses to this transaction will be decoded as described
 * by @ei into @c_struct.
 */
int qmi_txn_init(struct qmi_handle *qmi, struct qmi_txn *txn,
		 struct qmi_elem_info *ei, void *c_struct)
{
	int ret;

	memset(txn, 0, sizeof(*txn));

	init_completion(&txn->completion);
	txn->qmi = qmi;
	txn->ei = ei;
	txn->dest = c_struct;

	mutex_lock(&qmi->txn_lock);
	ret = idr_alloc_cyclic(&qmi->txns, txn, 0, INT_MAX, GFP_KERNEL);
	if (ret < 0)
		pr_err("failed to allocate transaction id\n");

	txn->id = ret;
	mutex_unlock(&qmi->txn_lock);

	return ret;
}
EXPORT_SYMBOL(qmi_txn_init);

/**
 * qmi_txn_wait() - wait for a response on a transaction
 * @txn:	transaction handle
 * @timeout:	timeout, in jiffies
 *
 * Returns the transaction response on success, negative errno on failure.
 *
 * If the transaction is decoded by the means of @ei and @c_struct the return
 * value will be the returned value of qmi_decode_message(), otherwise it's up
 * to the specified message handler to fill out the result.
 */
int qmi_txn_wait(struct qmi_txn *txn, unsigned long timeout)
{
	struct qmi_handle *qmi = txn->qmi;
	int ret;

	ret = wait_for_completion_interruptible_timeout(&txn->completion,
							timeout);

	mutex_lock(&qmi->txn_lock);
	idr_remove(&qmi->txns, txn->id);
	mutex_unlock(&qmi->txn_lock);

	if (ret < 0)
		return ret;
	else if (ret == 0)
		return -ETIMEDOUT;
	else
		return txn->result;
}
EXPORT_SYMBOL(qmi_txn_wait);

/**
 * qmi_txn_cancel() - cancel an ongoing transaction
 * @txn:	transaction id
 */
void qmi_txn_cancel(struct qmi_txn *txn)
{
	struct qmi_handle *qmi = txn->qmi;

	mutex_lock(&qmi->txn_lock);
	idr_remove(&qmi->txns, txn->id);
	mutex_unlock(&qmi->txn_lock);
}
EXPORT_SYMBOL(qmi_txn_cancel);

static void qmi_client_handle_data(struct qrtr_handle *qrtr,
				   struct sockaddr_qrtr *sq,
				   const void *buf, size_t len)
{
	const struct qmi_header *hdr = buf;
	struct qmi_handle *qmi = container_of(qrtr, struct qmi_handle, qrtr);
	struct qmi_msg_handler *handler;
	struct qmi_txn *txn = NULL;
	void *dest;
	int ret;

	if (len < sizeof(*hdr)) {
		pr_err("ignoring short QMI packet\n");
		return;
	}

	mutex_lock(&qmi->txn_lock);

	/* If this is a response, find the matching transaction handle */
	if (hdr->type == QMI_RESPONSE)
		txn = idr_find(&qmi->txns, hdr->txn_id);

	if (txn && txn->dest && txn->ei) {
		ret = qmi_decode_message(buf, len, txn->ei, txn->dest);
		if (ret < 0)
			pr_err("failed to decode incoming message\n");

		txn->result = ret;
		complete(&txn->completion);
	} else if (qmi->handlers) {
		for (handler = qmi->handlers; handler->fn; handler++) {
			if (handler->type == hdr->type &&
			    handler->msg_id == hdr->msg_id)
				break;
		}

		if (!handler->fn)
			goto out;

		dest = kzalloc(handler->decoded_size, GFP_KERNEL);
		if (!dest)
			goto out;

		ret = qmi_decode_message(buf, len, handler->ei, dest);
		if (ret < 0)
			pr_err("failed to decode incoming message\n");
		else
			handler->fn(qmi, sq, txn, dest);

		kfree(dest);
	}

out:
	mutex_unlock(&qmi->txn_lock);
}

/**
 * qmi_client_init() - initialize a QMI client handle
 * @qmi:	QMI handle to initialize
 * @max_msg_len: maximum size of incoming message
 * @handlers:	NULL-terminated list of QMI message handlers
 *
 * Returns 0 on success, negative errno on failure.
 *
 * This initializes the QMI client handle to allow sending and receiving QMI
 * messages. As messages are received the appropriate handler will be invoked.
 */
int qmi_client_init(struct qmi_handle *qmi, size_t max_msg_len,
		    struct qmi_msg_handler *handlers)
{
	struct qrtr_handle_ops ops = { 0 };
	int ret;

	mutex_init(&qmi->txn_lock);
	idr_init(&qmi->txns);

	ops.msg_handler = qmi_client_handle_data;

	ret = qrtr_client_init(&qmi->qrtr, max_msg_len, &ops);
	if (ret < 0)
		return ret;

	qmi->handlers = handlers;

	return 0;
}
EXPORT_SYMBOL(qmi_client_init);

/**
 * qrtr_client_release() - release the QMI client handle
 * @qmi:	QMI client handle
 *
 * This closes the underlying socket and stops any handling of QMI messages.
 */
void qmi_client_release(struct qmi_handle *qmi)
{
	qrtr_client_release(&qmi->qrtr);

	idr_destroy(&qmi->txns);

}
EXPORT_SYMBOL(qmi_client_release);

/**
 * qmi_send_message() - send a QMI message
 * @qmi:	QMI client handle
 * @sq:		destination sockaddr
 * @txn:	transaction object to use for the message
 * @type:	type of message to send
 * @msg_id:	message id
 * @len:	max length of the QMI message
 * @ei:		QMI message description
 * @c_struct:	object to be encoded
 *
 * Returns 0 on success, negative errno on failure.
 *
 * This function encodes @c_struct using @ei into a message of type @type,
 * with @msg_id and @txn into a buffer of maximum size @len, and sends this to
 * @sq.
 */
ssize_t qmi_send_message(struct qmi_handle *qmi,
			 struct sockaddr_qrtr *sq, struct qmi_txn *txn,
			 int type, int msg_id, size_t len,
			 struct qmi_elem_info *ei, const void *c_struct)
{
	struct qrtr_handle *qrtr = &qmi->qrtr;
	struct msghdr msghdr = {0};
	struct kvec iv;
	void *msg;
	int ret;

	msg = qmi_encode_message(type,
				 msg_id, &len,
				 txn->id, ei,
				 c_struct);
	if (IS_ERR(msg))
		return PTR_ERR(msg);

	iv.iov_base = msg;
	iv.iov_len = len;

	if (sq) {
		msghdr.msg_name = sq;
		msghdr.msg_namelen = sizeof(*sq);
	}

	ret = kernel_sendmsg(qrtr->sock, &msghdr, &iv, 1, len);
	if (ret < 0)
		pr_err("failed to send QMI message\n");

	kfree(msg);

	return ret < 0 ? ret : 0;
}
EXPORT_SYMBOL(qmi_send_message);
