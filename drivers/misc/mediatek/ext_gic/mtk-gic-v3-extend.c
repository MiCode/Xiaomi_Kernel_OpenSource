/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Mars.Cheng <mars.cheng@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/types.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/sizes.h>
#include <linux/irqchip/arm-gic-v3.h>
#include <linux/irqchip/mtk-gic-extend.h>
#include <linux/io.h>
#include <mach/mt_secure_api.h>

#define IOMEM(x)        ((void __force __iomem *)(x))
/* for cirq use */
void __iomem *GIC_DIST_BASE;
void __iomem *INT_POL_CTL0;
void __iomem *INT_POL_CTL1;
static void __iomem *GIC_REDIST_BASE;
static u32 wdt_irq;
static u32 reg_len_pol0;

#ifndef readq
/* for some kernel config, readq might not be defined, ex aarch32 */
static inline u64 readq(const void __iomem *addr)
{
	u64 ret = readl(addr + 4);

	ret <<= 32;
	ret |= readl(addr);

	return ret;
}
#endif

static inline unsigned int gic_irq(struct irq_data *d)
{
	return d->hwirq;
}

static int gic_populate_rdist(void __iomem **rdist_base)
{
	int cpu = smp_processor_id();

	*rdist_base = GIC_REDIST_BASE + cpu*SZ_64K*2;

	return 0;
}

bool mt_is_secure_irq(struct irq_data *d)
{
	if (gic_irq(d) == wdt_irq)
		return true;
	else
		return false;
}

bool mt_get_irq_gic_targets(struct irq_data *d, cpumask_t *mask)
{
	void __iomem *dist_base;
	void __iomem *routing_reg;
	u32 cpu;
	u32 cluster;
	u64 routing_val;
	u32 target_mask;

	/* for SPI/PPI, target to current cpu */
	if (gic_irq(d) < 32) {
		target_mask = 1<<smp_processor_id();
		goto build_mask;
	}

	/* for SPI, we read routing info to build current mask */
	dist_base = GIC_DIST_BASE;
	routing_reg = dist_base + GICD_IROUTER + (gic_irq(d)*8);
	routing_val = readq(routing_reg);

	/* if target all, target_mask should indicate all CPU */
	if (routing_val & GICD_IROUTER_SPI_MODE_ANY) {
		target_mask = (1<<num_possible_cpus())-1;
		pr_debug("%s:%d: irq(%d) targets all\n",
				__func__, __LINE__, gic_irq(d));
	} else {
		/* if not target all,
		 * it should be targted to specific cpu only */
		cluster = (routing_val&0xff00)>>8;
		cpu = routing_val&0xff;

		/* assume 1 cluster contain 4 cpu in little,
		 * and only the last cluster can contain less than 4 cpu */
		target_mask = 1<<(cluster*4 + cpu);

		pr_debug("%s:%d: irq(%d) target_mask(0x%x)\n",
				__func__, __LINE__, gic_irq(d), target_mask);
	}

build_mask:
	cpumask_clear(mask);
	for_each_cpu(cpu, cpu_possible_mask) {
		if (target_mask & (1<<cpu))
			cpumask_set_cpu(cpu, mask);
	}

	return true;
}

u32 mt_irq_get_pol(u32 irq)
{
	u32 reg;
	void __iomem *base = INT_POL_CTL0;

	if (irq < 32) {
		pr_err("Fail to set polarity of interrupt %d\n", irq);
		return 0;
	}

	reg = ((irq - 32)/32);

	/* if reg_len_pol0 != 0, means there is 2nd POL reg base,
	   compute the correct offset for polarity reg in 2nd POL reg */
	if ((reg_len_pol0 != 0) && (reg >= reg_len_pol0)) {
		if (!INT_POL_CTL1) {
			pr_err("MUST have 2nd INT_POL_CTRL\n");
			BUG_ON(1);
		}
		reg -= reg_len_pol0;
		base = INT_POL_CTL1;
	}

	return readl_relaxed(IOMEM(base + reg*4));
}
/*
 * mt_irq_mask_all: disable all interrupts
 * @mask: pointer to struct mtk_irq_mask for storing the original mask value.
 * Return 0 for success; return negative values for failure.
 * (This is ONLY used for the idle current measurement by the factory mode.)
 */
int mt_irq_mask_all(struct mtk_irq_mask *mask)
{
	void __iomem *dist_base;
	void __iomem *redist_base;

	dist_base = GIC_DIST_BASE;
	gic_populate_rdist(&redist_base);
	redist_base += SZ_64K;

	if (mask) {
		/* for SGI & PPI */
		mask->mask0 = readl((redist_base + GIC_DIST_ENABLE_SET));
		/* for SPI */
		mask->mask1 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x4));
		mask->mask2 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x8));
		mask->mask3 = readl((dist_base + GIC_DIST_ENABLE_SET + 0xc));
		mask->mask4 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x10));
		mask->mask5 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x14));
		mask->mask6 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x18));
		mask->mask7 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x1c));
		mask->mask8 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x20));
		mask->mask9 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x24));
		mask->mask10 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x28));
		mask->mask11 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x2c));
		mask->mask12 = readl((dist_base + GIC_DIST_ENABLE_SET + 0x30));

		/* for SGI & PPI */
		writel(0xFFFFFFFF, (redist_base + GIC_DIST_ENABLE_CLEAR));
		/* for SPI */
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x4));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x8));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0xC));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x10));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x14));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x18));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x1C));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x20));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x24));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x28));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x2c));
		writel(0xFFFFFFFF, (dist_base + GIC_DIST_ENABLE_CLEAR + 0x30));
		mb();

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
	void __iomem *dist_base;
	void __iomem *redist_base;

	dist_base = GIC_DIST_BASE;
	gic_populate_rdist(&redist_base);
	redist_base += SZ_64K;

	if (!mask)
		return -1;
	if (mask->header != IRQ_MASK_HEADER)
		return -1;
	if (mask->footer != IRQ_MASK_FOOTER)
		return -1;

	writel(mask->mask0, (redist_base + GIC_DIST_ENABLE_SET));
	writel(mask->mask1, (dist_base + GIC_DIST_ENABLE_SET + 0x4));
	writel(mask->mask2, (dist_base + GIC_DIST_ENABLE_SET + 0x8));
	writel(mask->mask3, (dist_base + GIC_DIST_ENABLE_SET + 0xc));
	writel(mask->mask4, (dist_base + GIC_DIST_ENABLE_SET + 0x10));
	writel(mask->mask5, (dist_base + GIC_DIST_ENABLE_SET + 0x14));
	writel(mask->mask6, (dist_base + GIC_DIST_ENABLE_SET + 0x18));
	writel(mask->mask7, (dist_base + GIC_DIST_ENABLE_SET + 0x1c));
	writel(mask->mask8, (dist_base + GIC_DIST_ENABLE_SET + 0x20));
	writel(mask->mask9, (dist_base + GIC_DIST_ENABLE_SET + 0x24));
	writel(mask->mask10, (dist_base + GIC_DIST_ENABLE_SET + 0x28));
	writel(mask->mask11, (dist_base + GIC_DIST_ENABLE_SET + 0x2c));
	writel(mask->mask12, (dist_base + GIC_DIST_ENABLE_SET + 0x30));
	mb();

	return 0;
}

u32 mt_irq_get_pending(unsigned int irq)
{
	void __iomem *base;
	u32 bit = 1 << (irq % 32);

	if (irq >= 32) {
		base = GIC_DIST_BASE;
	} else {
		gic_populate_rdist(&base);
		base += SZ_64K;
	}

	return (readl_relaxed(base + GIC_DIST_PENDING_SET + (irq/32)*4)&bit) ?
		1 : 0;
}

void mt_irq_set_pending(unsigned int irq)
{
	void __iomem *base;
	u32 bit = 1 << (irq % 32);

	if (irq >= 32) {
		base = GIC_DIST_BASE;
	} else {
		gic_populate_rdist(&base);
		base += SZ_64K;
	}

	writel(bit, base + GIC_DIST_PENDING_SET + (irq/32)*4);
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

	dist_base = GIC_DIST_BASE;

	if (irq < 16) {
		pr_err("Fail to enable interrupt %d\n", irq);
		return;
	}

	writel(mask, dist_base + GIC_DIST_ENABLE_SET + irq / 32 * 4);
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

	dist_base = GIC_DIST_BASE;

	if (irq < 16) {
		pr_err("Fail to enable interrupt %d\n", irq);
		return;
	}

	writel(mask, dist_base + GIC_DIST_ENABLE_CLEAR + irq / 32 * 4);
	mb();
}

char *mt_irq_dump_status_buf(int irq, char *buf)
{
	int rc;
	unsigned int result;
	char *ptr = buf;

	if (!ptr)
		return NULL;

	ptr += sprintf(ptr, "[mt gic dump] irq = %d\n", irq);
#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
	rc = mt_secure_call(MTK_SIP_KERNEL_GIC_DUMP, irq, 0, 0);
#else
	rc = -1;
#endif
	if (rc < 0) {
		ptr += sprintf(ptr, "[mt gic dump] not allowed to dump!\n");
		return ptr;
	}

	/* get mask */
	result = rc & 0x1;
	ptr += sprintf(ptr, "[mt gic dump] enable = %d\n", result);

	/* get group */
	result = (rc >> 1) & 0x1;
	ptr += sprintf(ptr, "[mt gic dump] group = %x (0x1:irq,0x0:fiq)\n",
			result);

	/* get priority */
	result = (rc >> 2) & 0xff;
	ptr += sprintf(ptr, "[mt gic dump] priority = %x\n", result);

	/* get sensitivity */
	result = (rc >> 10) & 0x1;
	ptr += sprintf(ptr, "[mt gic dump] sensitivity = %x ", result);
	ptr += sprintf(ptr, "(edge:0x1, level:0x0)\n");

	/* get pending status */
	result = (rc >> 11) & 0x1;
	ptr += sprintf(ptr, "[mt gic dump] pending = %x\n", result);

	/* get active status */
	result = (rc >> 12) & 0x1;
	ptr += sprintf(ptr, "[mt gic dump] active status = %x\n", result);

	/* get polarity */
	result = (rc >> 13) & 0x1;
	ptr += sprintf(ptr,
		"[mt gic dump] polarity = %x (0x0: high, 0x1:low)\n",
		result);

	/* get target cpu mask */
	result = (rc >> 14) & 0xffff;
	ptr += sprintf(ptr, "[mt gic dump] tartget cpu mask = 0x%x\n", result);

	return ptr;
}

void mt_irq_dump_status(int irq)
{
	char *buf = kmalloc(2048, GFP_ATOMIC);

	if (!buf)
		return;

	if (mt_irq_dump_status_buf(irq, buf))
		pr_warn("%s", buf);

	kfree(buf);
}
EXPORT_SYMBOL(mt_irq_dump_status);

static void _mt_set_pol_reg(void __iomem *add, u32 val)
{
	writel_relaxed(val, add);
}

void _mt_irq_set_polarity(unsigned int irq, unsigned int polarity)
{
	u32 offset, reg, value;
	void __iomem *base = INT_POL_CTL0;

	if (irq < 32) {
		pr_err("Fail to set polarity of interrupt %d\n", irq);
		return;
	}

	offset = irq%32;
	reg = ((irq - 32)/32);

	/* if reg_len_pol0 != 0, means there is 2nd POL reg base,
	   compute the correct offset for polarity reg in 2nd POL reg */
	if ((reg_len_pol0 != 0) && (reg >= reg_len_pol0)) {
		if (!INT_POL_CTL1) {
			pr_err("MUST have 2nd INT_POL_CTRL\n");
			BUG_ON(1);
		}
		reg -= reg_len_pol0;
		base = INT_POL_CTL1;
	}

	value = readl_relaxed(IOMEM(base + reg*4));
	if (polarity == 0) {
		/* active low */
		value |= (1 << offset);
	} else {
		/* active high */
		value &= ~(0x1 << offset);
	}
	/* some platforms has to write POL register in secure world */
	_mt_set_pol_reg(base + reg*4, value);
}

int __init mt_gic_ext_init(void)
{
	struct device_node *node;
#ifdef CONFIG_MTK_IRQ_NEW_DESIGN
	int i;
#endif

	node = of_find_compatible_node(NULL, NULL, "arm,gic-v3");
	if (!node) {
		pr_err("[gic_ext] find arm,gic-v3 node failed\n");
		return -EINVAL;
	}

	GIC_DIST_BASE = of_iomap(node, 0);
	if (IS_ERR(GIC_DIST_BASE))
		return -EINVAL;

	GIC_REDIST_BASE = of_iomap(node, 1);
	if (IS_ERR(GIC_REDIST_BASE))
		return -EINVAL;

	INT_POL_CTL0 = of_iomap(node, 2);
	if (IS_ERR(INT_POL_CTL0))
		return -EINVAL;

	/* if INT_POL_CTL1 get NULL,
	 * only means no extra polarity register,
	 * INT_POL_CTL0 is enough */
	INT_POL_CTL1 = of_iomap(node, 3);

	if (of_property_read_u32(node, "mediatek,reg_len_pol0",
				&reg_len_pol0))
		reg_len_pol0 = 0;

#ifdef CONFIG_MTK_IRQ_NEW_DESIGN
	for (i = 0; i <= CONFIG_NR_CPUS-1; ++i) {
		INIT_LIST_HEAD(&(irq_need_migrate_list[i].list));
		spin_lock_init(&(irq_need_migrate_list[i].lock));
	}

	if (of_property_read_u32(node, "mediatek,wdt_irq", &wdt_irq))
		wdt_irq = 0;
#endif

	pr_warn("### gic-v3 init done. ###\n");

	return 0;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek gicv3 extend Driver");
