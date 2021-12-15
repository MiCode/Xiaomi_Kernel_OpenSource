// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/sched/clock.h>

#include <common/mdla_driver.h>
#include <common/mdla_device.h>
#include <common/mdla_scheduler.h>
#include <common/mdla_power_ctrl.h>

#include <utilities/mdla_debug.h>
#include <utilities/mdla_util.h>
#include <utilities/mdla_profile.h>

#include <interface/mdla_cmd_data_v2_0.h>

#include <platform/mdla_plat_api.h>

#include "mdla_hw_reg_v2_0.h"
#include "mdla_sched_v2_0.h"


struct mdla_irq {
	u32 irq;
	struct mdla_dev *dev;
};

static int handle_num;
static struct mdla_irq *mdla_irq_desc;

/* platform static function */
static void mdla_backup_cmd(struct mdla_dev *mdla_device, struct command_entry *ce, u32 fin3)
{
	u32 core_id = mdla_device->mdla_id;
	const struct mdla_util_io_ops *io = mdla_util_io_ops_get();
	unsigned long flags;

	spin_lock_irqsave(&mdla_device->hw_lock, flags);

	ce->hw_sync1 = io->cmde.read(core_id, MREG_TOP_G_MDLA_HWSYNC1);
	ce->hw_sync2 = io->cmde.read(core_id, MREG_TOP_G_MDLA_HWSYNC2);
	ce->hw_sync3 = io->cmde.read(core_id, MREG_TOP_G_MDLA_HWSYNC3);

	spin_unlock_irqrestore(&mdla_device->hw_lock, flags);

	ce->fin_cid = fin3;

	mdla_verbose("%s: core %x, hw_sync1: %.8x, hw_sync2: %.8x, hw_sync3: %.8x\n",
			__func__, core_id, ce->hw_sync1, ce->hw_sync2, ce->hw_sync3);
}


static void mdla_sw_cmd_stop(struct mdla_dev *mdla_device, u32 fin3)
{
	u32 core_id = mdla_device->mdla_id;
	struct mdla_scheduler *sched = mdla_device->sched;
	struct command_entry *ce = sched->pro_ce;
	struct command_entry *new_ce;
	unsigned long flags;

	ce_func_trace(ce, F_INIRQ_STOP);

	mdla_prof_set_ts(core_id, TS_CMD_STOPPED,
				mdla_prof_get_ts(core_id, TS_HW_INTR));

	ce->exec_time += mdla_prof_get_ts(core_id, TS_CMD_STOPPED)
					- mdla_prof_get_ts(core_id, TS_HW_TRIGGER);

	spin_lock_irqsave(&mdla_device->stat_lock, flags);
	mdla_device->status = MDLA_STOP;
	spin_unlock_irqrestore(&mdla_device->stat_lock, flags);

	mdla_backup_cmd(mdla_device, ce, fin3);

	sched->enqueue_ce(core_id, ce, 1);
	new_ce = sched->dequeue_ce(core_id);

	if (ce->csn != new_ce->csn) {
		ce->state |= (1 << CE_PREEMPTED);
		new_ce->state |= (1 << CE_PREEMPTING);
		sched->pro_ce = new_ce;
	}

    //FIXME: unsafe reset flow
	/* MDLA sw reset */
	sched->sw_reset(core_id);
	mdla_pwr_ops_get()->hw_reset(core_id,
						mdla_dbg_get_reason_str(REASON_PREEMPTED));

	/* Fire new cmd */
	if (sched->pro_ce)
		sched->issue_ce(core_id);

	mdla_dbg_add_u32(FS_PREEMPTION_TIMES, 1);
}

static int mdla_sw_cmd_done(struct mdla_dev *mdla_device, u32 fin3)
{
	u32 core_id = mdla_device->mdla_id;
	struct mdla_scheduler *sched = mdla_device->sched;
	struct command_entry *ce = sched->pro_ce;
	const struct mdla_util_io_ops *io = mdla_util_io_ops_get();
	struct mdla_pmu_info *pmu;
	struct command_entry *new_ce;
	unsigned long flags;

	ce->fin_cid = fin3;

	if (ce->fin_cid < ce->count) {
		ce_func_trace(ce, F_CMDDONE_CE_FIN3ERROR);
		return -1;
	}

	ce_func_trace(ce, F_CMDDONE_CE_PASS);

	mdla_prof_set_ts(core_id, TS_HW_LAST_INTR,
				mdla_prof_get_ts(core_id, TS_HW_INTR));

	ce->exec_time += mdla_prof_get_ts(core_id, TS_HW_LAST_INTR)
					- mdla_prof_get_ts(core_id, TS_HW_TRIGGER);

	sched->complete_ce(core_id);
	sched->pro_ce = NULL;

	/* out pmu data */
	pmu = mdla_util_pmu_ops_get()->get_info(core_id, 0);
	if (pmu)
		mdla_util_pmu_ops_get()->reg_counter_save(core_id, pmu);


	/* Clear STREAM0, FIN4 */
	spin_lock_irqsave(&mdla_device->hw_lock, flags);
	io->cmde.write(core_id, MREG_TOP_G_STREAM0, 0);
	io->cmde.write(core_id, MREG_TOP_G_FIN4, 0);
	spin_unlock_irqrestore(&mdla_device->hw_lock, flags);

	new_ce = sched->dequeue_ce(core_id);

	if (new_ce != NULL) {
		sched->pro_ce = new_ce;
		ce_func_trace(new_ce, F_CMDDONE_GO_TO_ISSUE);
		sched->issue_ce(core_id);
	} else {
		spin_lock_irqsave(&mdla_device->stat_lock, flags);
		mdla_device->status = MDLA_FREE;
		spin_unlock_irqrestore(&mdla_device->stat_lock, flags);
	}

	return 0;
}

/* IRQ handler for hardware preemption */
static void mdla_irq_hw_sched(struct mdla_dev *mdla_device)
{
	struct mdla_scheduler *sched = mdla_device->sched;
	unsigned long flags, flags1;
	u32 core_id, event_id = 0, irq_status = 0, fin3;
	const struct mdla_util_io_ops *io = mdla_util_io_ops_get();

	core_id = mdla_device->mdla_id;

	if (unlikely(sched == NULL)) {
		mdla_device->error_bit |= IRQ_NO_SCHEDULER;
		return;
	}

	spin_lock_irqsave(&sched->lock, flags1);
	spin_lock_irqsave(&mdla_device->hw_lock, flags);

	irq_status = io->cmde.read(core_id, MREG_TOP_G_INTP0);
	if ((irq_status & INTR_SUPPORT_MASK) == 0) {
		/* ignore unexpected interrupt */
		spin_unlock_irqrestore(&mdla_device->hw_lock, flags);
		ce_func_trace(sched->pro_ce, F_INIRQ_ERROR);
		spin_unlock_irqrestore(&sched->lock, flags1);
		mdla_cmd_debug("%s:Rcv unexpected IRQ: core %x, irq_status: %x\n",
			__func__, core_id, irq_status);
		return;
	}

	if (unlikely((irq_status & INTR_CONV_GCU_SAT_EXCEPTION_INT) ||
				(irq_status & INRQ_CONV_AQU_ACC_SAT_EXCEPTION_INT) ||
				(irq_status & INRQ_CONV_AQU_ADD_SAT_EXCEPTION_INT))) {
		mdla_cmd_debug("%s:exception IRQ: core %x, irq_status: %x\n",
			__func__, core_id, irq_status);
	}

	event_id = io->cmde.read(core_id, MREG_TOP_G_STREAM0);
	fin3  = io->cmde.read(core_id, MREG_TOP_G_FIN3);
    /* clear intp0 to avoid irq fire twice */
	io->cmde.write(core_id, MREG_TOP_G_INTP0, INTR_SUPPORT_MASK);

	spin_unlock_irqrestore(&mdla_device->hw_lock, flags);

	if (unlikely(sched->pro_ce == NULL)) {
		mdla_device->error_bit |= IRQ_NO_PROCESSING_CE;
		goto unlock;
	}

	if (unlikely(sched->pro_ce->state & (1 << CE_FAIL))) {
		mdla_device->error_bit |= IRQ_TIMEOUT;
		ce_func_trace(sched->pro_ce, F_TIMEOUT | 1);
		goto unlock;
	}

	if (unlikely(time_after64(get_jiffies_64(),
					sched->pro_ce->deadline_t))) {
		sched->pro_ce->state |= (1 << CE_TIMEOUT);
		ce_func_trace(sched->pro_ce, F_TIMEOUT);
		goto unlock;
	}

	if (unlikely(event_id != sched->pro_ce->csn)) {
		sched->pro_ce->irq_state |= IRQ_RECORD_ERROR;
		ce_func_trace(sched->pro_ce, F_INIRQ_CDMA4ERROR);
		goto unlock;
	}

	sched->pro_ce->state |= (1 << CE_SCHED);

	if (irq_status & INTR_SWCMD_DONE) {
		mdla_sw_cmd_done(mdla_device, fin3);
		goto unlock;
	} else if (irq_status & INTR_FIN4_CMD_STOP_INT) {
		mdla_sw_cmd_stop(mdla_device, fin3);
	}

unlock:
	spin_unlock_irqrestore(&sched->lock, flags1);
}

/* IRQ handler for SW pre-emption */

/*
 * Return smp_cmd_id
 * smp_cmd_id = 0 => single cmd, issue directly
 * smp_cmd_id != 0 => SMP cmd, return and issue SMP later
 */
static uint64_t mdla_sw_issue_next(struct mdla_dev *mdla_device)
{
	struct mdla_scheduler *sched = mdla_device->sched;
	u32 core_id = mdla_device->mdla_id;
	struct command_entry *new_ce;
	uint64_t smp_cmd_id = 0;

	/* get the next CE to be processed */
	new_ce = sched->dequeue_ce(core_id);

	if (new_ce) {
		if (sched->pro_ce) {
			sched->preempt_ce(core_id, new_ce);
			sched->issue_ce(core_id);
		} else if ((new_ce->multicore_total > 1)
				&& (new_ce->priority == MDLA_LOW_PRIORITY)) {
			sched->enqueue_ce(core_id, new_ce, 1);
			smp_cmd_id = new_ce->cmd_id;
		} else {
			sched->pro_ce = new_ce;
			sched->issue_ce(core_id);
		}
	} else if (sched->pro_ce) {
		sched->issue_ce(core_id);
	}

	return smp_cmd_id;
}

static void mdla_irq_sw_sched(struct mdla_dev *mdla_device)
{
	struct mdla_scheduler *sched = mdla_device->sched;
	unsigned long flags;
	u32 status, core_id, cdma4 = 0, irq_status = 0;
	const struct mdla_util_io_ops *io = mdla_util_io_ops_get();
	u64 smp_cmd_id = 0;
	u64 ts;

	ts = sched_clock();
	core_id = mdla_device->mdla_id;

	/* clear intp0 to avoid irq fire twice */
	spin_lock_irqsave(&mdla_device->hw_lock, flags);

	irq_status = io->cmde.read(core_id, MREG_TOP_G_INTP0);
	io->cmde.write(core_id, MREG_TOP_G_INTP0, INTR_SWCMD_DONE);
	cdma4 = io->cmde.read(core_id, MREG_TOP_G_CDMA4);

	spin_unlock_irqrestore(&mdla_device->hw_lock, flags);

	if (unlikely(sched == NULL)) {
		mdla_device->error_bit |= IRQ_NO_SCHEDULER;
		return;
	}

	spin_lock_irqsave(&sched->lock, flags);

	if (unlikely(sched->pro_ce == NULL)) {
		mdla_device->error_bit |= IRQ_NO_PROCESSING_CE;
		goto unlock;
	}

	if (unlikely(sched->pro_ce->state & (1 << CE_FAIL))) {
		mdla_device->error_bit |= IRQ_TIMEOUT;
		ce_func_trace(sched->pro_ce, F_TIMEOUT | 1);
		goto unlock;
	}

	if (unlikely(time_after64(get_jiffies_64(),
					sched->pro_ce->deadline_t))) {
		sched->pro_ce->state |= (1 << CE_TIMEOUT);
		ce_func_trace(sched->pro_ce, F_TIMEOUT);
		goto unlock;
	}

	if (unlikely((irq_status & INTR_SWCMD_DONE) == 0)) {
		ce_func_trace(sched->pro_ce, F_INIRQ_ERROR);
		mdla_verbose("core %x: irq_status: %x\n", core_id, irq_status);
		goto unlock;
	}

	if (unlikely((irq_status & INTR_CONV_GCU_SAT_EXCEPTION_INT) ||
				(irq_status & INRQ_CONV_AQU_ACC_SAT_EXCEPTION_INT) ||
				(irq_status & INRQ_CONV_AQU_ADD_SAT_EXCEPTION_INT))) {
		pr_info("unexpected IRQ status: core %x, irq_status: %x\n",
			core_id, irq_status);
	}

	if (unlikely(cdma4 != sched->pro_ce->csn)) {
		sched->pro_ce->irq_state |= IRQ_RECORD_ERROR;
		ce_func_trace(sched->pro_ce, F_INIRQ_CDMA4ERROR);
		goto unlock;
	}

	sched->pro_ce->state |= (1 << CE_SCHED);

	sched->pro_ce->exec_time += mdla_prof_get_ts(core_id, TS_HW_INTR)
				- mdla_prof_get_ts(core_id, TS_HW_TRIGGER);

	/* process the current CE */
	status = sched->process_ce(core_id);

	if (status == CE_DONE) {
		mdla_prof_set_ts(core_id, TS_HW_LAST_INTR,
				mdla_prof_get_ts(core_id, TS_HW_INTR));

		sched->complete_ce(core_id);
	} else if (status == CE_NONE) {
		/* nothing to do but wait for the engine completed */
		goto unlock;
	}

	/* get the next CE to be processed */
	smp_cmd_id = mdla_sw_issue_next(mdla_device);

unlock:
	spin_unlock_irqrestore(&sched->lock, flags);

	if (smp_cmd_id)
		sched->issue_dual_lowce(core_id, smp_cmd_id);
}

/* IRQ handler w/o preemption */
static void mdla_irq_intr(struct mdla_dev *mdla_device)
{
	u32 core_id, id;
	u32 irq_status;
	unsigned long flags;
	const struct mdla_util_io_ops *io = mdla_util_io_ops_get();
	struct mdla_pmu_info *pmu;

	core_id = mdla_device->mdla_id;

	spin_lock_irqsave(&mdla_device->hw_lock, flags);

	irq_status = io->cmde.read(core_id, MREG_TOP_G_INTP0);
	io->cmde.write(core_id, MREG_TOP_G_INTP0, INTR_SUPPORT_MASK);

	/* Toggle for Latch Fin1 Tile ID */
	io->cmde.read(core_id, MREG_TOP_G_FIN0);
	id = io->cmde.read(core_id, MREG_TOP_G_FIN3);

	pmu = mdla_util_pmu_ops_get()->get_info(core_id, 0);
	if (pmu)
		mdla_util_pmu_ops_get()->reg_counter_save(core_id, pmu);

	mdla_device->max_cmd_id = id;

	if (irq_status & INTR_PMU_INT)
		io->cmde.write(core_id, MREG_TOP_G_INTP0, INTR_PMU_INT);

	spin_unlock_irqrestore(&mdla_device->hw_lock, flags);

	mdla_prof_set_ts(core_id, TS_HW_LAST_INTR,
			mdla_prof_get_ts(core_id, TS_HW_INTR));
	complete(&mdla_device->command_done);
}

static irqreturn_t mdla_irq_handler(int irq, void *dev_id)
{
	struct mdla_dev *mdla_device = (struct mdla_dev *)dev_id;

	if (unlikely(!mdla_device))
		return IRQ_HANDLED;

	mdla_prof_set_ts(mdla_device->mdla_id, TS_HW_INTR, sched_clock());

	if (mdla_plat_hw_preemption_support())
		mdla_irq_hw_sched(mdla_device);
	else if (mdla_plat_sw_preemption_support())
		mdla_irq_sw_sched(mdla_device);
	else
		mdla_irq_intr(mdla_device);

	return IRQ_HANDLED;
}

/* platform public function */

int mdla_v2_0_get_irq_num(u32 core_id)
{
	int i;

	for (i = 0; i < handle_num; i++) {
		if (mdla_irq_desc[i].dev == mdla_get_device(core_id))
			return mdla_irq_desc[i].irq;
	}

	return 0;
}

int mdla_v2_0_irq_request(struct device *dev, int irqdesc_num)
{
	int i;
	struct mdla_irq *irq_desc;
	struct device_node *node = dev->of_node;

	if (!node) {
		dev_info(dev, "get mdla device node err\n");
		return -1;
	}

	mdla_irq_desc = kcalloc(irqdesc_num, sizeof(struct mdla_irq), GFP_KERNEL);

	if (!mdla_irq_desc)
		return -1;

	handle_num = irqdesc_num;

	for (i = 0; i < irqdesc_num; i++) {
		irq_desc = &mdla_irq_desc[i];
		irq_desc->dev = mdla_get_device(i);
		irq_desc->irq  = irq_of_parse_and_map(node, i);
		if (!irq_desc->irq) {
			dev_info(dev, "get mdla irq: %d failed\n", i);
			goto err;
		}

		if (request_irq(irq_desc->irq, mdla_irq_handler,
			irq_get_trigger_type(irq_desc->irq),
			DRIVER_NAME, irq_desc->dev)) {
			dev_info(dev, "mtk_mdla[%d]: Could not allocate interrupt %d.\n",
					i, irq_desc->irq);
		}
		dev_info(dev, "request_irq %d done\n", irq_desc->irq);
	}

	return 0;

err:
	kfree(mdla_irq_desc);
	return -1;
}

void mdla_v2_0_irq_release(struct device *dev)
{
	int i;

	for (i = 0; i < handle_num; i++)
		free_irq(mdla_irq_desc[i].irq, mdla_irq_desc[i].dev);

	kfree(mdla_irq_desc);
}

