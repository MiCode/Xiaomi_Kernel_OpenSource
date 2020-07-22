// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020, The Linux Foundation. All rights reserved. */

#define pr_fmt(fmt) "cnss_genl: " fmt

#include <linux/err.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <net/netlink.h>
#include <net/genetlink.h>

#include "main.h"
#include "debug.h"

#define ICNSS_GENL_FAMILY_NAME "cnss-genl"
#define ICNSS_GENL_MCAST_GROUP_NAME "cnss-genl-grp"
#define ICNSS_GENL_VERSION 1
#define ICNSS_GENL_DATA_LEN_MAX (15 * 1024)
#define ICNSS_GENL_STR_LEN_MAX 16

enum {
	ICNSS_GENL_ATTR_MSG_UNSPEC,
	ICNSS_GENL_ATTR_MSG_TYPE,
	ICNSS_GENL_ATTR_MSG_FILE_NAME,
	ICNSS_GENL_ATTR_MSG_TOTAL_SIZE,
	ICNSS_GENL_ATTR_MSG_SEG_ID,
	ICNSS_GENL_ATTR_MSG_END,
	ICNSS_GENL_ATTR_MSG_DATA_LEN,
	ICNSS_GENL_ATTR_MSG_DATA,
	__ICNSS_GENL_ATTR_MAX,
};

#define ICNSS_GENL_ATTR_MAX (__ICNSS_GENL_ATTR_MAX - 1)

enum {
	ICNSS_GENL_CMD_UNSPEC,
	ICNSS_GENL_CMD_MSG,
	__ICNSS_GENL_CMD_MAX,
};

#define ICNSS_GENL_CMD_MAX (__ICNSS_GENL_CMD_MAX - 1)

static struct nla_policy icnss_genl_msg_policy[ICNSS_GENL_ATTR_MAX + 1] = {
	[ICNSS_GENL_ATTR_MSG_TYPE] = { .type = NLA_U8 },
	[ICNSS_GENL_ATTR_MSG_FILE_NAME] = { .type = NLA_NUL_STRING,
					   .len = ICNSS_GENL_STR_LEN_MAX },
	[ICNSS_GENL_ATTR_MSG_TOTAL_SIZE] = { .type = NLA_U32 },
	[ICNSS_GENL_ATTR_MSG_SEG_ID] = { .type = NLA_U32 },
	[ICNSS_GENL_ATTR_MSG_END] = { .type = NLA_U8 },
	[ICNSS_GENL_ATTR_MSG_DATA_LEN] = { .type = NLA_U32 },
	[ICNSS_GENL_ATTR_MSG_DATA] = { .type = NLA_BINARY,
				      .len = ICNSS_GENL_DATA_LEN_MAX },
};

static int icnss_genl_process_msg(struct sk_buff *skb, struct genl_info *info)
{
	return 0;
}

static struct genl_ops icnss_genl_ops[] = {
	{
		.cmd = ICNSS_GENL_CMD_MSG,
		.doit = icnss_genl_process_msg,
	},
};

static struct genl_multicast_group icnss_genl_mcast_grp[] = {
	{
		.name = ICNSS_GENL_MCAST_GROUP_NAME,
	},
};

static struct genl_family icnss_genl_family = {
	.id = 0,
	.hdrsize = 0,
	.name = ICNSS_GENL_FAMILY_NAME,
	.version = ICNSS_GENL_VERSION,
	.maxattr = ICNSS_GENL_ATTR_MAX,
	.policy = icnss_genl_msg_policy,
	.module = THIS_MODULE,
	.ops = icnss_genl_ops,
	.n_ops = ARRAY_SIZE(icnss_genl_ops),
	.mcgrps = icnss_genl_mcast_grp,
	.n_mcgrps = ARRAY_SIZE(icnss_genl_mcast_grp),
};

static int icnss_genl_send_data(u8 type, char *file_name, u32 total_size,
				u32 seg_id, u8 end, u32 data_len, u8 *msg_buff)
{
	struct sk_buff *skb = NULL;
	void *msg_header = NULL;
	int ret = 0;
	char filename[ICNSS_GENL_STR_LEN_MAX + 1];

	icnss_pr_dbg("type: %u, file_name %s, total_size: %x, seg_id %u, end %u, data_len %u\n",
		     type, file_name, total_size, seg_id, end, data_len);

	if (!file_name)
		strlcpy(filename, "default", sizeof(filename));
	else
		strlcpy(filename, file_name, sizeof(filename));

	skb = genlmsg_new(NLMSG_HDRLEN +
			  nla_total_size(sizeof(type)) +
			  nla_total_size(strlen(filename) + 1) +
			  nla_total_size(sizeof(total_size)) +
			  nla_total_size(sizeof(seg_id)) +
			  nla_total_size(sizeof(end)) +
			  nla_total_size(sizeof(data_len)) +
			  nla_total_size(data_len), GFP_KERNEL);
	if (!skb)
		return -ENOMEM;

	msg_header = genlmsg_put(skb, 0, 0,
				 &icnss_genl_family, 0,
				 ICNSS_GENL_CMD_MSG);
	if (!msg_header) {
		ret = -ENOMEM;
		goto fail;
	}

	ret = nla_put_u8(skb, ICNSS_GENL_ATTR_MSG_TYPE, type);
	if (ret < 0)
		goto fail;
	ret = nla_put_string(skb, ICNSS_GENL_ATTR_MSG_FILE_NAME, filename);
	if (ret < 0)
		goto fail;
	ret = nla_put_u32(skb, ICNSS_GENL_ATTR_MSG_TOTAL_SIZE, total_size);
	if (ret < 0)
		goto fail;
	ret = nla_put_u32(skb, ICNSS_GENL_ATTR_MSG_SEG_ID, seg_id);
	if (ret < 0)
		goto fail;
	ret = nla_put_u8(skb, ICNSS_GENL_ATTR_MSG_END, end);
	if (ret < 0)
		goto fail;
	ret = nla_put_u32(skb, ICNSS_GENL_ATTR_MSG_DATA_LEN, data_len);
	if (ret < 0)
		goto fail;
	ret = nla_put(skb, ICNSS_GENL_ATTR_MSG_DATA, data_len, msg_buff);
	if (ret < 0)
		goto fail;

	genlmsg_end(skb, msg_header);
	ret = genlmsg_multicast(&icnss_genl_family, skb, 0, 0, GFP_KERNEL);
	if (ret < 0)
		icnss_pr_err("Fail to send genl msg: %d\n", ret);

	return ret;
fail:
	icnss_pr_err("Fail to generate genl msg: %d\n", ret);
	if (skb)
		nlmsg_free(skb);
	return ret;
}

int icnss_genl_send_msg(void *buff, u8 type, char *file_name, u32 total_size)
{
	int ret = 0;
	u8 *msg_buff = buff;
	u32 remaining = total_size;
	u32 seg_id = 0;
	u32 data_len = 0;
	u8 end = 0;
	u8 retry;

	icnss_pr_dbg("type: %u, total_size: %x\n", type, total_size);

	while (remaining) {
		if (remaining > ICNSS_GENL_DATA_LEN_MAX) {
			data_len = ICNSS_GENL_DATA_LEN_MAX;
		} else {
			data_len = remaining;
			end = 1;
		}

		for (retry = 0; retry < 2; retry++) {
			ret = icnss_genl_send_data(type, file_name, total_size,
						   seg_id, end, data_len,
						   msg_buff);
			if (ret >= 0)
				break;
			msleep(100);
		}

		if (ret < 0) {
			icnss_pr_err("fail to send genl data, ret %d\n", ret);
			return ret;
		}

		remaining -= data_len;
		msg_buff += data_len;
		seg_id++;
	}

	return ret;
}

int icnss_genl_init(void)
{
	int ret = 0;

	ret = genl_register_family(&icnss_genl_family);
	if (ret != 0)
		icnss_pr_err("genl_register_family fail: %d\n", ret);

	return ret;
}

void icnss_genl_exit(void)
{
	genl_unregister_family(&icnss_genl_family);
}
