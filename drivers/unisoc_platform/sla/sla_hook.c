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
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/netfilter.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/icmp.h>
#include <linux/sysctl.h>
#include <net/route.h>
#include <net/ip.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>

#include "sla.h"

static unsigned int sla_input_hook(void *priv,
					  struct sk_buff *skb,
					  const struct nf_hook_state *state)
{
	//TODO;
	return NF_ACCEPT;
}

static unsigned int sla_output_hook(void *priv,
					  struct sk_buff *skb,
					  const struct nf_hook_state *state)
{
	//TODO;
	return NF_ACCEPT;
}

static struct nf_hook_ops sla_ops[] __read_mostly = {
	{
		.hook		= sla_output_hook,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_LOCAL_OUT,
		//must be here, for dns packet will do DNAT at mangle table with skb->mark
		.priority	= NF_IP_PRI_CONNTRACK + 1,
	},
	{
		.hook		= sla_input_hook,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_LOCAL_IN,
		.priority	= NF_IP_PRI_FILTER + 1,
	},
};

int __init nf_sla_hook_init(void)
{
	int ret;

	ret = nf_register_net_hooks(&init_net, sla_ops,
				    ARRAY_SIZE(sla_ops));
	if (ret < 0)
		SLA_PRT_DBG(SLA_PRT_ERR,  "v4 can't register hooks. ret=%d\n", ret);

	return ret;
}

void nf_sla_hook_uninit(void)
{
	nf_unregister_net_hooks(&init_net, sla_ops,
				    ARRAY_SIZE(sla_ops));
}
