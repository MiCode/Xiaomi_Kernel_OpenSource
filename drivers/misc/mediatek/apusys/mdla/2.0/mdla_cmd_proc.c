/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <asm/mman.h>
#include <linux/dmapool.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/sched/clock.h>
#include <linux/pm_wakeup.h>
#ifdef CONFIG_OF
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#endif

#include "mdla.h"
#include <linux/random.h>
#include "apusys_power_cust.h"
#include "mdla_trace.h"
#include "mdla_debug.h"
#include "mdla_plat_api.h"
#include "mdla_util.h"
#include "mdla_power_ctrl.h"
#include "mdla_cmd_proc.h"
#include "mdla_hw_reg.h"
#include "mdla_pmu.h"
#include "apusys_power.h"
#include <asm-generic/bug.h>       /* BUG_ON */
#include "apusys_device.h"


#ifdef CONFIG_PM_WAKELOCKS
static struct wakeup_source *mdla_ws;
static uint32_t ws_count;
#endif

void mdla_wakeup_source_init(void)
{
#ifdef CONFIG_PM_WAKELOCKS
	ws_count = 0;
	mdla_ws = wakeup_source_register("mdla");
	if (!mdla_ws)
		pr_debug("mdla wakelock register fail!\n");
#endif
}

/* if there's no more reqeusts
 * 1. delete command timeout timer
 * 2. setup delay power off timer
 * this function is protected by cmd_list_lock
 */
void mdla_command_done(int core_id)
{
	mutex_lock(&mdla_devices[core_id].power_lock);
	mdla_profile_stop(core_id, 1);
	mdla_setup_power_down(core_id);
	mutex_unlock(&mdla_devices[core_id].power_lock);
}

#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
static void
mdla_run_command_prepare(struct mdla_run_cmd *cd,
	struct apusys_cmd_hnd *apusys_hd, struct command_entry *ce)
#else
static void
mdla_run_command_prepare(struct mdla_run_cmd *cd, struct command_entry *ce)
#endif
{

	if (!ce)
		return;

	ce->mva = cd->mva + cd->offset;

	if (mdla_timeout_dbg)
		mdla_cmd_debug("%s: mva=%08x, offset=%08x, count: %u\n",
				__func__,
				cd->mva,
				cd->offset,
				cd->count);

	ce->state = CE_NONE;
	ce->flags = CE_NOP;
	ce->bandwidth = 0;
	ce->result = MDLA_CMD_SUCCESS;
	ce->count = cd->count;
	ce->receive_t = sched_clock();
	ce->kva = NULL;

#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
	if (apusys_hd != NULL)
		ce->kva = (void *)(apusys_hd->cmd_entry+cd->offset_code_buf);
#endif


#ifdef __APUSYS_PREEMPTION__
	// initialize members for preemption support
	//ce->kva = cd->buf.kva + cd->offset;
	ce->kva = cd->kva + cd->offset;
	ce->fin_cid = 0;
	ce->preempted = true;
	// TODO: get the batch size from command buffer
	ce->cmd_batch_size = 1;
#endif
}

static void mdla_performance_index(struct mdla_wait_cmd *wt,
		struct command_entry *ce)
{
	wt->queue_time = ce->poweron_t - ce->receive_t;
	wt->busy_time = ce->wait_t - ce->poweron_t;
	wt->bandwidth = ce->bandwidth;
}

#ifdef __APUSYS_PREEMPTION__
int mdla_run_command_sync(struct mdla_run_cmd *cd, struct mdla_wait_cmd *wt,
			  struct mdla_dev *mdla_info)
{
	struct command_entry ce;
	struct mdla_scheduler *scheduler;
	unsigned long flags, timeout;
	u64 deadline;
	long status;
	int core_id;

	if (!cd || !wt || !mdla_info)
		return -EINVAL;

	core_id = mdla_info->mdlaid;
	scheduler = mdla_info->scheduler;

	/* prepare CE */
	mdla_run_command_prepare(cd, &ce);

	/* trace start */
	mdla_trace_begin(core_id, &ce);

	/* setup timeout and deadline */
	timeout = (cfg_timer_en) ? usecs_to_jiffies(cfg_period) :
		msecs_to_jiffies(mdla_e1_detect_timeout);
	deadline = get_jiffies_64() + msecs_to_jiffies(mdla_timeout);

	mdla_timeout_debug("%s: last finished cmd id: %d total cmd count: %d\n",
			__func__, ce.fin_cid, ce.count);

	/* enqueue CE */
	spin_lock_irqsave(&scheduler->lock, flags);
	scheduler->enqueue_ce(core_id, &ce);
	spin_unlock_irqrestore(&scheduler->lock, flags);

	wt->result = 1;

	/* wait for deadline */
	while (time_before64(get_jiffies_64(), deadline)) {
		/* wait for timeout */
		status = wait_for_completion_interruptible_timeout(&ce.done,
								   timeout);
		/*
		 * check the CE completed or timeout
		 * case 1 (status >= 1): CE completed
		 * case 2 (status == 0): timeout, reason: preempted or SW bug
		 * case 3 (status == 0): timeout, reason: HW timeout issue
		 * case 4 (status == -ERESTARTSYS): interrupted, do nothing
		 */
		if (status > 0) {
			/* case 1: CE completed */
			wt->result = 0;
		} else if (status == 0) {
			/* case 2 or 3: check E1 HW timeout here */
			if (hw_e1_timeout_detect(core_id) != 0) {
				/* case 3: E1 HW timeout */
				mdla_reset_lock(core_id, REASON_TIMEOUT);

				spin_lock_irqsave(&scheduler->lock, flags);
				scheduler->issue_ce(core_id);
				spin_unlock_irqrestore(&scheduler->lock, flags);
			} else {
				/* case 2: might be SW issue */
			}
		}
	}

	mdla_trace_iter(core_id);
	ce.wait_t = sched_clock();

	if (wt->result != 0) {
		/* handle command timeout */
		mdla_timeout_debug("%s: total cmd count: %u, fin_cid: %u",
				   __func__, ce.count, ce.fin_cid);
		mdla_timeout_debug("%s: deadline:%llu, jiffies: %lu\n",
				   __func__, deadline, jiffies);
		mdla_dump_reg(core_id);
		mdla_dump_ce(&ce);
		mdla_reset_lock(core_id, REASON_TIMEOUT);
	}

	/* trace stop */
	mdla_trace_end(&ce);

	/* calculate all performance index */
	mdla_performance_index(wt, &ce);

	/* update id to the last finished command id */
	wt->id = ce.fin_cid;

	return 0;
}
#else
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
int mdla_run_command_sync(struct mdla_run_cmd *cd, struct mdla_dev *mdla_info,
			  struct apusys_cmd_hnd *apusys_hd)
#else
int mdla_run_command_sync(struct mdla_run_cmd *cd,
			  struct mdla_dev *mdla_info)
#endif
{
	int ret = 0;
	struct command_entry ce;
	u32 id = 0;
	u64 deadline = 0;
	int core_id = 0;
	int opp_rand = 0;
	long status = 0;
	/*forward compatibility temporary, This will be replaced by apusys*/
	struct mdla_wait_cmd mdla_wt;
	struct mdla_wait_cmd *wt = &mdla_wt;

	if (!cd || !mdla_info || (mdla_info->mdlaid >= mdla_max_num_core))
		return -EINVAL;

	memset(&ce, 0, sizeof(ce));
	core_id = mdla_info->mdlaid;
	ce.queue_t = sched_clock();

	/* The critical region of command enqueue */
	mutex_lock(&mdla_info->cmd_lock);

#ifdef CONFIG_PM_WAKELOCKS
	if (mdla_ws && !ws_count)
		__pm_stay_awake(mdla_ws);
	ws_count++;
#endif

#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
	mdla_run_command_prepare(cd, apusys_hd, &ce);
	if (!pmu_apusys_pmu_addr_check(apusys_hd))
		pmu_command_prepare(mdla_info, apusys_hd);
#else
	/*MDLA Pattern only*/
	mdla_run_command_prepare(cd, &ce);
#endif
	/* Compute deadline */
	deadline = get_jiffies_64() + msecs_to_jiffies(mdla_timeout);

	mdla_info->max_cmd_id = 0;

	id = ce.count;

	if (mdla_timeout_dbg)
		mdla_cmd_debug("%s: core: %d max_cmd_id: %d id: %d\n",
				__func__, core_id, mdla_info->max_cmd_id, id);

	ret = mdla_pwr_on(core_id);
	if (ret)
		goto mdla_cmd_done;


	if (apusys_hd != NULL)
		mdla_set_opp(core_id, apusys_hd->boost_val);

	if (mdla_dvfs_rand) {
		opp_rand = get_random_int() % APUSYS_MAX_NUM_OPPS;
		mdla_cmd_debug("core: %d, rand opp: %d\n",
			core_id, opp_rand);
		apu_device_set_opp(MDLA0+core_id, opp_rand);
	}

	/* Trace start */
	mdla_trace_begin(core_id, &ce);
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
	pmu_cmd_handle(mdla_info, apusys_hd);
#endif
	ce.poweron_t = sched_clock();
	ce.req_start_t = sched_clock();


	if (mdla_timeout_dbg) {
		mdla_dump_cmd_buf_free(mdla_info->mdlaid);
		mdla_create_dmp_cmd_buf(&ce, mdla_info);
	}


	/* Fill HW reg */
	mdla_process_command(core_id, &ce);

	/* Wait for timeout */
	while (mdla_info->max_cmd_id < id &&
		time_before64(get_jiffies_64(), deadline)) {
		unsigned long wait_event_timeouts;

		if (cfg_timer_en)
			wait_event_timeouts =
				usecs_to_jiffies(cfg_period);
		else
			wait_event_timeouts =
				msecs_to_jiffies(mdla_e1_detect_timeout);
		status = wait_for_completion_interruptible_timeout(
			&mdla_info->command_done, wait_event_timeouts);

		/*
		 * check the Command completed or timeout
		 * case 1 (status == 0): timeout, reason: HW timeout issue
		 */
		if (status == 0) {
			/* wakeup for check status */

		}
	}

	if (mdla_timeout_dbg)
		mdla_cmd_debug("%s: C:%d,FIN0:%.8x,FIN1: %.8x,FIN3: %.8x\n",
			__func__,
			core_id,
			mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_FIN0),
			mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_FIN1),
			mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_FIN3));

	mdla_pmu_debug("%s: PMU_CFG_PMCR: %8x, pmu_clk_cnt: %.8x\n",
		__func__,
		pmu_reg_read_with_mdlaid(core_id, PMU_CFG_PMCR),
		pmu_reg_read_with_mdlaid(core_id, PMU_CYCLE));

	ce.req_end_t = sched_clock();

	mdla_trace_iter(core_id);

	/* Trace stop */
	mdla_trace_end(core_id, 0, &ce);

	cd->id = mdla_info->max_cmd_id;

	if (mdla_timeout_dbg)
		mdla_cmd_debug("STE dst addr:%.8x\n",
		mdla_reg_read_with_mdlaid(core_id, 0xE3C));

	if (mdla_info->max_cmd_id >= id) {
		wt->result = 0;
	}
	else { // Command timeout
		mdla_timeout_debug("command: %d, max_cmd_id: %d\n",
				id,
				mdla_info->max_cmd_id);
		mdla_zero_skip_detect(core_id);
		mdla_dump_dbg(mdla_info, &ce);
		// Enable & Relase bus protect
		apu_device_power_off(MDLA0+core_id);
		apu_device_power_on(MDLA0+core_id);
		mdla_reset_lock(mdla_info->mdlaid, REASON_TIMEOUT);
		wt->result = 1;
		ret = -1;
	}

	/* Start power off timer */
	mdla_command_done(core_id);

	ce.wait_t = sched_clock();

	/* Calculate all performance index */
	mdla_performance_index(wt, &ce);

#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
	if (!pmu_apusys_pmu_addr_check(apusys_hd)) {
		pmu_command_counter_prt(mdla_info);
	}
	if (apusys_hd != NULL)
		apusys_hd->ip_time = (uint32_t)(wt->busy_time/1000);
#endif

mdla_cmd_done:
#ifdef CONFIG_PM_WAKELOCKS
	ws_count--;
	if (mdla_ws && !ws_count)
		__pm_relax(mdla_ws);
#endif
	mutex_unlock(&mdla_info->cmd_lock);
	return ret;
}
#endif
