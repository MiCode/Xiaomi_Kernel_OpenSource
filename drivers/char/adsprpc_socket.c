// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/uaccess.h>
#include <linux/qrtr.h>
#include <linux/mutex.h>
#include <net/sock.h>
#include <trace/events/fastrpc.h>
#include "adsprpc_shared.h"

// Registered QRTR service ID
#define FASTRPC_REMOTE_SERVER_SERVICE_ID 5012

// Number of remote domains
#define REMOTE_DOMAINS (2)

/*
 * Fastrpc remote server instance ID bit-map:
 *
 * bits 0-1   : channel ID
 * bits 2-7   : reserved
 * bits 8-9   : remote domains (SECURE_PD, GUEST_OS)
 * bits 10-31 : reserved
 */
#define REMOTE_DOMAIN_INSTANCE_INDEX (8)
#define GET_SERVER_INSTANCE(remote_domain, cid) \
	((remote_domain << REMOTE_DOMAIN_INSTANCE_INDEX) | cid)
#define GET_CID_FROM_SERVER_INSTANCE(remote_server_instance) \
	(remote_server_instance & 0x3)

// Maximun received fastprc packet size
#define FASTRPC_SOCKET_RECV_SIZE sizeof(union rsp)

union rsp {
	struct smq_invoke_rsp rsp;
	struct smq_invoke_rspv2 rsp2;
	struct smq_notif_rspv3 rsp3;
};

enum fastrpc_remote_domains_id {
	SECURE_PD = 0,
	GUEST_OS = 1,
};

struct fastrpc_socket {
	struct socket *sock;                   // Socket used to communicate with remote domain
	struct sockaddr_qrtr local_sock_addr;  // Local socket address on kernel side
	struct sockaddr_qrtr remote_sock_addr; // Remote socket address on remote domain side
	struct mutex socket_mutex;             // Mutex for socket synchronization
	void *recv_buf;                        // Received packet buffer
};

struct frpc_transport_session_control {
	struct fastrpc_socket frpc_socket;     // Fastrpc socket data structure
	uint32_t remote_server_instance;       // Unique remote server instance ID
	bool remote_domain_available;          // Flag to indicate if remote domain is enabled
	bool remote_server_online;             // Flag to indicate remote server status
};

/**
 * glist_session_ctrl
 * Static list containing socket session information for all remote domains.
 * Update session flag remote_domain_available whenever a remote domain will be using
 * kernel sockets.
 */
static struct frpc_transport_session_control glist_session_ctrl[NUM_CHANNELS][REMOTE_DOMAINS] = {
	[CDSP_DOMAIN_ID][SECURE_PD].remote_domain_available = true
};

/**
 * verify_transport_device()
 * @cid: Channel ID.
 * @trusted_vm: Flag to indicate whether session is for secure PD or guest OS.
 *
 * Obtain remote session information given channel ID and trusted_vm
 * and verify that socket has been created and remote server is up.
 *
 * Return: 0 on success or negative errno value on failure.
 */
inline int verify_transport_device(int cid, bool trusted_vm)
{
	int remote_domain, err = 0;
	struct frpc_transport_session_control *session_control = NULL;

	remote_domain = (trusted_vm) ? SECURE_PD : GUEST_OS;
	VERIFY(err, remote_domain < REMOTE_DOMAINS);
	if (err) {
		err = -ECHRNG;
		goto bail;
	}

	session_control = &glist_session_ctrl[cid][remote_domain];
	VERIFY(err, session_control->remote_domain_available);
	if (err) {
		err = -ECHRNG;
		goto bail;
	}

	mutex_lock(&session_control->frpc_socket.socket_mutex);
	VERIFY(err, session_control->frpc_socket.sock);
	VERIFY(err, session_control->remote_server_online);
	if (err) {
		err = -EPIPE;
		mutex_unlock(&session_control->frpc_socket.socket_mutex);
		goto bail;
	}
	mutex_unlock(&session_control->frpc_socket.socket_mutex);

bail:
	return err;
}

static void fastrpc_recv_new_server(struct frpc_transport_session_control *session_control,
				unsigned int service, unsigned int instance,
				unsigned int node, unsigned int port)
{
	uint32_t remote_server_instance = session_control->remote_server_instance;

	/* Ignore EOF marker */
	if (!node && !port)
		return;

	if (service != FASTRPC_REMOTE_SERVER_SERVICE_ID ||
		instance != remote_server_instance)
		return;

	mutex_lock(&session_control->frpc_socket.socket_mutex);
	session_control->frpc_socket.remote_sock_addr.sq_family = AF_QIPCRTR;
	session_control->frpc_socket.remote_sock_addr.sq_node = node;
	session_control->frpc_socket.remote_sock_addr.sq_port = port;
	session_control->remote_server_online = true;
	mutex_unlock(&session_control->frpc_socket.socket_mutex);
	ADSPRPC_INFO("Remote server is up: remote ID (0x%x)", remote_server_instance);
}

static void fastrpc_recv_del_server(struct frpc_transport_session_control *session_control,
				unsigned int node, unsigned int port)
{
	uint32_t remote_server_instance = session_control->remote_server_instance;

	/* Ignore EOF marker */
	if (!node && !port)
		return;

	if (node != session_control->frpc_socket.remote_sock_addr.sq_node ||
		port != session_control->frpc_socket.remote_sock_addr.sq_port)
		return;

	mutex_lock(&session_control->frpc_socket.socket_mutex);
	session_control->frpc_socket.remote_sock_addr.sq_node = 0;
	session_control->frpc_socket.remote_sock_addr.sq_port = 0;
	session_control->remote_server_online = false;
	mutex_unlock(&session_control->frpc_socket.socket_mutex);
	ADSPRPC_WARN("Remote server is down: remote ID (0x%x)", remote_server_instance);
}

/**
 * fastrpc_recv_ctrl_pkt()
 * @session_control: Data structure that contains information related to socket and
 *                   remote server availability.
 * @buf: Control packet.
 * @len: Control packet length.
 *
 * Handle control packet status notifications from remote domain.
 */
static void fastrpc_recv_ctrl_pkt(struct frpc_transport_session_control *session_control,
					const void *buf, size_t len)
{
	const struct qrtr_ctrl_pkt *pkt = buf;

	if (len < sizeof(struct qrtr_ctrl_pkt)) {
		ADSPRPC_WARN("Ignoring short control packet (%d bytes)", len);
		return;
	}

	switch (le32_to_cpu(pkt->cmd)) {
	case QRTR_TYPE_NEW_SERVER:
		fastrpc_recv_new_server(session_control,
				    le32_to_cpu(pkt->server.service),
				    le32_to_cpu(pkt->server.instance),
				    le32_to_cpu(pkt->server.node),
				    le32_to_cpu(pkt->server.port));
		break;
	case QRTR_TYPE_DEL_SERVER:
		fastrpc_recv_del_server(session_control,
				    le32_to_cpu(pkt->server.node),
				    le32_to_cpu(pkt->server.port));
		break;
	}
}

/**
 * fastrpc_socket_callback()
 * @sk: Sock data structure with information related to the callback response.
 *
 * Callback function to receive responses from socket layer.
 * We expect to receive control packets with remote domain status notifications or
 * RPC data packets from remote domain.
 */
static void fastrpc_socket_callback(struct sock *sk)
{
	int err = 0, cid = 0;
	struct kvec msg = {0};
	struct sockaddr_qrtr remote_sock_addr = {0};
	struct msghdr remote_server = {0};
	struct frpc_transport_session_control *session_control = NULL;

	remote_server.msg_name = &remote_sock_addr;
	remote_server.msg_namelen = sizeof(remote_sock_addr);
	trace_fastrpc_msg("socket_callback: begin");
	VERIFY(err, sk);
	if (err) {
		err = -EFAULT;
		goto bail;
	}

	rcu_read_lock();
	session_control = rcu_dereference_sk_user_data(sk);
	rcu_read_unlock();
	VERIFY(err, session_control);
	if (err) {
		err = -EFAULT;
		goto bail;
	}

	msg.iov_base = session_control->frpc_socket.recv_buf;
	msg.iov_len = FASTRPC_SOCKET_RECV_SIZE;
	err = kernel_recvmsg(session_control->frpc_socket.sock, &remote_server, &msg, 1,
				msg.iov_len, MSG_DONTWAIT);
	if (err < 0)
		goto bail;

	if (remote_sock_addr.sq_node == session_control->frpc_socket.local_sock_addr.sq_node &&
		remote_sock_addr.sq_port == QRTR_PORT_CTRL) {
		fastrpc_recv_ctrl_pkt(session_control, session_control->frpc_socket.recv_buf,
					FASTRPC_SOCKET_RECV_SIZE);
	} else {
		cid = GET_CID_FROM_SERVER_INSTANCE(session_control->remote_server_instance);
		VERIFY(err, VALID_FASTRPC_CID(cid));
		if (err) {
			err = -ECHRNG;
			goto bail;
		}
		fastrpc_handle_rpc_response(msg.iov_base, msg.iov_len, cid);
	}
bail:
	if (err < 0) {
		ADSPRPC_ERR(
			"invalid response data %pK, len %d from remote ID (0x%x) err %d\n",
			msg.iov_base, msg.iov_len, session_control->remote_server_instance, err);
	}

	trace_fastrpc_msg("socket_callback: end");
}

/**
 * fastrpc_transport_send()
 * @cid: Channel ID.
 * @rpc_msg: RPC message to send to remote domain.
 * @rpc_msg_size: RPC message size.
 * @trusted_vm: Flag to indicate whether to send message to secure PD or guest OS.
 *
 * Send RPC message to remote domain. Depending on trusted_vm flag message will be
 * sent to secure PD or guest OS on remote subsystem.
 * Depending on the channel ID and remote domain, a corresponding socket is retrieved
 * from glist_session_ctrl and is use to send RPC message.
 *
 * Return: 0 on success or negative errno value on failure.
 */
int fastrpc_transport_send(int cid, void *rpc_msg, uint32_t rpc_msg_size, bool trusted_vm)
{
	int err = 0, remote_domain;
	struct fastrpc_socket *frpc_socket = NULL;
	struct frpc_transport_session_control *session_control = NULL;
	struct msghdr remote_server = {0};
	struct kvec msg = {0};

	remote_domain = (trusted_vm) ? SECURE_PD : GUEST_OS;
	VERIFY(err, remote_domain < REMOTE_DOMAINS);
	if (err) {
		err = -ECHRNG;
		goto bail;
	}
	session_control = &glist_session_ctrl[cid][remote_domain];
	VERIFY(err, session_control->remote_domain_available);
	if (err) {
		err = -ECHRNG;
		goto bail;
	}
	frpc_socket = &session_control->frpc_socket;
	remote_server.msg_name = &frpc_socket->remote_sock_addr;
	remote_server.msg_namelen = sizeof(frpc_socket->remote_sock_addr);

	msg.iov_base = rpc_msg;
	msg.iov_len = rpc_msg_size;

	mutex_lock(&frpc_socket->socket_mutex);
	VERIFY(err, frpc_socket->sock);
	VERIFY(err, session_control->remote_server_online);
	if (err) {
		err = -EPIPE;
		mutex_unlock(&frpc_socket->socket_mutex);
		goto bail;
	}
	err = kernel_sendmsg(frpc_socket->sock, &remote_server, &msg, 1, msg.iov_len);
	mutex_unlock(&frpc_socket->socket_mutex);
bail:
	return err;
}

/**
 * create_socket()
 * @session_control: Data structure that contains information related to socket and
 *                   remote server availability.
 *
 * Initializes and creates a kernel socket.
 *
 * Return: pointer to a socket on success or negative errno value on failure.
 */
static struct socket *create_socket(struct frpc_transport_session_control *session_control)
{
	int err = 0;
	struct socket *sock = NULL;
	struct fastrpc_socket *frpc_socket = NULL;

	err = sock_create_kern(&init_net, AF_QIPCRTR, SOCK_DGRAM,
				   PF_QIPCRTR, &sock);
	if (err < 0) {
		ADSPRPC_ERR("sock_create_kern failed with err %d\n", err);
		goto bail;
	}
	frpc_socket = &session_control->frpc_socket;
	err = kernel_getsockname(sock, (struct sockaddr *)&frpc_socket->local_sock_addr);
	if (err < 0) {
		sock_release(sock);
		ADSPRPC_ERR("kernel_getsockname failed with err %d\n", err);
		goto bail;
	}
	rcu_assign_sk_user_data(sock->sk, session_control);
	sock->sk->sk_data_ready = fastrpc_socket_callback;
	sock->sk->sk_error_report = fastrpc_socket_callback;
bail:
	if (err < 0)
		return ERR_PTR(err);
	else
		return sock;
}

/**
 * register_remote_server_notifications()
 * @frpc_socket: Socket to send message to register for remote service notifications.
 * @remote_server_instance: ID to uniquely identify remote server
 *
 * Register socket to receive status notifications from remote service
 * using remote service ID FASTRPC_REMOTE_SERVER_SERVICE_ID and instance ID.
 *
 * Return: 0 on success or negative errno value on failure.
 */
static int register_remote_server_notifications(struct fastrpc_socket *frpc_socket,
				uint32_t remote_server_instance)
{
	struct qrtr_ctrl_pkt pkt = {0};
	struct sockaddr_qrtr sq = {0};
	struct msghdr remote_server = {0};
	struct kvec msg = { &pkt, sizeof(pkt) };
	int err = 0;

	memset(&pkt, 0, sizeof(pkt));
	pkt.cmd = cpu_to_le32(QRTR_TYPE_NEW_LOOKUP);
	pkt.server.service = cpu_to_le32(FASTRPC_REMOTE_SERVER_SERVICE_ID);
	pkt.server.instance = cpu_to_le32(remote_server_instance);

	sq.sq_family = frpc_socket->local_sock_addr.sq_family;
	sq.sq_node = frpc_socket->local_sock_addr.sq_node;
	sq.sq_port = QRTR_PORT_CTRL;

	remote_server.msg_name = &sq;
	remote_server.msg_namelen = sizeof(sq);

	err = kernel_sendmsg(frpc_socket->sock, &remote_server, &msg, 1, sizeof(pkt));
	if (err < 0)
		goto bail;

bail:
	if (err < 0)
		ADSPRPC_ERR("failed to send lookup registration: %d\n", err);

	return err;
}

inline void fastrpc_transport_session_init(int cid, char *subsys)
{
}

inline void fastrpc_transport_session_deinit(int cid)
{
}

int fastrpc_wait_for_transport_interrupt(int cid, unsigned int flags)
{
	return 0;
}

void fastrpc_rproc_trace_events(const char *name, const char *event,
				const char *subevent)
{
}

/**
 * fastrpc_transport_init() - Initialize sockets for fastrpc driver.
 *
 * Initialize and create all sockets that are enabled from all channels
 * and remote domains.
 * Traverse array glist_session_ctrl and initialize session if remote
 * domain is enabled.
 *
 * Return: 0 on success or negative errno value on failure.
 */
int fastrpc_transport_init(void)
{
	int err = 0, cid = 0, ii = 0;
	struct socket *sock = NULL;
	struct fastrpc_socket *frpc_socket = NULL;
	struct frpc_transport_session_control *session_control = NULL;

	for (cid = 0; cid < NUM_CHANNELS; cid++) {
		for (ii = 0; ii < REMOTE_DOMAINS; ii++) {
			session_control = &glist_session_ctrl[cid][ii];
			if (!session_control->remote_domain_available)
				continue;

			session_control->remote_server_online = false;
			frpc_socket = &session_control->frpc_socket;
			mutex_init(&frpc_socket->socket_mutex);

			sock = create_socket(session_control);
			if (IS_ERR_OR_NULL(sock)) {
				err = PTR_ERR(sock);
				goto bail;
			}

			frpc_socket->sock = sock;
			frpc_socket->recv_buf = kzalloc(FASTRPC_SOCKET_RECV_SIZE, GFP_KERNEL);
			if (!frpc_socket->recv_buf) {
				err = -ENOMEM;
				goto bail;
			}
			session_control->remote_server_instance = GET_SERVER_INSTANCE(ii, cid);
			err = register_remote_server_notifications(frpc_socket,
							session_control->remote_server_instance);
			if (err < 0)
				goto bail;
		}
	}
	err = 0;
bail:
	if (err)
		ADSPRPC_ERR("fastrpc_socket_init failed with err %d\n", err);
	return err;
}

/**
 * fastrpc_transport_deinit() - Deinitialize sockets for fastrpc driver.
 *
 * Deinitialize and release all sockets that are enabled from all channels
 * and remote domains.
 * Traverse array glist_session_ctrl and deinitialize session if remote
 * domain is enabled.
 */
void fastrpc_transport_deinit(void)
{
	int ii = 0;
	struct fastrpc_socket *frpc_socket = NULL;
	struct frpc_transport_session_control *session_control = NULL;
	int cid = -1;

	for (cid = 0; cid < NUM_CHANNELS; cid++) {
		for (ii = 0; ii < REMOTE_DOMAINS; ii++) {
			session_control = &glist_session_ctrl[cid][ii];
			frpc_socket = &session_control->frpc_socket;
			if (!session_control->remote_domain_available)
				continue;

			if (frpc_socket->sock)
				sock_release(frpc_socket->sock);

			kfree(frpc_socket->recv_buf);
			frpc_socket->recv_buf = NULL;
			frpc_socket->sock = NULL;
			mutex_destroy(&frpc_socket->socket_mutex);
		}
	}
}
