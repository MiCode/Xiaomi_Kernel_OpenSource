// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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

#include <linux/notifier.h>
#include <net/netfilter/nf_conntrack_tuple.h>
#include <linux/socket.h>
#include <net/netlink.h>
#include <linux/module.h>

#include "ursp_netlink.h"
#include "ursp.h"

static struct sock *ursp_sk;
static RAW_NOTIFIER_HEAD(ursp_chain);

int ursp_report_register_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&ursp_chain, nb);
}
EXPORT_SYMBOL(ursp_report_register_notifier);

int ursp_report_unregister_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&ursp_chain, nb);
}
EXPORT_SYMBOL(ursp_report_unregister_notifier);

void ursp_fill_v4_nlmsg(struct ursp_conn *ursp_ct, __u32 event,
			struct nl_msg_report *msg)
{
	struct nf_conntrack_tuple *tuple;

	tuple = &ursp_ct->tuple;
	msg->addr = tuple->dst.u3.ip;
	msg->port = ntohs(tuple->dst.u.all);
	msg->protocol = tuple->dst.protonum;
	msg->uid = ursp_ct->uid;
	msg->type = event;
}

void ursp_fill_v6_nlmsg(struct ursp_conn *ursp_ct, __u32 event,
			struct nl_msg_report *msg)
{
	struct nf_conntrack_tuple tuple;

	tuple = ursp_ct->tuple;
	memcpy(msg->addr6, tuple.dst.u3.ip6, sizeof(tuple.dst.u3.ip6));
	msg->port = ntohs(tuple.dst.u.all);
	msg->protocol = tuple.dst.protonum;
	msg->uid = ursp_ct->uid;
	msg->type = event;
}

void ursp_notify(struct ursp_conn *ursp_ct, unsigned long event)
{
	struct nl_msg_report msg = {0};
	__u16 pf;
	__u8 l4proto;

	pf = ursp_ct->tuple.src.l3num;
	l4proto = ursp_ct->tuple.dst.protonum;

	switch (pf) {
	case PF_INET:
		switch (l4proto) {
		case IPPROTO_TCP:
			switch (event) {
			case ESTABLISH_EVENT:
				ursp_fill_v4_nlmsg(ursp_ct, URSP_NL_MSG_IPv4_TCP_ESTABLISH, &msg);
				break;
			case CLOSE_EVENT:
				ursp_fill_v4_nlmsg(ursp_ct, URSP_NL_MSG_IPv4_TCP_CLOSE, &msg);
				break;
			}
			break;
		case IPPROTO_UDP:
			switch (event) {
			case ESTABLISH_EVENT:
				ursp_fill_v4_nlmsg(ursp_ct, URSP_NL_MSG_IPv4_UDP_ESTABLISH, &msg);
				break;
			case CLOSE_EVENT:
				ursp_fill_v4_nlmsg(ursp_ct, URSP_NL_MSG_IPv4_UDP_CLOSE, &msg);
				break;
			}
			break;
		}
		break;
	case PF_INET6:
		switch (l4proto) {
		case IPPROTO_TCP:
			switch (event) {
			case ESTABLISH_EVENT:
				ursp_fill_v6_nlmsg(ursp_ct, URSP_NL_MSG_IPv6_TCP_ESTABLISH, &msg);
				break;
			case CLOSE_EVENT:
				ursp_fill_v6_nlmsg(ursp_ct, URSP_NL_MSG_IPv6_TCP_CLOSE, &msg);
				break;
			}
			break;
		case IPPROTO_UDP:
			switch (event) {
			case ESTABLISH_EVENT:
				ursp_fill_v6_nlmsg(ursp_ct, URSP_NL_MSG_IPv6_UDP_ESTABLISH, &msg);
				break;
			case CLOSE_EVENT:
				ursp_fill_v6_nlmsg(ursp_ct, URSP_NL_MSG_IPv6_UDP_CLOSE, &msg);
				break;
			}
			break;
		}
		break;
	}

	ursp_notifier_call(event, &msg);
}

int ursp_notifier_call(unsigned long val, void *v)
{
	return raw_notifier_call_chain(&ursp_chain, val, v);
}

static void ursp_nl_add_payload(struct nlmsghdr *nlh,
				int len, void *data)
{
	struct nl_msg_report *msg = nlmsg_data(nlh);

	memset(msg, 0, len);
	memcpy(msg, data, sizeof(struct nl_msg_report));
}

static int ursp_nl_notify(unsigned long msgtype, void *data)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int len;

	len = sizeof(struct nl_msg_report);
	skb = nlmsg_new(len, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;
	nlh = nlmsg_put(skb, 0, 0, msgtype, len, 0);
	if (!nlh) {
		nlmsg_free(skb);
		return -EMSGSIZE;
	}
	ursp_nl_add_payload(nlh, len, data);
	NETLINK_CB(skb).dst_group = URSP_NL_GRP_EVENT;
	netlink_broadcast(ursp_sk, skb, 0, URSP_NL_GRP_EVENT, GFP_ATOMIC);
	return 0;
}

static int ursp_netlink_event_handler(struct notifier_block *nb,
				      unsigned long event, void *ptr)
{
	ursp_nl_notify(event, ptr);
	return NOTIFY_OK;
}

static struct notifier_block ursp_netlink_notifier = {
	.notifier_call	=	ursp_netlink_event_handler,
};

int nl_ursp_init(void)
{
	struct netlink_kernel_cfg nlcfg = {
	};

	ursp_sk = netlink_kernel_create(&init_net, NETLINK_ESTABLISH_REPORT, &nlcfg);
	ursp_report_register_notifier(&ursp_netlink_notifier);
	return 0;
}
EXPORT_SYMBOL(nl_ursp_init);

void nl_ursp_exit(void)
{
	ursp_report_unregister_notifier(&ursp_netlink_notifier);
	netlink_kernel_release(ursp_sk);
}
EXPORT_SYMBOL(nl_ursp_exit);

