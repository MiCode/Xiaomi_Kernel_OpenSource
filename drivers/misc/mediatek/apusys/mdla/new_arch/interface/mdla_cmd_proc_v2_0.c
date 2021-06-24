// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>

#include <apusys_device.h>

#include <common/mdla_device.h>
#include <common/mdla_cmd_proc.h>
#include <common/mdla_power_ctrl.h>
#include <common/mdla_ioctl.h>

#include <utilities/mdla_profile.h>
#include <utilities/mdla_util.h>
#include <utilities/mdla_trace.h>
#include <utilities/mdla_debug.h>

#include "mdla_cmd_data_v2_0.h"


static void mdla_cmd_prepare_v2_0(struct mdla_run_cmd *cd,
	struct apusys_cmd_hnd *apusys_hd, struct command_entry *ce)
{
	ce->mva = cd->mva + cd->offset;
	apusys_hd->ip_time = 0;
	ce->state = CE_NONE;
	ce->flags = CE_NOP;
	ce->bandwidth = 0;
	ce->result = MDLA_SUCCESS;
	ce->count = cd->count;
	ce->fin_cid = 0;
	ce->csn = (ce->mva & 0xFFFFFFFE);
	ce->footprint = 0;
	ce->kva = (void *)apusys_mem_query_kva((u32)ce->mva);

	/* Initialize timestamp*/
	ce->exec_time = 0;
	ce->poweron_t = 0;
	ce->req_start_t = 0;
	ce->req_end_t = 0;
	ce->queue_t = 0;

	mdla_cmd_debug("%s: kva=0x%llx mva=0x%08x(0x%08x+0x%x) cnt=%u sz=0x%x\n",
			__func__,
			(u64)ce->kva,
			ce->mva,
			cd->mva,
			cd->offset,
			ce->count,
			cd->size);
}

static void mdla_cmd_ut_prepare_v2_0(struct ioctl_run_cmd *cd,
					struct command_entry *ce)
{
	ce->mva = cd->buf.mva + cd->offset;

	mdla_cmd_debug("%s: mva=0x%08x, offset=0x%x, count: %u\n",
				__func__,
				cd->buf.mva,
				cd->offset,
				cd->count);

	ce->state = CE_NONE;
	ce->flags = CE_NOP;
	ce->bandwidth = 0;
	ce->result = MDLA_SUCCESS;
	ce->count = cd->count;
	ce->kva = NULL;
	ce->boost_val = cd->boost_value;

	/* Initialize timestamp */
	ce->wait_t = 0;
	ce->queue_t = 0;
}

int mdla_cmd_run_sync_v2_0(struct mdla_run_cmd_sync *cmd_data,
				struct mdla_dev *mdla_info,
				struct apusys_cmd_hnd *apusys_hd,
				uint32_t priority)
{
	u64 deadline = 0, cmd_start_t;
	struct mdla_run_cmd *cd = &cmd_data->req;
	struct command_entry ce;
	int ret = 0, boost_val;
	u32 core_id = 0;
	u16 prio = (u16)priority;
	uint64_t out_end;

	cmd_start_t = sched_clock();

	memset(&ce, 0, sizeof(struct command_entry));
	core_id = mdla_info->mdla_id;

	ce.receive_t = cmd_start_t;
	ce.queue_t = cmd_start_t;
	mdla_prof_set_ts(core_id, TS_CMD_START, cmd_start_t);

	/* The critical region of command enqueue */
	mutex_lock(&mdla_info->cmd_lock);
	mdla_pwr_ops_get()->wake_lock(core_id);

	mdla_cmd_prepare_v2_0(cd, apusys_hd, &ce);

	out_end = apusys_hd->cmd_entry + apusys_hd->cmd_size;
	if (mdla_cmd_plat_cb()->check_cmd_valid(out_end, &ce) == false)
		return -EINVAL;

	deadline = get_jiffies_64()
			+ msecs_to_jiffies(mdla_dbg_read_u32(FS_TIMEOUT));

	mdla_info->max_cmd_id = 0;

	mdla_verbose("%s: core: %d max_cmd_id: %d id: %d\n",
			__func__, core_id, mdla_info->max_cmd_id, ce.count);

	ret = mdla_pwr_ops_get()->on(core_id, false);
	if (ret)
		goto out;

	boost_val = apusys_hd->boost_val;

	if (unlikely(mdla_dbg_read_u32(FS_DVFS_RAND)))
		boost_val = mdla_pwr_get_random_boost_val();

	mdla_pwr_ops_get()->set_opp_by_boost(core_id, boost_val);

	mdla_prof_start(core_id);
	mdla_trace_begin(core_id, &ce);

	ce.poweron_t = sched_clock();

	mdla_cmd_plat_cb()->pre_cmd_handle(core_id, &ce);

	if (mdla_util_apu_pmu_handle(mdla_info, apusys_hd, prio) == 0)
		mdla_util_pmu_ops_get()->reset_write_evt_exec(core_id, prio);

	/* Fill HW reg */
	mdla_cmd_plat_cb()->process_command(core_id, &ce);

	/* Wait for timeout */
	while (mdla_info->max_cmd_id < ce.count
			&& time_before64(get_jiffies_64(), deadline)) {

		wait_for_completion_interruptible_timeout(
			&mdla_info->command_done,
			mdla_cmd_plat_cb()->get_wait_time(core_id));
	}

	mdla_cmd_plat_cb()->post_cmd_handle(core_id, &ce);
	mdla_cmd_plat_cb()->post_cmd_info(core_id);

	mdla_trace_end(core_id, 0, &ce);
	mdla_util_apu_pmu_update(mdla_info, apusys_hd, 0);

	cd->id = mdla_info->max_cmd_id;

	if (mdla_info->max_cmd_id < ce.count) {
		mdla_timeout_debug("command: %d, max_cmd_id: %d\n",
				ce.count,
				mdla_info->max_cmd_id);

		mdla_cmd_plat_cb()->post_cmd_hw_detect(core_id);

		mdla_dbg_dump(mdla_info, &ce);

		/* Enable & Relase bus protect */
		mdla_pwr_ops_get()->switch_off_on(core_id);

		mdla_pwr_ops_get()->hw_reset(mdla_info->mdla_id,
				mdla_dbg_get_reason_str(REASON_TIMEOUT));

		ret = -1;
	}

	mdla_pwr_ops_get()->off_timer_start(core_id);
	apusys_hd->ip_time = (u32)ce.exec_time / 1000;

out:
	mdla_pwr_ops_get()->wake_unlock(core_id);
	mutex_unlock(&mdla_info->cmd_lock);

	mdla_prof_set_ts(core_id, TS_CMD_FINISH, sched_clock());
	mdla_prof_iter(core_id);

	return ret;
}

int mdla_cmd_ut_run_sync_v2_0(void *run_cmd, void *wait_cmd,
			struct mdla_dev *mdla_info)
{
	u64 deadline = 0, cmd_start_t;
	struct ioctl_run_cmd *cd = (struct ioctl_run_cmd *)run_cmd;
	struct ioctl_wait_cmd *wt = (struct ioctl_wait_cmd *)wait_cmd;
	struct command_entry ce;
	int ret = 0, boost_val;
	u32 core_id = 0;

	cmd_start_t = sched_clock();

	memset(&ce, 0, sizeof(struct command_entry));
	core_id = mdla_info->mdla_id;
	ce.receive_t = cmd_start_t;
	ce.queue_t = cmd_start_t;
	wt->result = 0;

	/* The critical region of command enqueue */
	mutex_lock(&mdla_info->cmd_lock);
	mdla_pwr_ops_get()->wake_lock(core_id);

	mdla_cmd_ut_prepare_v2_0(cd,  &ce);

	deadline = get_jiffies_64()
			+ msecs_to_jiffies(mdla_dbg_read_u32(FS_TIMEOUT));

	mdla_info->max_cmd_id = 0;

	mdla_verbose("%s: core: %d max_cmd_id: %d id: %d\n",
			__func__, core_id, mdla_info->max_cmd_id, ce.count);

	ret = mdla_pwr_ops_get()->on(core_id, false);
	if (ret)
		goto out;

	boost_val = ce.boost_val;

	if (unlikely(mdla_dbg_read_u32(FS_DVFS_RAND)))
		boost_val = mdla_pwr_get_random_boost_val();

	mdla_pwr_ops_get()->set_opp_by_boost(core_id, boost_val);

	mdla_prof_start(core_id);
	mdla_trace_begin(core_id, &ce);

	ce.poweron_t = sched_clock();

	mdla_cmd_plat_cb()->pre_cmd_handle(core_id, &ce);

	/* Fill HW reg */
	mdla_cmd_plat_cb()->process_command(core_id, &ce);

	/* Wait for timeout */
	while (mdla_info->max_cmd_id < ce.count
			&& time_before64(get_jiffies_64(), deadline)) {

		wait_for_completion_interruptible_timeout(
			&mdla_info->command_done,
			mdla_cmd_plat_cb()->get_wait_time(core_id));
	}

	mdla_cmd_plat_cb()->post_cmd_info(core_id);

	mdla_prof_iter(core_id);
	mdla_trace_end(core_id, 0, &ce);

	cd->id = mdla_info->max_cmd_id;

	if (mdla_info->max_cmd_id < ce.count) {
		mdla_timeout_debug("command: %d, max_cmd_id: %d\n",
				ce.count,
				mdla_info->max_cmd_id);

		mdla_cmd_plat_cb()->post_cmd_hw_detect(core_id);

		mdla_dbg_dump(mdla_info, &ce);

		/* Enable & Relase bus protect */
		mdla_pwr_ops_get()->switch_off_on(core_id);

		mdla_pwr_ops_get()->hw_reset(mdla_info->mdla_id,
				mdla_dbg_get_reason_str(REASON_TIMEOUT));

		ret = -1;
		wt->result = 1;
	}

	mdla_pwr_ops_get()->off_timer_start(core_id);

	ce.wait_t = sched_clock();

	wt->queue_time = ce.poweron_t - ce.receive_t;
	wt->busy_time = ce.wait_t - ce.poweron_t;
	wt->bandwidth = ce.bandwidth;

out:
	mdla_pwr_ops_get()->wake_unlock(core_id);
	mutex_unlock(&mdla_info->cmd_lock);
	return ret;
}
