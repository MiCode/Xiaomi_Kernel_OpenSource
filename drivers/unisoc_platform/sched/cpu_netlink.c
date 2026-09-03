// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023, Unisoc (shanghai) Technologies Co., Ltd
 *
 * Author: Xuewen Yan <xuewen.yan@unisoc.com>
 *
 * Generic netlink for cpu management framework
 */

#include <linux/sched.h>
#include <net/genetlink.h>

#include "cpu_netlink.h"

static const struct genl_multicast_group cpu_genl_mcgrps[] = {
	{ .name = CPU_GENL_SAMPLING_GROUP_NAME, },
	{ .name = CPU_GENL_EVENT_GROUP_NAME,  },
};

static const struct nla_policy cpu_genl_policy[CPU_GENL_ATTR_MAX + 1] = {
	[CPU_GENL_ATTR_CPU_ID]		= { .type = NLA_U32 },
	[CPU_GENL_ATTR_HIGH_LOAD]	= { .type = NLA_U8 },
	[CPU_GENL_ATTR_VIP_COMM]	= { .type = NLA_STRING,
					    .len = TASK_COMM_LEN },
	[CPU_GENL_ATTR_VIP_PID]		= { .type = NLA_U32 },
	[CPU_GENL_ATTR_VIP_FORK_PID]	= { .type = NLA_U32 },
	[CPU_GENL_ATTR_CPU_LOADING]	= { .type = NLA_U32 },
};

struct param {
	struct nlattr **attrs;
	struct sk_buff *msg;

	const char *name;
	int tgid;
	int pid;

	int cpu_id;
	int high_load;
	int cpu_loading;
};

typedef int (*cb_t)(struct param *);

static struct genl_family cpu_gnl_family;

static int cpu_genl_event_vip_fork_task(struct param *p)
{
	if (nla_put_string(p->msg, CPU_GENL_ATTR_VIP_COMM, p->name) ||
	    nla_put_u32(p->msg, CPU_GENL_ATTR_VIP_PID, p->tgid) ||
	    nla_put_u32(p->msg, CPU_GENL_ATTR_VIP_FORK_PID, p->pid))
		return -EMSGSIZE;

	return 0;
}

static cb_t event_cb[] = {
	[CPU_GENL_EVENT_VIP_FORK]		= cpu_genl_event_vip_fork_task,
	[CPU_GENL_EVENT_HIGH_LOAD]		= NULL,
	[CPU_GENL_EVENT_CPU_LOAD]		= NULL,
};

static int cpu_genl_cmd_dumpit(struct sk_buff *skb, struct netlink_callback *cb)
{
	return 0;
}

static const struct genl_small_ops cpu_genl_ops[] = {
	{
		.cmd = CPU_GENL_CMD_GET_CPU_LOADING,
		.validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.dumpit = cpu_genl_cmd_dumpit,
	},
};
/*
 * Generic netlink event encoding
 */
static int cpu_genl_send_event(enum cpu_genl_event event, struct param *p)
{
	struct sk_buff *msg;
	int ret = -EMSGSIZE;
	void *hdr;

	msg = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;
	p->msg = msg;

	hdr = genlmsg_put(msg, 0, 0, &cpu_gnl_family, 0, event);
	if (!hdr)
		goto out_free_msg;

	ret = event_cb[event](p);
	if (ret)
		goto out_cancel_msg;

	genlmsg_end(msg, hdr);

	genlmsg_multicast(&cpu_gnl_family, msg, 0, 1, GFP_KERNEL);

	return 0;

out_cancel_msg:
	genlmsg_cancel(msg, hdr);
out_free_msg:
	nlmsg_free(msg);

	return ret;
}
int cpu_notify_vip_fork_task(int tgid, int pid, const char *name)
{
	struct param p = { .tgid = tgid, .pid = pid, .name = name };

	return cpu_genl_send_event(CPU_GENL_EVENT_VIP_FORK, &p);
}

static struct genl_family cpu_gnl_family = {
	.hdrsize	= 0,
	.name		= CPU_GENL_FAMILY_NAME,
	.version	= CPU_GENL_VERSION,
	.maxattr	= CPU_GENL_ATTR_MAX,
	.policy		= cpu_genl_policy,
	.small_ops	= cpu_genl_ops,
	.n_small_ops	= ARRAY_SIZE(cpu_genl_ops),
	.mcgrps		= cpu_genl_mcgrps,
	.n_mcgrps	= ARRAY_SIZE(cpu_genl_mcgrps),
};

int cpu_netlink_init(void)
{
	return genl_register_family(&cpu_gnl_family);
}
