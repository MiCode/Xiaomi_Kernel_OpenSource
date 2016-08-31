/*
 * drivers/video/tegra/host/host1x/host1x_intr.c
 *
 * Tegra Graphics Host Interrupt Management
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/ktime.h>

#include "nvhost_intr.h"
#include "dev.h"

/* Spacing between sync registers */
#define REGISTER_STRIDE 4

static void t20_intr_syncpt_intr_ack(struct nvhost_intr_syncpt *syncpt,
				     bool disable_intr);
static void t20_intr_enable_syncpt_intr(struct nvhost_intr *intr, u32 id);
static void t20_intr_set_syncpt_threshold(struct nvhost_intr *intr,
					  u32 id, u32 thresh);

static void syncpt_thresh_cascade_fn(struct work_struct *work)
{
	struct nvhost_intr_syncpt *sp =
		container_of(work, struct nvhost_intr_syncpt, work);
	nvhost_syncpt_thresh_fn(sp);
}

static irqreturn_t syncpt_thresh_cascade_isr(int irq, void *dev_id)
{
	struct nvhost_master *dev = dev_id;
	void __iomem *sync_regs = dev->sync_aperture;
	struct nvhost_intr *intr = &dev->intr;
	unsigned long reg;
	int i, id;

	for (i = 0; i < DIV_ROUND_UP(dev->info.nb_pts, 32); i++) {
		reg = readl(sync_regs +
				host1x_sync_syncpt_thresh_cpu0_int_status_r() +
				i * REGISTER_STRIDE);

		for_each_set_bit(id, &reg, 32) {
			struct nvhost_intr_syncpt *sp;
			int sp_id = i * 32 + id;

			if (unlikely(sp_id >= dev->info.nb_pts)) {
				dev_err(&dev->dev->dev, "%s(): syncpoint id %d is beyond the number of syncpoints (%d)\n",
					__func__, sp_id, dev->info.nb_pts);
				goto out;
			}

			sp = intr->syncpt + sp_id;
			ktime_get_ts(&sp->isr_recv);

			/* handle syncpoint 0 increments immediately */
			if (sp_id == 0) {
				dev_warn(&dev->dev->dev, "%s(): syncpoint id 0 incremented\n",
					 __func__);
				nvhost_syncpt_patch_check(&dev->syncpt);
				t20_intr_syncpt_intr_ack(sp, false);
			} else {
				t20_intr_syncpt_intr_ack(sp, true);
				queue_work(intr->wq, &sp->work);
			}
		}
	}

out:
	return IRQ_HANDLED;
}

static void t20_intr_init_host_sync(struct nvhost_intr *intr)
{
	struct nvhost_master *dev = intr_to_dev(intr);
	void __iomem *sync_regs = dev->sync_aperture;
	int i, err;

	intr_op().disable_all_syncpt_intrs(intr);

	for (i = 0; i < dev->info.nb_pts; i++)
		INIT_WORK(&intr->syncpt[i].work, syncpt_thresh_cascade_fn);

	err = request_irq(intr->syncpt_irq,
				syncpt_thresh_cascade_isr,
				IRQF_SHARED, "host_syncpt", dev);
	if (err)
		BUG();

	/* increase the auto-ack timout to the maximum value. 2d will hang
	 * otherwise on ap20.
	 */
	writel(0xff, sync_regs + host1x_sync_ctxsw_timeout_cfg_r());

	/* enable syncpoint 0 interrupt */
	t20_intr_set_syncpt_threshold(intr, 0, 1);
	t20_intr_enable_syncpt_intr(intr, 0);
}

static void t20_intr_set_host_clocks_per_usec(struct nvhost_intr *intr, u32 cpm)
{
	struct nvhost_master *dev = intr_to_dev(intr);
	void __iomem *sync_regs = dev->sync_aperture;
	/* write microsecond clock register */
	writel(cpm, sync_regs + host1x_sync_usec_clk_r());
	/* set the ip_busy_timeout */
	writel(cpm * 500000, sync_regs + host1x_sync_ip_busy_timeout_r());
}

static void t20_intr_set_syncpt_threshold(struct nvhost_intr *intr,
	u32 id, u32 thresh)
{
	struct nvhost_master *dev = intr_to_dev(intr);
	void __iomem *sync_regs = dev->sync_aperture;
	writel(thresh, sync_regs +
		(host1x_sync_syncpt_int_thresh_0_r() + id * REGISTER_STRIDE));
}

static void t20_intr_enable_syncpt_intr(struct nvhost_intr *intr, u32 id)
{
	struct nvhost_master *dev = intr_to_dev(intr);
	void __iomem *sync_regs = dev->sync_aperture;

	writel(bit_mask(id), sync_regs +
			host1x_sync_syncpt_thresh_int_enable_cpu0_r() +
			bit_word(id) * REGISTER_STRIDE);
}

static void t20_intr_disable_syncpt_intr(struct nvhost_intr *intr, u32 id)
{
	struct nvhost_master *dev = intr_to_dev(intr);
	void __iomem *sync_regs = dev->sync_aperture;

	writel(bit_mask(id), sync_regs +
			host1x_sync_syncpt_thresh_int_disable_r() +
			bit_word(id) * REGISTER_STRIDE);

	/* clear status for both cpu's */
	writel(bit_mask(id), sync_regs +
		host1x_sync_syncpt_thresh_cpu0_int_status_r() +
		bit_word(id) * REGISTER_STRIDE);
	writel(bit_mask(id), sync_regs +
		host1x_sync_syncpt_thresh_cpu1_int_status_r() +
		bit_word(id) * REGISTER_STRIDE);
}

static void t20_intr_disable_all_syncpt_intrs(struct nvhost_intr *intr)
{
	struct nvhost_master *dev = intr_to_dev(intr);
	void __iomem *sync_regs = dev->sync_aperture;
	u32 reg;

	for (reg = 0; reg < bit_word(dev->info.nb_pts) * REGISTER_STRIDE;
			reg += REGISTER_STRIDE) {
		/* disable interrupts for both cpu's */
		writel(0xffffffffu, sync_regs +
				host1x_sync_syncpt_thresh_int_disable_r() +
				reg);

		/* clear status for both cpu's */
		writel(0xffffffffu, sync_regs +
			host1x_sync_syncpt_thresh_cpu0_int_status_r() + reg);
		writel(0xffffffffu, sync_regs +
			host1x_sync_syncpt_thresh_cpu1_int_status_r() + reg);
	}
}

/*
 * Acknowledge that the syncpoint interrupt is handled. If disable_intr is set,
 * the syncpoint interrupt is also disabled.
 */
static void t20_intr_syncpt_intr_ack(struct nvhost_intr_syncpt *syncpt,
				     bool disable_intr)
{
	unsigned int id = syncpt->id;
	struct nvhost_intr *intr = intr_syncpt_to_intr(syncpt);

	void __iomem *sync_regs = intr_to_dev(intr)->sync_aperture;

	u32 reg = bit_word(id) * REGISTER_STRIDE;

	if (disable_intr)
		writel(bit_mask(id), sync_regs +
		       host1x_sync_syncpt_thresh_int_disable_r() + reg);

	writel(bit_mask(id), sync_regs +
		host1x_sync_syncpt_thresh_cpu0_int_status_r() + reg);
}

/**
 * Host general interrupt service function
 * Handles read / write failures
 */
static irqreturn_t t20_intr_host1x_isr(int irq, void *dev_id)
{
	struct nvhost_intr *intr = dev_id;
	void __iomem *sync_regs = intr_to_dev(intr)->sync_aperture;
	u32 stat;
	u32 ext_stat;
	u32 addr;
	unsigned long intstat;

	intstat = readl(sync_regs + host1x_sync_intstatus_r());
	intr->intstatus = intstat;

	/* Handle host1x interrupt in ISR */
	stat = readl(sync_regs + host1x_sync_hintstatus_r());
	ext_stat = readl(sync_regs + host1x_sync_hintstatus_ext_r());

	if (host1x_sync_hintstatus_ext_ip_read_int_v(ext_stat)) {
		addr = readl(sync_regs + host1x_sync_ip_read_timeout_addr_r());
		pr_err("Host read timeout at address %x\n", addr);
	}

	if (host1x_sync_hintstatus_ext_ip_write_int_v(ext_stat)) {
		addr = readl(sync_regs + host1x_sync_ip_write_timeout_addr_r());
		pr_err("Host write timeout at address %x\n", addr);
	}

	writel(ext_stat, sync_regs + host1x_sync_hintstatus_ext_r());
	writel(stat, sync_regs + host1x_sync_hintstatus_r());

	writel(intstat, sync_regs + host1x_sync_intstatus_r());
	return IRQ_HANDLED;
}

static int t20_intr_request_host_general_irq(struct nvhost_intr *intr)
{
	void __iomem *sync_regs = intr_to_dev(intr)->sync_aperture;
	int err;

	/* master disable for general (not syncpt) host interrupts */
	writel(0, sync_regs + host1x_sync_intmask_r());

	/* clear status & extstatus */
	writel(0xfffffffful, sync_regs + host1x_sync_hintstatus_ext_r());
	writel(0xfffffffful, sync_regs + host1x_sync_hintstatus_r());

	err = request_irq(intr->general_irq, t20_intr_host1x_isr,
			0, "host_status", intr);
	if (err)
		return err;

	/* enable extra interrupt sources IP_READ_INT and IP_WRITE_INT */
	writel(BIT(30) | BIT(31), sync_regs + host1x_sync_hintmask_ext_r());

	/* enable extra interrupt sources */
	writel(BIT(31), sync_regs + host1x_sync_hintmask_r());

	/* enable host module interrupt to CPU0 */
	writel(BIT(0), sync_regs + host1x_sync_intc0mask_r());

	/* master enable for general (not syncpt) host interrupts */
	writel(BIT(0), sync_regs + host1x_sync_intmask_r());

	return err;
}

static void t20_intr_free_host_general_irq(struct nvhost_intr *intr)
{
	void __iomem *sync_regs = intr_to_dev(intr)->sync_aperture;

	/* master disable for general (not syncpt) host interrupts */
	writel(0, sync_regs + host1x_sync_intmask_r());

	free_irq(intr->general_irq, intr);
}

static int t20_free_syncpt_irq(struct nvhost_intr *intr)
{
	struct nvhost_master *dev = intr_to_dev(intr);

	/* disable syncpoint 0 interrupt */
	t20_intr_disable_syncpt_intr(intr, 0);

	free_irq(INT_HOST1X_MPCORE_SYNCPT, dev);
	flush_workqueue(intr->wq);
	return 0;
}

static const struct nvhost_intr_ops host1x_intr_ops = {
	.init_host_sync = t20_intr_init_host_sync,
	.set_host_clocks_per_usec = t20_intr_set_host_clocks_per_usec,
	.set_syncpt_threshold = t20_intr_set_syncpt_threshold,
	.enable_syncpt_intr = t20_intr_enable_syncpt_intr,
	.disable_syncpt_intr = t20_intr_disable_syncpt_intr,
	.disable_all_syncpt_intrs = t20_intr_disable_all_syncpt_intrs,
	.request_host_general_irq = t20_intr_request_host_general_irq,
	.free_host_general_irq = t20_intr_free_host_general_irq,
	.free_syncpt_irq = t20_free_syncpt_irq,
};
