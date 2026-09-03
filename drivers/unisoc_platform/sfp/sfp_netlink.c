// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
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

#define pr_fmt(fmt) "sfp: " fmt

#include <net/genetlink.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include "sfp.h"

#define SFP_GENL_NAME		"sfp"
#define SFP_GENL_VERSION	0x1

static struct genl_family sfp_genl_family;

static struct nla_policy sfp_genl_policy[SFP_A_MAX + 1] = {
	[SFP_A_STATS]    = { .type = NLA_U32 },
};

static int sfp_nl_do_stats_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *nla;
	struct sk_buff *msg;
	void *hdr;

	nla = info->attrs[SFP_A_STATS];
	if (!nla) {
		pr_err("tuple attr not exist!");
		return -EINVAL;
	}

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	hdr = genlmsg_put(msg, info->snd_portid,
			  info->snd_seq, &sfp_genl_family,
			  0, SFP_NL_CMD_STATS);

	if (!hdr) {
		nlmsg_free(msg);
		return -EMSGSIZE;
	}

	if (nla_put_u32(msg, SFP_A_STATS, sfp_stats_bytes)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	pr_debug("sfp stats bytes: %d\n", sfp_stats_bytes);

	genlmsg_end(msg, hdr);
	genlmsg_reply(msg, info);
	return 0;
}

static struct genl_ops sfp_genl_ops[] = {
	{
		.cmd = SFP_NL_CMD_STATS,
		.flags = GENL_ADMIN_PERM,
		.policy = sfp_genl_policy,
		.doit = sfp_nl_do_stats_rule,
	},
};

static struct genl_family sfp_genl_family = {
	.hdrsize	= 0,
	.name		= SFP_GENL_NAME,
	.version	= SFP_GENL_VERSION,
	.maxattr	= SFP_A_MAX,
	.ops		= sfp_genl_ops,
	.n_ops		= ARRAY_SIZE(sfp_genl_ops),
};

int __init sfp_netlink_init(void)
{
	int err;

	err = genl_register_family(&sfp_genl_family);

	return err;
}

void sfp_netlink_exit(void)
{
	genl_unregister_family(&sfp_genl_family);
}

