// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>

#ifdef CONFIG_MTK_GIC_V3_EXT
#include <linux/irqchip/mtk-gic-extend.h>
#else
#define mt_irq_dump_status(n)
#endif

#include <apusys_device.h>

#include <common/mdla_device.h>
#include <common/mdla_cmd_proc.h>
#include <common/mdla_power_ctrl.h>
#include <common/mdla_ioctl.h>
#include <common/mdla_scheduler.h>

#include <utilities/mdla_profile.h>
#include <utilities/mdla_util.h>
#include <utilities/mdla_trace.h>
#include <utilities/mdla_debug.h>

#include "mdla_cmd_data_v1_x.h"


static void mdla_cmd_prepare_v1_x_sched(struct mdla_run_cmd *cd,
	struct apusys_cmd_hnd *apusys_hd,
	struct command_entry *ce, bool can_be_preempted)
{
	ce->mva = cd->mva + cd->offset;

	ce->state = CE_NONE;
	ce->flags = CE_NOP;
	ce->bandwidth = 0;
	ce->result = MDLA_SUCCESS;
	ce->count = cd->count;
	ce->receive_t = sched_clock();

	// initialize/resume members for preemption supportce->issue_t = 0;
	ce->deadline_t = get_jiffies_64()
			+ msecs_to_jiffies(mdla_dbg_read_u32(FS_TIMEOUT));

	/* It is new command list */
	ce->fin_cid = 0;
	ce->wish_fin_cid = ce->count;
	ce->irq_state = IRQ_SUCCESS;
	ce->req_start_t = 0;

	ce->boost_val = apusys_hd->boost_val;
	ce->ctx_id = apusys_hd->ctx_id;
	ce->context_callback = apusys_hd->context_callback;
	apusys_hd->ip_time = 0;
	ce->kva = (void *)(apusys_hd->cmd_entry + cd->offset_code_buf);
	ce->cmdbuf = apusys_hd->cmdbuf;
	ce->cmd_batch_en = can_be_preempted;
	if (apusys_hd->multicore_total == 2)
		ce->cmd_batch_size = cd->count + 1;
	else
		ce->cmd_batch_size = apusys_hd->cluster_size;

	init_completion(&ce->swcmd_done_wait);

	mdla_cmd_debug("%s: kva=0x%llx(0x%llx+0x%x) mva=0x%08x(0x%08x+0x%x) cnt=%u sz=0x%x\n",
			__func__,
			(u64)ce->kva,
			apusys_hd->cmd_entry,
			cd->offset_code_buf,
			ce->mva,
			cd->mva,
			cd->offset,
			ce->count,
			cd->size);
	mdla_verbose("%s: ctx_id=%d apu_hd_core_num=%d batch(en=%d sz=%d)\n",
			__func__,
			ce->ctx_id,
			apusys_hd->multicore_total,
			ce->cmd_batch_en,
			ce->cmd_batch_size);
}

static void mdla_cmd_ut_prepare_v1_x_sched(struct ioctl_run_cmd *cd,
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

	// initialize/resume members for preemption supportce->issue_t = 0;
	ce->deadline_t = get_jiffies_64()
			+ msecs_to_jiffies(mdla_dbg_read_u32(FS_TIMEOUT));

	/* It is new command list */
	ce->fin_cid = 0;
	ce->wish_fin_cid = ce->count;
	ce->irq_state = IRQ_SUCCESS;
	ce->req_start_t = 0;

	ce->boost_val = cd->boost_value;

	ce->cmd_batch_en = false;
	ce->batch_list_head = NULL;

	init_completion(&ce->swcmd_done_wait);
}

static struct command_entry *mdla_cmd_alloc_sched_ce(
		struct mdla_scheduler *sched, bool can_be_preempted)
{
	unsigned long flags;
	struct command_entry **ce = NULL;

	ce = can_be_preempted ? &sched->pro_ce_normal : &sched->pro_ce_high;

	/* initial global variable */
	spin_lock_irqsave(&sched->lock, flags);

	if (*ce) {
		spin_unlock_irqrestore(&sched->lock, flags);
		return NULL;
	}

	*ce = kzalloc(sizeof(struct command_entry), GFP_ATOMIC);

	spin_unlock_irqrestore(&sched->lock, flags);

	return *ce;
}

static void mdla_cmd_free_sched_ce(struct mdla_scheduler *sched,
					bool can_be_preempted)
{
	unsigned long flags;

	spin_lock_irqsave(&sched->lock, flags);
	if (can_be_preempted) {
		kfree(sched->pro_ce_normal);
		sched->pro_ce_normal = NULL;
	} else {
		kfree(sched->pro_ce_high);
		sched->pro_ce_high = NULL;
	}
	spin_unlock_irqrestore(&sched->lock, flags);
}

static void mdla_cmd_set_opp(u32 core_id, struct mdla_scheduler *sched, int ce_boost_val)
{
	unsigned long flags;
	int boost_val = ce_boost_val;

	spin_lock_irqsave(&sched->lock, flags);

	if (sched->pro_ce && ce_boost_val <= sched->pro_ce->boost_val)
		boost_val = 0;

	spin_unlock_irqrestore(&sched->lock, flags);

	if (unlikely(mdla_dbg_read_u32(FS_DVFS_RAND)))
		boost_val = mdla_pwr_get_random_boost_val();

	if (boost_val)
		mdla_pwr_ops_get()->set_opp_by_boost(core_id, boost_val);
}

static int mdla_cmd_wrong_count_handler(struct mdla_dev *mdla_info,
					struct command_entry *ce)
{
	struct mdla_scheduler *sched = mdla_info->sched;
	u32 core_id = mdla_info->mdla_id;
	unsigned long flags;
	int status = REASON_QUEUE_PREEMPTION;

	mdla_timeout_debug("Interrupt error status: %x\n",
		mdla_info->error_bit);
	mdla_timeout_debug("ce IRQ status: %x\n",
		ce->irq_state);
	mdla_info->error_bit = 0;
	mdla_timeout_debug("ce: total cmd count: %u, mva: %x",
			   ce->count, ce->mva);
	mdla_timeout_debug("ce: fin_cid: %u, deadline:%llu, ce state: %x\n",
		ce->fin_cid, ce->deadline_t, ce->state);
	mdla_timeout_debug("ce: wish fin_cid: %u\n",
		ce->wish_fin_cid);
	mdla_timeout_debug("ce: priority: %x, batch size = %u\n",
		(u32)ce->cmd_batch_en,
		ce->cmd_batch_size);
	if (sched->pro_ce != NULL) {
		mdla_timeout_debug("pro_ce IRQ status: %x\n",
			sched->pro_ce->irq_state);
		mdla_timeout_debug("pro_ce: total cmd count: %u, mva: %x",
				sched->pro_ce->count,
				sched->pro_ce->mva);
		mdla_timeout_debug("pro_ce: fin_cid: %u, state: %x\n",
			sched->pro_ce->fin_cid,
			sched->pro_ce->state);
		mdla_timeout_debug("pro_ce: wish fin_cid: %u\n",
			sched->pro_ce->wish_fin_cid);
		mdla_timeout_debug("pro_ce: priority: %x, batch size = %u\n",
			(u32)sched->pro_ce->cmd_batch_en,
			sched->pro_ce->cmd_batch_size);
	}
	/* handle command timeout */
	mdla_cmd_plat_cb()->post_cmd_hw_detect(core_id);
	mt_irq_dump_status(mdla_cmd_plat_cb()->get_irq_num(core_id));
	mdla_dbg_dump(mdla_info, ce);
	/* Enable & Relase bus protect */
	mdla_pwr_ops_get()->switch_off_on(core_id);
	mdla_pwr_ops_get()->hw_reset(mdla_info->mdla_id,
				mdla_dbg_get_reason_str(REASON_TIMEOUT));
	//wt->result = 1;
	/* error handling for scheudler by removing processing CE */
	spin_lock_irqsave(&sched->lock, flags);
	sched->pro_ce = NULL;

	while (status != REASON_QUEUE_NOCHANGE) {
		status = sched->dequeue_ce(core_id);
		sched->pro_ce = NULL;
		ce->state |= BIT(CE_FAIL);
		complete(&ce->swcmd_done_wait);
	}
	spin_unlock_irqrestore(&sched->lock, flags);

	return -REASON_MDLA_TIMEOUT;
}

int mdla_cmd_run_sync_v1_x_sched(struct mdla_run_cmd_sync *cmd_data,
				struct mdla_dev *mdla_info,
				struct apusys_cmd_hnd *apusys_hd,
				bool can_be_preempted)
{
	int ret = REASON_MDLA_SUCCESS;
	struct mdla_run_cmd *cd = &cmd_data->req;
	struct command_entry *ce;
	struct mdla_scheduler *sched = mdla_info->sched;

	u32 core_id = mdla_info->mdla_id;

	if (!cd || (cd->count == 0) || (apusys_hd->cmdbuf == NULL))
		return -EINVAL;

	/* need to define error code for scheduler is NULL */
	if (!sched)
		return -REASON_MDLA_NULLPOINT;

	mdla_pwr_ops_get()->wake_lock(core_id);

	/* initial global variable */
	ce = mdla_cmd_alloc_sched_ce(sched, can_be_preempted);
	if (ce == NULL)
		return -EINVAL;

	/* prepare CE */
	mdla_cmd_prepare_v1_x_sched(cd, apusys_hd, ce, can_be_preempted);

	ret = mdla_pwr_ops_get()->on(core_id, false);
	if (ret)
		goto out;

	mdla_cmd_set_opp(core_id, sched, ce->boost_val);

	ce->poweron_t = sched_clock();

	if (ce->cmd_batch_en && ce->cmd_batch_size < ce->count)
		mdla_sched_plat_cb()->split_alloc_cmd_batch(ce);
	else
		ce->batch_list_head = NULL;

	mdla_util_apu_pmu_handle(mdla_info,
			apusys_hd, ce->cmd_batch_en ? 1 : 0);
	mdla_prof_start(core_id);
	mdla_trace_begin(core_id, ce);

	mdla_cmd_plat_cb()->pre_cmd_handle(core_id, ce);
	/* enqueue CE */
	sched->enqueue_ce(core_id, ce);

	/* wait for deadline */
	while (ce->fin_cid < ce->count
			&& time_before64(get_jiffies_64(), ce->deadline_t)) {

		wait_for_completion_interruptible_timeout(
				&ce->swcmd_done_wait,
				mdla_cmd_plat_cb()->get_wait_time(core_id));

		if (ce->state & BIT(CE_FAIL))
			goto error_handle;
	};

	if (unlikely(ce->fin_cid < ce->count))
		ret = mdla_cmd_wrong_count_handler(mdla_info, ce);

	apusys_hd->ip_time +=
			(u32)(ce->req_end_t - ce->req_start_t)/1000;

	/* update id to the last finished command id */
	cd->id = ce->fin_cid;

error_handle:

	mdla_trace_end(core_id, 0, ce);
	mdla_prof_stop(core_id, 1);
	mdla_prof_iter(core_id);
	mdla_util_apu_pmu_update(mdla_info,
				apusys_hd, ce->cmd_batch_en ? 1 : 0);

	if (ce->cmd_batch_en && ce->batch_list_head != NULL)
		mdla_sched_plat_cb()->del_free_cmd_batch(ce);

	mdla_cmd_free_sched_ce(sched, can_be_preempted);

	mdla_pwr_ops_get()->off_timer_start(core_id);

out:
	mdla_pwr_ops_get()->wake_unlock(core_id);

	return ret;
}

int mdla_cmd_ut_run_sync_v1_x_sched(void *run_cmd, void *wait_cmd,
				struct mdla_dev *mdla_info)
{
	int ret = REASON_MDLA_SUCCESS;
	struct ioctl_run_cmd *cd = (struct ioctl_run_cmd *)run_cmd;
	struct ioctl_wait_cmd *wt = (struct ioctl_wait_cmd *)wait_cmd;
	struct command_entry *ce;
	struct mdla_scheduler *sched = mdla_info->sched;
	u32 core_id = mdla_info->mdla_id;

	if (!cd || (cd->count == 0))
		return -EINVAL;

	/* need to define error code for scheduler is NULL */
	if (!sched)
		return -REASON_MDLA_NULLPOINT;

	wt->result = 0;

	mdla_pwr_ops_get()->wake_lock(core_id);

	/* initial global variable */
	ce = mdla_cmd_alloc_sched_ce(sched, false);
	if (ce == NULL)
		return -EINVAL;

	/* prepare CE */
	mdla_cmd_ut_prepare_v1_x_sched(cd, ce);

	ret = mdla_pwr_ops_get()->on(core_id, false);
	if (ret)
		goto out;

	mdla_cmd_set_opp(core_id, sched, ce->boost_val);

	ce->poweron_t = sched_clock();

	mdla_prof_start(core_id);
	mdla_trace_begin(core_id, ce);

	mdla_cmd_plat_cb()->pre_cmd_handle(core_id, ce);
	/* enqueue CE */
	sched->enqueue_ce(core_id, ce);

	/* wait for deadline */
	while (ce->fin_cid < ce->count
			&& time_before64(get_jiffies_64(), ce->deadline_t)) {

		wait_for_completion_interruptible_timeout(
				&ce->swcmd_done_wait,
				mdla_cmd_plat_cb()->get_wait_time(core_id));

		if (ce->state & BIT(CE_FAIL))
			goto error_handle;
	};

	mdla_prof_iter(core_id);
	mdla_trace_end(core_id, 0, ce);

	if (unlikely(ce->fin_cid < ce->count))
		ret = mdla_cmd_wrong_count_handler(mdla_info, ce);

	/* update id to the last finished command id */
	wt->id = ce->fin_cid;
	cd->id = ce->fin_cid;

error_handle:

	ce->wait_t = sched_clock();

	wt->queue_time = ce->poweron_t - ce->receive_t;
	wt->busy_time = ce->wait_t - ce->poweron_t;
	wt->bandwidth = ce->bandwidth;

	mdla_cmd_free_sched_ce(sched, false);

	mdla_prof_stop(core_id, 1);
	mdla_pwr_ops_get()->off_timer_start(core_id);

out:
	mdla_pwr_ops_get()->wake_unlock(core_id);

	return ret;
}
