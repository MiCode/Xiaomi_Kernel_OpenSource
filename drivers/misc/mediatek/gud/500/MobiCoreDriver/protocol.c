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

#include "xen_be.h"
#include "xen_fe.h"
#include "protocol_common.h"
#include "protocol.h"

struct tee_protocol_fe_call_ops *fe_ops;

typedef struct tee_protocol_ops *(*check_func_t)(void);

static check_func_t check_funcs[] = {
	xen_be_check,
	xen_fe_check,
	NULL
};

static struct {
	int			(*start)(void);
	struct tee_protocol_ops	*ops;
	bool			start_fe;
	bool			probes_and_starts;
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
			if (l_ctx.ops->early_init) {
				ret = l_ctx.ops->early_init(probe, start);
				if (ret == 1)
					l_ctx.probes_and_starts = true;
			}

			break;
		}

		check_func++;
	}

	/* Default to bare-metal */
	l_ctx.start = start;
	return ret;
}

int protocol_init(void)
{
	int ret;

	if (!l_ctx.ops || !l_ctx.ops->init)
		return 0;

	ret = l_ctx.ops->init();
	if (ret)
		return ret;

	if (l_ctx.probes_and_starts)
		return 0;

	ret = l_ctx.start();
	if (ret)
		protocol_exit();

	return ret;
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

bool protocol_is_be(void)
{
	return !l_ctx.ops || !l_ctx.ops->fe_call_ops;
}

bool protocol_fe_uses_pages_and_vas(void)
{
	return l_ctx.ops && l_ctx.ops->fe_uses_pages_and_vas;
}
