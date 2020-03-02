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
#ifdef CONFIG_OF
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#endif

#include "mdla.h"
#include "mdla_trace.h"
#include "mdla_debug.h"
#include "mdla_plat_api.h"
#include "mdla_util.h"
#include "mdla_power_ctrl.h"
#include "mdla_cmd_proc.h"
#include "mdla_hw_reg.h"
#include "mdla_pmu.h"

/* internal function prototypes */
int mdla_run_command_sync(struct mdla_run_cmd *cd,
	struct mdla_wait_cmd *wt, struct mdla_dev *mdla_info);

/* if there's no more reqeusts
 * 1. delete command timeout timer
 * 2. setup delay power off timer
 * this function is protected by cmd_list_lock
 */
void mdla_command_done(int core_id)
{
	mutex_lock(&mdla_devices[core_id].power_lock);
	mdla_profile_stop(1);
	mdla_setup_power_down(core_id);
	mutex_unlock(&mdla_devices[core_id].power_lock);
}

static void
mdla_run_command_prepare(struct mdla_run_cmd *cd, struct command_entry *ce)
{

	char *ptr = (char *)cd->kva;
	unsigned int size  = cd->size;

	if (!ce)
		return;

	ce->mva = cd->mva + cd->offset;


	mdla_cmd_debug("%s: mva=%08x, offset=%08x, count: %u\n",
			__func__,
			cd->mva,
			cd->offset,
			cd->count);

	mdla_cmd_debug("%s: kva=%p, size =%08x\n",
			__func__,
			ptr,
			size);

	ce->state = CE_NONE;
	ce->flags = CE_NOP;
	ce->bandwidth = 0;
	ce->result = MDLA_CMD_SUCCESS;
	ce->count = cd->count;
#ifndef __APUSYS_MIDDLEWARE__
	ce->khandle = cd->buf.ion_khandle;
#endif
	//ce->type = cd->buf.type;
	//ce->priority = cd->priority;
	//ce->boost_value = cd->boost_value;
	ce->receive_t = sched_clock();
	ce->kva = NULL;

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
int mdla_run_command_sync(struct mdla_run_cmd *cd, struct mdla_wait_cmd *wt,
			  struct mdla_dev *mdla_info)
{
	int ret = 0;
	struct command_entry ce;
	u32 id;
	u64 deadline;
	int core_id = 0;

	if (!cd || !wt || !mdla_info)
		return -EINVAL;

	core_id = mdla_info->mdlaid;
	ce.queue_t = sched_clock();

	/* The critical region of command enqueue */
	mutex_lock(&mdla_info->cmd_lock);

	mdla_run_command_prepare(cd, &ce);

process_command:
	/* Compute deadline */
	deadline = get_jiffies_64() + msecs_to_jiffies(mdla_timeout);

	mdla_info->max_cmd_id = 0;

	id = ce.count;

	mdla_cmd_debug("%s: core: %d max_cmd_id: %d id: %d\n",
			__func__, core_id, mdla_info->max_cmd_id, id);

#ifdef __APUSYS_MIDDLEWARE__
	//TODO:FIXME for power on MDLA
	mdla_pwr_on(core_id);
#else
	mdla_power_on(&ce);
#endif

	/* Trace start */
	mdla_trace_begin(core_id, &ce);

	ce.poweron_t = sched_clock();
	ce.req_start_t = sched_clock();

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
		wait_for_completion_interruptible_timeout(
			&mdla_info->command_done, wait_event_timeouts);
		/* E1 HW timeout check here */
		if (hw_e1_timeout_detect(core_id) != 0) {
			mdla_reset_lock(mdla_info->mdlaid, REASON_TIMEOUT);
			goto process_command;
		}
	}

#ifdef __APUSYS_MDLA_UT__
	pr_info("%s: MREG_TOP_G_INTP0: %.8x, MREG_TOP_G_FIN0: %.8x, MREG_TOP_G_FIN1: %.8x\n",
		__func__,
		mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_INTP0),
		mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_FIN0),
		mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_FIN1));

	pr_info("%s: MREG_TOP_G_INTP3: %.8x\n",
		__func__,
		mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_FIN3));

	/*MDLA-PMU Command Counter*/
	mdla_cmd_debug("%s: PMU_CFG_PMCR: %8x, pmu_clk_cnt: %.8x\n",
				__func__,
				pmu_reg_read(PMU_CFG_PMCR),
				pmu_reg_read(PMU_CYCLE));

#endif

	ce.req_end_t = sched_clock();

	mdla_trace_iter(core_id);

	/* Trace stop */
	mdla_trace_end(&ce);

	wt->id = id;

	if (mdla_info->max_cmd_id >= id)
		wt->result = 0;
	else { // Command timeout
		pr_info("%s: command: %u, max_cmd_id: %u deadline:%llu, jiffies: %lu\n",
				__func__, id,
				mdla_info->max_cmd_id,
				deadline, jiffies);
		mdla_dump_reg(core_id);
		mdla_dump_ce(&ce);
		mdla_reset_lock(mdla_info->mdlaid, REASON_TIMEOUT);
		wt->result = 1;
	}

	/* Start power off timer */
	mdla_command_done(core_id);

	ce.wait_t = sched_clock();

	mutex_unlock(&mdla_info->cmd_lock);

	/* Calculate all performance index */
	mdla_performance_index(wt, &ce);

#ifdef __APUSYS_MDLA_UT__
	mdla_perf_debug("exec: id:%d, res:%u, que_t:%u, busy_t:%u,bandwidth: %u\n",
			wt->id, wt->result, wt->queue_time,
			wt->busy_time, wt->bandwidth);
#endif

	return ret;
}
#endif

#ifndef __APUSYS_MIDDLEWARE__
void mdla_wait_command(struct ioctl_wait_cmd *wt)
{
	struct list_head *ele, *next;
	struct wait_entry *we;

	wt->result = -1;
	mdla_cmd_debug("%s: id: %u\n", __func__, wt->id);
	mutex_lock(&cmd_list_lock);
	list_for_each_safe(ele, next, &cmd_list) {
		mdla_cmd_debug("%s: loop id: %u\n", __func__, wt->id);
		we = list_entry(ele, struct wait_entry, list);
		if (wt->id == we->async_id) {
			mdla_cmd_debug("%s: found id: %u\n", __func__, wt->id);
			memcpy(wt, &we->wt, sizeof(struct ioctl_wait_cmd));
			list_del(&we->list);
			kfree(we);
			break;
		}
	}
	mutex_unlock(&cmd_list_lock);
}



static int mdla_run_command_async(struct ioctl_run_cmd *cd)
{
	struct wait_entry *we = kmalloc(sizeof(struct wait_entry),
			GFP_KERNEL);

	if (we == NULL)
		return -1;

	mdla_run_command_sync(cd, &we->wt);
	if (we->wt.result != 0) {
		kfree(we);
		return -1;
	}
	mutex_lock(&cmd_list_lock);
	we->async_id = async_cmd_id++;
	list_add_tail(&we->list, &cmd_list);
	mutex_unlock(&cmd_list_lock);
	mdla_cmd_debug("%s: %d\n", __func__, we->async_id);
	return we->async_id;
}
#endif
