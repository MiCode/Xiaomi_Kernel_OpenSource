// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <common/mdla_device.h>
#include <common/mdla_scheduler.h>

static int dev_handle_capability[MAX_CORE_NUM];
static int cmd_priority_max;

static void mdla_sched_dummy_cmd_batch(struct command_entry *c) {}

static struct mdla_sched_cb_func mdla_scheduler_callback = {
	.split_alloc_cmd_batch = mdla_sched_dummy_cmd_batch,
	.del_free_cmd_batch    = mdla_sched_dummy_cmd_batch,
};

struct mdla_sched_cb_func *mdla_sched_plat_cb(void)
{
	return &mdla_scheduler_callback;
}


void mdla_sched_set_dev_handle_cap(u32 core_id, int cmd_num)
{
	if (core_id < MAX_CORE_NUM)
		dev_handle_capability[core_id] = cmd_num;
}

int mdla_sched_get_dev_handle_cap(u32 core_id)
{
	return (core_id < MAX_CORE_NUM) ? dev_handle_capability[core_id] : 0;
}

void mdla_sched_set_cmd_prio_lv(int max_lv)
{
	cmd_priority_max = max_lv;
}

int mdla_sched_get_cmd_prio_lv(void)
{
	return cmd_priority_max;
}

