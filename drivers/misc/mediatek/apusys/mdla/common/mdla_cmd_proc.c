// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/types.h>

#include <common/mdla_device.h>
#include <common/mdla_cmd_proc.h>

#include <utilities/mdla_debug.h>

static int mdla_cmd_dummy_run(struct mdla_run_cmd_sync *run_cmd,
			struct mdla_dev *mdla_info,
			struct apusys_cmd_handle *apusys_hd, uint32_t data)
{
	mdla_cmd_debug("%s() !!!\n", __func__);
	return 0;
}

static int mdla_cmd_dummy_ut_run_sync(void *run_cmd, void *wait_cmd,
			struct mdla_dev *mdla_info)
{
	mdla_cmd_debug("%s() !!!\n", __func__);
	return 0;
}

static void mdla_cmd_dumy_lock_ops(struct mdla_dev *mdla_device) {}

static struct mdla_cmd_ops mdla_command = {
	.run_sync     = mdla_cmd_dummy_run,
	.ut_run_sync  = mdla_cmd_dummy_ut_run_sync,
	.lock         = mdla_cmd_dumy_lock_ops,
	.unlock       = mdla_cmd_dumy_lock_ops,
	.list_lock    = mdla_cmd_dumy_lock_ops,
	.list_unlock  = mdla_cmd_dumy_lock_ops,
};

static void mdla_cmd_lock(struct mdla_dev *mdla_device)
{
	mutex_lock(&mdla_device->cmd_lock);
}

static void mdla_cmd_unlock(struct mdla_dev *mdla_device)
{
	mutex_unlock(&mdla_device->cmd_lock);
}

static void mdla_cmd_list_lock(struct mdla_dev *mdla_device)
{
	mutex_lock(&mdla_device->cmd_list_lock);
}

static void mdla_cmd_list_unlock(struct mdla_dev *mdla_device)
{
	mutex_unlock(&mdla_device->cmd_list_lock);
}

void mdla_cmd_setup(run_sync_t sync, ut_run_sync_t ut_sync)
{
	if (sync)
		mdla_command.run_sync = sync;

	if (ut_sync)
		mdla_command.ut_run_sync = ut_sync;

	mdla_command.lock           = mdla_cmd_lock;
	mdla_command.unlock         = mdla_cmd_unlock;
	mdla_command.list_lock      = mdla_cmd_list_lock;
	mdla_command.list_unlock    = mdla_cmd_list_unlock;
}

const struct mdla_cmd_ops *mdla_cmd_ops_get(void)
{
	return &mdla_command;
}

static int mdla_cmd_dummy_ops(u32 a0) { return 0; }
static unsigned long mdla_cmd_dummy_uint_int(u32 a0) { return 0; }
static void mdla_cmd_dummy_info(u32 a0) {}
static int mdla_cmd_dummy_ce_ops(u32 a0, struct command_entry *a1)
{
	return 0;
}

static struct mdla_cmd_cb_func mdla_command_callback = {

	.pre_cmd_handle           = mdla_cmd_dummy_ce_ops,
	.pre_cmd_info             = mdla_cmd_dummy_info,
	.process_command          = mdla_cmd_dummy_ce_ops,
	.process_command_no_lock  = mdla_cmd_dummy_ce_ops,
	.post_cmd_handle          = mdla_cmd_dummy_ce_ops,
	.post_cmd_info            = mdla_cmd_dummy_info,
	.wait_cmd_handle          = mdla_cmd_dummy_ce_ops,
	.get_wait_time            = mdla_cmd_dummy_uint_int,
	.get_irq_num              = mdla_cmd_dummy_ops,
	.wait_cmd_hw_detect       = mdla_cmd_dummy_ops,
	.post_cmd_hw_detect       = mdla_cmd_dummy_ops,
};


struct mdla_cmd_cb_func *mdla_cmd_plat_cb(void)
{
	return &mdla_command_callback;
}
