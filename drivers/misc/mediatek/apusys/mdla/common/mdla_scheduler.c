// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <common/mdla_scheduler.h>

static void mdla_sched_dummy_cmd_batch(struct command_entry *c) {}

static struct mdla_sched_cb_func mdla_scheduler_callback = {
	.split_alloc_cmd_batch = mdla_sched_dummy_cmd_batch,
	.del_free_cmd_batch    = mdla_sched_dummy_cmd_batch,
};

struct mdla_sched_cb_func *mdla_sched_plat_cb(void)
{
	return &mdla_scheduler_callback;
}
