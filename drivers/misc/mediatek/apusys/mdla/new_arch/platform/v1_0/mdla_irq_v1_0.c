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

#include <utilities/mdla_debug.h>
#include <utilities/mdla_util.h>

#include "mdla_hw_reg_v1_0.h"


struct mdla_irq {
	u32 irq;
	struct mdla_dev *dev;
};

static int handle_num;
static struct mdla_irq *mdla_irq_desc;

/* platform static function */

static irqreturn_t mdla_irq_handler(int irq, void *dev_id)
{
	u32 status_int, id;
	unsigned long flags;
	struct mdla_dev *mdla_device = (struct mdla_dev *)dev_id;
	const struct mdla_util_io_ops *io = mdla_util_io_ops_get();
	struct mdla_pmu_info *pmu;
	u32 core_id;

	if (unlikely(!mdla_device))
		return IRQ_HANDLED;

	core_id = mdla_device->mdla_id;

	spin_lock_irqsave(&mdla_device->hw_lock, flags);

	status_int = io->cmde.read(core_id, MREG_TOP_G_INTP0);
	id = io->cmde.read(core_id, MREG_TOP_G_FIN0);

	//mdla_pmu_reg_save(mdla_device->mdla_id, &mdla_device->pmu_info[0]);
	pmu = mdla_util_pmu_ops_get()->get_info(core_id, 0);
	if (pmu)
		mdla_util_pmu_ops_get()->reg_counter_save(core_id, pmu);

	/* avoid max_cmd_id lost after timeout reset */
	if (id > mdla_device->max_cmd_id)
		mdla_device->max_cmd_id = id;

	if (status_int & MDLA_IRQ_PMU_INTE)
		io->cmde.write(core_id, MREG_TOP_G_INTP0, MDLA_IRQ_PMU_INTE);

	spin_unlock_irqrestore(&mdla_device->hw_lock, flags);

	complete(&mdla_device->command_done);

	return IRQ_HANDLED;
}

/* platform public function */

int mdla_v1_0_irq_request(struct device *dev, int irqdesc_num)
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

void mdla_v1_0_irq_release(struct device *dev)
{
	int i;

	for (i = 0; i < handle_num; i++)
		free_irq(mdla_irq_desc[i].irq, mdla_irq_desc[i].dev);

	kfree(mdla_irq_desc);
}

