/*
 * File: mhi_raw.c
 *
 * Copyright (C) 2011 Renesas Mobile Corporation. All rights reserved.
 *
 * Author: Petri Mattila <petri.to.mattila@renesasmobile.com>
 *
 * RAW socket implementation for MHI protocol family.
 *
 * It uses the MHI socket framework in mhi_socket.c
 *
 * This implementation is the most basic frame passing interface.
 * The user space can use the sendmsg() and recvmsg() system calls
 * to access the frames. The socket is created with the socket()
 * system call, e.g. socket(PF_MHI,SOCK_RAW,l2proto).
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/socket.h>
#include <linux/mhi.h>
#include <linux/l2mux.h>

#include <asm/ioctls.h>

#include <net/af_mhi.h>
#include <net/mhi/sock.h>
#include <net/mhi/raw.h>

#ifdef CONFIG_MHI_DEBUG
# define DPRINTK(...)    printk(KERN_DEBUG "MHI/RAW: " __VA_ARGS__)
#else
# define DPRINTK(...)
#endif


/*** Prototypes ***/

static struct proto mhi_raw_proto;

static void mhi_raw_destruct(struct sock *sk);


/*** Functions ***/

int mhi_raw_sock_create(
	struct net		*net,
	struct socket		*sock,
	int			 proto,
	int			 kern)
{
	struct sock		*sk;
	struct mhi_sock	        *msk;

	DPRINTK("mhi_raw_sock_create: proto:%d type:%d\n",
		proto, sock->type);

	if (sock->type != SOCK_RAW)
		return -EPROTONOSUPPORT;

	sk = sk_alloc(net, PF_MHI, GFP_KERNEL, &mhi_raw_proto);
	if (!sk)
		return -ENOMEM;

	sock_init_data(sock, sk);

	sock->ops = &mhi_socket_ops;
	sock->state = SS_UNCONNECTED;

	if (proto != MHI_L3_ANY)
		sk->sk_protocol = proto;
	else
		sk->sk_protocol = 0;

	sk->sk_destruct = mhi_raw_destruct;
	sk->sk_backlog_rcv = sk->sk_prot->backlog_rcv;

	sk->sk_prot->init(sk);

	msk = mhi_sk(sk);

	msk->sk_l3proto = proto;
	msk->sk_ifindex = -1;

	return 0;
}

static int mhi_raw_init(struct sock *sk)
{
	return 0;
}

static void mhi_raw_destruct(struct sock *sk)
{
	skb_queue_purge(&sk->sk_receive_queue);
}

static void mhi_raw_close(struct sock *sk, long timeout)
{
	sk_common_release(sk);
}

static int mhi_raw_ioctl(struct sock *sk, int cmd, unsigned long arg)
{
	int err;

	DPRINTK("mhi_raw_ioctl: cmd:%d arg:%lu\n", cmd, arg);

	switch (cmd) {
	case SIOCOUTQ:
		{
			int len;
			len = sk_wmem_alloc_get(sk);
			err = put_user(len, (int __user *)arg);
		}
		break;

	case SIOCINQ:
		{
			struct sk_buff *skb;
			int len;

			lock_sock(sk);
			{
				skb = skb_peek(&sk->sk_receive_queue);
				len = skb ? skb->len : 0;
			}
			release_sock(sk);

			err = put_user(len, (int __user *)arg);
		}
		break;

	default:
		err = -ENOIOCTLCMD;
	}

	return err;
}

static int mhi_raw_sendmsg(
	struct kiocb		*iocb,
	struct sock			*sk,
	struct msghdr		*msg,
	size_t				len)
{
	struct mhi_sock	    *msk = mhi_sk(sk);
	struct net_device   *dev = NULL;
	struct sk_buff	    *skb;

	int err = -EFAULT;

	if (msg->msg_flags &
		~(MSG_DONTWAIT|MSG_EOR|MSG_NOSIGNAL|MSG_CMSG_COMPAT)) {
		printk(KERN_WARNING
			"mhi_raw_sendmsg: incompatible socket msg_flags: 0x%08X\n",
		    msg->msg_flags);
		err = -EOPNOTSUPP;
		goto out;
	}

	skb = sock_alloc_send_skb(sk,
						len,
						(msg->msg_flags & MSG_DONTWAIT),
						&err);
	if (!skb) {
		printk(KERN_ERR
			"mhi_raw_sendmsg: sock_alloc_send_skb failed: %d\n",
			err);
		goto out;
	}

	err = memcpy_fromiovec((void *)skb_put(skb, len), msg->msg_iov, len);
	if (err < 0) {
		printk(KERN_ERR
			"mhi_raw_sendmsg: memcpy_fromiovec failed: %d\n",
			err);
		goto drop;
	}

	if (msk->sk_ifindex)
		dev = dev_get_by_index(sock_net(sk), msk->sk_ifindex);

	if (!dev) {
		printk(KERN_ERR
			"mhi_raw_sendmsg: no device for ifindex:%d\n",
			msk->sk_ifindex);
		goto drop;
	}

	if (!(dev->flags & IFF_UP)) {
		printk(KERN_ERR
			"mhi_raw_sendmsg: device %d not IFF_UP\n",
			msk->sk_ifindex);
		err = -ENETDOWN;
		goto drop;
	}

	if (len > dev->mtu) {
		err = -EMSGSIZE;
		goto drop;
	}

	skb_reset_network_header(skb);
	skb_reset_mac_header(skb);

	err = mhi_skb_send(skb, dev, sk->sk_protocol);

	goto put;

drop:
	kfree(skb);
put:
	if (dev)
		dev_put(dev);
out:
	return err;
}

static int mhi_raw_recvmsg(
	struct kiocb *iocb,
	struct sock *sk,
	struct msghdr *msg,
	size_t len,
	int noblock,
	int flags,
	int *addr_len)
{
	struct sk_buff *skb = NULL;
	int cnt, err;

	err = -EOPNOTSUPP;

	if (flags &
		~(MSG_PEEK|MSG_TRUNC|MSG_DONTWAIT|
			MSG_NOSIGNAL|MSG_CMSG_COMPAT)) {
		printk(KERN_WARNING
			"mhi_raw_recvmsg: incompatible socket flags: 0x%08X",
			flags);
		goto out2;
	}

	if (addr_len)
		addr_len[0] = 0;

	skb = skb_recv_datagram(sk, flags, noblock, &err);
	if (!skb)
		goto out2;

	cnt = skb->len;
	if (len < cnt) {
		msg->msg_flags |= MSG_TRUNC;
		cnt = len;
	}

	err = skb_copy_datagram_iovec(skb, 0, msg->msg_iov, cnt);
	if (err)
		goto out;

	if (flags & MSG_TRUNC)
		err = skb->len;
	else
		err = cnt;

out:
	skb_free_datagram(sk, skb);
out2:
	return err;
}

static int mhi_raw_backlog_rcv(struct sock *sk, struct sk_buff *skb)
{
	if (sock_queue_rcv_skb(sk, skb) < 0) {
		kfree_skb(skb);
		return NET_RX_DROP;
	}

	return NET_RX_SUCCESS;
}


static struct proto mhi_raw_proto = {
	.name		= "MHI-RAW",
	.owner		= THIS_MODULE,
	.close		= mhi_raw_close,
	.ioctl		= mhi_raw_ioctl,
	.init		= mhi_raw_init,
	.sendmsg	= mhi_raw_sendmsg,
	.recvmsg	= mhi_raw_recvmsg,
	.backlog_rcv	= mhi_raw_backlog_rcv,
	.hash		= mhi_sock_hash,
	.unhash		= mhi_sock_unhash,
	.obj_size	= sizeof(struct mhi_sock),
};


int mhi_raw_proto_init(void)
{
	DPRINTK("mhi_raw_proto_init\n");

	return proto_register(&mhi_raw_proto, 1);
}

void mhi_raw_proto_exit(void)
{
	DPRINTK("mhi_raw_proto_exit\n");

	proto_unregister(&mhi_raw_proto);
}

