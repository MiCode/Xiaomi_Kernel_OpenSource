// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved. */

#include <net/genetlink.h>
#include <net/cnss_nl.h>
#include <linux/module.h>

#define CLD80211_GENL_NAME "cld80211"

#define CLD80211_MULTICAST_GROUP_SVC_MSGS       "svc_msgs"
#define CLD80211_MULTICAST_GROUP_HOST_LOGS      "host_logs"
#define CLD80211_MULTICAST_GROUP_FW_LOGS        "fw_logs"
#define CLD80211_MULTICAST_GROUP_PER_PKT_STATS  "per_pkt_stats"
#define CLD80211_MULTICAST_GROUP_DIAG_EVENTS    "diag_events"
#define CLD80211_MULTICAST_GROUP_FATAL_EVENTS   "fatal_events"
#define CLD80211_MULTICAST_GROUP_OEM_MSGS       "oem_msgs"

static const struct genl_multicast_group nl_mcgrps[] = {
	[CLD80211_MCGRP_SVC_MSGS] = { .name =
			CLD80211_MULTICAST_GROUP_SVC_MSGS},
	[CLD80211_MCGRP_HOST_LOGS] = { .name =
			CLD80211_MULTICAST_GROUP_HOST_LOGS},
	[CLD80211_MCGRP_FW_LOGS] = { .name =
			CLD80211_MULTICAST_GROUP_FW_LOGS},
	[CLD80211_MCGRP_PER_PKT_STATS] = { .name =
			CLD80211_MULTICAST_GROUP_PER_PKT_STATS},
	[CLD80211_MCGRP_DIAG_EVENTS] = { .name =
			CLD80211_MULTICAST_GROUP_DIAG_EVENTS},
	[CLD80211_MCGRP_FATAL_EVENTS] = { .name =
			CLD80211_MULTICAST_GROUP_FATAL_EVENTS},
	[CLD80211_MCGRP_OEM_MSGS] = { .name =
			CLD80211_MULTICAST_GROUP_OEM_MSGS},
};

struct cld_ops {
	cld80211_cb cb;
	void *cb_ctx;
};

struct cld80211_nl_data {
	struct cld_ops cld_ops[CLD80211_MAX_COMMANDS];
};

static struct cld80211_nl_data nl_data;

static inline struct cld80211_nl_data *get_local_ctx(void)
{
	return &nl_data;
}

static struct genl_ops nl_ops[CLD80211_MAX_COMMANDS];

/* policy for the attributes */
static const struct nla_policy cld80211_policy[CLD80211_ATTR_MAX + 1] = {
	[CLD80211_ATTR_VENDOR_DATA] = { .type = NLA_NESTED },
	[CLD80211_ATTR_DATA] = { .type = NLA_BINARY,
				 .len = CLD80211_MAX_NL_DATA },
	[CLD80211_ATTR_META_DATA] = { .type = NLA_BINARY,
				 .len = CLD80211_MAX_NL_DATA },
	[CLD80211_ATTR_CMD] = { .type = NLA_U32 },
	[CLD80211_ATTR_CMD_TAG_DATA] = { .type = NLA_NESTED },
};

static int cld80211_pre_doit(const struct genl_ops *ops, struct sk_buff *skb,
			     struct genl_info *info)
{
	u8 cmd_id = ops->cmd;
	struct cld80211_nl_data *nl = get_local_ctx();

	if (cmd_id < 1 || cmd_id > CLD80211_MAX_COMMANDS) {
		pr_err("CLD80211: Command Not supported: %u\n", cmd_id);
		return -EOPNOTSUPP;
	}
	info->user_ptr[0] = nl->cld_ops[cmd_id - 1].cb;
	info->user_ptr[1] = nl->cld_ops[cmd_id - 1].cb_ctx;

	return 0;
}

/* The netlink family */
static struct genl_family cld80211_fam __ro_after_init = {
	.name = CLD80211_GENL_NAME,
	.hdrsize = 0,			/* no private header */
	.version = 1,			/* no particular meaning now */
	.maxattr = CLD80211_ATTR_MAX,
	.policy = cld80211_policy,
	.netnsok = true,
	.pre_doit = cld80211_pre_doit,
	.post_doit = NULL,
	.module = THIS_MODULE,
	.ops = nl_ops,
	.n_ops = ARRAY_SIZE(nl_ops),
	.mcgrps = nl_mcgrps,
	.n_mcgrps = ARRAY_SIZE(nl_mcgrps),
};

int register_cld_cmd_cb(u8 cmd_id, cld80211_cb func, void *cb_ctx)
{
	struct cld80211_nl_data *nl = get_local_ctx();

	pr_debug("CLD80211: Registering command: %d\n", cmd_id);
	if (!cmd_id || cmd_id > CLD80211_MAX_COMMANDS) {
		pr_debug("CLD80211: invalid command: %d\n", cmd_id);
		return -EINVAL;
	}

	nl->cld_ops[cmd_id - 1].cb = func;
	nl->cld_ops[cmd_id - 1].cb_ctx = cb_ctx;

	return 0;
}
EXPORT_SYMBOL(register_cld_cmd_cb);

int deregister_cld_cmd_cb(u8 cmd_id)
{
	struct cld80211_nl_data *nl = get_local_ctx();

	pr_debug("CLD80211: De-registering command: %d\n", cmd_id);
	if (!cmd_id || cmd_id > CLD80211_MAX_COMMANDS) {
		pr_debug("CLD80211: invalid command: %d\n", cmd_id);
		return -EINVAL;
	}

	nl->cld_ops[cmd_id - 1].cb = NULL;
	nl->cld_ops[cmd_id - 1].cb_ctx = NULL;

	return 0;
}
EXPORT_SYMBOL(deregister_cld_cmd_cb);

struct genl_family *cld80211_get_genl_family(void)
{
	return &cld80211_fam;
}
EXPORT_SYMBOL(cld80211_get_genl_family);

static int cld80211_doit(struct sk_buff *skb, struct genl_info *info)
{
	cld80211_cb cld_cb;
	void *cld_ctx;

	cld_cb = info->user_ptr[0];

	if (!cld_cb) {
		pr_err("CLD80211: Not supported\n");
		return -EOPNOTSUPP;
	}
	cld_ctx = info->user_ptr[1];

	if (info->attrs[CLD80211_ATTR_VENDOR_DATA]) {
		cld_cb(nla_data(info->attrs[CLD80211_ATTR_VENDOR_DATA]),
		       nla_len(info->attrs[CLD80211_ATTR_VENDOR_DATA]),
		       cld_ctx, info->snd_portid);
	} else {
		pr_err("CLD80211: No CLD80211_ATTR_VENDOR_DATA\n");
		return -EINVAL;
	}
	return 0;
}

static int __cld80211_init(void)
{
	int err, i;

	memset(&nl_ops[0], 0, sizeof(nl_ops));

	pr_info("CLD80211: Initializing\n");
	for (i = 0; i < CLD80211_MAX_COMMANDS; i++) {
		nl_ops[i].cmd = i + 1;
		nl_ops[i].doit = cld80211_doit;
		nl_ops[i].flags = GENL_ADMIN_PERM;
	}

	err = genl_register_family(&cld80211_fam);
	if (err) {
		pr_err("CLD80211: Failed to register cld80211 family: %d\n",
		       err);
	}

	return err;
}

static void __cld80211_exit(void)
{
	genl_unregister_family(&cld80211_fam);
}

static int __init cld80211_init(void)
{
	return __cld80211_init();
}

static void __exit cld80211_exit(void)
{
	__cld80211_exit();
}

module_init(cld80211_init);
module_exit(cld80211_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CNSS generic netlink module");
