/* SPDX-License-Identifier: GPL-2.0-only */
/* Atlantic Network Driver
 *
 * Copyright (C) 2019 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ATLFWD_MSG_H_
#define _ATLFWD_MSG_H_

#include "atlfwd_ctx.h"

int atlnl_msg_alloc(struct nl_context *ctx, const int msg_type, const int cmd,
		    const unsigned int flags, const int version);
int atlnl_msg_realloc(struct nl_context *ctx, const unsigned int new_size);
void atlnl_msg_free(struct nl_context *ctx);

#endif
