/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#ifdef CONFIG_ANDROID_PARANOID_NETWORK
#include <linux/android_aid.h>
#endif

#include <asm/string.h>
#include <asm/atomic.h>

#include <net/sock.h>

#include "ipc_router.h"

#define msm_ipc_sk(sk) ((struct msm_ipc_sock *)(sk))
#define msm_ipc_sk_port(sk) ((struct msm_ipc_port *)(msm_ipc_sk(sk)->port))

static int sockets_enabled;
static struct proto msm_ipc_proto;
static const struct proto_ops msm_ipc_proto_ops;

#ifdef CONFIG_ANDROID_PARANOID_NETWORK
static inline int check_permissions(void)
{
	int rc = 0;
	if (!current_euid() || in_egroup_p(AID_NET_RAW))
		rc = 1;
	return rc;
}
# else
static inline int check_permissions(void)
{
	return 1;
}
#endif

static struct sk_buff_head *msm_ipc_router_build_msg(unsigned int num_sect,
					  struct iovec const *msg_sect,
					  size_t total_len)
{
	struct sk_buff_head *msg_head;
	struct sk_buff *msg;
	int i, copied, first = 1;
	int data_size = 0, request_size, offset;
	void *data;

	for (i = 0; i < num_sect; i++)
		data_size += msg_sect[i].iov_len;

	if (!data_size)
		return NULL;

	msg_head = kmalloc(sizeof(struct sk_buff_head), GFP_KERNEL);
	if (!msg_head) {
		pr_err("%s: cannot allocate skb_head\n", __func__);
		return NULL;
	}
	skb_queue_head_init(msg_head);

	for (copied = 1, i = 0; copied && (i < num_sect); i++) {
		data_size = msg_sect[i].iov_len;
		offset = 0;
		while (offset != msg_sect[i].iov_len) {
			request_size = data_size;
			if (first)
				request_size += IPC_ROUTER_HDR_SIZE;

			msg = alloc_skb(request_size, GFP_KERNEL);
			if (!msg) {
				if (request_size <= (PAGE_SIZE/2)) {
					pr_err("%s: cannot allocated skb\n",
						__func__);
					goto msg_build_failure;
				}
				data_size = data_size / 2;
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
				pr_err("%s: copy_from_user failed\n",
					__func__);
				kfree_skb(msg);
				goto msg_build_failure;
			}
			skb_queue_tail(msg_head, msg);
			offset += data_size;
			data_size = msg_sect[i].iov_len - offset;
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
				      struct sk_buff_head *msg_head)
{
	struct sockaddr_msm_ipc *addr = (struct sockaddr_msm_ipc *)m->msg_name;
	struct rr_header *hdr;
	struct sk_buff *temp;
	int offset = 0, data_len = 0, copy_len;

	if (!m || !msg_head) {
		pr_err("%s: Invalid pointers passed\n", __func__);
		return -EINVAL;
	}

	temp = skb_peek(msg_head);
	hdr = (struct rr_header *)(temp->data);
	if (addr || (hdr->src_port_id != IPC_ROUTER_ADDRESS)) {
		addr->family = AF_MSM_IPC;
		addr->address.addrtype = MSM_IPC_ADDR_ID;
		addr->address.addr.port_addr.node_id = hdr->src_node_id;
		addr->address.addr.port_addr.port_id = hdr->src_port_id;
		m->msg_namelen = sizeof(struct sockaddr_msm_ipc);
	}

	data_len = hdr->size;
	skb_pull(temp, IPC_ROUTER_HDR_SIZE);
	skb_queue_walk(msg_head, temp) {
		copy_len = data_len < temp->len ? data_len : temp->len;
		if (copy_to_user(m->msg_iov->iov_base + offset, temp->data,
				 copy_len)) {
			pr_err("%s: Copy to user failed\n", __func__);
			return -EFAULT;
		}
		offset += copy_len;
		data_len -= copy_len;
	}
	return offset;
}

static void msm_ipc_router_release_msg(struct sk_buff_head *msg_head)
{
	struct sk_buff *temp;

	if (!msg_head) {
		pr_err("%s: Invalid msg pointer\n", __func__);
		return;
	}

	while (!skb_queue_empty(msg_head)) {
		temp = skb_dequeue(msg_head);
		kfree_skb(temp);
	}
	kfree(msg_head);
}

static int msm_ipc_router_create(struct net *net,
				 struct socket *sock,
				 int protocol,
				 int kern)
{
	struct sock *sk;
	struct msm_ipc_port *port_ptr;
	void *pil;

	if (!check_permissions()) {
		pr_err("%s: Do not have permissions\n", __func__);
		return -EPERM;
	}

	if (unlikely(protocol != 0)) {
		pr_err("%s: Protocol not supported\n", __func__);
		return -EPROTONOSUPPORT;
	}

	switch (sock->type) {
	case SOCK_DGRAM:
		break;
	default:
		pr_err("%s: Protocol type not supported\n", __func__);
		return -EPROTOTYPE;
	}

	sk = sk_alloc(net, AF_MSM_IPC, GFP_KERNEL, &msm_ipc_proto);
	if (!sk) {
		pr_err("%s: sk_alloc failed\n", __func__);
		return -ENOMEM;
	}

	port_ptr = msm_ipc_router_create_raw_port(sk, NULL, NULL);
	if (!port_ptr) {
		pr_err("%s: port_ptr alloc failed\n", __func__);
		sk_free(sk);
		return -ENOMEM;
	}

	sock->ops = &msm_ipc_proto_ops;
	sock_init_data(sock, sk);
	sk->sk_rcvtimeo = DEFAULT_RCV_TIMEO;

	pil = msm_ipc_load_default_node();
	msm_ipc_sk(sk)->port = port_ptr;
	msm_ipc_sk(sk)->default_pil = pil;

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

	if (!uaddr_len) {
		pr_err("%s: Invalid address length\n", __func__);
		return -EINVAL;
	}

	if (addr->family != AF_MSM_IPC) {
		pr_err("%s: Address family is incorrect\n", __func__);
		return -EAFNOSUPPORT;
	}

	if (addr->address.addrtype != MSM_IPC_ADDR_NAME) {
		pr_err("%s: Address type is incorrect\n", __func__);
		return -EINVAL;
	}

	port_ptr = msm_ipc_sk_port(sk);
	if (!port_ptr)
		return -ENODEV;

	lock_sock(sk);

	ret = msm_ipc_router_register_server(port_ptr, &addr->address);

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

	if (!dest)
		return -EDESTADDRREQ;

	if (m->msg_namelen < sizeof(*dest) || dest->family != AF_MSM_IPC)
		return -EINVAL;

	if (total_len > MAX_IPC_PKT_SIZE)
		return -EINVAL;

	lock_sock(sk);
	msg = msm_ipc_router_build_msg(m->msg_iovlen, m->msg_iov, total_len);
	if (!msg) {
		pr_err("%s: Msg build failure\n", __func__);
		ret = -ENOMEM;
		goto out_sendmsg;
	}

	ret = msm_ipc_router_send_to(port_ptr, msg, &dest->address);
	if (ret == (IPC_ROUTER_HDR_SIZE + total_len))
		ret = total_len;

out_sendmsg:
	release_sock(sk);
	return ret;
}

static int msm_ipc_router_recvmsg(struct kiocb *iocb, struct socket *sock,
				  struct msghdr *m, size_t buf_len, int flags)
{
	struct sock *sk = sock->sk;
	struct msm_ipc_port *port_ptr = msm_ipc_sk_port(sk);
	struct sk_buff_head *msg;
	long timeout;
	int ret;

	if (m->msg_iovlen != 1)
		return -EOPNOTSUPP;

	if (!buf_len)
		return -EINVAL;

	lock_sock(sk);
	timeout = sk->sk_rcvtimeo;
	mutex_lock(&port_ptr->port_rx_q_lock);
	while (list_empty(&port_ptr->port_rx_q)) {
		mutex_unlock(&port_ptr->port_rx_q_lock);
		release_sock(sk);
		if (timeout < 0) {
			ret = wait_event_interruptible(
					port_ptr->port_rx_wait_q,
					!list_empty(&port_ptr->port_rx_q));
			if (ret)
				return ret;
		} else if (timeout > 0) {
			timeout = wait_event_interruptible_timeout(
					port_ptr->port_rx_wait_q,
					!list_empty(&port_ptr->port_rx_q),
					timeout);
			if (timeout < 0)
				return -EFAULT;
		}

		if (timeout == 0)
			return -ETIMEDOUT;
		lock_sock(sk);
		mutex_lock(&port_ptr->port_rx_q_lock);
	}
	mutex_unlock(&port_ptr->port_rx_q_lock);

	ret = msm_ipc_router_read(port_ptr, &msg, buf_len);
	if (ret <= 0 || !msg) {
		release_sock(sk);
		return ret;
	}

	ret = msm_ipc_router_extract_msg(m, msg);
	msm_ipc_router_release_msg(msg);
	msg = NULL;
	release_sock(sk);
	return ret;
}

static int msm_ipc_router_ioctl(struct socket *sock,
				unsigned int cmd, unsigned long arg)
{
	struct sock *sk = sock->sk;
	struct msm_ipc_port *port_ptr;
	struct server_lookup_args server_arg;
	struct msm_ipc_port_addr *port_addr = NULL;
	unsigned int n, port_addr_sz = 0;
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
		n = IPC_ROUTER_VERSION;
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
			port_addr_sz = server_arg.num_entries_in_array *
					sizeof(*port_addr);
			port_addr = kmalloc(port_addr_sz, GFP_KERNEL);
			if (!port_addr) {
				ret = -ENOMEM;
				break;
			}
		}
		ret = msm_ipc_router_lookup_server_name(&server_arg.port_name,
				port_addr, server_arg.num_entries_in_array,
				server_arg.lookup_mask);
		if (ret < 0) {
			pr_err("%s: Server not found\n", __func__);
			ret = -ENODEV;
			kfree(port_addr);
			break;
		}
		server_arg.num_entries_found = ret;

		ret = copy_to_user((void *)arg, &server_arg,
				   sizeof(server_arg));
		if (port_addr_sz) {
			ret = copy_to_user((void *)(arg + sizeof(server_arg)),
					   port_addr, port_addr_sz);
			if (ret)
				ret = -EFAULT;
			kfree(port_addr);
		}
		break;

	case IPC_ROUTER_IOCTL_BIND_CONTROL_PORT:
		ret = msm_ipc_router_bind_control_port(port_ptr);
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

	return mask;
}

static int msm_ipc_router_close(struct socket *sock)
{
	struct sock *sk = sock->sk;
	struct msm_ipc_port *port_ptr = msm_ipc_sk_port(sk);
	void *pil = msm_ipc_sk(sk)->default_pil;
	int ret;

	lock_sock(sk);
	ret = msm_ipc_router_close_port(port_ptr);
	msm_ipc_unload_default_node(pil);
	release_sock(sk);
	sock_put(sk);
	sock->sk = NULL;

	return ret;
}

static const struct net_proto_family msm_ipc_family_ops = {
	.owner		= THIS_MODULE,
	.family		= AF_MSM_IPC,
	.create		= msm_ipc_router_create
};

static const struct proto_ops msm_ipc_proto_ops = {
	.owner		= THIS_MODULE,
	.family         = AF_MSM_IPC,
	.bind		= msm_ipc_router_bind,
	.connect	= sock_no_connect,
	.sendmsg	= msm_ipc_router_sendmsg,
	.recvmsg	= msm_ipc_router_recvmsg,
	.ioctl		= msm_ipc_router_ioctl,
	.poll		= msm_ipc_router_poll,
	.setsockopt	= sock_no_setsockopt,
	.getsockopt	= sock_no_getsockopt,
	.release	= msm_ipc_router_close,
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
		pr_err("Failed to register MSM_IPC protocol type\n");
		goto out_init_sockets;
	}

	ret = sock_register(&msm_ipc_family_ops);
	if (ret) {
		pr_err("Failed to register MSM_IPC socket type\n");
		proto_unregister(&msm_ipc_proto);
		goto out_init_sockets;
	}

	sockets_enabled = 1;
out_init_sockets:
	return ret;
}

void msm_ipc_router_exit_sockets(void)
{
	if (!sockets_enabled)
		return;

	sockets_enabled = 0;
	sock_unregister(msm_ipc_family_ops.family);
	proto_unregister(&msm_ipc_proto);
}
