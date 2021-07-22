// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>

#if IS_ENABLED(CONFIG_MTK_GIC_V3_EXT)
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
					struct apusys_cmd_handle *apusys_hd,
					struct command_entry *ce, int priority)
{
	ce->state = CE_NONE;
	ce->flags = CE_NOP;
	ce->bandwidth = 0;
	ce->result = MDLA_SUCCESS;
	ce->count = cd->count;
	ce->receive_t = sched_clock();
	ce->csn = priority + 1;

	ce->deadline_t = get_jiffies_64() +
			 msecs_to_jiffies(mdla_dbg_read_u32(FS_TIMEOUT));

	/* It is new command list */
	ce->fin_cid = 0;
	ce->wish_fin_cid = ce->count;
	ce->irq_state = IRQ_SUCCESS;
	ce->req_start_t = 0;

	ce->priority = priority;

	ce->boost_val = (int)apusys_hd->boost;
	ce->ctx_id = apusys_hd->vlm_ctx;
	ce->context_callback = apusys_hd->context_callback;
	apusys_hd->ip_time = 0;

	ce->cmdbuf = &apusys_hd->cmdbufs[CMD_CODEBUF_IDX];
	ce->kva = ce->cmdbuf->kva + cd->offset;
	ce->mva = apusys_mem_query_iova((u64)ce->kva);

	if (apusys_hd->multicore_total == 2) {
		ce->cmd_id = apusys_hd->kid;
		ce->multicore_total = apusys_hd->multicore_total;
		ce->cmd_batch_size = cd->count + 1;
	} else {
		ce->cmd_batch_size = apusys_hd->cluster_size;
	}

	init_completion(&ce->swcmd_done_wait);

	mdla_cmd_debug(
		"%s: kva=0x%llx(0x%llx+0x%x) mva=0x%08x cnt=%u sz=0x%x\n",
		__func__, (u64)ce->kva, (u64)ce->cmdbuf->kva, cd->offset, ce->mva,
		ce->count, ce->cmdbuf->size);
	mdla_verbose("%s: ctx_id=%d apu_hd_core_num=%d prio=%d batch_sz=%d)\n",
		     __func__, ce->ctx_id, apusys_hd->multicore_total,
		     ce->priority, ce->cmd_batch_size);
}

static void mdla_cmd_set_opp(u32 core_id, struct command_entry *ce,
			     int boost_val)
{
	if (ce->boost_val > boost_val)
		boost_val = ce->boost_val;
	else
		boost_val = 0;

	if (unlikely(mdla_dbg_read_u32(FS_DVFS_RAND)))
		boost_val = mdla_pwr_get_random_boost_val();

	if (boost_val)
		mdla_pwr_ops_get()->set_opp_by_boost(core_id, boost_val);
}

static int mdla_cmd_wrong_count_handler(struct mdla_dev *mdla_info,
					struct command_entry *ce)
{
	struct command_entry *timeout_ce;
	struct mdla_scheduler *sched = mdla_info->sched;
	u32 core_id = mdla_info->mdla_id;
	unsigned long flags;
	int prio;

	mdla_timeout_debug("Interrupt error status: %x\n",
			   mdla_info->error_bit);
	mdla_info->error_bit = 0;

	mdla_timeout_debug("Print current ce\n");
	mdla_dbg_ce_info(core_id, ce);
	mdla_timeout_debug("=====================\n");

	if (sched->pro_ce != NULL) {
		mdla_timeout_debug("Print process ce\n");
		mdla_dbg_ce_info(core_id, sched->pro_ce);
		mdla_timeout_debug("=====================\n");
	}

	for (prio = 0; prio < PRIORITY_LEVEL; prio++) {
		if (!sched->ce[prio])
			continue;
		mdla_timeout_debug("Print ce in driver\n");
		mdla_dbg_ce_info(core_id, sched->ce[prio]);
		mdla_timeout_debug("=====================\n");
	}

	/* handle command timeout */
	mdla_cmd_plat_cb()->post_cmd_hw_detect(core_id);
	mt_irq_dump_status(mdla_cmd_plat_cb()->get_irq_num(core_id));
	mdla_dbg_dump(mdla_info, ce);
	/* Enable & Relase bus protect */
	mdla_pwr_ops_get()->switch_off_on(core_id);
	mdla_pwr_ops_get()->hw_reset(mdla_info->mdla_id,
				     mdla_dbg_get_reason_str(REASON_TIMEOUT));

	/* error handling for scheudler by removing processing CE */
	spin_lock_irqsave(&sched->lock, flags);
	sched->pro_ce = NULL;

	do {
		timeout_ce = sched->dequeue_ce(core_id);
		if (timeout_ce != NULL) {
			timeout_ce->state |= (1 << CE_FAIL);
			complete(&timeout_ce->swcmd_done_wait);
		}
	} while (timeout_ce != NULL);

	spin_unlock_irqrestore(&sched->lock, flags);

	return -REASON_MDLA_TIMEOUT;
}

int mdla_cmd_run_sync_v1_x_sched(struct mdla_run_cmd_sync *cmd_data,
				 struct mdla_dev *mdla_info,
				 struct apusys_cmd_handle *apusys_hd,
				 uint32_t priority)
{
	int ret = REASON_MDLA_SUCCESS;
	unsigned long flags;
	u64 pwron_t;
	int pro_boost_val = 0;
	struct mdla_run_cmd *cd = &cmd_data->req;
	struct command_entry *ce;
	struct mdla_scheduler *sched = mdla_info->sched;
	u32 core_id = mdla_info->mdla_id;

	if (!cd || (apusys_hd->cmdbufs[CMD_INFO_IDX].size <
		    sizeof(struct mdla_run_cmd)))
		return -EINVAL;

	if ((cd->count == 0) ||
	    (cd->offset >= apusys_hd->cmdbufs[CMD_CODEBUF_IDX].size))
		return -1;

	/* need to define error code for scheduler is NULL */
	if (!sched)
		return -REASON_MDLA_NULLPOINT;

	if (unlikely(priority >= PRIORITY_LEVEL))
		return -EINVAL;

	ret = mdla_pwr_ops_get()->on(core_id, false);
	if (ret)
		return ret;
	pwron_t = sched_clock();

	/* initial global variable */
	spin_lock_irqsave(&sched->lock, flags);

	if (unlikely(sched->ce[priority])) {
		spin_unlock_irqrestore(&sched->lock, flags);
		return -EINVAL;
	}

	ce = kzalloc(sizeof(struct command_entry), GFP_ATOMIC);
	sched->ce[priority] = ce;

	if (sched->pro_ce)
		pro_boost_val = sched->pro_ce->boost_val;

	spin_unlock_irqrestore(&sched->lock, flags);

	if (ce == NULL)
		return -ENOMEM;

	mdla_pwr_ops_get()->wake_lock(core_id);

	/* prepare CE */
	mdla_cmd_prepare_v1_x_sched(cd, apusys_hd, ce, priority);

	ce->poweron_t = pwron_t;

	mdla_cmd_set_opp(core_id, ce, pro_boost_val);

	if (priority == MDLA_LOW_PRIORITY && ce->cmd_batch_size < ce->count)
		mdla_sched_plat_cb()->split_alloc_cmd_batch(ce);
	else
		ce->batch_list_head = NULL;

	mdla_util_apu_pmu_handle(mdla_info, apusys_hd, (u16)priority);

	mdla_prof_start(core_id);
	mdla_trace_begin(core_id, ce);

	mdla_cmd_plat_cb()->pre_cmd_handle(core_id, ce);

	if (ce->multicore_total > 1 && ce->priority == MDLA_LOW_PRIORITY) {
		spin_lock_irqsave(&sched->lock, flags);
		sched->enqueue_ce(core_id, ce, 0);
		spin_unlock_irqrestore(&sched->lock, flags);
		sched->issue_dual_lowce(core_id, ce->cmd_id);
	} else {
		spin_lock_irqsave(&sched->lock, flags);
		if (sched->pro_ce == NULL) {
			sched->pro_ce = ce;
			sched->issue_ce(core_id);
		} else {
			sched->enqueue_ce(core_id, ce, 0);
		}
		spin_unlock_irqrestore(&sched->lock, flags);
	}

	/* wait for deadline */
	while (ce->fin_cid < ce->count &&
	       time_before64(get_jiffies_64(), ce->deadline_t)) {
		wait_for_completion_interruptible_timeout(
			&ce->swcmd_done_wait,
			mdla_cmd_plat_cb()->get_wait_time(core_id));

		mdla_cmd_plat_cb()->wait_cmd_handle(core_id, ce);

		if (ce->state & BIT(CE_FAIL))
			goto error_handle;
	}

	if (unlikely(ce->fin_cid < ce->count))
		ret = mdla_cmd_wrong_count_handler(mdla_info, ce);

	apusys_hd->ip_time = (u32)ce->exec_time / 1000;

	/* update id to the last finished command id */
	cd->id = ce->fin_cid;

error_handle:

	mdla_trace_end(core_id, 0, ce);
	mdla_prof_iter(core_id);
	mdla_util_apu_pmu_update(mdla_info, apusys_hd, (u16)priority);

	spin_lock_irqsave(&sched->lock, flags);

	if (priority == MDLA_LOW_PRIORITY && ce->batch_list_head != NULL)
		mdla_sched_plat_cb()->del_free_cmd_batch(ce);

	kfree(sched->ce[priority]);
	sched->ce[priority] = NULL;

	spin_unlock_irqrestore(&sched->lock, flags);

	mdla_pwr_ops_get()->off_timer_start(core_id);
	mdla_pwr_ops_get()->wake_unlock(core_id);

	return ret;
}

int mdla_cmd_ut_run_sync_v1_x_sched(void *run_cmd, void *wait_cmd,
				    struct mdla_dev *mdla_info)
{
	return 0;
}
