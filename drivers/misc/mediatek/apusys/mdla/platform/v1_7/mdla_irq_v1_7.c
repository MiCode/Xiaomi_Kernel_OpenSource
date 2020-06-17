// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/device.h>

#include <common/mdla_driver.h>
#include <common/mdla_device.h>
#include <common/mdla_scheduler.h>

#include <utilities/mdla_debug.h>
#include <utilities/mdla_util.h>

#include "mdla_hw_reg_v1_7.h"
#include "mdla_sched_v1_7.h"


struct mdla_irq {
	unsigned int irq;
	struct mdla_dev *dev;
};

static int handle_num;
static struct mdla_irq *mdla_irq_desc;

/* platform static function */

static void mdla_irq_sched(struct mdla_dev *mdla_device)
{
	struct mdla_scheduler *sched = mdla_device->sched;
	unsigned long flags;
	unsigned int status, core_id;
	struct mdla_util_io_ops *io = mdla_util_io_ops_get();
	u32 irq_status = 0;

	core_id = mdla_device->mdla_id;

	/* clear intp0 to avoid irq fire twice */
	spin_lock_irqsave(&mdla_device->hw_lock, flags);

	irq_status = io->cmde.read(core_id, MREG_TOP_G_INTP0);
	io->cmde.write(core_id, MREG_TOP_G_INTP0, MDLA_IRQ_SWCMD_DONE);

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
		goto unlock;
	}

	if (unlikely(time_after64(get_jiffies_64(),
					sched->pro_ce->deadline_t))) {
		sched->pro_ce->state |= (1 << CE_TIMEOUT);
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

	if (sched->pro_ce != NULL) {
		if ((irq_status & MDLA_IRQ_CDMA_FIFO_EMPTY) == 0) {
			if ((sched->pro_ce->irq_state & IRQ_N_EMPTY_IN_ISSUE))
				sched->pro_ce->irq_state |= IRQ_NE_SCHED_FIRST;
			sched->pro_ce->irq_state |= IRQ_N_EMPTY_IN_SCHED;
		}
	}

	/* get the next CE to be processed */
	sched->dequeue_ce(core_id);
	//if (likely(sched->pro_ce != NULL))
	sched->issue_ce(core_id);

unlock:
	spin_unlock_irqrestore(&sched->lock, flags);
}

static void mdla_irq_intr(struct mdla_dev *mdla_device)
{
	u32 status_int, id;
	unsigned long flags;
	unsigned int core_id;
	struct mdla_util_io_ops *io = mdla_util_io_ops_get();
	struct mdla_pmu_info *pmu;

	core_id = mdla_device->mdla_id;
	status_int = io->cmde.read(core_id, MREG_TOP_G_INTP0);

	spin_lock_irqsave(&mdla_device->hw_lock, flags);

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

	if (unlikely(!mdla_device))
		return IRQ_HANDLED;

	if (mdla_util_sw_preemption_support())
		mdla_irq_sched(mdla_device);
	else
		mdla_irq_intr(mdla_device);

	return IRQ_HANDLED;
}

/* platform public function */

int mdla_v1_7_get_irq_num(int core_id)
{
	int i;

	for (i = 0; i < handle_num; i++) {
		if (mdla_irq_desc[i].dev == mdla_get_device(core_id))
			return mdla_irq_desc[i].irq;
	}

	return 0;
}

int mdla_v1_7_irq_request(struct device *dev, int irqdesc_num)
{
	int i;
	struct mdla_irq *irq_desc;
	struct device_node *node = dev->of_node;

	if (!node) {
		dev_info(dev, "get mdla device node err\n");
		return -1;
	}

	mdla_irq_desc = kcalloc(irqdesc_num, sizeof(struct mdla_irq),
					GFP_KERNEL);

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
			IRQF_TRIGGER_HIGH, DRIVER_NAME, irq_desc->dev)) {
			dev_info(dev, "mtk_mdla[%d]: Could not allocate interrupt %d.\n",
					i, irq_desc->irq);
			/* IRQF_TRIGGER_HIGH for Simulator workaroud only */
			//if (request_irq(irq_desc->irq,
			//			irq_desc->handler,
			//			IRQF_TRIGGER_HIGH,
			//			DRIVER_NAME, dev)) {
			//	dev_info(dev, "mtk_mdla[%d]: %s %d.\n",
			//			"Could not allocate interrupt",
			//			i, irq_desc->irq);
			//	goto err;
			//}
		}
		dev_info(dev, "request_irq %d done\n", irq_desc->irq);
	}

	return 0;

err:
	kfree(mdla_irq_desc);
	return -1;
}

void mdla_v1_7_irq_release(struct device *dev)
{
	int i;

	for (i = 0; i < handle_num; i++)
		free_irq(mdla_irq_desc[i].irq, mdla_irq_desc[i].dev);

	kfree(mdla_irq_desc);
}

