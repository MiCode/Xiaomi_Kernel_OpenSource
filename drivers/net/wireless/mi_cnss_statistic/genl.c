// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved. */

#include <linux/err.h>
#include <linux/module.h>
#include <net/netlink.h>
#include <net/genetlink.h>

#define CNSS_STATISTIC_GENL_NAME "cnss_statistic"
#define CNSS_STATISTIC_GENL_VERSION 1
#define CNSS_STATISTIC_MULTICAST_GROUP_EVENT       "events"

uint8_t wow_suspend_type;

EXPORT_SYMBOL(wow_suspend_type);

enum qdf_suspend_type {
	QDF_SYSTEM_SUSPEND,
	QDF_RUNTIME_SUSPEND
};

enum wow_cloud_ctrl_type {
	WOW_CLOUD_CTRL_DISABLED,
	WOW_CLOUD_CTRL_ENABLED
};

enum cnss_statistic_genl_commands {
	CNSS_STATISTIC_GENL_CMD_UNSPEC,
	CNSS_STATISTIC_GENL_EVENT_WOW_WAKEUP,
	CNSS_STATISTIC_GENL_CMD_DEBUG,
	CNSS_STATISTIC_GENL_CMD_ENABLE,
	CNSS_STATISTIC_GENL_CMD_DISABLE,
	__CNSS_STATISTIC_GENL_CMD_MAX,
};
#define CNSS_STATISTIC_GENL_CMD_MAX (__CNSS_STATISTIC_GENL_CMD_MAX - 1)

enum {
	CNSS_STATISTIC_GENL_ATTR_UNSPEC,
	CNSS_STATISTIC_GENL_ATTR_PROTO_SUBTYPE,
	CNSS_STATISTIC_GENL_ATTR_SRC_PORT,
	CNSS_STATISTIC_GENL_ATTR_DST_PORT,
	__CNSS_STATISTIC_GENL_ATTR_MAX,
};
#define CNSS_STATISTIC_GENL_ATTR_MAX (__CNSS_STATISTIC_GENL_ATTR_MAX - 1)

static struct nla_policy cnss_statistic_genl_policy[CNSS_STATISTIC_GENL_ATTR_MAX + 1] = {
	[CNSS_STATISTIC_GENL_ATTR_PROTO_SUBTYPE] = { .type = NLA_U16 },
	[CNSS_STATISTIC_GENL_ATTR_SRC_PORT] = { .type = NLA_U16 },
	[CNSS_STATISTIC_GENL_ATTR_DST_PORT] = { .type = NLA_U16 },
};

static int wow_statistic_enable = WOW_CLOUD_CTRL_DISABLED;

int cnss_statistic_wow_wakeup(u16 proto_subtype, u16 src_port, u16 target_port);

static int cnss_statistic_genl_debug(struct sk_buff *skb, struct genl_info *info)
{
	pr_debug("mi_cnss_statistic: todo");
	return 0;
}

static int cnss_statistic_genl_enable(struct sk_buff *skb, struct genl_info *info)
{
	pr_info("mi_cnss_statistic: enable");
	wow_statistic_enable = WOW_CLOUD_CTRL_ENABLED;
	return 0;
}

static int cnss_statistic_genl_disable(struct sk_buff *skb, struct genl_info *info)
{
	pr_info("mi_cnss_statistic: disable");
	wow_statistic_enable = WOW_CLOUD_CTRL_DISABLED;
	return 0;
}

static struct genl_ops cnss_statistic_genl_ops[] = {
	{
		.cmd = CNSS_STATISTIC_GENL_CMD_DEBUG,
		.doit = cnss_statistic_genl_debug,
	},
	{
		.cmd = CNSS_STATISTIC_GENL_CMD_ENABLE,
		.doit = cnss_statistic_genl_enable,
	},
	{
		.cmd = CNSS_STATISTIC_GENL_CMD_DISABLE,
		.doit = cnss_statistic_genl_disable,
	},
};

static struct genl_multicast_group cnss_statistic_genl_mcgrps[] = {
	{
		.name = CNSS_STATISTIC_MULTICAST_GROUP_EVENT,
	},
};

static struct genl_family cnss_statistic_genl_family = {
	.id = 0,
	.hdrsize = 0,
	.name = CNSS_STATISTIC_GENL_NAME,
	.version = CNSS_STATISTIC_GENL_VERSION,
	.maxattr = CNSS_STATISTIC_GENL_ATTR_MAX,
	.module = THIS_MODULE,
	.ops = cnss_statistic_genl_ops,
	.policy = cnss_statistic_genl_policy,
	.n_ops = ARRAY_SIZE(cnss_statistic_genl_ops),
	.mcgrps = cnss_statistic_genl_mcgrps,
	.n_mcgrps = ARRAY_SIZE(cnss_statistic_genl_mcgrps),
};

int cnss_statistic_wow_wakeup(u16 proto_subtype, u16 src_port, u16 target_port)
{
	struct sk_buff *msg;
	void *hdr;
	int ret = 0;

	if (wow_suspend_type != QDF_SYSTEM_SUSPEND ||
		wow_statistic_enable != WOW_CLOUD_CTRL_ENABLED)
		return 0;
	msg = nlmsg_new(NLMSG_DEFAULT_SIZE,GFP_ATOMIC);
	if (!msg)
		return -ENOMEM;

	hdr = genlmsg_put(msg, 0, 0, &cnss_statistic_genl_family, 0,
			  CNSS_STATISTIC_GENL_EVENT_WOW_WAKEUP);
	if (!hdr)
		goto free_msg;

	ret = nla_put_u16(msg, CNSS_STATISTIC_GENL_ATTR_PROTO_SUBTYPE, proto_subtype);
	if (ret < 0)
		goto nla_put_failure;
	ret = nla_put_u16(msg, CNSS_STATISTIC_GENL_ATTR_SRC_PORT, src_port);
	if (ret < 0)
		goto nla_put_failure;
	ret = nla_put_u16(msg, CNSS_STATISTIC_GENL_ATTR_DST_PORT, target_port);
	if (ret < 0)
		goto nla_put_failure;

	genlmsg_end(msg, hdr);
	ret = genlmsg_multicast(&cnss_statistic_genl_family, msg, 0, 0, GFP_ATOMIC);
	if (ret < 0)
		pr_err("cnss_statistic: Failed to send genl msg: %d\n", ret);

	return ret;

nla_put_failure:
free_msg:
	nlmsg_free(msg);
	return ret;
}

EXPORT_SYMBOL(cnss_statistic_wow_wakeup);

static int __cnss_statistic_init(void)
{
	int err;
	pr_info("mi_cnss_statistic: Initializing\n");
	err = genl_register_family(&cnss_statistic_genl_family);
	if (err)
		pr_err("mi_cnss_statistic: Failed to register cnss_statistic family: %d\n",
		       err);
	return err;
}

static void __cnss_statistic_exit(void)
{
	genl_unregister_family(&cnss_statistic_genl_family);
}

static int __init cnss_statistic_init(void)
{
	return __cnss_statistic_init();
}

static void cnss_statistic_exit(void)
{
	__cnss_statistic_exit();
}

module_init(cnss_statistic_init);
module_exit(cnss_statistic_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MI CNSS STATISTIC module");


