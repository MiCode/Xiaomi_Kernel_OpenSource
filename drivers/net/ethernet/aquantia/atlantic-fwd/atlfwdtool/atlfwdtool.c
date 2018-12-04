// SPDX-License-Identifier: GPL-2.0-only
/* Atlantic Network Driver
 *
 * Copyright (C) 2019 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "atlfwd_args.h"
#include "atlfwd_ctx.h"
#include "atlfwd_msg.h"
#include "atlfwd_reply.h"

#define ATL_FWD_CMD_STR(cmd)\
[cmd] = #cmd
static const char *cmd_str[NUM_ATL_FWD_CMD] = {
	ATL_FWD_CMD_STR(ATL_FWD_CMD_REQUEST_RING),
	ATL_FWD_CMD_STR(ATL_FWD_CMD_RELEASE_RING),
	ATL_FWD_CMD_STR(ATL_FWD_CMD_ENABLE_RING),
	ATL_FWD_CMD_STR(ATL_FWD_CMD_DISABLE_RING),
	ATL_FWD_CMD_STR(ATL_FWD_CMD_DISABLE_REDIRECTIONS),
	ATL_FWD_CMD_STR(ATL_FWD_CMD_FORCE_ICMP_TX_VIA),
	ATL_FWD_CMD_STR(ATL_FWD_CMD_FORCE_TX_VIA),
	ATL_FWD_CMD_STR(ATL_FWD_CMD_RING_STATUS),
	ATL_FWD_CMD_STR(ATL_FWD_CMD_DUMP_RING),
	ATL_FWD_CMD_STR(ATL_FWD_CMD_SET_TX_BUNCH),
	ATL_FWD_CMD_STR(ATL_FWD_CMD_REQUEST_EVENT),
	ATL_FWD_CMD_STR(ATL_FWD_CMD_RELEASE_EVENT),
	ATL_FWD_CMD_STR(ATL_FWD_CMD_ENABLE_EVENT),
	ATL_FWD_CMD_STR(ATL_FWD_CMD_DISABLE_EVENT),
	ATL_FWD_CMD_STR(ATL_FWD_CMD_GET_RX_QUEUE),
	ATL_FWD_CMD_STR(ATL_FWD_CMD_GET_TX_QUEUE),
};
#define ATL_FWD_ATTR_STR(attr)\
[attr] = #attr
static const char *attr_str[NUM_ATL_FWD_ATTR] = {
	ATL_FWD_ATTR_STR(ATL_FWD_ATTR_FLAGS),
	ATL_FWD_ATTR_STR(ATL_FWD_ATTR_RING_SIZE),
	ATL_FWD_ATTR_STR(ATL_FWD_ATTR_BUF_SIZE),
	ATL_FWD_ATTR_STR(ATL_FWD_ATTR_PAGE_ORDER),
	ATL_FWD_ATTR_STR(ATL_FWD_ATTR_RING_INDEX),
	ATL_FWD_ATTR_STR(ATL_FWD_ATTR_RING_STATUS),
	ATL_FWD_ATTR_STR(ATL_FWD_ATTR_RING_IS_TX),
	ATL_FWD_ATTR_STR(ATL_FWD_ATTR_RING_FLAGS),
	ATL_FWD_ATTR_STR(ATL_FWD_ATTR_TX_BUNCH_SIZE),
	ATL_FWD_ATTR_STR(ATL_FWD_ATTR_QUEUE_INDEX),
};

/* get atl_fwd family id */
static int atlnl_family_cb(const struct nlmsghdr *nlhdr, void *data)
{
	struct nl_context *ctx = (struct nl_context *)data;
	const struct nlattr *attr;

	ctx->family_id = 0;
	mnl_attr_for_each(attr, nlhdr, GENL_HDRLEN)
	{
		if (mnl_attr_get_type(attr) == CTRL_ATTR_FAMILY_ID) {
			ctx->family_id = mnl_attr_get_u16(attr);
			break;
		}
	}

	return (ctx->family_id ? MNL_CB_OK : MNL_CB_ERROR);
}

static int atlnl_get_family_id(struct nl_context *ctx)
{
	int ret = atlnl_msg_alloc(ctx, GENL_ID_CTRL, CTRL_CMD_GETFAMILY,
				  NLM_F_REQUEST | NLM_F_ACK, 1);
	if (ctx->verbose)
		printf("calling %s\n", "CTRL_CMD_GETFAMILY");
	if (ret != 0)
		goto err_msgfree;

	mnl_attr_put_strz(ctx->nlhdr, CTRL_ATTR_FAMILY_NAME, ATL_FWD_GENL_NAME);

	if (mnl_socket_sendto(ctx->sock, ctx->nlhdr, ctx->nlhdr->nlmsg_len) < 0)
		goto err_msgfree;

	ret = atlnl_process_reply(ctx, atlnl_family_cb);
	if (ret < 0)
		goto err_msgfree;

	ret = ctx->family_id ? 0 : -EADDRNOTAVAIL;

	if (ctx->verbose)
		printf("family id: %d\n", ctx->family_id);

err_msgfree:
	atlnl_msg_free(ctx);
	return ret;
}

static int atlnl_reqring_cb(const struct nlmsghdr *nlhdr, void *data)
{
	const struct nlattr *attr;
	int ring_index = -1;

	mnl_attr_for_each(attr, nlhdr, GENL_HDRLEN)
	{
		if (mnl_attr_get_type(attr) == ATL_FWD_ATTR_RING_INDEX) {
			ring_index = (int)mnl_attr_get_u32(attr);
			break;
		}
	}

	if (ring_index == -1) {
		fprintf(stderr, "Error: %s attribute is missing in reply\n",
			attr_str[ATL_FWD_ATTR_RING_INDEX]);
		return MNL_CB_ERROR;
	}

	printf("Ring index: %d\n", ring_index);
	return MNL_CB_OK;
}

static int atlnl_getqueue_cb(const struct nlmsghdr *nlhdr, void *data)
{
	const struct nlattr *attr;
	int queue = -1;

	mnl_attr_for_each(attr, nlhdr, GENL_HDRLEN)
	{
		if (mnl_attr_get_type(attr) == ATL_FWD_ATTR_QUEUE_INDEX) {
			queue = (int)mnl_attr_get_u32(attr);
			break;
		}
	}

	if (queue == -1) {
		fprintf(stderr, "Error: %s attribute is missing in reply\n",
			attr_str[ATL_FWD_ATTR_QUEUE_INDEX]);
		return MNL_CB_ERROR;
	}

	printf("%d\n", queue);
	return MNL_CB_OK;
}

static bool is_ring_created(const enum atlfwd_nl_ring_status ring_status)
{
	switch (ring_status) {
	case ATL_FWD_RING_STATUS_CREATED_DISABLED:
	case ATL_FWD_RING_STATUS_ENABLED:
		return true;
	default:
		return false;
	}

	return false;
}

static const char *
ring_status_string(const enum atlfwd_nl_ring_status ring_status)
{
	static const char *status_str[NUM_ATL_FWD_RING_STATUS] = {
		[ATL_FWD_RING_STATUS_INVALID] = "invalid status",
		[ATL_FWD_RING_STATUS_RELEASED] = "released",
		[ATL_FWD_RING_STATUS_CREATED_DISABLED] = "disabled",
		[ATL_FWD_RING_STATUS_ENABLED] = "enabled",
	};

	return (ring_status < NUM_ATL_FWD_RING_STATUS ?
			status_str[ring_status] :
			status_str[ATL_FWD_RING_STATUS_INVALID]);
}

struct ring_status {
	enum atlfwd_nl_ring_status status;
	bool is_tx;
	int size;
	unsigned int flags;
};

static void print_ring_status(const int ring_index,
			      const struct ring_status *status)
{
	char prefix[16];

	snprintf(prefix, MNL_ARRAY_SIZE(prefix), "fwd_ring_%d_", ring_index);

	printf("%sstatus:\t%s\n", prefix, ring_status_string(status->status));
	printf("%sdirection:\t%s\n", prefix, status->is_tx ? "tx" : "rx");
	if (is_ring_created(status->status)) {
		printf("%sflags:\t%d\n", prefix, status->flags);
		printf("%ssize:\t%d\n", prefix, status->size);
	} else {
		printf("%sflags:\t%s\n", prefix, "n/a");
		printf("%ssize:\t%s\n", prefix, "n/a");
	}
}

static int atlnl_ringstatus_cb(const struct nlmsghdr *nlhdr, void *data)
{
	const struct nlattr *attr;
	int ring_index = -1;
	struct ring_status status = {
		.status = ATL_FWD_RING_STATUS_INVALID,
		.is_tx = false,
		.size = 0,
		.flags = 0,
	};

	mnl_attr_for_each(attr, nlhdr, GENL_HDRLEN)
	{
		switch (mnl_attr_get_type(attr)) {
		case ATL_FWD_ATTR_RING_INDEX:
			if (ring_index >= 0)
				print_ring_status(ring_index, &status);
			ring_index = (int)mnl_attr_get_u32(attr);
			break;
		case ATL_FWD_ATTR_RING_STATUS:
			status.status = (int)mnl_attr_get_u32(attr);
			break;
		case ATL_FWD_ATTR_RING_IS_TX:
			status.is_tx = (bool)mnl_attr_get_u32(attr);
			break;
		case ATL_FWD_ATTR_RING_SIZE:
			status.size = (int)mnl_attr_get_u32(attr);
			break;
		case ATL_FWD_ATTR_RING_FLAGS:
			status.flags = (unsigned int)mnl_attr_get_u32(attr);
			break;
		default:
			fprintf(stderr, "Error: %s\n",
				"Unexpected attribute in reply");
			break;
		}
	}

	if (ring_index == -1) {
		fprintf(stderr, "Error: %s\n", "Incorrect reply");
		return MNL_CB_ERROR;
	}

	print_ring_status(ring_index, &status);

	return MNL_CB_OK;
}

static int atlnl_cmd_generic_u32_args(struct nl_context *ctx,
				      const enum atlfwd_nl_command cmd,
				      mnl_cb_t reply_cb, const int attr_count,
				      ...)
{
	int ret = atlnl_msg_alloc(ctx, ctx->family_id, cmd,
				  NLM_F_REQUEST | NLM_F_ACK, 1);
	va_list args;
	int i;

	if (ret != 0) {
		fprintf(stderr, "Error: %s\n", "message allocation failed.");
		goto err_msgfree;
	}

	if (ctx->verbose)
		printf("calling %s\n",
		       cmd_str[cmd] ? cmd_str[cmd] : "unknown command");

	if (ctx->devname != NULL)
		mnl_attr_put_strz(ctx->nlhdr, ATL_FWD_ATTR_IFNAME,
				  ctx->devname);

	va_start(args, attr_count);
	for (i = 0; i != attr_count; i++) {
		const enum atlfwd_nl_attribute attr =
			va_arg(args, enum atlfwd_nl_attribute);
		const uint32_t value = va_arg(args, uint32_t);

		mnl_attr_put_u32(ctx->nlhdr, attr, value);

		if (ctx->verbose)
			printf("\t(%s: %u)\n",
			       attr_str[attr] ? attr_str[attr] :
						"unknown attribute",
			       value);
	}
	va_end(args);

	if (mnl_socket_sendto(ctx->sock, ctx->nlhdr, ctx->nlhdr->nlmsg_len) < 0)
		goto err_msgfree;

	ret = atlnl_process_reply(ctx, reply_cb);

err_msgfree:
	atlnl_msg_free(ctx);
	return ret;
}

int main(int argc, char **argv)
{
	struct atlfwd_args *args = parse_args(argc, argv);
	static struct nl_context nlctx;
	int result = EINVAL;
	int ret = 0;

	if (!args)
		return result;

	nlctx.devname = args->devname;
	nlctx.verbose = args->verbose;

	result = ECONNREFUSED;

	/* open socket */
	nlctx.sock = mnl_socket_open(NETLINK_GENERIC);
	if (!nlctx.sock)
		return result;

#ifdef NETLINK_EXT_ACK
	/* request extended acknowledgment */
	unsigned int true_val = 1;
	ret = mnl_socket_setsockopt(nlctx.sock, NETLINK_EXT_ACK, &true_val,
					sizeof(true_val));
	if (ret < 0)
		goto err_sockclose;
#endif

	/* bind and get the port id */
	ret = mnl_socket_bind(nlctx.sock, 0, MNL_SOCKET_AUTOPID);
	if (ret < 0) {
		result = -ret;
		goto err_sockclose;
	}
	nlctx.port = mnl_socket_get_portid(nlctx.sock);

	/* parse command line options */
	result = EINVAL;

	/* obtain family id by name */
	ret = atlnl_get_family_id(&nlctx);
	if (ret < 0)
		goto err_sockclose;

	/* process command(s) */
	switch (args->cmd) {
	case ATL_FWD_CMD_REQUEST_RING:
		ret = atlnl_cmd_generic_u32_args(
			&nlctx, args->cmd, atlnl_reqring_cb, 4,
			ATL_FWD_ATTR_FLAGS, args->flags, ATL_FWD_ATTR_RING_SIZE,
			args->ring_size, ATL_FWD_ATTR_BUF_SIZE, args->buf_size,
			ATL_FWD_ATTR_PAGE_ORDER, args->page_order);
		break;
	case ATL_FWD_CMD_RELEASE_RING:
		/* fall through */
	case ATL_FWD_CMD_ENABLE_RING:
		/* fall through */
	case ATL_FWD_CMD_DISABLE_RING:
		/* fall through */
	case ATL_FWD_CMD_DUMP_RING:
		/* fall through */
	case ATL_FWD_CMD_REQUEST_EVENT:
		/* fall through */
	case ATL_FWD_CMD_RELEASE_EVENT:
		/* fall through */
	case ATL_FWD_CMD_ENABLE_EVENT:
		/* fall through */
	case ATL_FWD_CMD_DISABLE_EVENT:
		/* fall through */
	case ATL_FWD_CMD_FORCE_ICMP_TX_VIA:
		/* fall through */
	case ATL_FWD_CMD_FORCE_TX_VIA:
		ret = atlnl_cmd_generic_u32_args(&nlctx, args->cmd, NULL, 1,
						 ATL_FWD_ATTR_RING_INDEX,
						 (uint32_t)args->ring_index);
		break;
	case ATL_FWD_CMD_GET_RX_QUEUE:
		/* fall through */
	case ATL_FWD_CMD_GET_TX_QUEUE:
		ret = atlnl_cmd_generic_u32_args(&nlctx, args->cmd,
						 atlnl_getqueue_cb, 1,
						 ATL_FWD_ATTR_RING_INDEX,
						 (uint32_t)args->ring_index);
		break;
	case ATL_FWD_CMD_RING_STATUS:
		if (args->ring_index < 0)
			ret = atlnl_cmd_generic_u32_args(
				&nlctx, args->cmd, atlnl_ringstatus_cb, 0);
		else
			ret = atlnl_cmd_generic_u32_args(
				&nlctx, args->cmd, atlnl_ringstatus_cb, 1,
				ATL_FWD_ATTR_RING_INDEX,
				(uint32_t)args->ring_index);
		break;
	case ATL_FWD_CMD_DISABLE_REDIRECTIONS:
		ret = atlnl_cmd_generic_u32_args(&nlctx, args->cmd, NULL, 0);
		break;
	case ATL_FWD_CMD_SET_TX_BUNCH:
		ret = atlnl_cmd_generic_u32_args(&nlctx, args->cmd, NULL, 1,
						 ATL_FWD_ATTR_TX_BUNCH_SIZE,
						 args->tx_bunch);
		break;
	default:
		fprintf(stderr, "Unknown command %d\n", args->cmd);
		goto err_sockclose;
	}

	result = 0;
	if (ret < 0) {
		result = -ret;
		perror("cmd error");
	}

err_sockclose:
	mnl_socket_close(nlctx.sock);
	return result;
}
