// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/skbuff.h>
#include <net/genetlink.h>

#include "connsys_debug_utility.h"
#include "conndump_netlink.h"


/*******************************************************************************
*                             MACROS
********************************************************************************
*/
#define MAX_BIND_PROCESS    (4)

#define CONNSYS_DUMP_NETLINK_FAMILY_NAME_WIFI	"CONNDUMP_WIFI"
#define CONNSYS_DUMP_NETLINK_FAMILY_NAME_BT	"CONNDUMP_BT"

#ifndef GENL_ID_GENERATE
#define GENL_ID_GENERATE	0
#endif

#define CONNSYS_DUMP_PKT_SIZE NLMSG_DEFAULT_SIZE

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

enum {
	__CONNDUMP_ATTR_INVALID,
	CONNDUMP_ATTR_MSG,
	CONNDUMP_ATTR_PORT,
	CONNDUMP_ATTR_HEADER,
	CONNDUMP_ATTR_MSG_SIZE,
	CONNDUMP_ATTR_LAST,
	__CONNDUMP_ATTR_MAX,
};
#define CONNDUMP_ATTR_MAX       (__CONNDUMP_ATTR_MAX - 1)

enum {
	__CONNDUMP_COMMAND_INVALID,
	CONNDUMP_COMMAND_BIND,
	CONNDUMP_COMMAND_DUMP,
	CONNDUMP_COMMAND_END,
	CONNDUMP_COMMAND_RESET,
	__CONNDUMP_COMMAND_MAX,
};

enum LINK_STATUS {
	LINK_STATUS_INIT,
	LINK_STATUS_INIT_DONE,
	LINK_STATUS_MAX,
};

struct dump_netlink_ctx {
	int conn_type;
	pid_t bind_pid[MAX_BIND_PROCESS];
	unsigned int num_bind_process;
	struct genl_family gnl_family;
	unsigned int seqnum;
	struct mutex nl_lock;
	enum LINK_STATUS status;
	void* coredump_ctx;
	struct netlink_event_cb cb;
};

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static int conndump_nl_bind_wifi(struct sk_buff *skb, struct genl_info *info);
static int conndump_nl_dump_wifi(struct sk_buff *skb, struct genl_info *info);
static int conndump_nl_dump_end_wifi(struct sk_buff *skb, struct genl_info *info);
static int conndump_nl_reset_wifi(struct sk_buff *skb, struct genl_info *info);
static int conndump_nl_bind_bt(struct sk_buff *skb, struct genl_info *info);
static int conndump_nl_dump_bt(struct sk_buff *skb, struct genl_info *info);
static int conndump_nl_dump_end_bt(struct sk_buff *skb, struct genl_info *info);
static int conndump_nl_reset_bt(struct sk_buff *skb, struct genl_info *info);

/*******************************************************************************
*                  G L O B A L  V A R I A B L E
********************************************************************************
*/

/* attribute policy */
static struct nla_policy conndump_genl_policy[CONNDUMP_ATTR_MAX + 1] = {
	[CONNDUMP_ATTR_MSG] = {.type = NLA_NUL_STRING},
	[CONNDUMP_ATTR_PORT] = {.type = NLA_U32},
	[CONNDUMP_ATTR_HEADER] = {.type = NLA_NUL_STRING},
	[CONNDUMP_ATTR_MSG_SIZE] = {.type = NLA_U32},
	[CONNDUMP_ATTR_LAST] = {.type = NLA_U32},
};


/* operation definition */
static struct genl_ops conndump_gnl_ops_array_wifi[] = {
	{
		.cmd = CONNDUMP_COMMAND_BIND,
		.flags = 0,
		.policy = conndump_genl_policy,
		.doit = conndump_nl_bind_wifi,
		.dumpit = NULL,
	},
	{
		.cmd = CONNDUMP_COMMAND_DUMP,
		.flags = 0,
		.policy = conndump_genl_policy,
		.doit = conndump_nl_dump_wifi,
		.dumpit = NULL,
	},
	{
		.cmd = CONNDUMP_COMMAND_END,
		.flags = 0,
		.policy = conndump_genl_policy,
		.doit = conndump_nl_dump_end_wifi,
		.dumpit = NULL,
	},
	{
		.cmd = CONNDUMP_COMMAND_RESET,
		.flags = 0,
		.policy = conndump_genl_policy,
		.doit = conndump_nl_reset_wifi,
		.dumpit = NULL,
	},
};

static struct genl_ops conndump_gnl_ops_array_bt[] = {
	{
		.cmd = CONNDUMP_COMMAND_BIND,
		.flags = 0,
		.policy = conndump_genl_policy,
		.doit = conndump_nl_bind_bt,
		.dumpit = NULL,
	},
	{
		.cmd = CONNDUMP_COMMAND_DUMP,
		.flags = 0,
		.policy = conndump_genl_policy,
		.doit = conndump_nl_dump_bt,
		.dumpit = NULL,
	},
	{
		.cmd = CONNDUMP_COMMAND_END,
		.flags = 0,
		.policy = conndump_genl_policy,
		.doit = conndump_nl_dump_end_bt,
		.dumpit = NULL,
	},
	{
		.cmd = CONNDUMP_COMMAND_RESET,
		.flags = 0,
		.policy = conndump_genl_policy,
		.doit = conndump_nl_reset_bt,
		.dumpit = NULL,
	},
};

struct dump_netlink_ctx g_netlink_ctx[2] = {
	/* WIFI */
	{
		.conn_type = CONN_DEBUG_TYPE_WIFI,
		.gnl_family = {
			.id = GENL_ID_GENERATE,
			.hdrsize = 0,
			.name = CONNSYS_DUMP_NETLINK_FAMILY_NAME_WIFI,
			.version = 1,
			.maxattr = CONNDUMP_ATTR_MAX,
			.ops = conndump_gnl_ops_array_wifi,
			.n_ops = ARRAY_SIZE(conndump_gnl_ops_array_wifi),
		},
		.status = LINK_STATUS_INIT,
		.num_bind_process = 0,
		.seqnum = 0,
	},
	/* BT */
	{
		.conn_type = CONN_DEBUG_TYPE_BT,
		.gnl_family = {
			.id = GENL_ID_GENERATE,
			.hdrsize = 0,
			.name = CONNSYS_DUMP_NETLINK_FAMILY_NAME_BT,
			.version = 1,
			.maxattr = CONNDUMP_ATTR_MAX,
			.ops = conndump_gnl_ops_array_bt,
			.n_ops = ARRAY_SIZE(conndump_gnl_ops_array_bt),
		},
		.status = LINK_STATUS_INIT,
		.num_bind_process = 0,
		.seqnum = 0,
	},
};

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

static int conndump_nl_bind_internal(struct dump_netlink_ctx* ctx, struct sk_buff *skb, struct genl_info *info)
{
	int i;
	struct nlattr *port_na;
	unsigned int port;

	if (info == NULL)
		goto out;

	if (mutex_lock_killable(&ctx->nl_lock))
		return -1;

	port_na = info->attrs[CONNDUMP_ATTR_PORT];
	if (port_na) {
		port = (unsigned int)nla_get_u32(port_na);
	} else {
		pr_err("%s:-> no port_na found\n");
		return -1;
	}

	for (i = 0; i < MAX_BIND_PROCESS; i ++) {
		if (ctx->bind_pid[i] == 0) {
			ctx->bind_pid[i] = port;
			ctx->num_bind_process++;
			pr_info("%s():-> pid  = %d\n", __func__, port);
			break;
		}
	}
	if (i == MAX_BIND_PROCESS) {
		pr_err("%s(): exceeding binding limit %d\n", __func__, MAX_BIND_PROCESS);
	}
	mutex_unlock(&ctx->nl_lock);

out:
	return 0;
}

static int conndump_nl_dump_end_internal(struct dump_netlink_ctx* ctx, struct sk_buff *skb, struct genl_info *info)
{
	if (ctx && ctx->cb.coredump_end) {
		pr_info("Get coredump end command, type=%d", ctx->conn_type);
		ctx->cb.coredump_end(ctx->coredump_ctx);
	}
	return 0;
}

static int conndump_nl_bind_wifi(struct sk_buff *skb, struct genl_info *info)
{
	int ret = 0;

	ret = conndump_nl_bind_internal(&g_netlink_ctx[CONN_DEBUG_TYPE_WIFI], skb, info);
	return ret;
}

static int conndump_nl_dump_wifi(struct sk_buff *skb, struct genl_info *info)
{
	pr_err("%s(): should not be invoked\n", __func__);

	return 0;
}

static int conndump_nl_dump_end_wifi(struct sk_buff *skb, struct genl_info *info)
{
	int ret = 0;

	ret = conndump_nl_dump_end_internal(&g_netlink_ctx[CONN_DEBUG_TYPE_WIFI], skb, info);

	return ret;
}

static int conndump_nl_reset_wifi(struct sk_buff *skb, struct genl_info *info)
{
	pr_err("%s(): should not be invoked\n", __func__);

	return 0;
}

static int conndump_nl_bind_bt(struct sk_buff *skb, struct genl_info *info)
{
	int ret = 0;

	ret = conndump_nl_bind_internal(&g_netlink_ctx[CONN_DEBUG_TYPE_BT], skb, info);
	return ret;
}

static int conndump_nl_dump_bt(struct sk_buff *skb, struct genl_info *info)
{
	pr_err("%s(): should not be invoked\n", __func__);

	return 0;
}

static int conndump_nl_dump_end_bt(struct sk_buff *skb, struct genl_info *info)
{
	int ret = 0;

	ret = conndump_nl_dump_end_internal(&g_netlink_ctx[CONN_DEBUG_TYPE_BT], skb, info);

	return ret;

}

static int conndump_nl_reset_bt(struct sk_buff *skb, struct genl_info *info)
{
	pr_err("%s(): should not be invoked\n", __func__);

	return 0;
}

/*****************************************************************************
 * FUNCTION
 *  conndump_netlink_init
 * DESCRIPTION
 *
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
int conndump_netlink_init(int conn_type, void* dump_ctx, struct netlink_event_cb* cb)
{
	int ret = 0;
	struct dump_netlink_ctx* ctx;

	if (conn_type < CONN_DEBUG_TYPE_WIFI || conn_type > CONN_DEBUG_TYPE_BT) {
		pr_err("Incorrect type (%d)\n", conn_type);
		return -1;
	}

	ctx = &g_netlink_ctx[conn_type];
	mutex_init(&ctx->nl_lock);
	ret = genl_register_family(&ctx->gnl_family);
	if (ret != 0) {
		pr_err("%s(): GE_NELINK family registration fail (ret=%d)\n", __func__, ret);
		return -2;
	}
	ctx->status = LINK_STATUS_INIT_DONE;
	memset(ctx->bind_pid, 0, sizeof(ctx->bind_pid));
	ctx->coredump_ctx = dump_ctx;
	memcpy(&(ctx->cb), cb, sizeof(struct netlink_event_cb));

	return ret;
}

int conndump_netlink_msg_send(struct dump_netlink_ctx* ctx, char* tag, char* buf, unsigned int length, pid_t pid, unsigned int seq)
{
	struct sk_buff *skb;
	void* msg_head = NULL;
	int ret = 0;
	int conn_type = ctx->conn_type;

	/* Allocating a Generic Netlink message buffer */
	skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (skb != NULL) {
		/* Create message header */
		msg_head = genlmsg_put(skb, 0, seq, &ctx->gnl_family, 0, CONNDUMP_COMMAND_DUMP);
		if (msg_head == NULL) {
			pr_err("%s(): genlmsg_put fail(conn_type=%d).\n", __func__, conn_type);
			nlmsg_free(skb);
			return -EMSGSIZE;
		}
		/* Add message attribute and content */
		ret = nla_put_string(skb, CONNDUMP_ATTR_HEADER, tag);
		if (ret != 0) {
			pr_err("%s(): nla_put_string header fail(conn_type=%d): %d\n", __func__, conn_type, ret);
			genlmsg_cancel(skb, msg_head);
			nlmsg_free(skb);
			return ret;
		}
		if (length) {
			ret = nla_put(skb, CONNDUMP_ATTR_MSG, length, buf);
			if (ret != 0) {
				pr_err("%s(): nla_put fail(conn_type=%d): %d\n", __func__, conn_type, ret);
				genlmsg_cancel(skb, msg_head);
				nlmsg_free(skb);
				return ret;
			}
			ret = nla_put_u32(skb, CONNDUMP_ATTR_MSG_SIZE, length);
			if (ret != 0) {
				pr_err("%s(): nal_put_u32 fail(conn_type=%d): %d\n", __func__, conn_type, ret);
				genlmsg_cancel(skb, msg_head);
				nlmsg_free(skb);
				return ret;
			}
		}
		/* finalize the message */
		genlmsg_end(skb, msg_head);

		/* sending message */
		ret = genlmsg_unicast(&init_net, skb, pid);
	} else {
		pr_err("Allocate message error\n");
		ret = -ENOMEM;
	}

	return ret;
}

/*****************************************************************************
 * FUNCTION
 *  conndump_send_to_native
 * DESCRIPTION
 *  Send dump content to native layer (AEE or dump service)
 * PARAMETERS
 *  conn_type      [IN]        subsys type
 *  tag            [IN]        the tag for the content (null end string)
 *                             [M] for coredump
 *  buf            [IN]        the content to dump (a buffer may not have end character)
 *  length         [IN]        dump length
 * RETURNS
 *
 *****************************************************************************/
int conndump_netlink_send_to_native_internal(struct dump_netlink_ctx* ctx, char* tag, char* buf, unsigned int length)
{
	int killed_num = 0;
	int i, j, ret = 0;
	unsigned int retry;

	for (i = 0; i < ctx->num_bind_process; i++) {
		ret =conndump_netlink_msg_send(ctx, tag, buf, length, ctx->bind_pid[i], ctx->seqnum);
		if (ret != 0) {
			pr_err("%s(): genlmsg_unicast fail (ret=%d): pid = %d seq=%d tag=%s\n",
				__func__, ret, ctx->bind_pid[i], ctx->seqnum, tag);
			if (ret == -EAGAIN) {
				retry = 0;
				while (retry < 100 && ret == -EAGAIN) {
					msleep(10);
					ret =conndump_netlink_msg_send(ctx, tag, buf, length, ctx->bind_pid[i], ctx->seqnum);
					retry ++;
					pr_err("%s(): genlmsg_unicast retry (%d)...: ret = %d pid = %d seq=%d tag=%s\n",
							__func__, retry, ret, ctx->bind_pid[i], ctx->seqnum, tag);
				}
				if (ret) {
					pr_err("%s(): genlmsg_unicast fail (ret=%d) after retry %d times: pid = %d seq=%d tag=%s\n",
							__func__, ret, retry, ctx->bind_pid[i], ctx->seqnum, tag);
				}
			}
			if (ret == -ECONNREFUSED) {
				ctx->bind_pid[i] = 0;
				killed_num++;
			}
		}
	}

	ctx->seqnum ++;

	/* Clean up invalid bind_pid */
	if (killed_num > 0) {
		if (mutex_lock_killable(&ctx->nl_lock)) {
		/* if fail to get lock, it is fine to update bind_pid[] later */
			return ret;
		}
		for (i = 0; i < ctx->num_bind_process - killed_num; i++) {
			if (ctx->bind_pid[i] == 0) {
				for (j = ctx->num_bind_process - 1; j > i; j--) {
					if (ctx->bind_pid[j] > 0) {
						ctx->bind_pid[i] = ctx->bind_pid[j];
						ctx->bind_pid[j] = 0;
					}
				}
			}
		}
		ctx->num_bind_process -= killed_num;
		mutex_unlock(&ctx->nl_lock);
	}

	return ret;
}


/*****************************************************************************
 * FUNCTION
 *  conndump_send_to_native
 * DESCRIPTION
 *  Send dump content to native layer (AEE or dump service)
 * PARAMETERS
 *  conn_type      [IN]        subsys type
 *  tag            [IN]        the tag for the content (null end string)
 *                             [M] for coredump
 *  buf            [IN]        the content to dump (a buffer may not have end character)
 *  length         [IN]        dump length
 * RETURNS
 *
 *****************************************************************************/
int conndump_netlink_send_to_native(int conn_type, char* tag, char* buf, unsigned int length)
{
	struct dump_netlink_ctx* ctx;
	int idx = 0;
	unsigned int send_len;
	unsigned int remain_len = length;
	int ret;

	pr_info("[%s] conn_type=%d tag=%s buf=0x%x length=%d\n",
		__func__, conn_type, tag, buf, length);
	if ((conn_type < CONN_DEBUG_TYPE_WIFI || conn_type > CONN_DEBUG_TYPE_BT) || tag == NULL) {
		pr_err("Incorrect type (%d), tag = %s\n", conn_type, tag);
		return -1;
	}

	ctx = &g_netlink_ctx[conn_type];
	if (ctx->status != LINK_STATUS_INIT_DONE) {
		pr_err("%s(): netlink should be init (type=%d).\n", __func__, conn_type);
		return -2;
	}

	if (ctx->num_bind_process == 0) {
		pr_err("No bind service\n");
		return -3;
	}

	while (remain_len) {
		send_len = (remain_len > CONNSYS_DUMP_PKT_SIZE? CONNSYS_DUMP_PKT_SIZE : remain_len);
		ret = conndump_netlink_send_to_native_internal(ctx, tag, &buf[idx], send_len);
		if (ret) {
			pr_err("[%s] from %d with len=%d fail, ret=%d\n", __func__, idx, send_len, ret);
			break;
		}
		remain_len -= send_len;
		idx += send_len;
	}
	return idx;
}

