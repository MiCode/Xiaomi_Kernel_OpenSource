/*
 * drivers/video/tegra/camera/camera_irq.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include "camera_priv_defs.h"
#include "camera_priv.h"
#include "camera_reg.h"

int tegra_camera_enable_irq(struct tegra_camera *camera)
{
	int val;

	/* Reset VI status register */
	val = tegra_camera_readl(camera, VI_INTERRUPT_STATUS_0);
	tegra_camera_writel(camera,
			val,
			VI_INTERRUPT_STATUS_0);

	/* Enable FIFO Overflow Interrupt */
	tegra_camera_writel(camera,
			SECOND_OUTPUT_DROP_MC_DATA |
			FIRST_OUTPUT_DROP_MC_DATA,
			VI_INTERRUPT_MASK_0);

	enable_irq(camera->irq);
	return 0;
}

int tegra_camera_disable_irq(struct tegra_camera *camera)
{
	int val;

	disable_irq(camera->irq);

	/* Disable FIFO Overflow Interrupt */
	tegra_camera_writel(camera,
			0,
			VI_INTERRUPT_MASK_0);

	/* Reset VI status register */
	val = tegra_camera_readl(camera, VI_INTERRUPT_STATUS_0);
	tegra_camera_writel(camera,
			val,
			VI_INTERRUPT_STATUS_0);

	return 0;
}

static irqreturn_t tegra_camera_isr(int irq, void *dev_id)
{
	struct tegra_camera *camera = (struct tegra_camera *)dev_id;
	int val;

	dev_dbg(camera->dev, "%s: ++", __func__);

	val = tegra_camera_readl(camera, VI_INTERRUPT_STATUS_0);

	if (val & FIRST_OUTPUT_DROP_MC_DATA)
		atomic_inc(&(camera->vi_out0.overflow));

	if (val & SECOND_OUTPUT_DROP_MC_DATA)
		atomic_inc(&(camera->vi_out1.overflow));

	/* Reset interrupt status register */
	tegra_camera_writel(camera, val, VI_INTERRUPT_STATUS_0);

	schedule_work(&camera->stats_work);
	return IRQ_HANDLED;
}

void tegra_camera_stats_worker(struct work_struct *work)
{
	struct tegra_camera *camera = container_of(work,
						struct tegra_camera,
						stats_work);

	dev_dbg(camera->dev,
		"%s: vi_out0 dropped data %u times", __func__,
		atomic_read(&(camera->vi_out0.overflow)));
	dev_dbg(camera->dev,
		"%s: vi_out1 dropped data %u times", __func__,
		atomic_read(&(camera->vi_out1.overflow)));
}

int tegra_camera_intr_init(struct tegra_camera *camera)
{
	int ret;

	struct platform_device *ndev = to_platform_device(camera->dev);

	dev_dbg(camera->dev, "%s: ++", __func__);

	camera->irq = platform_get_irq(ndev, 0);
	if (IS_ERR_VALUE(camera->irq)) {
		dev_err(camera->dev, "missing camera irq\n");
		return -ENXIO;
	}

	ret = request_irq(camera->irq,
			tegra_camera_isr,
			IRQF_SHARED,
			dev_name(camera->dev),
			camera);
	if (ret) {
		BUG();
		dev_err(camera->dev, "failed to get irq\n");
		return -EBUSY;
	}

	disable_irq(camera->irq);

	return 0;
}

int tegra_camera_intr_free(struct tegra_camera *camera)
{
	dev_dbg(camera->dev, "%s: ++", __func__);

	free_irq(camera->irq, camera);

	return 0;
}
