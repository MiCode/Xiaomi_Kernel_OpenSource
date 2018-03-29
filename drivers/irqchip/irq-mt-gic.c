/*
 * Copyright (C) 2015 MediaTek Inc.
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

/*
 * Copy from ARM GIC and add mediatek interrupt specific control codes
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/cpumask.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/irqchip/mt-gic.h>

#include <asm/irq.h>
#include <asm/exception.h>
#include <asm/smp_plat.h>

#include "irqchip.h"
#include <mach/mt_secure_api.h>
#include <mt-plat/mt_io.h>

union gic_base {
	void __iomem *common_base;
	void __percpu __iomem **percpu_base;
};

struct gic_chip_data {
	union gic_base dist_base;
	union gic_base cpu_base;
#ifdef CONFIG_CPU_PM
	u32 saved_spi_enable[DIV_ROUND_UP(1020, 32)];
	u32 saved_spi_conf[DIV_ROUND_UP(1020, 16)];
	u32 saved_spi_target[DIV_ROUND_UP(1020, 4)];
	u32 __percpu *saved_ppi_enable;
	u32 __percpu *saved_ppi_conf;
#endif
	struct irq_domain *domain;
	unsigned int gic_irqs;
#ifdef CONFIG_GIC_NON_BANKED
	void __iomem *(*get_base)(union gic_base *);
#endif
};


void (*irq_pol_workaround)(phys_addr_t addr, u32 value);



static DEFINE_RAW_SPINLOCK(irq_controller_lock);

/*
 * The GIC mapping of CPU interfaces does not necessarily match
 * the logical CPU numbering.  Let's use a mapping as returned
 * by the GIC itself.
 */
#define NR_GIC_CPU_IF 8
static u8 gic_cpu_map[NR_GIC_CPU_IF] __read_mostly;


#ifndef MAX_GIC_NR
#define MAX_GIC_NR	1
#endif

#ifndef NR_GIC_SGI
#define NR_GIC_SGI	16
#endif

#ifndef NR_GIC_PPI
#define NR_GIC_PPI	16
#endif

static struct gic_chip_data gic_data[MAX_GIC_NR] __read_mostly;

#ifdef CONFIG_GIC_NON_BANKED
static void __iomem *gic_get_percpu_base(union gic_base *base)
{
	return *__this_cpu_ptr(base->percpu_base);
}

static void __iomem *gic_get_common_base(union gic_base *base)
{
	return base->common_base;
}

static inline void __iomem *gic_data_dist_base(struct gic_chip_data *data)
{
	return data->get_base(&data->dist_base);
}

static inline void __iomem *gic_data_cpu_base(struct gic_chip_data *data)
{
	return data->get_base(&data->cpu_base);
}

static inline void gic_set_base_accessor(struct gic_chip_data *data,
					 void __iomem * (*f)(union gic_base *))
{
	data->get_base = f;
}
#else
#define gic_data_dist_base(d)	((d)->dist_base.common_base)
#define gic_data_cpu_base(d)	((d)->cpu_base.common_base)
#define gic_set_base_accessor(d, f)
#endif

static inline void __iomem *gic_dist_base(struct irq_data *d)
{
	struct gic_chip_data *gic_data = irq_data_get_irq_chip_data(d);

	return gic_data_dist_base(gic_data);
}

static inline void __iomem *gic_cpu_base(struct irq_data *d)
{
	struct gic_chip_data *gic_data = irq_data_get_irq_chip_data(d);

	return gic_data_cpu_base(gic_data);
}

static inline unsigned int gic_irq(struct irq_data *d)
{
	return d->hwirq;
}

/*
 * Routines to acknowledge, disable and enable interrupts
 */
static void gic_mask_irq(struct irq_data *d)
{
	u32 mask = 1 << (gic_irq(d) % 32);

	raw_spin_lock(&irq_controller_lock);
	writel_relaxed(mask, gic_dist_base(d) + GIC_DIST_ENABLE_CLEAR + (gic_irq(d) / 32) * 4);
	raw_spin_unlock(&irq_controller_lock);
}

static void gic_unmask_irq(struct irq_data *d)
{
	u32 mask = 1 << (gic_irq(d) % 32);

	raw_spin_lock(&irq_controller_lock);
	writel_relaxed(mask, gic_dist_base(d) + GIC_DIST_ENABLE_SET + (gic_irq(d) / 32) * 4);
	raw_spin_unlock(&irq_controller_lock);
}

static void gic_eoi_irq(struct irq_data *d)
{
	writel_relaxed(gic_irq(d), gic_cpu_base(d) + GIC_CPU_EOI);
}


void __iomem *GIC_DIST_BASE;
void __iomem *GIC_CPU_BASE;
void __iomem *INT_POL_CTL0;
phys_addr_t INT_POL_CTL0_phys;

__weak void mt_set_pol_reg(u32 reg_index, u32 value)
{
#ifndef mcusys_smc_write_phy
	writel_relaxed(value, (INT_POL_CTL0 + (reg_index * 4)));
#else
	mcusys_smc_write_phy((INT_POL_CTL0_phys + (reg_index * 4)), value);
#endif
}

void mt_irq_set_polarity(unsigned int irq, unsigned int polarity)
{
	u32 offset, reg_index, value;

	if (irq < (NR_GIC_SGI + NR_GIC_PPI)) {
		pr_crit("Fail to set polarity of interrupt %d\n", irq);
		return;
	}

	offset = (irq - (NR_GIC_SGI + NR_GIC_PPI)) & 0x1F;
	reg_index = (irq - (NR_GIC_SGI + NR_GIC_PPI)) >> 5;

	/*
	raw_spin_lock(&irq_controller_lock);
	*/

	if (polarity == 0) {
		/* active low */
		value = readl_relaxed(IOMEM(INT_POL_CTL0 + (reg_index * 4)));
		value |= (1 << offset);
		/* some platforms has to write POL register in secure world. USE PHYSICALL ADDRESS */
		mt_set_pol_reg(reg_index, value);
	} else {
		/* active high */
		value = readl_relaxed(IOMEM(INT_POL_CTL0 + (reg_index * 4)));
		value &= ~(0x1 << offset);
		/* some platforms has to write POL register in secure world */
		mt_set_pol_reg(reg_index, value);
	}

	/*
	raw_spin_unlock(&irq_controller_lock);
	*/
}
/* EXPORT_SYMBOL(mt_irq_set_polarity); */

static int gic_set_type(struct irq_data *d, unsigned int type)
{
	void __iomem *base = gic_dist_base(d);
	unsigned int gicirq = gic_irq(d);
	u32 enablemask = 1 << (gicirq % 32);
	u32 enableoff = (gicirq / 32) * 4;
	u32 confmask = 0x2 << ((gicirq % 16) * 2);
	u32 confoff = (gicirq / 16) * 4;
	bool enabled = false;
	u32 val;

	/* Interrupt configuration for SGIs can't be changed */
	if (gicirq < 16)
		return -EINVAL;

	/*
	if (type != IRQ_TYPE_LEVEL_HIGH && type != IRQ_TYPE_EDGE_RISING)
	return -EINVAL;
	*/

	raw_spin_lock(&irq_controller_lock);

	val = readl_relaxed(base + GIC_DIST_CONFIG + confoff);
	if ((type == IRQ_TYPE_LEVEL_HIGH) || (type == IRQ_TYPE_LEVEL_LOW)) {
		val &= ~confmask;
	} else if ((type == IRQ_TYPE_EDGE_RISING) || (type == IRQ_TYPE_EDGE_FALLING)) {
		val |= confmask;
	} else {
		pr_err("[GIC] not correct trigger type (0x%x)\n", type);
		dump_stack();
	}

	/*
	 * As recommended by the spec, disable the interrupt before changing
	 * the configuration
	 */
	if (readl_relaxed(base + GIC_DIST_ENABLE_SET + enableoff) & enablemask) {
		writel_relaxed(enablemask, base + GIC_DIST_ENABLE_CLEAR + enableoff);
		enabled = true;
	}

	writel_relaxed(val, base + GIC_DIST_CONFIG + confoff);

	if (enabled)
		writel_relaxed(enablemask, base + GIC_DIST_ENABLE_SET + enableoff);

	/*mtk polarity setting */
	if (type & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING))
		mt_irq_set_polarity(gicirq, (type & IRQF_TRIGGER_FALLING) ? 0 : 1);
	else if (type & (IRQF_TRIGGER_HIGH | IRQF_TRIGGER_LOW))
		mt_irq_set_polarity(gicirq, (type & IRQF_TRIGGER_LOW) ? 0 : 1);

	raw_spin_unlock(&irq_controller_lock);

	return 0;
}

static int gic_retrigger(struct irq_data *d)
{
	/* the genirq layer expects 0 if we can't retrigger in hardware */
	return 0;
}

/* set the priority mask to 0x00 for masking all irqs to this cpu */
void gic_set_primask(void)
{
	struct gic_chip_data *gic = &gic_data[0];
	void __iomem *base = gic_data_cpu_base(gic);

	writel_relaxed(0x00, base + GIC_CPU_PRIMASK);
}

/* restore the priority mask value */
void gic_clear_primask(void)
{
	struct gic_chip_data *gic = &gic_data[0];
	void __iomem *base = gic_data_cpu_base(gic);

	writel_relaxed(0xf0, base + GIC_CPU_PRIMASK);
}

void mt_set_irq_priority(unsigned int irq, unsigned int priority)
{
	unsigned int bit_offset = (irq % 4) * 8;
	unsigned int reg_offset = irq / 4 * 4;
	u32 val = readl(IOMEM(GIC_DIST_BASE +
			GIC_DIST_PRI + reg_offset));
	val &= ~(0xff << bit_offset);
	val |= (priority << bit_offset);
	writel_relaxed(val, IOMEM(GIC_DIST_BASE + GIC_DIST_PRI + reg_offset));
}

unsigned int mt_get_irq_priority(unsigned int irq)
{
	unsigned int bit_offset = (irq % 4) * 8;
	unsigned int reg_offset = irq / 4 * 4;

	return (readl(IOMEM(GIC_DIST_BASE + GIC_DIST_PRI + reg_offset)) >> bit_offset) & 0xff;
}

#ifdef CONFIG_SMP
static int gic_set_affinity(struct irq_data *d, const struct cpumask *mask_val, bool force)
{
	void __iomem *reg = gic_dist_base(d) + GIC_DIST_TARGET + (gic_irq(d) & ~3);
	unsigned int cpu, shift = (gic_irq(d) % 4) * 8;
	u32 val, bit = 0;
#ifndef CONFIG_MTK_IRQ_NEW_DESIGN
	u32 mask;
#endif

#ifndef CONFIG_MTK_IRQ_NEW_DESIGN
	if (!force)
		cpu = cpumask_any_and(mask_val, cpu_online_mask);
	else
		cpu = cpumask_first(mask_val);

	if (cpu >= NR_GIC_CPU_IF || cpu >= nr_cpu_ids)
		return -EINVAL;

	mask = 0xff << shift;
	bit = gic_cpu_map[cpu] << shift;

	raw_spin_lock(&irq_controller_lock);
	val = readl_relaxed(reg) & ~mask;
	writel_relaxed(val | bit, reg);
	raw_spin_unlock(&irq_controller_lock);
#else
	/*
	 * no need to update when:
	 * input mask is equal to the current setting
	 */
	if (cpumask_equal(d->affinity, mask_val))
		return IRQ_SET_MASK_OK_NOCOPY;

	/*
	 * cpumask_first_and() returns >= nr_cpu_ids when the intersection
	 * of inputs is an empty set -> return error when this is not a "forced" update
	 */
	if (!force && (cpumask_first_and(mask_val, cpu_online_mask) >= nr_cpu_ids))
		return -EINVAL;

	/* set target cpus */
	for_each_cpu(cpu, mask_val)
		bit |= gic_cpu_map[cpu] << shift;

	/* update gic register */
	raw_spin_lock(&irq_controller_lock);
	val = readl_relaxed(reg) & ~(0xff << shift);
	writel_relaxed(val | bit, reg);
	raw_spin_unlock(&irq_controller_lock);
#endif
	return IRQ_SET_MASK_OK;
}
#endif

#ifdef CONFIG_PM
static int gic_set_wake(struct irq_data *d, unsigned int on)
{
	int ret = -ENXIO;

	return ret;
}

#else
#define gic_set_wake	NULL
#endif

static asmlinkage void __exception_irq_entry gic_handle_irq(struct pt_regs *regs)
{
	u32 irqstat, irqnr;
	struct gic_chip_data *gic = &gic_data[0];
	void __iomem *cpu_base = gic_data_cpu_base(gic);

	do {
		irqstat = readl_relaxed(cpu_base + GIC_CPU_INTACK);
		irqnr = irqstat & ~0x1c00;

		if (likely(irqnr > 15 && irqnr < 1021)) {
			handle_domain_irq(gic->domain, irqnr, regs);
			continue;
		}
		if (irqnr < 16) {
			writel_relaxed(irqstat, cpu_base + GIC_CPU_EOI);
#ifdef CONFIG_SMP
			handle_IPI(irqnr, regs);
#endif
			continue;
		}
		break;
	} while (1);
}

static void gic_handle_cascade_irq(unsigned int irq, struct irq_desc *desc)
{
	struct gic_chip_data *chip_data = irq_get_handler_data(irq);
	struct irq_chip *chip = irq_get_chip(irq);
	unsigned int cascade_irq, gic_irq;
	unsigned long status;

	chained_irq_enter(chip, desc);

	raw_spin_lock(&irq_controller_lock);
	status = readl_relaxed(gic_data_cpu_base(chip_data) + GIC_CPU_INTACK);
	raw_spin_unlock(&irq_controller_lock);

	gic_irq = (status & 0x3ff);
	if (gic_irq == 1023)
		goto out;

	cascade_irq = irq_find_mapping(chip_data->domain, gic_irq);
	if (unlikely(gic_irq < 32 || gic_irq > 1020))
		handle_bad_irq(cascade_irq, desc);
	else
		generic_handle_irq(cascade_irq);

out:
	chained_irq_exit(chip, desc);
}

static struct irq_chip gic_chip = {
	.name = "GIC",
	.irq_mask = gic_mask_irq,
	.irq_unmask = gic_unmask_irq,
	.irq_eoi = gic_eoi_irq,
	.irq_set_type = gic_set_type,
	.irq_retrigger = gic_retrigger,
#ifdef CONFIG_SMP
	.irq_set_affinity = gic_set_affinity,
#endif
	.irq_set_wake = gic_set_wake,
};

void __init mt_gic_cascade_irq(unsigned int gic_nr, unsigned int irq)
{
	if (gic_nr >= MAX_GIC_NR)
		BUG();
	if (irq_set_handler_data(irq, &gic_data[gic_nr]) != 0)
		BUG();
	irq_set_chained_handler(irq, gic_handle_cascade_irq);
}

/*
   static u8 gic_get_cpumask(struct gic_chip_data *gic)
   {
   void __iomem *base = gic_data_dist_base(gic);
   u32 mask, i;

   for (i = mask = 0; i < 32; i += 4) {
   mask = readl_relaxed(base + GIC_DIST_TARGET + i);
   mask |= mask >> 16;
   mask |= mask >> 8;
   if (mask)
   break;
   }

   if (!mask)
   pr_crit("GIC CPU mask not found - kernel will fail to boot.\n");

   return mask;
   }
   */

static void __init gic_dist_init(struct gic_chip_data *gic)
{
	unsigned int i;
	u32 cpumask;
	unsigned int gic_irqs = gic->gic_irqs;
	void __iomem *base = gic_data_dist_base(gic);

	writel_relaxed(0, base + GIC_DIST_CTRL);

	/*
	 * Set all global interrupts to be level triggered, active low.
	 */
	for (i = 32; i < gic_irqs; i += 16)
		writel_relaxed(0, base + GIC_DIST_CONFIG + i * 4 / 16);

	/*
	 * Set all global interrupts to this CPU only.
	 */
	/*
	cpumask = gic_get_cpumask(gic);
	*/
	/*FIXME*/ cpumask = 1 << smp_processor_id();
	cpumask |= cpumask << 8;
	cpumask |= cpumask << 16;
	for (i = 32; i < gic_irqs; i += 4)
#ifndef CONFIG_MTK_IRQ_NEW_DESIGN
		writel_relaxed(cpumask, base + GIC_DIST_TARGET + i * 4 / 4);
#else
		writel_relaxed(0xffffffff, base + GIC_DIST_TARGET + i * 4 / 4);
#endif

	/*
	 * Set priority on all global interrupts.
	 */
	for (i = 32; i < gic_irqs; i += 4)
		writel_relaxed(0xa0a0a0a0, base + GIC_DIST_PRI + i * 4 / 4);

	/*
	 * Disable all interrupts.  Leave the PPI and SGIs alone
	 * as these enables are banked registers.
	 */
	for (i = 32; i < gic_irqs; i += 32)
		writel_relaxed(0xffffffff, base + GIC_DIST_ENABLE_CLEAR + i * 4 / 32);

	writel_relaxed(1, base + GIC_DIST_CTRL);
}

static void __cpuinit gic_cpu_init(struct gic_chip_data *gic)
{
	void __iomem *dist_base = gic_data_dist_base(gic);
	void __iomem *base = gic_data_cpu_base(gic);
	unsigned int cpu_mask, cpu = smp_processor_id();
	int i;

	/*
	 * Get what the GIC says our CPU mask is.
	 */
	BUG_ON(cpu >= NR_GIC_CPU_IF);
	/*
	cpu_mask = gic_get_cpumask(gic);
	FIXME
	*/
	cpu_mask = 1 << smp_processor_id();
	gic_cpu_map[cpu] = cpu_mask;

	/*
	 * Clear our mask from the other map entries in case they're
	 * still undefined.
	 */
	for (i = 0; i < NR_GIC_CPU_IF; i++)
		if (i != cpu)
			gic_cpu_map[i] &= ~cpu_mask;

	/*
	 * Deal with the banked PPI and SGI interrupts - disable all
	 * PPI interrupts, ensure all SGI interrupts are enabled.
	 */
	writel_relaxed(0xffff0000, dist_base + GIC_DIST_ENABLE_CLEAR);
	writel_relaxed(0x0000ffff, dist_base + GIC_DIST_ENABLE_SET);

	/*
	 * Set priority on PPI and SGI interrupts
	 */
	for (i = 0; i < (NR_GIC_SGI + NR_GIC_PPI); i += 4)
		writel_relaxed(0xa0a0a0a0, dist_base + GIC_DIST_PRI + i * 4 / 4);

	writel_relaxed(0xf0, base + GIC_CPU_PRIMASK);
	writel_relaxed(1, base + GIC_CPU_CTRL);
}

#ifdef CONFIG_CPU_PM
/*
 * Saves the GIC distributor registers during suspend or idle.  Must be called
 * with interrupts disabled but before powering down the GIC.  After calling
 * this function, no interrupts will be delivered by the GIC, and another
 * platform-specific wakeup source must be enabled.
 */
static void gic_dist_save(unsigned int gic_nr)
{
	unsigned int gic_irqs;
	void __iomem *dist_base;
	int i;

	if (gic_nr >= MAX_GIC_NR)
		BUG();

	gic_irqs = gic_data[gic_nr].gic_irqs;
	dist_base = gic_data_dist_base(&gic_data[gic_nr]);

	if (!dist_base)
		return;

	for (i = 0; i < DIV_ROUND_UP(gic_irqs, 16); i++)
		gic_data[gic_nr].saved_spi_conf[i] =
		    readl_relaxed(dist_base + GIC_DIST_CONFIG + i * 4);

	for (i = 0; i < DIV_ROUND_UP(gic_irqs, 4); i++)
		gic_data[gic_nr].saved_spi_target[i] =
		    readl_relaxed(dist_base + GIC_DIST_TARGET + i * 4);

	for (i = 0; i < DIV_ROUND_UP(gic_irqs, 32); i++)
		gic_data[gic_nr].saved_spi_enable[i] =
		    readl_relaxed(dist_base + GIC_DIST_ENABLE_SET + i * 4);
}

/*
 * Restores the GIC distributor registers during resume or when coming out of
 * idle.  Must be called before enabling interrupts.  If a level interrupt
 * that occurred while the GIC was suspended is still present, it will be
 * handled normally, but any edge interrupts that occurred will not be seen by
 * the GIC and need to be handled by the platform-specific wakeup source.
 */
static void gic_dist_restore(unsigned int gic_nr)
{
	unsigned int gic_irqs;
	unsigned int i;
	void __iomem *dist_base;

	if (gic_nr >= MAX_GIC_NR)
		BUG();

	gic_irqs = gic_data[gic_nr].gic_irqs;
	dist_base = gic_data_dist_base(&gic_data[gic_nr]);

	if (!dist_base)
		return;

	writel_relaxed(0, dist_base + GIC_DIST_CTRL);

	for (i = 0; i < DIV_ROUND_UP(gic_irqs, 16); i++)
		writel_relaxed(gic_data[gic_nr].saved_spi_conf[i],
			       dist_base + GIC_DIST_CONFIG + i * 4);

	for (i = 0; i < DIV_ROUND_UP(gic_irqs, 4); i++)
		writel_relaxed(0xa0a0a0a0, dist_base + GIC_DIST_PRI + i * 4);

	for (i = 0; i < DIV_ROUND_UP(gic_irqs, 4); i++)
		writel_relaxed(gic_data[gic_nr].saved_spi_target[i],
			       dist_base + GIC_DIST_TARGET + i * 4);

	for (i = 0; i < DIV_ROUND_UP(gic_irqs, 32); i++)
		writel_relaxed(gic_data[gic_nr].saved_spi_enable[i],
			       dist_base + GIC_DIST_ENABLE_SET + i * 4);

	writel_relaxed(1, dist_base + GIC_DIST_CTRL);
}

static void gic_cpu_save(unsigned int gic_nr)
{
	int i;
	u32 *ptr;
	void __iomem *dist_base;
	void __iomem *cpu_base;

	if (gic_nr >= MAX_GIC_NR)
		BUG();

	dist_base = gic_data_dist_base(&gic_data[gic_nr]);
	cpu_base = gic_data_cpu_base(&gic_data[gic_nr]);

	if (!dist_base || !cpu_base)
		return;

	ptr = raw_cpu_ptr(gic_data[gic_nr].saved_ppi_enable);
	for (i = 0; i < DIV_ROUND_UP(32, 32); i++)
		ptr[i] = readl_relaxed(dist_base + GIC_DIST_ENABLE_SET + i * 4);

	ptr = raw_cpu_ptr(gic_data[gic_nr].saved_ppi_conf);
	for (i = 0; i < DIV_ROUND_UP(32, 16); i++)
		ptr[i] = readl_relaxed(dist_base + GIC_DIST_CONFIG + i * 4);

}

static void gic_cpu_restore(unsigned int gic_nr)
{
	int i;
	u32 *ptr;
	void __iomem *dist_base;
	void __iomem *cpu_base;

	if (gic_nr >= MAX_GIC_NR)
		BUG();

	dist_base = gic_data_dist_base(&gic_data[gic_nr]);
	cpu_base = gic_data_cpu_base(&gic_data[gic_nr]);

	if (!dist_base || !cpu_base)
		return;

	ptr = raw_cpu_ptr(gic_data[gic_nr].saved_ppi_enable);
	for (i = 0; i < DIV_ROUND_UP(32, 32); i++)
		writel_relaxed(ptr[i], dist_base + GIC_DIST_ENABLE_SET + i * 4);

	ptr = raw_cpu_ptr(gic_data[gic_nr].saved_ppi_conf);
	for (i = 0; i < DIV_ROUND_UP(32, 16); i++)
		writel_relaxed(ptr[i], dist_base + GIC_DIST_CONFIG + i * 4);

	for (i = 0; i < DIV_ROUND_UP(32, 4); i++)
		writel_relaxed(0xa0a0a0a0, dist_base + GIC_DIST_PRI + i * 4);

	writel_relaxed(0xf0, cpu_base + GIC_CPU_PRIMASK);
	writel_relaxed(1, cpu_base + GIC_CPU_CTRL);
}

static int gic_notifier(struct notifier_block *self, unsigned long cmd, void *v)
{
	int i;

	for (i = 0; i < MAX_GIC_NR; i++) {
#ifdef CONFIG_GIC_NON_BANKED
		/* Skip over unused GICs */
		if (!gic_data[i].get_base)
			continue;
#endif
		switch (cmd) {
		case CPU_PM_ENTER:
			gic_cpu_save(i);
			break;
		case CPU_PM_ENTER_FAILED:
		case CPU_PM_EXIT:
			gic_cpu_restore(i);
			break;
		case CPU_CLUSTER_PM_ENTER:
			gic_dist_save(i);
			break;
		case CPU_CLUSTER_PM_ENTER_FAILED:
		case CPU_CLUSTER_PM_EXIT:
			gic_dist_restore(i);
			break;
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block gic_notifier_block = {
	.notifier_call = gic_notifier,
};

static void __init gic_pm_init(struct gic_chip_data *gic)
{
	gic->saved_ppi_enable = __alloc_percpu(DIV_ROUND_UP(32, 32) * 4, sizeof(u32));
	BUG_ON(!gic->saved_ppi_enable);

	gic->saved_ppi_conf = __alloc_percpu(DIV_ROUND_UP(32, 16) * 4, sizeof(u32));
	BUG_ON(!gic->saved_ppi_conf);

	if (gic == &gic_data[0])
		cpu_pm_register_notifier(&gic_notifier_block);
}
#else
static void __init gic_pm_init(struct gic_chip_data *gic)
{
}
#endif

#ifdef CONFIG_SMP
void mt_gic_raise_softirq(const struct cpumask *mask, unsigned int irq)
{
	int cpu;
	unsigned long map = 0;

	/* Convert our logical CPU mask into a physical one. */
	for_each_cpu(cpu, mask)
		map |= gic_cpu_map[cpu];

	/*
	 * Ensure that stores to Normal memory are visible to the
	 * other CPUs before issuing the IPI.
	 */
	mb();

	/* this always happens on GIC0 */
	writel_relaxed(map << 16 | irq, gic_data_dist_base(&gic_data[0]) + GIC_DIST_SOFTINT);
}
#endif

static int gic_irq_domain_map(struct irq_domain *d, unsigned int irq, irq_hw_number_t hw)
{
	if (hw < 32) {
		irq_set_percpu_devid(irq);
		irq_set_chip_and_handler(irq, &gic_chip, handle_percpu_devid_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_NOAUTOEN);
	} else {
		irq_set_chip_and_handler(irq, &gic_chip, handle_fasteoi_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}
	irq_set_chip_data(irq, d->host_data);
	return 0;
}

static int gic_irq_domain_xlate(struct irq_domain *d,
				struct device_node *controller,
				const u32 *intspec, unsigned int intsize,
				unsigned long *out_hwirq, unsigned int *out_type)
{
	if (d->of_node != controller)
		return -EINVAL;
	if (intsize < 3)
		return -EINVAL;

	/* Get the interrupt number and add 16 to skip over SGIs */
	*out_hwirq = intspec[1] + NR_GIC_SGI;

	/* For SPIs, we need to add 16 more to get the GIC irq ID number */
	if (!intspec[0])
		*out_hwirq += NR_GIC_SGI;

	*out_type = intspec[2] & IRQ_TYPE_SENSE_MASK;
	return 0;
}

void mt_gic_register_sgi(unsigned int gic_nr, int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (desc)
		desc->irq_data.hwirq = irq;
	irq_set_chip_and_handler(irq, &gic_chip, handle_fasteoi_irq);
	set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	irq_set_chip_data(irq, &gic_data[gic_nr]);
}

#ifdef CONFIG_SMP
static int __cpuinit gic_secondary_init(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	if (action == CPU_STARTING || action == CPU_STARTING_FROZEN)
		gic_cpu_init(&gic_data[0]);
	return NOTIFY_OK;
}

/*
 * Notifier for enabling the GIC CPU interface. Set an arbitrarily high
 * priority because the GIC needs to be up before the ARM generic timers.
 */
static struct notifier_block gic_cpu_notifier __cpuinitdata = {
	.notifier_call = gic_secondary_init,
	.priority = 100,
};
#endif

const struct irq_domain_ops mt_gic_irq_domain_ops = {
	.map = gic_irq_domain_map,
	.xlate = gic_irq_domain_xlate,
};

void __init mt_gic_init_bases(unsigned int gic_nr, int irq_start,
			      void __iomem *dist_base, void __iomem *cpu_base,
			      u32 percpu_offset, struct device_node *node)
{
	irq_hw_number_t hwirq_base;
	struct gic_chip_data *gic;
	int gic_irqs, irq_base, i;

	BUG_ON(gic_nr >= MAX_GIC_NR);

	gic = &gic_data[gic_nr];
#ifdef CONFIG_GIC_NON_BANKED
	if (percpu_offset) {	/* Frankein-GIC without banked registers... */
		unsigned int cpu;

		gic->dist_base.percpu_base = alloc_percpu(void __iomem *);
		gic->cpu_base.percpu_base = alloc_percpu(void __iomem *);
		if (WARN_ON(!gic->dist_base.percpu_base || !gic->cpu_base.percpu_base)) {
			free_percpu(gic->dist_base.percpu_base);
			free_percpu(gic->cpu_base.percpu_base);
			return;
		}

		for_each_possible_cpu(cpu) {
			unsigned long offset = percpu_offset * cpu_logical_map(cpu);
			*per_cpu_ptr(gic->dist_base.percpu_base, cpu) = dist_base + offset;
			*per_cpu_ptr(gic->cpu_base.percpu_base, cpu) = cpu_base + offset;
		}

		gic_set_base_accessor(gic, gic_get_percpu_base);
	} else
#endif
	{
		/* Normal, sane GIC... */
		WARN(percpu_offset,
		     "GIC_NON_BANKED not enabled, ignoring %08x offset!", percpu_offset);
		gic->dist_base.common_base = dist_base;
		gic->cpu_base.common_base = cpu_base;
		gic_set_base_accessor(gic, gic_get_common_base);
	}

	/*
	 * Initialize the CPU interface map to all CPUs.
	 * It will be refined as each CPU probes its ID.
	 */
	for (i = 0; i < NR_GIC_CPU_IF; i++)
		gic_cpu_map[i] = 0xff;

	/*
	 * For primary GICs, skip over SGIs.
	 * For secondary GICs, skip over PPIs, too.
	 */
	if (gic_nr == 0 && (irq_start & 31) > 0) {
		hwirq_base = 16;
		if (irq_start != -1)
			irq_start = (irq_start & ~31) + 16;
	} else {
		hwirq_base = 32;
	}

	/*
	 * Find out how many interrupts are supported.
	 * The GIC only supports up to 1020 interrupt sources.
	 */
	gic_irqs = readl_relaxed(gic_data_dist_base(gic) + GIC_DIST_CTR) & 0x1f;
	gic_irqs = (gic_irqs + 1) * 32;
	if (gic_irqs > 1020)
		gic_irqs = 1020;
	gic->gic_irqs = gic_irqs;

	gic_irqs -= hwirq_base;	/* calculate # of irqs to allocate */
	irq_base = irq_alloc_descs(irq_start, 16, gic_irqs, numa_node_id());
	if (IS_ERR_VALUE(irq_base)) {
		WARN(1, "Cannot allocate irq_descs @ IRQ%d, assuming pre-allocated\n", irq_start);
		irq_base = irq_start;
	}
	gic->domain = irq_domain_add_legacy(node, gic_irqs, irq_base,
					    hwirq_base, &mt_gic_irq_domain_ops, gic);
	if (WARN_ON(!gic->domain))
		return;

#ifdef CONFIG_SMP
	set_smp_cross_call(mt_gic_raise_softirq);
	register_cpu_notifier(&gic_cpu_notifier);
#endif

	set_handle_irq(gic_handle_irq);

	gic_dist_init(gic);
	gic_cpu_init(gic);
	gic_pm_init(gic);
}


/* Special APIs for specific modules */

static spinlock_t irq_lock;

/*
 * mt_irq_mask_all: disable all interrupts
 * @mask: pointer to struct mtk_irq_mask for storing the original mask value.
 * Return 0 for success; return negative values for failure.
 * (This is ONLY used for the idle current measurement by the factory mode.)
 */
int mt_irq_mask_all(struct mtk_irq_mask *mask)
{
	unsigned long flags;
	void __iomem *dist_base;

	dist_base = gic_data_dist_base(&gic_data[0]);

	if (mask) {
		/*
		#if defined(CONFIG_FIQ_GLUE)
			local_fiq_disable();
		#endif
		*/
		spin_lock_irqsave(&irq_lock, flags);

		mask->mask0 = readl((dist_base + GIC_DIST_ENABLE_SET));
		mask->mask1 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x4));
		mask->mask2 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x8));
		mask->mask3 = readl((dist_base + GIC_DIST_ENABLE_SET + 0xC));
		mask->mask4 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x10));
		mask->mask5 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x14));
		mask->mask6 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x18));
		mask->mask7 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x1C));
		mask->mask8 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x20));

		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x4));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x8));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0xC));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x10));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x14));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x18));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x1C));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x20));
		mb();

		spin_unlock_irqrestore(&irq_lock, flags);
		/*
		#if defined(CONFIG_FIQ_GLUE)
		local_fiq_enable();
		#endif
		*/
		mask->header = IRQ_MASK_HEADER;
		mask->footer = IRQ_MASK_FOOTER;

		return 0;
	} else {
		return -1;
	}
}

/*
 * mt_irq_mask_restore: restore all interrupts
 * @mask: pointer to struct mtk_irq_mask for storing the original mask value.
 * Return 0 for success; return negative values for failure.
 * (This is ONLY used for the idle current measurement by the factory mode.)
 */
int mt_irq_mask_restore(struct mtk_irq_mask *mask)
{
	unsigned long flags;
	void __iomem *dist_base;

	dist_base = gic_data_dist_base(&gic_data[0]);

	if (!mask)
		return -1;
	if (mask->header != IRQ_MASK_HEADER)
		return -1;
	if (mask->footer != IRQ_MASK_FOOTER)
		return -1;

	/*
	#if defined(CONFIG_FIQ_GLUE)
	      local_fiq_disable();
	#endif
	*/
	spin_lock_irqsave(&irq_lock, flags);

	writel(mask->mask0, (dist_base + GIC_DIST_ENABLE_SET));
	writel(mask->mask1, (dist_base + GIC_DIST_ENABLE_SET + 0x4));
	writel(mask->mask2, (dist_base + GIC_DIST_ENABLE_SET + 0x8));
	writel(mask->mask3, (dist_base + GIC_DIST_ENABLE_SET + 0xC));
	writel(mask->mask4, (dist_base + GIC_DIST_ENABLE_SET + 0x10));
	writel(mask->mask5, (dist_base + GIC_DIST_ENABLE_SET + 0x14));
	writel(mask->mask6, (dist_base + GIC_DIST_ENABLE_SET + 0x18));
	writel(mask->mask7, (dist_base + GIC_DIST_ENABLE_SET + 0x1C));
	writel(mask->mask8, (dist_base + GIC_DIST_ENABLE_SET + 0x20));
	mb();

	spin_unlock_irqrestore(&irq_lock, flags);
	/*
	#if defined(CONFIG_FIQ_GLUE)
	      local_fiq_enable();
	#endif
	*/
	return 0;
}

/*
 * mt_irq_set_pending_for_sleep: pending an interrupt for the sleep manager's use
 * @irq: interrupt id
 * (THIS IS ONLY FOR SLEEP FUNCTION USE. DO NOT USE IT YOURSELF!)
 */
void mt_irq_set_pending_for_sleep(unsigned int irq)
{
	void __iomem *dist_base;
	u32 mask = 1 << (irq % 32);

	dist_base = gic_data_dist_base(&gic_data[0]);

	if (irq < 16) {
		pr_err("Fail to set a pending on interrupt %d\n", irq);
		return;
	}

	*(volatile u32 *)(dist_base + GIC_DIST_PENDING_SET + irq / 32 * 4) = mask;
	pr_debug("irq:%d, 0x%p=0x%x\n", irq,
		  dist_base + GIC_DIST_PENDING_SET + irq / 32 * 4, mask);
	mb();
}

u32 mt_irq_get_pending(unsigned int irq)
{
	void __iomem *dist_base;
	u32 bit = 1 << (irq % 32);

	dist_base = gic_data_dist_base(&gic_data[0]);

	return (readl_relaxed(dist_base + GIC_DIST_PENDING_SET + irq / 32 * 4) & bit) ? 1 : 0;
}

void mt_irq_set_pending(unsigned int irq)
{
	void __iomem *dist_base;
	u32 bit = 1 << (irq % 32);

	dist_base = gic_data_dist_base(&gic_data[0]);
	writel(bit, dist_base + GIC_DIST_PENDING_SET + irq / 32 * 4);
}

/*
 * mt_irq_unmask_for_sleep: enable an interrupt for the sleep manager's use
 * @irq: interrupt id
 * (THIS IS ONLY FOR SLEEP FUNCTION USE. DO NOT USE IT YOURSELF!)
 */
void mt_irq_unmask_for_sleep(unsigned int irq)
{
	void __iomem *dist_base;
	u32 mask = 1 << (irq % 32);

	dist_base = gic_data_dist_base(&gic_data[0]);

	if (irq < 16) {
		pr_err("Fail to enable interrupt %d\n", irq);
		return;
	}

	*(volatile u32 *)(dist_base + GIC_DIST_ENABLE_SET + irq / 32 * 4) = mask;
	mb();
}

/*
 * mt_irq_mask_for_sleep: disable an interrupt for the sleep manager's use
 * @irq: interrupt id
 * (THIS IS ONLY FOR SLEEP FUNCTION USE. DO NOT USE IT YOURSELF!)
 */
void mt_irq_mask_for_sleep(unsigned int irq)
{
	void __iomem *dist_base;
	u32 mask = 1 << (irq % 32);

	dist_base = gic_data_dist_base(&gic_data[0]);

	if (irq < 16) {
		pr_err("Fail to enable interrupt %d\n", irq);
		return;
	}

	*(volatile u32 *)(dist_base + GIC_DIST_ENABLE_CLEAR + irq / 32 * 4) = mask;
	mb();
}

/*
 * mt_irq_set_sens: set the interrupt sensitivity
 * @irq: interrupt id
 * @sens: sensitivity
 */
void mt_irq_set_sens(unsigned int irq, unsigned int sens)
{
	unsigned long flags;
	u32 config;

	if (irq < (NR_GIC_SGI + NR_GIC_PPI)) {
		pr_err("Fail to set sensitivity of interrupt %d\n", irq);
		return;
	}

	spin_lock_irqsave(&irq_lock, flags);

	if (sens == MT_EDGE_SENSITIVE) {
		config = readl(GIC_DIST_BASE + GIC_DIST_CONFIG + (irq / 16) * 4);
		config |= (0x2 << (irq % 16) * 2);
		writel(config, GIC_DIST_BASE + GIC_DIST_CONFIG + (irq / 16) * 4);
	} else {
		config = readl(GIC_DIST_BASE + GIC_DIST_CONFIG + (irq / 16) * 4);
		config &= ~(0x2 << (irq % 16) * 2);
		writel(config, GIC_DIST_BASE + GIC_DIST_CONFIG + (irq / 16) * 4);
	}

	spin_unlock_irqrestore(&irq_lock, flags);

	mb();
}
/* EXPORT_SYMBOL(mt_irq_set_sens); */

char *mt_irq_dump_status_buf(int irq, char *buf, int len)
{
	int rc;
	char *ptr = buf;
	char *pend = ptr + len;
	int strlen = 0;

	if (!ptr)
		return NULL;

	if (irq > mt_get_supported_irq_num()) {
		strlen = snprintf(ptr, pend - ptr,
				  "error! irq = %d not support\n",
				  irq);

		if (strlen < 0)
			goto overflow;
		ptr += strlen;

		return ptr;
	}

#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
	rc = mt_secure_call(MTK_SIP_KERNEL_GIC_DUMP, irq, 0, 0);
#else
	rc = -1;
#endif
	if (rc < 0) {
		strlen = snprintf(ptr, pend - ptr,
				  "irq = %d not allowed to dump!\n",
				  irq);

		if (strlen < 0)
			goto overflow;
		ptr += strlen;
		return ptr;
	}

	strlen = snprintf(ptr, pend - ptr,
			  "%s%s=%d\n%s=%d\n%s=%x%s\n%s=0x%x\n%s=%x%s\n%s=%x\n%s=%x\n%s=%x%s\n%s=0x%x\n",
			  "[mt gic dump]\n",
			  "irq", irq,
			  "enable", rc & 0x1,
			  "group", (rc >> 1) & 0x1, " (0x1:irq,0x0:fiq)",
			  "priority", (rc >> 2) & 0xff,
			  "sensitivity", (rc >> 10) & 0x1, " (edge:0x1, level:0x0)",
			  "pending", (rc >> 11) & 0x1,
			  "active", (rc >> 12) & 0x1,
			  "polarity", (rc >> 13) & 0x1, " (0x0: high, 0x1:low)",
			  "cpu mask", (rc >> 14) & 0xff);
	if (strlen < 0)
		goto overflow;
	ptr += strlen;

	return ptr;

overflow:
	pr_err("%s, input buffer overflow\n", __func__);
	/* Because the buffer has useful string, return it for dump */
	return ptr;


}

void mt_irq_dump_status(int irq)
{
	int len = 2048;
	char *buf = kmalloc(len, GFP_ATOMIC);

	if (!buf)
		return;

	if (mt_irq_dump_status_buf(irq, buf, len))
		pr_debug("%s", buf);

	kfree(buf);
}
EXPORT_SYMBOL(mt_irq_dump_status);


#ifdef CONFIG_MTK_IRQ_NEW_DESIGN
unsigned int wdt_irq;

bool mt_is_secure_irq(struct irq_data *d)
{
	return (gic_irq(d) == wdt_irq);
}
EXPORT_SYMBOL(mt_is_secure_irq);

bool mt_get_irq_gic_targets(struct irq_data *d, cpumask_t *mask)
{
	unsigned int irq = gic_irq(d);
	unsigned int cpu, shift, irq_targets = 0;
	void __iomem *reg;
	int rc;

	/* check whether this IRQ is configured as FIQ */
	if (mt_is_secure_irq(d)) {
		/* secure call for get the irq targets */
#ifndef CONFIG_MTK_PSCI
		rc = -1;
#else
		rc = mt_secure_call(MTK_SIP_KERNEL_GIC_DUMP, irq, 0, 0);
#endif

		if (rc < 0) {
			pr_err("[mt_get_gicd_itargetsr] not allowed to dump!\n");
			return false;
		}
		irq_targets = (rc >> 14) & 0xff;
	} else {
		shift = (irq % 4) * 8;
		reg = gic_dist_base(d) + GIC_DIST_TARGET + (irq & ~3);
		irq_targets = (readl_relaxed(reg) & (0xff << shift)) >> shift;
	}

	cpumask_clear(mask);
	for_each_cpu(cpu, cpu_possible_mask)
		if (irq_targets & (1<<cpu))
			cpumask_set_cpu(cpu, mask);

	return true;
}
EXPORT_SYMBOL(mt_get_irq_gic_targets);
#endif

#include <linux/platform_device.h>

static unsigned long dump_irq;

static struct platform_driver gic_debug_drv = {
	.driver = {
		.name = "gic_debug",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
	}
};

static ssize_t dump_irq_show(struct device_driver *driver, char *buf)
{
	if (mt_irq_dump_status_buf(dump_irq, buf, PAGE_SIZE))
		return strlen(buf);

	else
		return 0;
}

static ssize_t dump_irq_store(struct device_driver *driver, const char *buf, size_t count)
{
	int ret;

	ret = kstrtoul(buf, 10, (unsigned long *)&dump_irq);
	if (ret != 0) {
		pr_err("usage: echo $irq_num > /sys/bus/platform/drivers/gic_debug/dump_irq\n");
		return -EINVAL;
	}

	mt_irq_dump_status(dump_irq);
	return count;
}
DRIVER_ATTR(dump_irq, 0644, dump_irq_show, dump_irq_store);

int __init gic_debug_drv_init(void)
{
	int ret;

	ret = driver_register(&gic_debug_drv.driver);
	if (ret)
		pr_err("fail to create gic debug driver\n");
	else
		pr_err("success to create gic debug driver\n");

	ret = driver_create_file(&gic_debug_drv.driver, &driver_attr_dump_irq);
	if (ret)
		pr_err("fail to create dump_irq sysfs files\n");
	else
		pr_err("success to create dump_irq sysfs files\n");
	return 0;
}
arch_initcall(gic_debug_drv_init);

static unsigned int get_pol(int irq)
{
	unsigned int bit;

	bit = 1 << (irq % 32);
	/* 0x0: high, 0x1:low */
	return (readl(INT_POL_CTL0 + ((irq - 32) / 32 * 4)) & bit) ? 1 : 0;
}

static unsigned int get_sens(int irq)
{
	unsigned int bit;

	bit = 0x3 << ((irq % 16) * 2);
	/* edge:0x2, level:0x1 */
	return (readl(GIC_DIST_BASE + GIC_DIST_CONFIG + irq / 16 * 4) & bit) >> ((irq % 16) * 2);
}

static irqreturn_t gic_test_isr(void)
{
	return IRQ_HANDLED;
}

#define MT_EDGE_SENSITIVE 0
#define MT_LEVEL_SENSITIVE 1
#define MT_POLARITY_LOW   0
#define MT_POLARITY_HIGH  1
int mt_gic_test(int irq, int type)
{
	int ret;

	ret = request_irq(irq, (irq_handler_t) gic_test_isr, type, "mtk_watchdog", NULL);
	if (ret < 0)
		pr_err("mtk gic test failed! fail num = %d\n", ret);

	switch (type) {
	case IRQF_TRIGGER_RISING:
		if (get_pol(irq) == !MT_POLARITY_HIGH)
			pr_debug
			    ("[IRQF_TRIGGER_RISING]mt_irq_set_polarity GIC_POL_HIGH test passed!!!\n");
		else
			pr_debug
			    ("[IRQF_TRIGGER_RISING]mt_irq_set_polarity GIC_POL_HIGH test failed!!!\n");
		if (get_sens(irq) >> 1)
			pr_debug
			    ("[IRQF_TRIGGER_RISING]mt_irq_set_sens MT_EDGE_SENSITIVE test passed!!!\n");
		else
			pr_debug
			    ("[IRQF_TRIGGER_RISING]mt_irq_set_sens MT_EDGE_SENSITIVE test failed!!!\n");
		break;
	case IRQF_TRIGGER_FALLING:
		if (get_pol(irq) == !MT_POLARITY_LOW)
			pr_debug
			    ("[IRQF_TRIGGER_FALLING]mt_irq_set_polarity GIC_POL_LOW test passed!!!\n");
		else
			pr_debug
			    ("[IRQF_TRIGGER_FALLING]mt_irq_set_polarity GIC_POL_LOW test failed!!!\n");
		if (get_sens(irq) >> 1)
			pr_debug
			    ("[IRQF_TRIGGER_FALLING]mt_irq_set_sens MT_EDGE_SENSITIVE test passed!!!\n");
		else
			pr_debug
			    ("[IRQF_TRIGGER_FALLING]mt_irq_set_sens MT_EDGE_SENSITIVE test failed!!!\n");
		break;
	case IRQF_TRIGGER_HIGH:
		if (get_pol(irq) == !MT_POLARITY_HIGH)
			pr_debug
			    ("[IRQF_TRIGGER_HIGH]mt_irq_set_polarity GIC_POL_HIGH test passed!!!\n");
		else
			pr_debug
			    ("[IRQF_TRIGGER_HIGH]mt_irq_set_polarity GIC_POL_HIGH test failed!!!\n");
		if (!(get_sens(irq) >> 1))
			pr_debug
			    ("[IRQF_TRIGGER_HIGH]mt_irq_set_sens MT_LEVEL_SENSITIVE test passed!!!\n");
		else
			pr_debug
			    ("[IRQF_TRIGGER_HIGH]mt_irq_set_sens MT_LEVEL_SENSITIVE test failed!!!\n");
		break;
	case IRQF_TRIGGER_LOW:
		if (get_pol(irq) == !MT_POLARITY_LOW)
			pr_debug
			    ("[IRQF_TRIGGER_LOW]mt_irq_set_polarity GIC_POL_LOW test passed!!!\n");
		else
			pr_debug
			    ("[IRQF_TRIGGER_LOW]mt_irq_set_polarity GIC_POL_LOW test failed!!!\n");
		if (!(get_sens(irq) >> 1))
			pr_debug
			    ("[IRQF_TRIGGER_LOW]mt_irq_set_sens MT_LEVEL_SENSITIVE test passed!!!\n");
		else
			pr_debug
			    ("[IRQF_TRIGGER_LOW]mt_irq_set_sens MT_LEVEL_SENSITIVE test failed!!!\n");
		break;
	default:
		pr_err("[GIC] not correct trigger type\n");
		return -1;
	}

	return ret;
}

#ifdef CONFIG_OF
static int gic_cnt __initdata;

int mt_get_supported_irq_num(void)
{
	void __iomem *dist_base;
	int ret = 0;

	dist_base = gic_data_dist_base(&gic_data[0]);
	if (dist_base) {
		ret = ((readl_relaxed(dist_base + GIC_DIST_CTR) & 0x1f) + 1) * 32;
		pr_debug("gic supported max = %d\n", ret);
	} else
		pr_warn("gic dist_base is unknown\n");

	return ret;
}
EXPORT_SYMBOL(mt_get_supported_irq_num);

int __init mt_gic_of_init(struct device_node *node, struct device_node *parent)
{
	void __iomem *cpu_base;
	void __iomem *dist_base;
	void __iomem *pol_base;
	u32 percpu_offset;
	int irq;
	struct resource res;
#ifdef CONFIG_MTK_IRQ_NEW_DESIGN
	int i;
#endif

	if (WARN_ON(!node))
		return -ENODEV;

	spin_lock_init(&irq_lock);

	dist_base = of_iomap(node, 0);
	WARN(!dist_base, "unable to map gic dist registers\n");
	GIC_DIST_BASE = dist_base;

	cpu_base = of_iomap(node, 1);
	WARN(!cpu_base, "unable to map gic cpu registers\n");
	GIC_CPU_BASE = cpu_base;

	pol_base = of_iomap(node, 2);
	WARN(!pol_base, "unable to map pol registers\n");
	INT_POL_CTL0 = pol_base;
	if (of_address_to_resource(node, 2, &res))
		WARN(!pol_base, "unable to map pol registers\n");

	INT_POL_CTL0_phys = res.start;

	if (of_property_read_u32(node, "cpu-offset", &percpu_offset))
		percpu_offset = 0;

	mt_gic_init_bases(gic_cnt, -1, dist_base, cpu_base, percpu_offset, node);

	if (parent) {
		irq = irq_of_parse_and_map(node, 0);
		mt_gic_cascade_irq(gic_cnt, irq);
	}
	gic_cnt++;

#ifdef CONFIG_MTK_IRQ_NEW_DESIGN
	for (i = 0; i <= CONFIG_NR_CPUS-1; ++i) {
		INIT_LIST_HEAD(&(irq_need_migrate_list[i].list));
		spin_lock_init(&(irq_need_migrate_list[i].lock));
	}

	if (of_property_read_u32(node, "mediatek,wdt_irq", &wdt_irq))
		wdt_irq = 0;
#endif

	return 0;
}

IRQCHIP_DECLARE(mt_gic, "mediatek,mt6735-gic", mt_gic_of_init);

#endif
