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

#include "atlfwd_reply.h"

#include <errno.h>

#include "atlfwd_msg.h"

int atlnl_process_ack(struct nl_context *ctx, const ssize_t len)
{
	struct nlmsgerr *nlerr = mnl_nlmsg_get_payload(ctx->nlhdr);
	unsigned int tlv_offset = sizeof(*nlerr);

	if (len < NLMSG_HDRLEN + tlv_offset)
		return -EFAULT;

#ifdef NLM_F_ACK_TLVS
	if (ctx->nlhdr->nlmsg_flags & NLM_F_ACK_TLVS) {
		if (!(ctx->nlhdr->nlmsg_flags & NLM_F_CAPPED))
			tlv_offset += MNL_ALIGN(
				mnl_nlmsg_get_payload_len(&nlerr->msg));

		const struct nlattr *attr;

		mnl_attr_for_each(attr, ctx->nlhdr, tlv_offset)
		{
			if (mnl_attr_get_type(attr) == NLMSGERR_ATTR_MSG &&
			    (ctx->verbose || nlerr->error)) {
				const char *msg = mnl_attr_get_str(attr);

				fprintf(stderr, "netlink %s: %s\n",
					nlerr->error ? "error" : "warning",
					msg);
			}
		}
	}
#endif

	if (nlerr->error) {
		errno = -nlerr->error;
		perror("netlink error");
	} else if (ctx->verbose) {
		printf("%s\n", "got ack");
	}

	return nlerr->error;
}

int atlnl_process_reply(struct nl_context *ctx, mnl_cb_t reply_cb)
{
	int ret = 0;

	atlnl_msg_realloc(ctx, 4096);
	do {
		ssize_t len = mnl_socket_recvfrom(ctx->sock, ctx->msgbuf,
						  ctx->msgbufsize);

		if (len == 0)
			return 0;
		if (len < NLMSG_HDRLEN)
			return -EFAULT;

		ctx->nlhdr = (struct nlmsghdr *)ctx->msgbuf;
		if (ctx->nlhdr->nlmsg_type == NLMSG_ERROR)
			return atlnl_process_ack(ctx, len);

		ctx->gnlhdr = mnl_nlmsg_get_payload(ctx->nlhdr);
		ret = mnl_cb_run(ctx->msgbuf, len, ctx->seq, ctx->port,
				 reply_cb, ctx);
	} while (ret > 0);

	return ret;
}
