// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*******************************************************************************/
/*                    E X T E R N A L   R E F E R E N C E S                    */
/*******************************************************************************/
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/skbuff.h>
#include <net/genetlink.h>
#include "aoltest_netlink.h"

/*******************************************************************************/
/*                             M A C R O S                                     */
/*******************************************************************************/
#define AOLTEST_NETLINK_FAMILY_NAME "AOL_TEST"
#define AOLTEST_PKT_SIZE NLMSG_DEFAULT_SIZE
#define MAX_BIND_PROCESS    4

#ifndef GENL_ID_GENERATE
#define GENL_ID_GENERATE    0
#endif

#define AOLTEST_ATTR_MAX       (__AOL_ATTR_MAX - 1)

/*******************************************************************************/
/*                             D A T A   T Y P E S                             */
/*******************************************************************************/
// Define netlink command id
enum aoltest_netlink_cmd_id {
	_AOLTEST_NL_CMD_INVALID,
	AOLTEST_NL_CMD_BIND,
	AOLTEST_NL_CMD_SEND,
	AOLTEST_NL_CMD_START_TEST,
	AOLTEST_NL_CMD_STOP_TEST,
	AOLTEST_NL_CMD_START_DATA_TRANS,
	AOLTEST_NL_CMD_STOP_DATA_TRANS,
	_AOLTEST_NL_CMD_MAX,
};

// Define netlink message formats
enum aoltest_attr {
	_AOLTEST_ATTR_DEFAULT,
	AOLTEST_ATTR_PORT,
	AOLTEST_ATTR_HEADER,
	AOLTEST_ATTR_MSG,
	AOLTEST_ATTR_MSG_ID,
	AOLTEST_ATTR_MSG_SIZE,
	AOLTEST_ATTR_TEST_INFO,
	_AOLTEST_ATTR_MAX,
};

enum testinfo_attr {
	_TESTINFO_ATTR_DEFAULT,
	TESTINFO_ATTR_WIFI_ENABLED,
	TESTINFO_ATTR_WIFI_SCAN_INTVL,
	TESTINFO_ATTR_WIFI_CB_INTVL,
	TESTINFO_ATTR_BT_ENABLED,
	TESTINFO_ATTR_BT_SCAN_INTVL,
	TESTINFO_ATTR_BT_CB_INTVL,
	TESTINFO_ATTR_GPS_ENABLED,
	TESTINFO_ATTR_GPS_SCAN_INTVL,
	TESTINFO_ATTR_GPS_CB_INTVL,
	_TESTINFO_ATTR_MAX,
};

enum link_status {
	LINK_STATUS_INIT,
	LINK_STATUS_INIT_DONE,
	LINK_STATUS_MAX,
};

struct aoltest_netlink_ctx {
	pid_t bind_pid;
	struct genl_family gnl_family;
	unsigned int seqnum;
	struct mutex nl_lock;
	enum link_status status;
	struct netlink_event_cb cb;
};

/*******************************************************************************/
/*                  F U N C T I O N   D E C L A R A T I O N S                  */
/*******************************************************************************/
static int aoltest_nl_bind(struct sk_buff *skb, struct genl_info *info);
static int aoltest_nl_start_test(struct sk_buff *skb, struct genl_info *info);
static int aoltest_nl_stop_test(struct sk_buff *skb, struct genl_info *info);
static int aoltest_nl_start_data_transmission(struct sk_buff *skb, struct genl_info *info);
static int aoltest_nl_stop_data_transmission(struct sk_buff *skb, struct genl_info *info);

/*******************************************************************************/
/*                  G L O B A L  V A R I A B L E                               */
/*******************************************************************************/
/* Attribute policy */
static struct nla_policy aoltest_genl_policy[_AOLTEST_ATTR_MAX + 1] = {
	[AOLTEST_ATTR_PORT] = {.type = NLA_U32},
	[AOLTEST_ATTR_HEADER] = {.type = NLA_NUL_STRING},
	[AOLTEST_ATTR_MSG] = {.type = NLA_NUL_STRING},
	[AOLTEST_ATTR_MSG_ID] = {.type = NLA_U32},
	[AOLTEST_ATTR_MSG_SIZE] = {.type = NLA_U32},
	[AOLTEST_ATTR_TEST_INFO] = {.type = NLA_NESTED},
	//[AOLTEST_ATTR_TEST_INFO] = {.type = NLA_UNSPEC, .len = sizeof(struct test_info)},
};

static struct nla_policy test_info_policy[_TESTINFO_ATTR_MAX + 1] = {
	[TESTINFO_ATTR_WIFI_ENABLED]	= {.type = NLA_U32},
	[TESTINFO_ATTR_WIFI_SCAN_INTVL]	= {.type = NLA_U32},
	[TESTINFO_ATTR_WIFI_CB_INTVL]	= {.type = NLA_U32},
	[TESTINFO_ATTR_BT_ENABLED]	= {.type = NLA_U32},
	[TESTINFO_ATTR_BT_SCAN_INTVL]	= {.type = NLA_U32},
	[TESTINFO_ATTR_BT_CB_INTVL]	= {.type = NLA_U32},
	[TESTINFO_ATTR_GPS_ENABLED]	= {.type = NLA_U32},
	[TESTINFO_ATTR_GPS_SCAN_INTVL]	= {.type = NLA_U32},
	[TESTINFO_ATTR_GPS_CB_INTVL]	= {.type = NLA_U32},
};

/* Operation definition */
static struct genl_ops aoltest_gnl_ops_array[] = {
	{
		.cmd = AOLTEST_NL_CMD_BIND,
		.flags = 0,
		.policy = aoltest_genl_policy,
		.doit = aoltest_nl_bind,
		.dumpit = NULL,
	},
	{
		.cmd = AOLTEST_NL_CMD_START_TEST,
		.flags = 0,
		.policy = aoltest_genl_policy,
		.doit = aoltest_nl_start_test,
		.dumpit = NULL,
	},
	{
		.cmd = AOLTEST_NL_CMD_STOP_TEST,
		.flags = 0,
		.policy = aoltest_genl_policy,
		.doit = aoltest_nl_stop_test,
		.dumpit = NULL,
	},
	{
		.cmd = AOLTEST_NL_CMD_START_DATA_TRANS,
		.flags = 0,
		.policy = aoltest_genl_policy,
		.doit = aoltest_nl_start_data_transmission,
		.dumpit = NULL,
	},
	{
		.cmd = AOLTEST_NL_CMD_STOP_DATA_TRANS,
		.flags = 0,
		.policy = aoltest_genl_policy,
		.doit = aoltest_nl_stop_data_transmission,
		.dumpit = NULL,
	},
};

const struct genl_multicast_group g_mcgrps = {
	.name = "AOL_TEST",
};

struct aoltest_netlink_ctx g_aoltest_netlink_ctx = {
	.gnl_family = {
		.id = GENL_ID_GENERATE,
		.hdrsize = 0,
		.name = AOLTEST_NETLINK_FAMILY_NAME,
		.version = 1,
		.maxattr = _AOLTEST_ATTR_MAX,
		.ops = aoltest_gnl_ops_array,
		.n_ops = ARRAY_SIZE(aoltest_gnl_ops_array),
	},
	.status = LINK_STATUS_INIT,
	.seqnum = 0,
};

struct aoltest_netlink_ctx *g_ctx = &g_aoltest_netlink_ctx;
static bool g_already_bind;

/*******************************************************************************/
/*                              F U N C T I O N S                              */
/*******************************************************************************/
static int aoltest_nl_bind(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *port_na;
	unsigned int port;

	pr_info("[%s]\n", __func__);

	if (info == NULL)
		goto out;

	if (mutex_lock_killable(&g_ctx->nl_lock))
		return -1;

	port_na = info->attrs[AOLTEST_ATTR_PORT];
	if (port_na) {
		port = (unsigned int)nla_get_u32(port_na);
	} else {
		pr_info("[%s] No port_na found\n", __func__);
		mutex_unlock(&g_ctx->nl_lock);
		return -1;
	}

	if (g_already_bind) {
		pr_info("[%s] Already bind before, only change port=[%d]", __func__, port);
		g_ctx->bind_pid = port;
		mutex_unlock(&g_ctx->nl_lock);
		goto out;
	}

	g_ctx->bind_pid = port;
	g_already_bind = true;

	mutex_unlock(&g_ctx->nl_lock);

	if (g_ctx && g_ctx->cb.aoltest_bind)
		g_ctx->cb.aoltest_bind();

out:
	return 0;
}

static int aoltest_nl_start_test(struct sk_buff *skb, struct genl_info *info)
{
	int ret = 0;
	struct nlattr *attr_info = NULL;
	struct nlattr *test_attr[_TESTINFO_ATTR_MAX + 1];
	struct test_info params = {0};

	pr_info("[%s]", __func__);
	if (mutex_lock_killable(&g_ctx->nl_lock))
		return -1;

	attr_info = info->attrs[AOLTEST_ATTR_TEST_INFO];

	if (attr_info) {
		ret = nla_parse_nested_deprecated(test_attr, _TESTINFO_ATTR_MAX,
						attr_info, test_info_policy, NULL);
		if (ret < 0) {
			pr_info("[%s] Fail to parse nested attributes, ret=[%d]\n", __func__, ret);
			mutex_unlock(&g_ctx->nl_lock);
			return -1;
		}
		// WiFi
		if (test_attr[TESTINFO_ATTR_WIFI_ENABLED])
			params.wifi_enabled =
					nla_get_u32(test_attr[TESTINFO_ATTR_WIFI_ENABLED]);
		if (test_attr[TESTINFO_ATTR_WIFI_SCAN_INTVL])
			params.wifi_scan_intvl =
					nla_get_u32(test_attr[TESTINFO_ATTR_WIFI_SCAN_INTVL]);
		if (test_attr[TESTINFO_ATTR_WIFI_CB_INTVL])
			params.wifi_cb_intvl =
					nla_get_u32(test_attr[TESTINFO_ATTR_WIFI_CB_INTVL]);
		// BT
		if (test_attr[TESTINFO_ATTR_BT_ENABLED])
			params.bt_enabled =
					nla_get_u32(test_attr[TESTINFO_ATTR_BT_ENABLED]);
		if (test_attr[TESTINFO_ATTR_BT_SCAN_INTVL])
			params.bt_scan_intvl =
					nla_get_u32(test_attr[TESTINFO_ATTR_BT_SCAN_INTVL]);
		if (test_attr[TESTINFO_ATTR_BT_CB_INTVL])
			params.bt_cb_intvl =
					nla_get_u32(test_attr[TESTINFO_ATTR_BT_CB_INTVL]);
		// GPS
		if (test_attr[TESTINFO_ATTR_GPS_ENABLED])
			params.gps_enabled =
					nla_get_u32(test_attr[TESTINFO_ATTR_GPS_ENABLED]);
		if (test_attr[TESTINFO_ATTR_GPS_SCAN_INTVL])
			params.gps_scan_intvl =
					nla_get_u32(test_attr[TESTINFO_ATTR_GPS_SCAN_INTVL]);
		if (test_attr[TESTINFO_ATTR_GPS_CB_INTVL])
			params.gps_cb_intvl =
					nla_get_u32(test_attr[TESTINFO_ATTR_GPS_CB_INTVL]);
	} else {
		pr_info("[%s] No test info found\n", __func__);
		mutex_unlock(&g_ctx->nl_lock);
		return -1;
	}

	mutex_unlock(&g_ctx->nl_lock);

	pr_info("[%s] start test param: (%d, %d, %d)(%d, %d, %d)(%d, %d, %d)\n", __func__,
			params.wifi_enabled, params.wifi_scan_intvl, params.wifi_cb_intvl,
			params.bt_enabled, params.bt_scan_intvl, params.bt_cb_intvl,
			params.gps_enabled, params.gps_scan_intvl, params.gps_cb_intvl);

	if (g_ctx && g_ctx->cb.aoltest_handler) {
		pr_info("[%s] call aoltest_handler: start test\n", __func__);
		g_ctx->cb.aoltest_handler(AOLTEST_CMD_START_TEST, (void *)&params);
	}

	return 0;
}

static int aoltest_nl_stop_test(struct sk_buff *skb, struct genl_info *info)
{
	if (g_ctx && g_ctx->cb.aoltest_handler)
		g_ctx->cb.aoltest_handler(AOLTEST_CMD_STOP_TEST, NULL);

	return 0;
}

static int aoltest_nl_start_data_transmission(struct sk_buff *skb, struct genl_info *info)
{
	if (g_ctx && g_ctx->cb.aoltest_handler)
		g_ctx->cb.aoltest_handler(AOLTEST_CMD_START_DATA_TRANS, NULL);
	return 0;
}

static int aoltest_nl_stop_data_transmission(struct sk_buff *skb, struct genl_info *info)
{
	if (g_ctx && g_ctx->cb.aoltest_handler)
		g_ctx->cb.aoltest_handler(AOLTEST_CMD_STOP_DATA_TRANS, NULL);
	return 0;
}

static int aoltest_netlink_msg_send(char *tag, unsigned int msg_id, char *buf, unsigned int length,
								pid_t pid, unsigned int seq)
{
	struct sk_buff *skb;
	void *msg_head = NULL;
	int ret = 0;

	// Allocate a generic netlink message buffer
	skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (skb != NULL) {
		// Create message header
		msg_head = genlmsg_put(skb, 0, seq, &g_ctx->gnl_family, 0, AOLTEST_NL_CMD_SEND);
		if (msg_head == NULL) {
			pr_info("[%s] genlmsg_put fail\n", __func__);
			nlmsg_free(skb);
			return -EMSGSIZE;
		}

		// Add message attribute and content
		ret = nla_put_string(skb, AOLTEST_ATTR_HEADER, tag);
		if (ret != 0) {
			pr_info("[%s] nla_put_string header fail, ret=[%d]\n", __func__, ret);
			genlmsg_cancel(skb, msg_head);
			nlmsg_free(skb);
			return ret;
		}

		if (length) {
			ret = nla_put(skb, AOLTEST_ATTR_MSG, length, buf);
			if (ret != 0) {
				pr_info("[%s] nla_put fail, ret=[%d]\n", __func__, ret);
				genlmsg_cancel(skb, msg_head);
				nlmsg_free(skb);
				return ret;
			}

			ret = nla_put_u32(skb, AOLTEST_ATTR_MSG_ID, msg_id);
			if (ret != 0) {
				pr_info("[%s] nal_put_u32 fail, ret=[%d]\n", __func__, ret);
				genlmsg_cancel(skb, msg_head);
				nlmsg_free(skb);
				return ret;
			}

			ret = nla_put_u32(skb, AOLTEST_ATTR_MSG_SIZE, length);
			if (ret != 0) {
				pr_info("[%s] nal_put_u32 fail, ret=[%d]\n", __func__, ret);
				genlmsg_cancel(skb, msg_head);
				nlmsg_free(skb);
				return ret;
			}
		}

		// Finalize the message
		genlmsg_end(skb, msg_head);

		// Send message
		ret = genlmsg_unicast(&init_net, skb, pid);
		if (ret == 0)
			pr_info("[%s] Send msg succeed\n", __func__);
	} else {
		pr_info("[%s] Allocate message error\n", __func__);
		ret = -ENOMEM;
	}

	return ret;
}

static int aoltest_netlink_send_to_native_internal(char *tag,
				unsigned int msg_id, char *buf, unsigned int length)
{
	int ret = 0;
	unsigned int retry;

	ret = aoltest_netlink_msg_send(tag, msg_id, buf, length, g_ctx->bind_pid, g_ctx->seqnum);

	if (ret != 0) {
		pr_info("[%s] genlmsg_unicast fail, ret=[%d], pid=[%d], seq=[%d], tag=[%s]\n",
				__func__, ret, g_ctx->bind_pid, g_ctx->seqnum, tag);

		if (ret == -EAGAIN) {
			retry = 0;

			while (retry < 100 && ret == -EAGAIN) {
				msleep(20);
				ret = aoltest_netlink_msg_send(tag, msg_id, buf, length,
								g_ctx->bind_pid, g_ctx->seqnum);
				retry++;
				pr_info("[%s] genlmsg_unicast retry(%d)...: ret=[%d] pid=[%d] seq=[%d] tag=[%s]\n",
					__func__, retry, ret, g_ctx->bind_pid, g_ctx->seqnum, tag);
			}

			if (ret) {
				pr_info("[%s] genlmsg_unicast fail, ret=[%d] after retry %d times: pid=[%d], seq=[%d], tag=[%s]\n",
					__func__, ret, retry, g_ctx->bind_pid, g_ctx->seqnum, tag);
			}
		}
	}

	g_ctx->seqnum++;

	return ret;
}

int aoltest_netlink_send_to_native(char *tag, unsigned int msg_id, char *buf, unsigned int length)
{
	int ret = 0;
	int idx = 0;
	unsigned int send_len;
	unsigned int remain_len = length;

	if (g_ctx->status != LINK_STATUS_INIT_DONE) {
		pr_info("[%s] Netlink should be init\n", __func__);
		return -2;
	}

	if (g_ctx->bind_pid == 0) {
		pr_info("[%s] No bind service\n", __func__);
		return -3;
	}

	while (remain_len) {
		send_len = (remain_len > AOLTEST_PKT_SIZE ? AOLTEST_PKT_SIZE : remain_len);
		ret = aoltest_netlink_send_to_native_internal(tag, msg_id, &buf[idx], send_len);

		if (ret) {
			pr_info("[%s] From %d with len=[%d] fail, ret=[%d]\n"
							, __func__, idx, send_len, ret);
			break;
		}

		remain_len -= send_len;
		idx += send_len;
	}

	return idx;
}

int aoltest_netlink_init(struct netlink_event_cb *cb)
{
	int ret = 0;

	mutex_init(&g_ctx->nl_lock);
	ret = genl_register_family(&g_ctx->gnl_family);

	if (ret != 0) {
		pr_info("[%s] GE_NELINK family registration fail, ret=[%d]\n", __func__, ret);
		return -2;
	}

	g_ctx->status = LINK_STATUS_INIT_DONE;
	g_ctx->bind_pid = 0;
	memcpy(&(g_ctx->cb), cb, sizeof(struct netlink_event_cb));
	pr_info("[%s] aoltest netlink init succeed\n", __func__);

	return ret;
}

void aoltest_netlink_deinit(void)
{
	g_ctx->status = LINK_STATUS_INIT;
	genl_unregister_family(&g_ctx->gnl_family);
}
