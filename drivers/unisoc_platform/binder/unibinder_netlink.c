/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright 2023 Unisoc(Shanghai) Technologies Co.Ltd
 * This program is free software; you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation; version 2.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

/**
 * Generic netlink for binder mechanism
 */
#define pr_fmt(fmt) "unisoc_binder: " fmt

#include <net/genetlink.h>

#include "unibinder.h"
#include "unibinder_netlink.h"

static struct genl_family binder_genl_fam;

static int skr_cmd_action(struct sk_buff *skb, struct genl_info *info)
{
	if (info && info->attrs[BINDRE_GENL_ATTR_PID] &&
		info->attrs[BINDRE_GENL_ATTR_SKIP_RESTORE]) {

		int *pid = nla_data(info->attrs[BINDRE_GENL_ATTR_PID]);
		int *skip_restore = nla_data(info->attrs[BINDRE_GENL_ATTR_SKIP_RESTORE]);

		set_thread_flags(*pid, UBFF_SCHED_SKIP_RESTORE, *skip_restore);
	} else {
		pr_info("skip restore message lack info");
		return -EINVAL;
	}
	return 0;
}

static int ihrt_cmd_action(struct sk_buff *skb, struct genl_info *info)
{
	if (info && info->attrs[BINDRE_GENL_ATTR_PID] &&
		info->attrs[BINDRE_GENL_ATTR_INHERIT_RT]) {

		int *pid = nla_data(info->attrs[BINDRE_GENL_ATTR_PID]);
		int *inherit_rt = nla_data(info->attrs[BINDRE_GENL_ATTR_INHERIT_RT]);

		set_thread_flags(*pid, UBFF_SCHED_INHERIT_RT, *inherit_rt);
	} else {
		pr_info("inherit rt message lack info");
		return -EINVAL;
	}
	return 0;
}

/* Attribute validation policy for binder command */
static struct nla_policy binder_genl_policy[BINDER_GENL_ATTR_MAX + 1] = {
	[BINDRE_GENL_ATTR_PID]		  = { .type = NLA_U32 },
	[BINDRE_GENL_ATTR_SKIP_RESTORE] = { .type = NLA_U8 },
	[BINDRE_GENL_ATTR_INHERIT_RT]   = { .type = NLA_U8 },
};

/* Operations for binder Generic Netlink family */
static struct genl_ops binder_genl_ops[] = {
	{
		.cmd	= BINDER_GENL_CMD_SKIP_RESTORE,
		.doit	= skr_cmd_action,
	},
	{
		.cmd	= BINDER_GENL_CMD_INHERIT_RT,
		.doit   = ihrt_cmd_action,
	},
};

/* Multicast groups for our family */
static const struct genl_multicast_group binder_genl_mcgrps[] = {
	{ .name = BINDER_GENL_MC_GRP_NAME },
};

/* Generic Netlink family */
static struct genl_family binder_genl_fam = {
	.name	  = BINDER_GENL_FAMILY_NAME,
	.version  = BINDER_GENL_VERSION,
	.maxattr  = BINDER_GENL_ATTR_MAX,
	.ops	  = binder_genl_ops,
	.policy   = binder_genl_policy,
	.n_ops	  = ARRAY_SIZE(binder_genl_ops),
	.mcgrps	  = binder_genl_mcgrps,
	.n_mcgrps = ARRAY_SIZE(binder_genl_mcgrps),
};

int binder_netlink_init(void)
{
	int ret = 0;

	ret = genl_register_family(&binder_genl_fam);
	pr_info("%s register family %d", __func__, ret);

	return ret;
}

void binder_netlink_exit(void)
{
	if (unlikely(genl_unregister_family(&binder_genl_fam))) {
		pr_err("failed to unregister binder generic netlink family");
		return;
	}

	pr_info("%s faimly unregistered.", __func__);
}
