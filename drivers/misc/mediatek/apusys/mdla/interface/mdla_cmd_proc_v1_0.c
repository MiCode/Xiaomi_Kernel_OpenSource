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

#include "mdla_cmd_data_v1_0.h"

static void mdla_cmd_cal_perf_index(struct ioctl_wait_cmd *wt,
				    struct command_entry *ce)
{
	wt->queue_time = ce->poweron_t - ce->receive_t;
	wt->busy_time = ce->wait_t - ce->poweron_t;
	wt->bandwidth = ce->bandwidth;
}

static void mdla_cmd_prepare_v1_0(struct mdla_run_cmd *cd,
				  struct apusys_cmd_handle *apusys_hd,
				  struct command_entry *ce)
{
	ce->state = CE_NONE;
	ce->flags = CE_NOP;
	ce->bandwidth = 0;
	ce->result = MDLA_SUCCESS;
	ce->count = cd->count;
	ce->receive_t = sched_clock();
	ce->kva = apusys_hd->cmdbufs[CMD_CODEBUF_IDX].kva + cd->offset;
	ce->mva = apusys_mem_query_iova((u64)ce->kva);

	mdla_cmd_debug(
		"%s: kva=0x%llx(0x%llx+0x%x) mva=0x%08x cnt=%u sz=0x%x\n",
		__func__, (u64)ce->kva, (u64)ce->cmdbuf->kva, cd->offset, ce->mva,
		ce->count, ce->cmdbuf->size);
}

static void mdla_cmd_ut_prepare_v1_0(struct ioctl_run_cmd *cd,
				     struct command_entry *ce)
{
	if (!ce)
		return;

	ce->mva = cd->buf.mva + cd->offset;

	mdla_cmd_debug(
		"%s: mva=0x%08x, offset=0x%x, count: %u, priority: %u, boost: %u\n",
		__func__, cd->buf.mva, cd->offset, cd->count, cd->priority,
		cd->boost_value);

	ce->state = CE_NONE;
	ce->flags = CE_NOP;
	ce->bandwidth = 0;
	ce->result = MDLA_CMD_SUCCESS;
	ce->count = cd->count;
	ce->receive_t = sched_clock();
	ce->kva = NULL;
}

int mdla_cmd_run_sync_v1_0(struct mdla_run_cmd_sync *cmd_data,
			   struct mdla_dev *mdla_info,
			   struct apusys_cmd_handle *apusys_hd,
			   uint32_t priority)
{
	u64 deadline = 0;
	struct mdla_run_cmd *cd = &cmd_data->req;
	struct command_entry ce;
	int ret = 0, boost_val;
	u32 core_id = 0;

	if (!cd || (apusys_hd->cmdbufs[CMD_INFO_IDX].size <
		    sizeof(struct mdla_run_cmd)))
		return -EINVAL;

	if ((cd->count == 0) ||
	    (cd->offset >= apusys_hd->cmdbufs[CMD_CODEBUF_IDX].size))
		return -1;

	memset(&ce, 0, sizeof(struct command_entry));
	core_id = mdla_info->mdla_id;
	ce.queue_t = sched_clock();

	/* The critical region of command enqueue */
	mutex_lock(&mdla_info->cmd_lock);
	mdla_pwr_ops_get()->wake_lock(core_id);

	mdla_cmd_prepare_v1_0(cd, apusys_hd, &ce);

process_command:
	deadline = get_jiffies_64() +
		   msecs_to_jiffies(mdla_dbg_read_u32(FS_TIMEOUT));

	mdla_info->max_cmd_id = 0;

	mdla_verbose("%s: core: %d max_cmd_id: %d id: %d\n", __func__, core_id,
		     mdla_info->max_cmd_id, ce.count);

	ret = mdla_pwr_ops_get()->on(core_id, false);
	if (ret)
		goto out;

	boost_val = (int)apusys_hd->boost;

	if (unlikely(mdla_dbg_read_u32(FS_DVFS_RAND)))
		boost_val = mdla_pwr_get_random_boost_val();

	mdla_pwr_ops_get()->set_opp_by_boost(core_id, boost_val);

	mdla_prof_start(core_id);
	mdla_trace_begin(core_id, &ce);

	mdla_util_apu_pmu_handle(mdla_info, apusys_hd, 0);

	ce.poweron_t = sched_clock();
	ce.req_start_t = sched_clock();

	mdla_cmd_plat_cb()->pre_cmd_handle(core_id, &ce);

	/* Fill HW reg */
	mdla_cmd_plat_cb()->process_command(core_id, &ce);

	/* Wait for timeout */
	while (mdla_info->max_cmd_id < ce.count &&
	       time_before64(get_jiffies_64(), deadline)) {
		wait_for_completion_interruptible_timeout(
			&mdla_info->command_done,
			mdla_cmd_plat_cb()->get_wait_time(core_id));

		/* E1 HW timeout check here */
		if (mdla_cmd_plat_cb()->wait_cmd_hw_detect(core_id) != 0) {
			mdla_pwr_ops_get()->hw_reset(
				mdla_info->mdla_id,
				mdla_dbg_get_reason_str(REASON_TIMEOUT));
			goto process_command;
		}
	}

	mdla_cmd_plat_cb()->post_cmd_info(core_id);

	ce.req_end_t = sched_clock();

	mdla_prof_iter(core_id);
	mdla_trace_end(core_id, 0, &ce);

	cd->id = mdla_info->max_cmd_id;

	if (mdla_info->max_cmd_id < ce.count) {
		mdla_timeout_debug("command: %d, max_cmd_id: %d\n", ce.count,
				   mdla_info->max_cmd_id);

		mdla_dbg_dump(mdla_info, &ce);
		mdla_pwr_ops_get()->hw_reset(
			mdla_info->mdla_id,
			mdla_dbg_get_reason_str(REASON_TIMEOUT));
		ret = -1;
	}

	mdla_pwr_ops_get()->off_timer_start(core_id);

	ce.wait_t = sched_clock();

	mdla_util_apu_pmu_update(mdla_info, apusys_hd, 0);

	apusys_hd->ip_time = (u32)((ce.wait_t - ce.poweron_t) / 1000);

out:
	mdla_pwr_ops_get()->wake_unlock(core_id);
	mutex_unlock(&mdla_info->cmd_lock);
	return ret;
}

int mdla_cmd_ut_run_sync_v1_0(void *run_cmd, void *wait_cmd,
			      struct mdla_dev *mdla_info)
{
	u64 deadline = 0;
	int ret = 0;
	struct ioctl_run_cmd *cd = (struct ioctl_run_cmd *)run_cmd;
	struct ioctl_wait_cmd *wt = (struct ioctl_wait_cmd *)wait_cmd;
	struct command_entry ce;
	u32 core_id = mdla_info->mdla_id;

	memset((void *)&ce, 0, sizeof(struct command_entry));
	ce.queue_t = sched_clock();

	/* The critical region of command enqueue */
	mutex_lock(&mdla_info->cmd_lock);
	mdla_pwr_ops_get()->wake_lock(core_id);

	mdla_cmd_ut_prepare_v1_0(cd, &ce);

	mdla_trace_begin(0, &ce);

process_command:
	/* Compute deadline */
	deadline = get_jiffies_64() +
		   msecs_to_jiffies(mdla_dbg_read_u32(FS_TIMEOUT));

	mdla_info->max_cmd_id = 0;

	mdla_timeout_debug("%s: max_cmd_id: %d id: %d\n", __func__,
			   mdla_info->max_cmd_id, ce.count);

	ret = mdla_pwr_ops_get()->on(core_id, false);
	if (ret)
		goto out;
	mdla_pwr_ops_get()->set_opp_by_boost(core_id, cd->boost_value);
	mdla_prof_start(core_id);

	ce.poweron_t = sched_clock();
	ce.req_start_t = sched_clock();

	/* Fill HW reg */
	mdla_cmd_plat_cb()->process_command(core_id, &ce);

	/* Wait for timeout */
	while (mdla_info->max_cmd_id < ce.count &&
	       time_before64(get_jiffies_64(), deadline)) {
		wait_for_completion_interruptible_timeout(
			&mdla_info->command_done,
			mdla_cmd_plat_cb()->get_wait_time(core_id));

		/* E1 HW timeout check here */
		if (mdla_cmd_plat_cb()->wait_cmd_hw_detect(core_id) != 0) {
			mdla_pwr_ops_get()->hw_reset(
				mdla_info->mdla_id,
				mdla_dbg_get_reason_str(REASON_TIMEOUT));
			goto process_command;
		}
	}
	ce.req_end_t = sched_clock();

	mdla_prof_iter(core_id);
	mdla_trace_end(core_id, 0, &ce);

	wt->id = ce.count;

	if (mdla_info->max_cmd_id >= ce.count)
		wt->result = 0;
	else { // Command timeout
		mdla_timeout_debug(
			"%s: command: %u, max_cmd_id: %u deadline:%llu, jiffies: %lu\n",
			__func__, ce.count, mdla_info->max_cmd_id, deadline,
			jiffies);
		mdla_dbg_dump(mdla_info, &ce);
		mdla_pwr_ops_get()->hw_reset(
			mdla_info->mdla_id,
			mdla_dbg_get_reason_str(REASON_TIMEOUT));
		wt->result = 1;
	}

	mdla_pwr_ops_get()->off_timer_start(core_id);

	ce.wait_t = sched_clock();

	/* Calculate all performance index */
	mdla_cmd_cal_perf_index(wt, &ce);

out:
	mdla_pwr_ops_get()->wake_unlock(core_id);
	mutex_unlock(&mdla_info->cmd_lock);
	return 0;
}
