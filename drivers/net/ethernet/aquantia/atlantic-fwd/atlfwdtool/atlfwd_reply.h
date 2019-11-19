/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2019 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#ifndef _ATLFWD_REPLY_H_
#define _ATLFWD_REPLY_H_

#include "atlfwd_ctx.h"

int atlnl_process_ack(struct nl_context *ctx, const ssize_t len);
int atlnl_process_reply(struct nl_context *ctx, mnl_cb_t reply_cb);

#endif
