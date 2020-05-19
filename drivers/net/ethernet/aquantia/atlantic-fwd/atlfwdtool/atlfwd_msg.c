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

#include "atlfwd_msg.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

int atlnl_msg_alloc(struct nl_context *ctx, const int msg_type, const int cmd,
		    const unsigned int flags, const int version)
{
	ctx->msgbuf = malloc(MNL_SOCKET_BUFFER_SIZE);
	if (!ctx->msgbuf)
		return -ENOMEM;
	ctx->msgbufsize = MNL_SOCKET_BUFFER_SIZE;
	memset(ctx->msgbuf, 0, MNL_SOCKET_BUFFER_SIZE);

	struct nlmsghdr *nlhdr = mnl_nlmsg_put_header(ctx->msgbuf);

	nlhdr->nlmsg_type = msg_type;
	nlhdr->nlmsg_flags = flags;
	nlhdr->nlmsg_seq = ++ctx->seq;
	ctx->nlhdr = nlhdr;

	struct genlmsghdr *gnlhdr =
		mnl_nlmsg_put_extra_header(nlhdr, sizeof(*gnlhdr));

	gnlhdr->cmd = cmd;
	gnlhdr->version = version;
	ctx->gnlhdr = gnlhdr;

	return 0;
}

int atlnl_msg_realloc(struct nl_context *ctx, const unsigned int new_size)
{
	const unsigned int old_size = ctx->msgbufsize;
	unsigned int nlhdr_offset = (char *)ctx->nlhdr - ctx->msgbuf;
	unsigned int gnlhdr_offset = (char *)ctx->gnlhdr - ctx->msgbuf;
	char *new_buff = realloc(ctx->msgbuf, new_size);

	if (!new_buff)
		return -ENOMEM;

	memset(new_buff + old_size, 0, new_size - old_size);
	ctx->msgbuf = new_buff;
	ctx->msgbufsize = new_size;
	ctx->nlhdr = (struct nlmsghdr *)(new_buff + nlhdr_offset);
	ctx->gnlhdr = (struct genlmsghdr *)(new_buff + gnlhdr_offset);

	return 0;
}

void atlnl_msg_free(struct nl_context *ctx)
{
	if (ctx->msgbuf)
		free(ctx->msgbuf);

	ctx->msgbuf = NULL;
	ctx->msgbufsize = 0;
	ctx->nlhdr = NULL;
	ctx->gnlhdr = NULL;
}
