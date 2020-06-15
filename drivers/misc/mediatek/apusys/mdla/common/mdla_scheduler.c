// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <common/mdla_scheduler.h>

static bool mdla_sched_dummy_bool_void(void) { return false; }
static void mdla_sched_dummy_bool_p(struct command_entry *c) {}

static struct mdla_sched_cb_func mdla_scheduler_callback = {
	.sw_preemption_support = mdla_sched_dummy_bool_void,
	.split_alloc_cmd_batch = mdla_sched_dummy_bool_p,
	.del_free_cmd_batch    = mdla_sched_dummy_bool_p,
};

struct mdla_sched_cb_func *mdla_sched_plat_cb(void)
{
	return &mdla_scheduler_callback;
}
