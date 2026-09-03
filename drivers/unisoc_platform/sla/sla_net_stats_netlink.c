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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <net/netlink.h>
#include "sla_net_stats_netlink.h"

static struct sock *sla_nl_sk;

static int sla_send_netstats(unsigned long msgtype, struct sla_net_stats_info *info)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	struct sla_net_stats_info *msg;
	int len, ret = -1;

	if (!sla_nl_sk) {
		pr_err("[sla] Error socket\n");
		return -EINVAL;
	}
	len = sizeof(struct sla_net_stats_info);
	skb = nlmsg_new(len, GFP_ATOMIC);
	if (!skb) {
		pr_err("[sla] nlmsg_new fail\n");
		return -ENOMEM;
	}

	nlh = nlmsg_put(skb, 0, 0, msgtype, len, 0);
	if (!nlh) {
		nlmsg_free(skb);
		pr_err("[sla] nlmsg_put fail\n");
		return -EMSGSIZE;
	}

	msg = nlmsg_data(nlh);
	memset(msg, 0, len);
	memcpy(msg, info, len);

	NETLINK_CB(skb).portid = 0;
	NETLINK_CB(skb).dst_group = SLA_NL_GRP_EVENT;

	ret = netlink_broadcast(sla_nl_sk, skb, 0, SLA_NL_GRP_EVENT, GFP_ATOMIC);

	return ret;
}

int sla_netlink_notify(unsigned long msgtype, void *data)
{
	int ret = -1;

	switch (msgtype) {
	case SLA_NL_NET_STATS_MSG:
	case SLA_NL_NET_STATS_GOOD_RTT:
	case SLA_NL_NET_STATS_GOOD_RTT_2:
		ret = sla_send_netstats(msgtype, data);
		break;
	default:
		pr_err("[sla] Not support msgtype=%lu\n", msgtype);
		break;
	}

	return ret;
}

static void sla_netlink_input(struct sk_buff *skb)
{
	int *precv;
	struct nlmsghdr *nlh;

	nlh = nlmsg_hdr(skb);
	if (nlh->nlmsg_len < NLMSG_HDRLEN || skb->len < nlh->nlmsg_len) {
		pr_err("[sla]warning  nlmsg_len=%d, skb->len=%d,  nlmsg_len=%d\n",
			nlh->nlmsg_len, skb->len,  nlh->nlmsg_len);
		return;
	}

	if (!(nlh->nlmsg_flags & NLM_F_REQUEST)) {
		pr_err("[sla] warning nlmsg_flags=0x%x\n", nlh->nlmsg_flags);
		return;
	}

	switch (nlh->nlmsg_type) {
	case SLA_NL_NET_STATS_ENABLE:
		precv = nlmsg_data(nlh);
		sla_net_stats_set(*precv);
		break;
	default:
		pr_err("[sla] Not support input msgtype=%d\n", nlh->nlmsg_type);
		break;
	}
}

int sla_nl_init(void)
{
	struct netlink_kernel_cfg cfg = {
		.input = sla_netlink_input,
	};

	sla_nl_sk = netlink_kernel_create(&init_net, NETLINK_SLA, &cfg);
	if (!sla_nl_sk) {
		pr_err("[sla] Error creating socket\n");
		return -ENOMEM;
	}

	pr_info("[sla] sla_netlink_init\n");
	return 0;
}

void sla_nl_exit(void)
{
	pr_info("[sla] sla_netlink_exit\n");
	netlink_kernel_release(sla_nl_sk);
	sla_nl_sk = NULL;
}

