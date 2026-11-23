// SPDX-License-Identifier: GPL-2.0
/*
 * cpu_genetlink.c - Generic netlink transport for CPU monitor
 *
 * Copyright (c) 2026 Xiaomi, Inc., All rights reserved.
 */

#include <net/genetlink.h>
#include <linux/notifier.h>

#include "cpu_netlink.h"

#define XM_MIMON_FAMILY_NAME	"xm_mimon"
#define XM_MIMON_FAMILY_VERSION	1
#define XM_MIMON_MAX_DATA_LEN	20

enum xm_mimon_cmd {
	XM_MIMON_CMD_UNSPEC,
	XM_MIMON_CMD_REGISTER,
	XM_MIMON_CMD_EVENT,
	__XM_MIMON_CMD_MAX,
};

#define XM_MIMON_CMD_MAX (__XM_MIMON_CMD_MAX - 1)

enum xm_mimon_attr {
	XM_MIMON_ATTR_UNSPEC,
	XM_MIMON_ATTR_MSG_TYPE,
	XM_MIMON_ATTR_LEN,
	XM_MIMON_ATTR_DATA,
	__XM_MIMON_ATTR_MAX,
};

#define XM_MIMON_ATTR_MAX (__XM_MIMON_ATTR_MAX - 1)

static const struct nla_policy xm_mimon_policy[XM_MIMON_ATTR_MAX + 1] = {
	[XM_MIMON_ATTR_MSG_TYPE]	= { .type = NLA_U32 },
	[XM_MIMON_ATTR_LEN]		= { .type = NLA_U32 },
	[XM_MIMON_ATTR_DATA]		= { .type = NLA_BINARY,
					    .len = XM_MIMON_MAX_DATA_LEN *
						   sizeof(int) },
};

static u32 recv_portid;

static int xm_mimon_register_doit(struct sk_buff *skb, struct genl_info *info)
{
	if (!info)
		return -EINVAL;

	WRITE_ONCE(recv_portid, info->snd_portid);
	pr_info("cpuload: registered listener portid=%u\n", recv_portid);
	return 0;
}

static const struct genl_ops xm_mimon_ops[] = {
	{
		.cmd	= XM_MIMON_CMD_REGISTER,
		.flags	= GENL_ADMIN_PERM,
		.doit	= xm_mimon_register_doit,
	},
};

static struct genl_family xm_mimon_family = {
	.name		= XM_MIMON_FAMILY_NAME,
	.version	= XM_MIMON_FAMILY_VERSION,
	.maxattr	= XM_MIMON_ATTR_MAX,
	.policy		= xm_mimon_policy,
	.ops		= xm_mimon_ops,
	.n_ops		= ARRAY_SIZE(xm_mimon_ops),
};

/**
 * send_to_user - send a message to userspace via generic netlink
 * @sock_no: message type identifier (enum sock_no)
 * @len: number of int elements in @data
 * @data: payload array
 *
 * Return: 0 on success, negative errno on failure.
 */
int send_to_user(int sock_no, size_t len, const int *data)
{
	struct sk_buff *skb;
	void *msg_head;
	u32 portid;
	int ret;

	if (!data)
		return -EINVAL;

	portid = READ_ONCE(recv_portid);
	if (!portid)
		return -ENODEV;

	if (len > XM_MIMON_MAX_DATA_LEN) {
		pr_err("cpu_netlink: oversized payload (%zu > %d)\n",
		       len, XM_MIMON_MAX_DATA_LEN);
		return -EINVAL;
	}

	skb = genlmsg_new(NLMSG_GOODSIZE, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	msg_head = genlmsg_put(skb, 0, 0, &xm_mimon_family, 0,
			       XM_MIMON_CMD_EVENT);
	if (!msg_head) {
		nlmsg_free(skb);
		return -EMSGSIZE;
	}

	ret = nla_put_u32(skb, XM_MIMON_ATTR_MSG_TYPE, (u32)sock_no);
	if (ret)
		goto nla_err;

	ret = nla_put_u32(skb, XM_MIMON_ATTR_LEN, (u32)len);
	if (ret)
		goto nla_err;

	ret = nla_put(skb, XM_MIMON_ATTR_DATA, len * sizeof(int), data);
	if (ret)
		goto nla_err;

	genlmsg_end(skb, msg_head);

	ret = genlmsg_unicast(&init_net, skb, portid);
	if (ret < 0)
		pr_warn("cpu_netlink: unicast failed errno=%d\n", ret);

	return ret;

nla_err:
	genlmsg_cancel(skb, msg_head);
	nlmsg_free(skb);
	return ret;
}

static int xm_mimon_netlink_event(struct notifier_block *nb,
				  unsigned long event, void *ptr)
{
	struct netlink_notify *n = ptr;

	if (event == NETLINK_URELEASE &&
	    n->protocol == NETLINK_GENERIC &&
	    READ_ONCE(recv_portid) == n->portid) {
		WRITE_ONCE(recv_portid, 0);
		pr_info("cpuload: listener portid=%u disconnected\n",
			n->portid);
	}
	return NOTIFY_DONE;
}

static struct notifier_block xm_mimon_netlink_notifier = {
	.notifier_call = xm_mimon_netlink_event,
};

int cpu_netlink_init(void)
{
	int ret;

	recv_portid = 0;
	netlink_register_notifier(&xm_mimon_netlink_notifier);

	ret = genl_register_family(&xm_mimon_family);
	if (ret) {
		pr_err("cpu_netlink: register family failed ret=%d\n", ret);
		netlink_unregister_notifier(&xm_mimon_netlink_notifier);
	}
	return ret;
}

void cpu_netlink_exit(void)
{
	recv_portid = 0;
	netlink_unregister_notifier(&xm_mimon_netlink_notifier);
	genl_unregister_family(&xm_mimon_family);
}
