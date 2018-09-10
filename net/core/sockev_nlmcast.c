/*
 * Copyright (c) 2014-2015, 2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * Default SOCKEV client implementation
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/netlink.h>
#include <linux/sockev.h>
#include <net/sock.h>

static int registration_status;
static struct sock *socknlmsgsk;

static void sockev_skmsg_recv(struct sk_buff *skb)
{
	pr_debug("%s(): Got unsolicited request\n", __func__);
}

static struct netlink_kernel_cfg nlcfg = {
	.input = sockev_skmsg_recv
};

static void _sockev_event(unsigned long event, __u8 *evstr, int buflen)
{
	memset(evstr, 0, buflen);

	switch (event) {
	case SOCKEV_SOCKET:
		strlcpy(evstr, "SOCKEV_SOCKET", buflen);
		break;
	case SOCKEV_BIND:
		strlcpy(evstr, "SOCKEV_BIND", buflen);
		break;
	case SOCKEV_LISTEN:
		strlcpy(evstr, "SOCKEV_LISTEN", buflen);
		break;
	case SOCKEV_ACCEPT:
		strlcpy(evstr, "SOCKEV_ACCEPT", buflen);
		break;
	case SOCKEV_CONNECT:
		strlcpy(evstr, "SOCKEV_CONNECT", buflen);
		break;
	case SOCKEV_SHUTDOWN:
		strlcpy(evstr, "SOCKEV_SHUTDOWN", buflen);
		break;
	default:
		strlcpy(evstr, "UNKOWN", buflen);
	}
}

static int sockev_client_cb(struct notifier_block *nb,
			    unsigned long event, void *data)
{

	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	struct sknlsockevmsg *smsg;
	struct socket *sock;
	struct sock *sk;

	sock = (struct socket *)data;
	if (!socknlmsgsk || !sock)
		goto done;

	sk = sock->sk;
	if (!sk)
		goto done;

	if (sk->sk_family != AF_INET && sk->sk_family != AF_INET6)
		goto done;

	if (event != SOCKEV_BIND && event != SOCKEV_LISTEN)
		goto done;

	skb = nlmsg_new(sizeof(struct sknlsockevmsg), GFP_KERNEL);
	if (skb == NULL)
		goto done;

	nlh = nlmsg_put(skb, 0, 0, event, sizeof(struct sknlsockevmsg), 0);
	if (nlh == NULL) {
		kfree_skb(skb);
		goto done;
	}

	NETLINK_CB(skb).dst_group = SKNLGRP_SOCKEV;

	smsg = nlmsg_data(nlh);
	smsg->pid = current->pid;
	_sockev_event(event, smsg->event, sizeof(smsg->event));
	smsg->skfamily = sk->sk_family;
	smsg->skstate = sk->sk_state;
	smsg->skprotocol = sk->sk_protocol;
	smsg->sktype = sk->sk_type;
	smsg->skflags = sk->sk_flags;
	nlmsg_notify(socknlmsgsk, skb, 0, SKNLGRP_SOCKEV, 0, GFP_KERNEL);
done:
	return 0;
}

static struct notifier_block sockev_notifier_client = {
	.notifier_call = sockev_client_cb,
	.next = 0,
	.priority = 0
};

/* ***************** Startup/Shutdown *************************************** */

static int __init sockev_client_init(void)
{
	int rc;
	registration_status = 1;
	rc = sockev_register_notify(&sockev_notifier_client);
	if (rc != 0) {
		registration_status = 0;
		pr_err("%s(): Failed to register cb (%d)\n", __func__, rc);
	}
	socknlmsgsk = netlink_kernel_create(&init_net, NETLINK_SOCKEV, &nlcfg);
	if (!socknlmsgsk) {
		pr_err("%s(): Failed to initialize netlink socket\n", __func__);
		if (registration_status)
			sockev_unregister_notify(&sockev_notifier_client);
		registration_status = 0;
	}

	return rc;
}
static void __exit sockev_client_exit(void)
{
	if (registration_status)
		sockev_unregister_notify(&sockev_notifier_client);
}
module_init(sockev_client_init)
module_exit(sockev_client_exit)
MODULE_LICENSE("GPL v2");

