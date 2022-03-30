// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "vlx_be.h"
#include "vlx_fe.h"
#include "xen_be.h"
#include "xen_fe.h"
#include "protocol_common.h"
#include "protocol.h"

struct tee_protocol_fe_call_ops *fe_ops;
struct tee_protocol_be_call_ops *be_ops;

typedef struct tee_protocol_ops *(*check_func_t)(void);

static check_func_t check_funcs[] = {
	vlx_be_check,
	vlx_fe_check,
	xen_be_check,
	xen_fe_check,
	NULL
};

static struct {
	struct tee_protocol_ops	*ops;
} l_ctx;

/* Try all protocols in turn */
int protocol_early_init(int (*probe)(void), int (*start)(void))
{
	check_func_t *check_func = &check_funcs[0];
	int ret = 0;

	while (*check_func) {
		struct tee_protocol_ops *ops = (*check_func)();

		if (IS_ERR(ops))
			return PTR_ERR(ops);

		if (ops) {
			mc_dev_info("running in a VM, name='%s'", ops->name);
			l_ctx.ops = ops;
			fe_ops = l_ctx.ops->fe_call_ops;
			be_ops = l_ctx.ops->be_call_ops;
			if (l_ctx.ops->early_init)
				ret = l_ctx.ops->early_init(probe, start);

			break;
		}

		check_func++;
	}

	return ret;
}

int protocol_init(void)
{
	if (!l_ctx.ops || !l_ctx.ops->init)
		return 0;

	return l_ctx.ops->init();
}

void protocol_exit(void)
{
	if (!l_ctx.ops || !l_ctx.ops->exit)
		return;

	l_ctx.ops->exit();
}

int protocol_start(void)
{
	if (!l_ctx.ops || !l_ctx.ops->start)
		return 0;

	return l_ctx.ops->start();
}

void protocol_stop(void)
{
	if (!l_ctx.ops || !l_ctx.ops->stop)
		return;

	l_ctx.ops->stop();
}

/* Only for back-ends, front-end VM ID is set by backend on client creation */
const char *protocol_vm_id(void)
{
	if (!l_ctx.ops || l_ctx.ops->fe_call_ops)
		return "";

	return l_ctx.ops->vm_id;
}

bool protocol_is_fe(void)
{
	return l_ctx.ops && l_ctx.ops->fe_call_ops;
}

bool protocol_fe_uses_pages_and_vas(void)
{
	return l_ctx.ops && l_ctx.ops->fe_uses_pages_and_vas;
}
