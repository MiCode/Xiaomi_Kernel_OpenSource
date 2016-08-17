/*
 * drivers/video/tegra/host/host1x/host1x_intr.c
 *
 * Tegra Graphics Host Interrupt Management
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (c) 2010-2012, NVIDIA Corporation.
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
#include <asm/mach/irq.h>

#include "nvhost_intr.h"
#include "dev.h"

/* Spacing between sync registers */
#define REGISTER_STRIDE 4

static void t20_intr_syncpt_thresh_isr(struct nvhost_intr_syncpt *syncpt);

static void syncpt_thresh_cascade_fn(struct work_struct *work)
{
	struct nvhost_intr_syncpt *sp =
		container_of(work, struct nvhost_intr_syncpt, work);
	nvhost_syncpt_thresh_fn(sp->irq, sp);
}

static irqreturn_t syncpt_thresh_cascade_isr(int irq, void *dev_id)
{
	struct nvhost_master *dev = dev_id;
	void __iomem *sync_regs = dev->sync_aperture;
	struct nvhost_intr *intr = &dev->intr;
	unsigned long reg;
	int i, id;

	for (i = 0; i < dev->info.nb_pts / BITS_PER_LONG; i++) {
		reg = readl(sync_regs +
				host1x_sync_syncpt_thresh_cpu0_int_status_r() +
				i * REGISTER_STRIDE);
		for_each_set_bit(id, &reg, BITS_PER_LONG) {
			struct nvhost_intr_syncpt *sp =
				intr->syncpt + (i * BITS_PER_LONG + id);
			t20_intr_syncpt_thresh_isr(sp);
			queue_work(intr->wq, &sp->work);
		}
	}

	return IRQ_HANDLED;
}

static void t20_intr_init_host_sync(struct nvhost_intr *intr)
{
	struct nvhost_master *dev = intr_to_dev(intr);
	void __iomem *sync_regs = dev->sync_aperture;
	int i, err;

	writel(0xffffffffUL,
		sync_regs + host1x_sync_syncpt_thresh_int_disable_r());
	writel(0xffffffffUL,
		sync_regs + host1x_sync_syncpt_thresh_cpu0_int_status_r());

	for (i = 0; i < dev->info.nb_pts; i++)
		INIT_WORK(&intr->syncpt[i].work, syncpt_thresh_cascade_fn);

	err = request_irq(INT_HOST1X_MPCORE_SYNCPT,
				syncpt_thresh_cascade_isr,
				IRQF_SHARED, "host_syncpt", dev);
	if (err)
		BUG();

	/* disable the ip_busy_timeout. this prevents write drops, etc.
	 * there's no real way to recover from a hung client anyway.
	 */
	writel(0, sync_regs + host1x_sync_ip_busy_timeout_r());

	/* increase the auto-ack timout to the maximum value. 2d will hang
	 * otherwise on ap20.
	 */
	writel(0xff, sync_regs + host1x_sync_ctxsw_timeout_cfg_r());
}

static void t20_intr_set_host_clocks_per_usec(struct nvhost_intr *intr, u32 cpm)
{
	struct nvhost_master *dev = intr_to_dev(intr);
	void __iomem *sync_regs = dev->sync_aperture;
	/* write microsecond clock register */
	writel(cpm, sync_regs + host1x_sync_usec_clk_r());
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

	writel(BIT_MASK(id), sync_regs +
			host1x_sync_syncpt_thresh_int_enable_cpu0_r() +
			BIT_WORD(id) * REGISTER_STRIDE);
}

static void t20_intr_disable_syncpt_intr(struct nvhost_intr *intr, u32 id)
{
	struct nvhost_master *dev = intr_to_dev(intr);
	void __iomem *sync_regs = dev->sync_aperture;

	writel(BIT_MASK(id), sync_regs +
			host1x_sync_syncpt_thresh_int_disable_r() +
			BIT_WORD(id) * REGISTER_STRIDE);

	/* clear status for both cpu's */
	writel(BIT_MASK(id), sync_regs +
		host1x_sync_syncpt_thresh_cpu0_int_status_r() +
		BIT_WORD(id) * REGISTER_STRIDE);
	writel(BIT_MASK(id), sync_regs +
		host1x_sync_syncpt_thresh_cpu1_int_status_r() +
		BIT_WORD(id) * REGISTER_STRIDE);
}

static void t20_intr_disable_all_syncpt_intrs(struct nvhost_intr *intr)
{
	struct nvhost_master *dev = intr_to_dev(intr);
	void __iomem *sync_regs = dev->sync_aperture;
	u32 reg;

	for (reg = 0; reg <= BIT_WORD(dev->info.nb_pts) * REGISTER_STRIDE;
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

/**
 * Sync point threshold interrupt service function
 * Handles sync point threshold triggers, in interrupt context
 */
static void t20_intr_syncpt_thresh_isr(struct nvhost_intr_syncpt *syncpt)
{
	unsigned int id = syncpt->id;
	struct nvhost_intr *intr = intr_syncpt_to_intr(syncpt);

	void __iomem *sync_regs = intr_to_dev(intr)->sync_aperture;

	u32 reg = BIT_WORD(id) * REGISTER_STRIDE;

	writel(BIT_MASK(id), sync_regs +
		host1x_sync_syncpt_thresh_int_disable_r() + reg);
	writel(BIT_MASK(id), sync_regs +
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
	int i;

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

	for_each_set_bit(i, &intstat, BITS_PER_LONG) {
		if (intr->generic_isr[i])
			intr->generic_isr[i]();
	}

	writel(intstat, sync_regs + host1x_sync_intstatus_r());
	/* If we received only host1x interrupt, mark it as handled.
	 * For other interrupts, wake up the thread */
	return (intstat & ~BIT(0)) ? IRQ_WAKE_THREAD : IRQ_HANDLED;
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

	err = request_threaded_irq(intr->host_general_irq,
			t20_intr_host1x_isr, nvhost_intr_irq_fn,
			0, "host_status", intr);
	if (err)
		return err;

	/* enable extra interrupt sources IP_READ_INT and IP_WRITE_INT */
	writel(BIT(30) | BIT(31), sync_regs + host1x_sync_hintmask_ext_r());

	/* enable extra interrupt sources */
	writel(BIT(12) | BIT(31), sync_regs + host1x_sync_hintmask_r());

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

	free_irq(intr->host_general_irq, intr);
}

static void host1x_intr_enable_general_irq(struct nvhost_intr *intr, int irq)
{
	void __iomem *sync_regs = intr_to_dev(intr)->sync_aperture;
	u32 intmask;

	intmask = readl(sync_regs + host1x_sync_intc0mask_r());
	intmask |= BIT(irq);
	writel(intmask, sync_regs + host1x_sync_intc0mask_r());
}

static void host1x_intr_disable_general_irq(struct nvhost_intr *intr, int irq)
{
	void __iomem *sync_regs = intr_to_dev(intr)->sync_aperture;
	u32 intmask;

	intmask = readl(sync_regs + host1x_sync_intc0mask_r());
	intmask &= ~BIT(irq);
	writel(intmask, sync_regs + host1x_sync_intc0mask_r());
}

static int t20_free_syncpt_irq(struct nvhost_intr *intr)
{
	struct nvhost_master *dev = intr_to_dev(intr);
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
	.enable_general_irq = host1x_intr_enable_general_irq,
	.disable_general_irq = host1x_intr_disable_general_irq,
	.free_syncpt_irq = t20_free_syncpt_irq,
};
