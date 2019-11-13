// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2019, The Linux Foundation. All rights reserved. */

#define pr_fmt(fmt) "cnss_genl: " fmt

#include <linux/err.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <net/netlink.h>
#include <net/genetlink.h>

#include "main.h"
#include "debug.h"

#define CNSS_GENL_FAMILY_NAME "cnss-genl"
#define CNSS_GENL_MCAST_GROUP_NAME "cnss-genl-grp"
#define CNSS_GENL_VERSION 1
#define CNSS_GENL_DATA_LEN_MAX (15 * 1024)
#define CNSS_GENL_STR_LEN_MAX 16

enum {
	CNSS_GENL_ATTR_MSG_UNSPEC,
	CNSS_GENL_ATTR_MSG_TYPE,
	CNSS_GENL_ATTR_MSG_FILE_NAME,
	CNSS_GENL_ATTR_MSG_TOTAL_SIZE,
	CNSS_GENL_ATTR_MSG_SEG_ID,
	CNSS_GENL_ATTR_MSG_END,
	CNSS_GENL_ATTR_MSG_DATA_LEN,
	CNSS_GENL_ATTR_MSG_DATA,
	__CNSS_GENL_ATTR_MAX,
};

#define CNSS_GENL_ATTR_MAX (__CNSS_GENL_ATTR_MAX - 1)

enum {
	CNSS_GENL_CMD_UNSPEC,
	CNSS_GENL_CMD_MSG,
	__CNSS_GENL_CMD_MAX,
};

#define CNSS_GENL_CMD_MAX (__CNSS_GENL_CMD_MAX - 1)

static struct nla_policy cnss_genl_msg_policy[CNSS_GENL_ATTR_MAX + 1] = {
	[CNSS_GENL_ATTR_MSG_TYPE] = { .type = NLA_U8 },
	[CNSS_GENL_ATTR_MSG_FILE_NAME] = { .type = NLA_NUL_STRING,
					   .len = CNSS_GENL_STR_LEN_MAX },
	[CNSS_GENL_ATTR_MSG_TOTAL_SIZE] = { .type = NLA_U32 },
	[CNSS_GENL_ATTR_MSG_SEG_ID] = { .type = NLA_U32 },
	[CNSS_GENL_ATTR_MSG_END] = { .type = NLA_U8 },
	[CNSS_GENL_ATTR_MSG_DATA_LEN] = { .type = NLA_U32 },
	[CNSS_GENL_ATTR_MSG_DATA] = { .type = NLA_BINARY,
				      .len = CNSS_GENL_DATA_LEN_MAX },
};

static int cnss_genl_process_msg(struct sk_buff *skb, struct genl_info *info)
{
	return 0;
}

static struct genl_ops cnss_genl_ops[] = {
	{
		.cmd = CNSS_GENL_CMD_MSG,
		.policy = cnss_genl_msg_policy,
		.doit = cnss_genl_process_msg,
	},
};

static struct genl_multicast_group cnss_genl_mcast_grp[] = {
	{
		.name = CNSS_GENL_MCAST_GROUP_NAME,
	},
};

static struct genl_family cnss_genl_family = {
	.id = 0,
	.hdrsize = 0,
	.name = CNSS_GENL_FAMILY_NAME,
	.version = CNSS_GENL_VERSION,
	.maxattr = CNSS_GENL_ATTR_MAX,
	.module = THIS_MODULE,
	.ops = cnss_genl_ops,
	.n_ops = ARRAY_SIZE(cnss_genl_ops),
	.mcgrps = cnss_genl_mcast_grp,
	.n_mcgrps = ARRAY_SIZE(cnss_genl_mcast_grp),
};

static int cnss_genl_send_data(u8 type, char *file_name, u32 total_size,
			       u32 seg_id, u8 end, u32 data_len, u8 *msg_buff)
{
	struct sk_buff *skb = NULL;
	void *msg_header = NULL;
	int ret = 0;
	char filename[CNSS_GENL_STR_LEN_MAX + 1];

	cnss_pr_dbg("type: %u, file_name %s, total_size: %x, seg_id %u, end %u, data_len %u\n",
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
				 &cnss_genl_family, 0,
				 CNSS_GENL_CMD_MSG);
	if (!msg_header) {
		ret = -ENOMEM;
		goto fail;
	}

	ret = nla_put_u8(skb, CNSS_GENL_ATTR_MSG_TYPE, type);
	if (ret < 0)
		goto fail;
	ret = nla_put_string(skb, CNSS_GENL_ATTR_MSG_FILE_NAME, filename);
	if (ret < 0)
		goto fail;
	ret = nla_put_u32(skb, CNSS_GENL_ATTR_MSG_TOTAL_SIZE, total_size);
	if (ret < 0)
		goto fail;
	ret = nla_put_u32(skb, CNSS_GENL_ATTR_MSG_SEG_ID, seg_id);
	if (ret < 0)
		goto fail;
	ret = nla_put_u8(skb, CNSS_GENL_ATTR_MSG_END, end);
	if (ret < 0)
		goto fail;
	ret = nla_put_u32(skb, CNSS_GENL_ATTR_MSG_DATA_LEN, data_len);
	if (ret < 0)
		goto fail;
	ret = nla_put(skb, CNSS_GENL_ATTR_MSG_DATA, data_len, msg_buff);
	if (ret < 0)
		goto fail;

	genlmsg_end(skb, msg_header);
	ret = genlmsg_multicast(&cnss_genl_family, skb, 0, 0, GFP_KERNEL);
	if (ret < 0)
		cnss_pr_err("Fail to send genl msg: %d\n", ret);

	return ret;
fail:
	cnss_pr_err("Fail to generate genl msg: %d\n", ret);
	if (skb)
		nlmsg_free(skb);
	return ret;
}

int cnss_genl_send_msg(void *buff, u8 type, char *file_name, u32 total_size)
{
	int ret = 0;
	u8 *msg_buff = buff;
	u32 remaining = total_size;
	u32 seg_id = 0;
	u32 data_len = 0;
	u8 end = 0;
	u8 retry;

	cnss_pr_dbg("type: %u, total_size: %x\n", type, total_size);

	while (remaining) {
		if (remaining > CNSS_GENL_DATA_LEN_MAX) {
			data_len = CNSS_GENL_DATA_LEN_MAX;
		} else {
			data_len = remaining;
			end = 1;
		}

		for (retry = 0; retry < 2; retry++) {
			ret = cnss_genl_send_data(type, file_name, total_size,
						  seg_id, end, data_len,
						  msg_buff);
			if (ret >= 0)
				break;
			msleep(100);
		}

		if (ret < 0) {
			cnss_pr_err("fail to send genl data, ret %d\n", ret);
			return ret;
		}

		remaining -= data_len;
		msg_buff += data_len;
		seg_id++;
	}

	return ret;
}

int cnss_genl_init(void)
{
	int ret = 0;

	ret = genl_register_family(&cnss_genl_family);
	if (ret != 0)
		cnss_pr_err("genl_register_family fail: %d\n", ret);

	return ret;
}

void cnss_genl_exit(void)
{
	genl_unregister_family(&cnss_genl_family);
}
