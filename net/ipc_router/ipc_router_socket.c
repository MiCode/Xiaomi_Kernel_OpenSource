/* Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/fcntl.h>
#include <linux/gfp.h>
#include <linux/msm_ipc.h>
#include <linux/sched.h>
#include <linux/thread_info.h>
#include <linux/qmi_encdec.h>
#include <linux/slab.h>
#include <linux/kmemleak.h>
#include <linux/ipc_logging.h>
#include <linux/string.h>
#include <linux/atomic.h>
#include <linux/ipc_router.h>

#include <net/sock.h>

#include "ipc_router_private.h"
#include "ipc_router_security.h"

#define msm_ipc_sk(sk) ((struct msm_ipc_sock *)(sk))
#define msm_ipc_sk_port(sk) ((struct msm_ipc_port *)(msm_ipc_sk(sk)->port))

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

static int sockets_enabled;
static struct proto msm_ipc_proto;
static const struct proto_ops msm_ipc_proto_ops;
static RAW_NOTIFIER_HEAD(ipcrtr_af_init_chain);
static DEFINE_MUTEX(ipcrtr_af_init_lock);

static struct sk_buff_head *msm_ipc_router_build_msg(unsigned int num_sect,
					  struct iovec const *msg_sect,
					  size_t total_len)
{
	struct sk_buff_head *msg_head;
	struct sk_buff *msg;
	int i, copied, first = 1;
	int data_size = 0, request_size, offset;
	void *data;
	int last = 0;
	int align_size;

	for (i = 0; i < num_sect; i++)
		data_size += msg_sect[i].iov_len;

	if (!data_size)
		return NULL;
	align_size = ALIGN_SIZE(data_size);

	msg_head = kmalloc(sizeof(struct sk_buff_head), GFP_KERNEL);
	if (!msg_head) {
		IPC_RTR_ERR("%s: cannot allocate skb_head\n", __func__);
		return NULL;
	}
	skb_queue_head_init(msg_head);

	for (copied = 1, i = 0; copied && (i < num_sect); i++) {
		data_size = msg_sect[i].iov_len;
		offset = 0;
		if (i == (num_sect - 1))
			last = 1;
		while (offset != msg_sect[i].iov_len) {
			request_size = data_size;
			if (first)
				request_size += IPC_ROUTER_HDR_SIZE;
			if (last)
				request_size += align_size;

			msg = alloc_skb(request_size, GFP_KERNEL);
			if (!msg) {
				if (request_size <= (PAGE_SIZE/2)) {
					IPC_RTR_ERR(
					"%s: cannot allocated skb\n",
					__func__);
					goto msg_build_failure;
				}
				data_size = data_size / 2;
				last = 0;
				continue;
			}

			if (first) {
				skb_reserve(msg, IPC_ROUTER_HDR_SIZE);
				first = 0;
			}

			data = skb_put(msg, data_size);
			copied = !copy_from_user(msg->data,
					msg_sect[i].iov_base + offset,
					data_size);
			if (!copied) {
				IPC_RTR_ERR("%s: copy_from_user failed\n",
					__func__);
				kfree_skb(msg);
				goto msg_build_failure;
			}
			skb_queue_tail(msg_head, msg);
			offset += data_size;
			data_size = msg_sect[i].iov_len - offset;
			if (i == (num_sect - 1))
				last = 1;
		}
	}
	return msg_head;

msg_build_failure:
	while (!skb_queue_empty(msg_head)) {
		msg = skb_dequeue(msg_head);
		kfree_skb(msg);
	}
	kfree(msg_head);
	return NULL;
}

static int msm_ipc_router_extract_msg(struct msghdr *m,
				      struct rr_packet *pkt)
{
	struct sockaddr_msm_ipc *addr;
	struct rr_header_v1 *hdr;
	struct sk_buff *temp;
	union rr_control_msg *ctl_msg;
	int offset = 0, data_len = 0, copy_len;

	if (!m || !pkt) {
		IPC_RTR_ERR("%s: Invalid pointers passed\n", __func__);
		return -EINVAL;
	}
	addr = (struct sockaddr_msm_ipc *)m->msg_name;

	hdr = &(pkt->hdr);
	if (addr && (hdr->type == IPC_ROUTER_CTRL_CMD_RESUME_TX)) {
		temp = skb_peek(pkt->pkt_fragment_q);
		if (!temp || !temp->data) {
			IPC_RTR_ERR("%s: Invalid skb\n", __func__);
			return -EINVAL;
		}
		ctl_msg = (union rr_control_msg *)(temp->data);
		addr->family = AF_MSM_IPC;
		addr->address.addrtype = MSM_IPC_ADDR_ID;
		addr->address.addr.port_addr.node_id = ctl_msg->cli.node_id;
		addr->address.addr.port_addr.port_id = ctl_msg->cli.port_id;
		m->msg_namelen = sizeof(struct sockaddr_msm_ipc);
		return offset;
	}
	if (addr && (hdr->type == IPC_ROUTER_CTRL_CMD_DATA)) {
		addr->family = AF_MSM_IPC;
		addr->address.addrtype = MSM_IPC_ADDR_ID;
		addr->address.addr.port_addr.node_id = hdr->src_node_id;
		addr->address.addr.port_addr.port_id = hdr->src_port_id;
		m->msg_namelen = sizeof(struct sockaddr_msm_ipc);
	}

	data_len = hdr->size;
	skb_queue_walk(pkt->pkt_fragment_q, temp) {
		copy_len = data_len < temp->len ? data_len : temp->len;
		if (copy_to_user(m->msg_iov->iov_base + offset, temp->data,
				 copy_len)) {
			IPC_RTR_ERR("%s: Copy to user failed\n", __func__);
			return -EFAULT;
		}
		offset += copy_len;
		data_len -= copy_len;
	}
	return offset;
}

static int msm_ipc_router_create(struct net *net,
				 struct socket *sock,
				 int protocol,
				 int kern)
{
	struct sock *sk;
	struct msm_ipc_port *port_ptr;

	if (unlikely(protocol != 0)) {
		IPC_RTR_ERR("%s: Protocol not supported\n", __func__);
		return -EPROTONOSUPPORT;
	}

	switch (sock->type) {
	case SOCK_DGRAM:
		break;
	default:
		IPC_RTR_ERR("%s: Protocol type not supported\n", __func__);
		return -EPROTOTYPE;
	}

	sk = sk_alloc(net, AF_MSM_IPC, GFP_KERNEL, &msm_ipc_proto);
	if (!sk) {
		IPC_RTR_ERR("%s: sk_alloc failed\n", __func__);
		return -ENOMEM;
	}

	sock->ops = &msm_ipc_proto_ops;
	sock_init_data(sock, sk);
	sk->sk_data_ready = NULL;
	sk->sk_write_space = ipc_router_dummy_write_space;
	sk->sk_rcvtimeo = DEFAULT_RCV_TIMEO;
	sk->sk_sndtimeo = DEFAULT_SND_TIMEO;

	port_ptr = msm_ipc_router_create_raw_port(sk, NULL, NULL);
	if (!port_ptr) {
		IPC_RTR_ERR("%s: port_ptr alloc failed\n", __func__);
		sock_put(sk);
		sock->sk = NULL;
		return -ENOMEM;
	}

	port_ptr->check_send_permissions = msm_ipc_check_send_permissions;
	msm_ipc_sk(sk)->port = port_ptr;
	msm_ipc_sk(sk)->default_node_vote_info = NULL;

	return 0;
}

int msm_ipc_router_bind(struct socket *sock, struct sockaddr *uaddr,
			       int uaddr_len)
{
	struct sockaddr_msm_ipc *addr = (struct sockaddr_msm_ipc *)uaddr;
	struct sock *sk = sock->sk;
	struct msm_ipc_port *port_ptr;
	int ret;

	if (!sk)
		return -EINVAL;

	if (!check_permissions()) {
		IPC_RTR_ERR("%s: %s Do not have permissions\n",
			__func__, current->comm);
		return -EPERM;
	}

	if (!uaddr_len) {
		IPC_RTR_ERR("%s: Invalid address length\n", __func__);
		return -EINVAL;
	}

	if (addr->family != AF_MSM_IPC) {
		IPC_RTR_ERR("%s: Address family is incorrect\n", __func__);
		return -EAFNOSUPPORT;
	}

	if (addr->address.addrtype != MSM_IPC_ADDR_NAME) {
		IPC_RTR_ERR("%s: Address type is incorrect\n", __func__);
		return -EINVAL;
	}

	port_ptr = msm_ipc_sk_port(sk);
	if (!port_ptr)
		return -ENODEV;

	if (!msm_ipc_sk(sk)->default_node_vote_info)
		msm_ipc_sk(sk)->default_node_vote_info =
			msm_ipc_load_default_node();
	lock_sock(sk);

	ret = msm_ipc_router_register_server(port_ptr, &addr->address);

	release_sock(sk);
	return ret;
}

static int ipc_router_connect(struct socket *sock, struct sockaddr *uaddr,
			      int uaddr_len, int flags)
{
	struct sockaddr_msm_ipc *addr = (struct sockaddr_msm_ipc *)uaddr;
	struct sock *sk = sock->sk;
	struct msm_ipc_port *port_ptr;
	int ret;

	if (!sk)
		return -EINVAL;

	if (uaddr_len <= 0) {
		IPC_RTR_ERR("%s: Invalid address length\n", __func__);
		return -EINVAL;
	}

	if (!addr) {
		IPC_RTR_ERR("%s: Invalid address\n", __func__);
		return -EINVAL;
	}

	if (addr->family != AF_MSM_IPC) {
		IPC_RTR_ERR("%s: Address family is incorrect\n", __func__);
		return -EAFNOSUPPORT;
	}

	port_ptr = msm_ipc_sk_port(sk);
	if (!port_ptr)
		return -ENODEV;

	lock_sock(sk);
	ret = ipc_router_set_conn(port_ptr, &addr->address);
	release_sock(sk);
	return ret;
}

static int msm_ipc_router_sendmsg(struct kiocb *iocb, struct socket *sock,
				  struct msghdr *m, size_t total_len)
{
	struct sock *sk = sock->sk;
	struct msm_ipc_port *port_ptr = msm_ipc_sk_port(sk);
	struct sockaddr_msm_ipc *dest = (struct sockaddr_msm_ipc *)m->msg_name;
	struct sk_buff_head *msg;
	int ret;
	struct msm_ipc_addr dest_addr = {0};
	long timeout;

	if (dest) {
		if (m->msg_namelen < sizeof(*dest) ||
		    dest->family != AF_MSM_IPC)
			return -EINVAL;
		memcpy(&dest_addr, &dest->address, sizeof(dest_addr));
	} else {
		if (port_ptr->conn_status == NOT_CONNECTED) {
			return -EDESTADDRREQ;
		} else if (port_ptr->conn_status < CONNECTION_RESET) {
			return -ENETRESET;
		} else {
			memcpy(&dest_addr.addr.port_addr, &port_ptr->dest_addr,
				sizeof(struct msm_ipc_port_addr));
			dest_addr.addrtype = MSM_IPC_ADDR_ID;
		}
	}

	if (total_len > MAX_IPC_PKT_SIZE)
		return -EINVAL;

	lock_sock(sk);
	timeout = sock_sndtimeo(sk, m->msg_flags & MSG_DONTWAIT);
	msg = msm_ipc_router_build_msg(m->msg_iovlen, m->msg_iov, total_len);
	if (!msg) {
		IPC_RTR_ERR("%s: Msg build failure\n", __func__);
		ret = -ENOMEM;
		goto out_sendmsg;
	}
	kmemleak_not_leak(msg);

	if (port_ptr->type == CLIENT_PORT)
		wait_for_irsc_completion();
	ret = msm_ipc_router_send_to(port_ptr, msg, &dest_addr, timeout);
	if (ret != total_len) {
		if (ret < 0) {
			if (ret != -EAGAIN)
				IPC_RTR_ERR("%s: Send_to failure %d\n",
							__func__, ret);
			msm_ipc_router_free_skb(msg);
		} else if (ret >= 0) {
			ret = -EFAULT;
		}
	}

out_sendmsg:
	release_sock(sk);
	return ret;
}

static int msm_ipc_router_recvmsg(struct kiocb *iocb, struct socket *sock,
				  struct msghdr *m, size_t buf_len, int flags)
{
	struct sock *sk = sock->sk;
	struct msm_ipc_port *port_ptr = msm_ipc_sk_port(sk);
	struct rr_packet *pkt;
	long timeout;
	int ret;

	if (m->msg_iovlen != 1)
		return -EOPNOTSUPP;
	lock_sock(sk);
	if (!buf_len) {
		if (flags & MSG_PEEK)
			ret = msm_ipc_router_get_curr_pkt_size(port_ptr);
		else
			ret = -EINVAL;
		release_sock(sk);
		return ret;
	}
	timeout = sock_rcvtimeo(sk, flags & MSG_DONTWAIT);

	ret = msm_ipc_router_rx_data_wait(port_ptr, timeout);
	if (ret) {
		release_sock(sk);
		if (ret == -ENOMSG)
			m->msg_namelen = 0;
		return ret;
	}

	ret = msm_ipc_router_read(port_ptr, &pkt, buf_len);
	if (ret <= 0 || !pkt) {
		release_sock(sk);
		return ret;
	}

	ret = msm_ipc_router_extract_msg(m, pkt);
	release_pkt(pkt);
	release_sock(sk);
	return ret;
}

static int msm_ipc_router_ioctl(struct socket *sock,
				unsigned int cmd, unsigned long arg)
{
	struct sock *sk = sock->sk;
	struct msm_ipc_port *port_ptr;
	struct server_lookup_args server_arg;
	struct msm_ipc_server_info *srv_info = NULL;
	unsigned int n;
	size_t srv_info_sz = 0;
	int ret;

	if (!sk)
		return -EINVAL;

	lock_sock(sk);
	port_ptr = msm_ipc_sk_port(sock->sk);
	if (!port_ptr) {
		release_sock(sk);
		return -EINVAL;
	}

	switch (cmd) {
	case IPC_ROUTER_IOCTL_GET_VERSION:
		n = IPC_ROUTER_V1;
		ret = put_user(n, (unsigned int *)arg);
		break;

	case IPC_ROUTER_IOCTL_GET_MTU:
		n = (MAX_IPC_PKT_SIZE - IPC_ROUTER_HDR_SIZE);
		ret = put_user(n, (unsigned int *)arg);
		break;

	case IPC_ROUTER_IOCTL_GET_CURR_PKT_SIZE:
		ret = msm_ipc_router_get_curr_pkt_size(port_ptr);
		break;

	case IPC_ROUTER_IOCTL_LOOKUP_SERVER:
		if (!msm_ipc_sk(sk)->default_node_vote_info)
			msm_ipc_sk(sk)->default_node_vote_info =
				msm_ipc_load_default_node();

		ret = copy_from_user(&server_arg, (void *)arg,
				     sizeof(server_arg));
		if (ret) {
			ret = -EFAULT;
			break;
		}

		if (server_arg.num_entries_in_array < 0) {
			ret = -EINVAL;
			break;
		}
		if (server_arg.num_entries_in_array) {
			if (server_arg.num_entries_in_array >
				(SIZE_MAX / sizeof(*srv_info))) {
				IPC_RTR_ERR("%s: Integer Overflow %zu * %d\n",
					__func__, sizeof(*srv_info),
					server_arg.num_entries_in_array);
				ret = -EINVAL;
				break;
			}
			srv_info_sz = server_arg.num_entries_in_array *
					sizeof(*srv_info);
			srv_info = kmalloc(srv_info_sz, GFP_KERNEL);
			if (!srv_info) {
				ret = -ENOMEM;
				break;
			}
		}
		ret = msm_ipc_router_lookup_server_name(&server_arg.port_name,
				srv_info, server_arg.num_entries_in_array,
				server_arg.lookup_mask);
		if (ret < 0) {
			IPC_RTR_ERR("%s: Server not found\n", __func__);
			ret = -ENODEV;
			kfree(srv_info);
			break;
		}
		server_arg.num_entries_found = ret;

		ret = copy_to_user((void *)arg, &server_arg,
				   sizeof(server_arg));

		n = min(server_arg.num_entries_found,
			server_arg.num_entries_in_array);

		if (ret == 0 && n) {
			ret = copy_to_user((void *)(arg + sizeof(server_arg)),
					   srv_info, n * sizeof(*srv_info));
		}

		if (ret)
			ret = -EFAULT;
		kfree(srv_info);
		break;

	case IPC_ROUTER_IOCTL_BIND_CONTROL_PORT:
		ret = msm_ipc_router_bind_control_port(port_ptr);
		break;

	case IPC_ROUTER_IOCTL_CONFIG_SEC_RULES:
		ret = msm_ipc_config_sec_rules((void *)arg);
		if (ret != -EPERM)
			port_ptr->type = IRSC_PORT;
		break;

	default:
		ret = -EINVAL;
	}
	release_sock(sk);
	return ret;
}

static unsigned int msm_ipc_router_poll(struct file *file,
			struct socket *sock, poll_table *wait)
{
	struct sock *sk = sock->sk;
	struct msm_ipc_port *port_ptr;
	uint32_t mask = 0;

	if (!sk)
		return -EINVAL;

	port_ptr = msm_ipc_sk_port(sk);
	if (!port_ptr)
		return -EINVAL;

	poll_wait(file, &port_ptr->port_rx_wait_q, wait);

	if (!list_empty(&port_ptr->port_rx_q))
		mask |= (POLLRDNORM | POLLIN);

	if (port_ptr->conn_status == CONNECTION_RESET)
		mask |= (POLLHUP | POLLERR);

	return mask;
}

static int msm_ipc_router_close(struct socket *sock)
{
	struct sock *sk = sock->sk;
	struct msm_ipc_port *port_ptr;
	int ret;

	if (!sk)
		return -EINVAL;

	lock_sock(sk);
	port_ptr = msm_ipc_sk_port(sk);
	if (!port_ptr) {
		release_sock(sk);
		return -EINVAL;
	}
	ret = msm_ipc_router_close_port(port_ptr);
	msm_ipc_unload_default_node(msm_ipc_sk(sk)->default_node_vote_info);
	release_sock(sk);
	sock_put(sk);
	sock->sk = NULL;

	return ret;
}

/**
 * register_ipcrtr_af_init_notifier() - Register for ipc router socket
 *				address family initialization callback
 * @nb: Notifier block which will be notified when address family is
 *	initialized.
 *
 * Return: 0 on success, standard error code otherwise.
 */
int register_ipcrtr_af_init_notifier(struct notifier_block *nb)
{
	int ret;

	if (!nb)
		return -EINVAL;
	mutex_lock(&ipcrtr_af_init_lock);
	if (sockets_enabled)
		nb->notifier_call(nb, IPCRTR_AF_INIT, NULL);
	ret = raw_notifier_chain_register(&ipcrtr_af_init_chain, nb);
	mutex_unlock(&ipcrtr_af_init_lock);
	return ret;
}
EXPORT_SYMBOL(register_ipcrtr_af_init_notifier);

/**
 * unregister_ipcrtr_af_init_notifier() - Unregister for ipc router socket
 *				address family initialization callback
 * @nb: Notifier block which will be notified once address family is
 *	initialized.
 *
 * Return: 0 on success, standard error code otherwise.
 */
int unregister_ipcrtr_af_init_notifier(struct notifier_block *nb)
{
	int ret;

	if (!nb)
		return -EINVAL;
	ret = raw_notifier_chain_unregister(&ipcrtr_af_init_chain, nb);
	return ret;
}
EXPORT_SYMBOL(unregister_ipcrtr_af_init_notifier);

static const struct net_proto_family msm_ipc_family_ops = {
	.owner		= THIS_MODULE,
	.family		= AF_MSM_IPC,
	.create		= msm_ipc_router_create
};

static const struct proto_ops msm_ipc_proto_ops = {
	.family			= AF_MSM_IPC,
	.owner			= THIS_MODULE,
	.release		= msm_ipc_router_close,
	.bind			= msm_ipc_router_bind,
	.connect		= ipc_router_connect,
	.socketpair		= sock_no_socketpair,
	.accept			= sock_no_accept,
	.getname		= sock_no_getname,
	.poll			= msm_ipc_router_poll,
	.ioctl			= msm_ipc_router_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl		= msm_ipc_router_ioctl,
#endif
	.listen			= sock_no_listen,
	.shutdown		= sock_no_shutdown,
	.setsockopt		= sock_no_setsockopt,
	.getsockopt		= sock_no_getsockopt,
#ifdef CONFIG_COMPAT
	.compat_setsockopt	= sock_no_setsockopt,
	.compat_getsockopt	= sock_no_getsockopt,
#endif
	.sendmsg		= msm_ipc_router_sendmsg,
	.recvmsg		= msm_ipc_router_recvmsg,
	.mmap			= sock_no_mmap,
	.sendpage		= sock_no_sendpage,
};

static struct proto msm_ipc_proto = {
	.name           = "MSM_IPC",
	.owner          = THIS_MODULE,
	.obj_size       = sizeof(struct msm_ipc_sock),
};

int msm_ipc_router_init_sockets(void)
{
	int ret;

	ret = proto_register(&msm_ipc_proto, 1);
	if (ret) {
		IPC_RTR_ERR("%s: Failed to register MSM_IPC protocol type\n",
								__func__);
		goto out_init_sockets;
	}

	ret = sock_register(&msm_ipc_family_ops);
	if (ret) {
		IPC_RTR_ERR("%s: Failed to register MSM_IPC socket type\n",
								__func__);
		proto_unregister(&msm_ipc_proto);
		goto out_init_sockets;
	}

	mutex_lock(&ipcrtr_af_init_lock);
	sockets_enabled = 1;
	raw_notifier_call_chain(&ipcrtr_af_init_chain,
				IPCRTR_AF_INIT, NULL);
	mutex_unlock(&ipcrtr_af_init_lock);
out_init_sockets:
	return ret;
}

void msm_ipc_router_exit_sockets(void)
{
	if (!sockets_enabled)
		return;

	sock_unregister(msm_ipc_family_ops.family);
	proto_unregister(&msm_ipc_proto);
	mutex_lock(&ipcrtr_af_init_lock);
	sockets_enabled = 0;
	raw_notifier_call_chain(&ipcrtr_af_init_chain,
				IPCRTR_AF_DEINIT, NULL);
	mutex_unlock(&ipcrtr_af_init_lock);
}
