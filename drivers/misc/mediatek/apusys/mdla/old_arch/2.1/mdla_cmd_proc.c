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
#include <linux/irqchip/mtk-gic-extend.h>

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


#ifdef CONFIG_PM_SLEEP
static struct wakeup_source *mdla_ws;
static uint32_t ws_count;
#endif

static void mdla_run_command_prepare(
	struct mdla_run_cmd *cd,
	struct apusys_cmd_hnd *apusys_hd,
	struct command_entry *ce,
	u16 priority);

static void mdla_timeout_print_ce(
	unsigned int core_id,
	struct command_entry *ce);

static int mdla_timeout_handle(
	unsigned int core_id,
	struct command_entry *ce);

static unsigned long mdla_get_dual_deadline(
	u16 priority
);

int mdla_run_command_sync(
	struct mdla_run_cmd *cd,
	struct mdla_dev *mdla_info,
	struct apusys_cmd_hnd *apusys_hd,
	u16 priority)
{
	int ret = REASON_MDLA_SUCCESS;
	struct command_entry *ce;
	struct mdla_scheduler *sched;
	unsigned long flags;
	long status;
	unsigned int core_id;
	/*forward compatibility temporary, This will be replaced by apusys*/
	int opp_rand = 0;
	int boost_val = 0;

	if (unlikely(cd == NULL || mdla_info == NULL))
		return -EINVAL;
	if (unlikely(cd->count == 0))
		return 0;
	/*
	 * apusys_hd == NULL: it call from UT
	 */
	if (unlikely(apusys_hd != NULL)) {
		if (unlikely(apusys_hd->cmdbuf == NULL))
			return -EINVAL;
	}
	core_id = mdla_info->mdlaid;
	sched = mdla_info->sched;
	/* need to define error code for scheduler is NULL */
	if (unlikely(sched == NULL))
		return -REASON_MDLA_NULLPOINT;

#ifdef CONFIG_PM_SLEEP
	mutex_lock(&wake_lock_mutex);
	if (mdla_ws && !ws_count)
		__pm_stay_awake(mdla_ws);
	ws_count++;
	mutex_unlock(&wake_lock_mutex);
#endif

	ret = mdla_pwr_on(core_id, false);
	if (unlikely(ret))
		goto mdla_cmd_done;
	/* initial global variable */
	spin_lock_irqsave(&sched->lock, flags);
	if (unlikely(sched->ce[priority] != NULL)) {
		spin_unlock_irqrestore(&sched->lock, flags);
		return -EINVAL;
	}
	sched->ce[priority] =
		kzalloc(sizeof(struct command_entry), GFP_ATOMIC);
	if (unlikely(sched->ce[priority] == NULL)) {
		spin_unlock_irqrestore(&sched->lock, flags);
		return -ENOMEM;
	}
	ce = sched->ce[priority];
	if (unlikely(ce == NULL)) {
		spin_unlock_irqrestore(&sched->lock, flags);
		return -EINVAL;
	}
	/* Get now boost_val */
	if (unlikely(sched->pro_ce != NULL))
		boost_val = sched->pro_ce->boost_val;
	spin_unlock_irqrestore(&sched->lock, flags);

	/* prepare CE */
	mdla_run_command_prepare(cd, apusys_hd, ce, priority);
#ifdef __APUSYS_MDLA_PMU_SUPPORT__
	if (likely(!pmu_apusys_pmu_addr_check(apusys_hd))) {
		pmu_command_prepare(
			mdla_info,
			apusys_hd,
			ce->priority);
	}
#endif//__APUSYS_MDLA_PMU_SUPPORT__
	if (likely(apusys_hd != NULL)) {
		if (ce->boost_val > boost_val)
			mdla_set_opp(core_id, ce->boost_val);
	} else if (mdla_dvfs_rand) {
		opp_rand = get_random_int() % APUSYS_MAX_NUM_OPPS;
		mdla_cmd_debug("core: %d, rand opp: %d\n",
			core_id, opp_rand);
		apu_device_set_opp(MDLA0+core_id, opp_rand);
	}
	if (ce->priority == MDLA_LOW_PRIORITY && ce->batch_list_head != NULL)
		mdla_split_command_batch(ce);

	/* trace start */
	mdla_trace_begin(core_id, ce);

#ifdef __APUSYS_MDLA_PMU_SUPPORT__
	pmu_cmd_handle(mdla_info, apusys_hd, ce->priority);
#endif

	/* enqueue/issue CE */
	init_completion(&ce->swcmd_done_wait);
	if (ce->multicore_total == 2 && ce->priority == MDLA_LOW_PRIORITY) {
		spin_lock_irqsave(&sched->lock, flags);
		sched->enqueue_ce(core_id, ce, 0);
		spin_unlock_irqrestore(&sched->lock, flags);
		sched->issue_dual_lowce(core_id, ce->cmd_id);
	} else {
		spin_lock_irqsave(&sched->lock, flags);
		if (sched->pro_ce == NULL) {
			sched->pro_ce = ce;
			sched->issue_ce(core_id);
		} else
			sched->enqueue_ce(core_id, ce, 0);
		spin_unlock_irqrestore(&sched->lock, flags);
	}

	/* wait for deadline */
	do {
		unsigned long wait_event_timeouts;
		unsigned long deadline;

		if (cfg_timer_en)
			wait_event_timeouts =
				usecs_to_jiffies(cfg_period);
		else
			wait_event_timeouts =
				msecs_to_jiffies(mdla_e1_detect_timeout);

		status =
			wait_for_completion_interruptible_timeout(
				&ce->swcmd_done_wait, wait_event_timeouts);
		/* update SMP CMD deadline  */
		if (ce->multicore_total == 2) {
			deadline = mdla_get_dual_deadline(ce->priority);
			if (deadline > ce->deadline_t)
				ce->deadline_t = deadline;
		}
		if (unlikely(ce->state & (1 << CE_FAIL)))
			goto error_handle;
	} while (ce->fin_cid < ce->count &&
		time_before64(get_jiffies_64(), ce->deadline_t));
	mdla_trace_iter(core_id);

	/* trace stop */
	mdla_trace_end(core_id, 0, ce);
	if (unlikely(mdla_timeout_dbg))
		mdla_cmd_debug("dst addr:%.8x\n",
			mdla_reg_read_with_mdlaid(core_id, 0xE3C));

	if (unlikely(ce->fin_cid < ce->count))
		ret = mdla_timeout_handle(core_id, ce);
	if (likely(apusys_hd != NULL)) {
		apusys_hd->ip_time +=
			(uint32_t)(ce->req_end_t - ce->req_start_t)/1000;
	}
	/* update id to the last finished command id */
	cd->id = ce->fin_cid;
#ifdef __APUSYS_MDLA_PMU_SUPPORT__
	if (!pmu_apusys_pmu_addr_check(apusys_hd))
		pmu_command_counter_prt(mdla_info, ce->priority);
#endif

error_handle:
	ce->wait_t = sched_clock();

	spin_lock_irqsave(&sched->lock, flags);
	if (ce->priority == MDLA_LOW_PRIORITY && ce->batch_list_head != NULL)
		mdla_del_free_command_batch(ce);
	kfree(sched->ce[priority]);
	sched->ce[priority] = NULL;
	spin_unlock_irqrestore(&sched->lock, flags);

	/* Start power off timer */
	mdla_command_done(core_id);
mdla_cmd_done:
#ifdef CONFIG_PM_SLEEP
	mutex_lock(&wake_lock_mutex);
	ws_count--;
	if (mdla_ws && !ws_count)
		__pm_relax(mdla_ws);
	mutex_unlock(&wake_lock_mutex);
#endif
	return ret;
}

static void mdla_timeout_print_ce(
	unsigned int core_id,
	struct command_entry *ce
)
{
	mdla_timeout_debug("core_id: %x, ce(%x) IRQ status: %x, footprint: %llx\n",
		core_id,
		(u32)ce->priority,
		ce->irq_state,
		ce->footprint);
	mdla_timeout_debug("core_id: %x, ce(%x): total cmd count: %u, mva: %x",
		core_id,
		(u32)ce->priority,
		ce->count,
		ce->mva);
	mdla_timeout_debug("core_id: %x, ce(%x): fin_cid: %u, ce state: %x\n",
		core_id,
		(u32)ce->priority,
		ce->fin_cid, ce->state);
	mdla_timeout_debug("core_id: %x, ce(%x): wish fin_cid: %u, dual: %d\n",
		core_id,
		(u32)ce->priority,
		ce->wish_fin_cid,
		ce->multicore_total);
	mdla_timeout_debug("core_id: %x, ce(%x): batch size = %u\n",
		core_id,
		(u32)ce->priority,
		ce->cmd_batch_size);
}

static int mdla_timeout_handle(
	unsigned int core_id,
	struct command_entry *ce)
{
	struct mdla_dev *mdla_info = &mdla_devices[core_id];
	struct command_entry *timeout_ce;
	struct mdla_scheduler *sched = mdla_info->sched;
	unsigned long flags;
	uint16_t i;

	mdla_timeout_debug("Interrupt error status: %x\n",
		mdla_info->error_bit);
	mdla_info->error_bit = 0;
	mdla_timeout_debug("Print timeout ce\n");
	mdla_timeout_print_ce(core_id, ce);
	mdla_timeout_debug("==========================\n");
	if (sched->pro_ce != NULL) {
		mdla_timeout_debug("Print process ce\n");
		mdla_timeout_print_ce(core_id, sched->pro_ce);
		mdla_timeout_debug("==========================\n");
	}
	for (i = 0; i < PRIORITY_LEVEL; i++) {
		if (sched->ce[i] != NULL) {
			mdla_timeout_debug("Print ce in driver\n");
			mdla_timeout_print_ce(core_id, sched->ce[i]);
			mdla_timeout_debug("==========================\n");
		}
	}
	/* handle command timeout */
	mdla_zero_skip_detect(core_id);
	mt_irq_dump_status(mdla_irqdesc[core_id].irq);
	mdla_dump_dbg(mdla_info, ce);
	apu_device_power_off(MDLA0+core_id);
	apu_device_power_on(MDLA0+core_id);
	mdla_reset_lock(mdla_info->mdlaid, REASON_TIMEOUT);
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

static void mdla_run_command_prepare(
	struct mdla_run_cmd *cd,
	struct apusys_cmd_hnd *apusys_hd,
	struct command_entry *ce,
	u16 priority)
{
	uint32_t cb_size = 0;
	u64	deadline =
		get_jiffies_64() + msecs_to_jiffies(mdla_timeout);

	ce->mva = cd->mva + cd->offset;

	if (unlikely(mdla_timeout_dbg))
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
	ce->csn   = priority + 1;
	ce->receive_t = sched_clock();
	ce->kva = NULL;

#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
	/* It is new command list */
	ce->fin_cid = 0;
	ce->wish_fin_cid = ce->count;
	ce->irq_state = IRQ_SUCCESS;
	if (likely(apusys_hd != NULL)) {
		ce->boost_val = apusys_hd->boost_val;
		ce->ctx_id = apusys_hd->ctx_id;
		ce->context_callback = apusys_hd->context_callback;
		apusys_hd->ip_time = 0;
		ce->kva = (void *)(apusys_hd->cmd_entry+cd->offset_code_buf);
		ce->cmdbuf = apusys_hd->cmdbuf;
		ce->priority = priority;
		if (apusys_hd->multicore_total == 2) {
			unsigned long flags;

			ce->cmd_id = apusys_hd->cmd_id;
			ce->multicore_total = apusys_hd->multicore_total;
			ce->cmd_batch_size = cd->count + 1;
			/* initial SMP deadline */
			spin_lock_irqsave(
				&g_smp_deadline[priority].lock,
				flags);
			g_smp_deadline[priority].deadline = deadline;
			spin_unlock_irqrestore(
				&g_smp_deadline[priority].lock,
				flags);
			ce->deadline_t = deadline;
		} else {
			ce->cmd_batch_size = apusys_hd->cluster_size;
			ce->deadline_t = deadline;
		}
	} else {
		ce->ctx_id = INT_MAX;
		ce->context_callback = NULL;
		ce->priority = MDLA_LOW_PRIORITY;
		ce->cmd_batch_size = cd->count + 1;
		ce->deadline_t = deadline;
	}
	cb_size = ce->cmd_batch_size;
	if (priority == MDLA_LOW_PRIORITY && cb_size < ce->count) {
		ce->batch_list_head =
			kzalloc(sizeof(struct list_head), GFP_KERNEL);
		if (unlikely(!ce->batch_list_head))
			return;
		INIT_LIST_HEAD(ce->batch_list_head);
	} else
		ce->batch_list_head = NULL;
	return;
#endif //__APUSYS_MDLA_SW_PORTING_WORKAROUND__
}

void mdla_command_done(unsigned int core_id)
{
	mutex_lock(&mdla_devices[core_id].power_lock);
	mdla_profile_stop(core_id, 1);
	mdla_setup_power_down(core_id);
	mutex_unlock(&mdla_devices[core_id].power_lock);
}

static unsigned long mdla_get_dual_deadline(
	u16 priority
)
{
	unsigned long deadline;
	unsigned long flags;

	spin_lock_irqsave(&g_smp_deadline[priority].lock, flags);
	deadline = g_smp_deadline[priority].deadline;
	spin_unlock_irqrestore(&g_smp_deadline[priority].lock, flags);
	return deadline;
}

void mdla_wakeup_source_init(void)
{
#ifdef CONFIG_PM_SLEEP
	char ws_name[16];

	if (snprintf(ws_name, sizeof(ws_name)-1, "mdla") < 0) {
		pr_debug("init mdla wakeup source fail\n");
		return;
	}
	ws_count = 0;
	mdla_ws = wakeup_source_register(NULL, ws_name);
	if (!mdla_ws)
		pr_debug("mdla wakelock register fail!\n");
#endif
}
