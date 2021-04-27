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

#include <utilities/mdla_debug.h>
#include <utilities/mdla_util.h>
#include <utilities/mdla_profile.h>

#include <interface/mdla_cmd_data_v1_x.h>

#include <platform/mdla_plat_api.h>

#include "mdla_hw_reg_v1_x.h"
#include "mdla_sched_v1_x.h"


struct mdla_irq {
	u32 irq;
	struct mdla_dev *dev;
};

static int handle_num;
static struct mdla_irq *mdla_irq_desc;

/* platform static function */

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

static void mdla_irq_sched(struct mdla_dev *mdla_device)
{
	struct mdla_scheduler *sched = mdla_device->sched;
	unsigned long flags;
	u32 status, core_id, cdma4 = 0, irq_status = 0;
	const struct mdla_util_io_ops *io = mdla_util_io_ops_get();
	u64 smp_cmd_id = 0;

	core_id = mdla_device->mdla_id;

	/* clear intp0 to avoid irq fire twice */
	spin_lock_irqsave(&mdla_device->hw_lock, flags);

	irq_status = io->cmde.read(core_id, MREG_TOP_G_INTP0);
	io->cmde.write(core_id, MREG_TOP_G_INTP0, MDLA_IRQ_SWCMD_DONE | MDLA_IRQ_PMU_INTE);
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

	if (unlikely((irq_status & MDLA_IRQ_SWCMD_DONE) == 0)) {
		ce_func_trace(sched->pro_ce, F_INIRQ_ERROR);
		goto unlock;
	}

	if (unlikely(cdma4 != sched->pro_ce->csn)) {
		sched->pro_ce->irq_state |= IRQ_RECORD_ERROR;
		ce_func_trace(sched->pro_ce, F_INIRQ_CDMA4ERROR);
		goto unlock;
	}

	sched->pro_ce->state |= (1 << CE_SCHED);

	/* process the current CE */
	status = sched->process_ce(core_id);

	if (status == CE_DONE) {
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

static void mdla_irq_intr(struct mdla_dev *mdla_device)
{
	u32 status_int, core_id, id, mask;
	unsigned long flags;
	const struct mdla_util_io_ops *io = mdla_util_io_ops_get();
	struct mdla_pmu_info *pmu;

	core_id = mdla_device->mdla_id;

	spin_lock_irqsave(&mdla_device->hw_lock, flags);

	status_int = io->cmde.read(core_id, MREG_TOP_G_INTP0);
	mask = io->cmde.read(core_id, MREG_TOP_G_INTP2) | MDLA_IRQ_SWCMD_DONE;
	io->cmde.write(core_id, MREG_TOP_G_INTP2, mask);
	io->cmde.write(core_id, MREG_TOP_G_INTP0, mask);

	/* Toggle for Latch Fin1 Tile ID */
	io->cmde.read(core_id, MREG_TOP_G_FIN0);
	id = io->cmde.read(core_id, MREG_TOP_G_FIN3);

	pmu = mdla_util_pmu_ops_get()->get_info(core_id, 0);
	if (pmu)
		mdla_util_pmu_ops_get()->reg_counter_save(core_id, pmu);

	mdla_device->max_cmd_id = id;

	if (status_int & MDLA_IRQ_PMU_INTE)
		io->cmde.write(core_id,
				MREG_TOP_G_INTP0, MDLA_IRQ_PMU_INTE);

	spin_unlock_irqrestore(&mdla_device->hw_lock, flags);

	complete(&mdla_device->command_done);
}

static irqreturn_t mdla_irq_handler(int irq, void *dev_id)
{
	struct mdla_dev *mdla_device = (struct mdla_dev *)dev_id;
	struct command_entry *curr_ce;
	u64 ts;

	if (unlikely(!mdla_device))
		return IRQ_HANDLED;

	ts = sched_clock();
	curr_ce = mdla_device->sched->pro_ce;

	if (curr_ce && curr_ce->req_start_t) {
		curr_ce->exec_time += ts - curr_ce->req_start_t;

		curr_ce->req_start_t = 0;
		/* for trace check */
		curr_ce->req_end_t = ts;
	}

	if (mdla_plat_sw_preemption_support())
		mdla_irq_sched(mdla_device);
	else
		mdla_irq_intr(mdla_device);

	return IRQ_HANDLED;
}

/* platform public function */

int mdla_v1_x_get_irq_num(u32 core_id)
{
	int i;

	for (i = 0; i < handle_num; i++) {
		if (mdla_irq_desc[i].dev == mdla_get_device(core_id))
			return mdla_irq_desc[i].irq;
	}

	return 0;
}

int mdla_v1_x_irq_request(struct device *dev, int irqdesc_num)
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

void mdla_v1_x_irq_release(struct device *dev)
{
	int i;

	for (i = 0; i < handle_num; i++)
		free_irq(mdla_irq_desc[i].irq, mdla_irq_desc[i].dev);

	kfree(mdla_irq_desc);
}

