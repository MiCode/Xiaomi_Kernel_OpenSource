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
#include <mt-plat/mtk_secure_api.h>
#ifdef CONFIG_CPU_PM
#include <linux/cpu_pm.h>
#endif
#ifdef CONFIG_PM_SLEEP
#include <linux/syscore_ops.h>
#endif

#define IOMEM(x)	((void __force __iomem *)(x))
/* for cirq use */
void __iomem *GIC_DIST_BASE;
void __iomem *INT_POL_CTL0;
void __iomem *INT_POL_CTL1;
#ifndef CONFIG_MTK_INDIRECT_ACCESS
void __iomem *INT_MSK_CTL0;
#else
void __iomem *INDIRECT_ACCESS_BASE;
void __iomem *INDIRECT_ACCESS_EN_BASE;
#endif
static void __iomem *GIC_REDIST_BASE;
void __iomem *MCUSYS_BASE;
static u32 reg_len_pol0;

__weak unsigned int irq_mask_mode_support(void)
{
	return 1;
}

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


#ifdef CONFIG_FAST_CIRQ_CLONE_FLUSH
void __iomem *get_dist_base(void)
{
	return GIC_DIST_BASE;
}
#endif

static unsigned int gicd_read_iidr(void __iomem *gicd_base)
{
	return readl(gicd_base + GICD_IIDR);
}

static int gic_populate_rdist(void __iomem **rdist_base)
{
	int cpu = smp_processor_id();

	*rdist_base = GIC_REDIST_BASE + cpu*SZ_64K*2;

	return 0;
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
		 * it should be targted to specific cpu only
		 */
		cluster = (routing_val&0xff00)>>8;
		cpu = routing_val&0xff;

		/* assume 1 cluster contain 4 cpu in little,
		 * and only the last cluster can contain less than 4 cpu
		 */
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

u32 mt_irq_get_pol_hw(u32 hwirq)
{
	u32 reg;
	void __iomem *base = INT_POL_CTL0;

	if (hwirq < 32) {
		pr_notice("Fail to set polarity of interrupt %d\n", hwirq);
		return 0;
	}

	reg = ((hwirq - 32)/32);

	/* if reg_len_pol0 != 0, means there is 2nd POL reg base,
	 * compute the correct offset for polarity reg in 2nd POL reg
	 */
	if ((reg_len_pol0 != 0) && (reg >= reg_len_pol0)) {
		if (!INT_POL_CTL1) {
			pr_notice("MUST have 2nd INT_POL_CTRL\n");
			/* is a bug */
			WARN_ON(1);
		}
		reg -= reg_len_pol0;
		base = INT_POL_CTL1;
	}

	return readl_relaxed(IOMEM(base + reg*4));
}

u32 mt_irq_get_pol(u32 irq)
{
	u32 hwirq = virq_to_hwirq(irq);

	return mt_irq_get_pol_hw(hwirq);
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

	dist_base = GIC_DIST_BASE;

	if (mask) {
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
		/* make the writes prior to the writes behind */
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

	dist_base = GIC_DIST_BASE;

	if (!mask)
		return -1;
	if (mask->header != IRQ_MASK_HEADER)
		return -1;
	if (mask->footer != IRQ_MASK_FOOTER)
		return -1;

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


	/* make the writes prior to the writes behind */
	mb();

	return 0;
}

u32 mt_irq_get_pending_hw(unsigned int hwirq)
{
	void __iomem *base;
	u32 bit = 1 << (hwirq % 32);

	if (hwirq >= 32) {
		base = GIC_DIST_BASE;
	} else {
		gic_populate_rdist(&base);
		base += SZ_64K;
	}

	return (readl_relaxed(base + GIC_DIST_PENDING_SET + (hwirq/32)*4)&bit) ?
		1 : 0;
}

u32 mt_irq_get_pending(unsigned int irq)
{
	unsigned int hwirq = virq_to_hwirq(irq);

	return mt_irq_get_pending_hw(hwirq);
}

u32 mt_irq_get_pending_vec(u32 start_irq)
{
	void __iomem *base = 0;
	u32 pending_vec = 0;
	u32 reg = start_irq/32;
	u32 LSB_num, MSB_num;
	u32 LSB_vec, MSB_vec;

	if (start_irq >= 32) {
		base = GIC_DIST_BASE;
	} else {
		gic_populate_rdist(&base);
		base += SZ_64K;
	}

	/* if start_irq is not aligned 32, do some assembling */
	MSB_num = start_irq%32;
	if (MSB_num != 0) {
		LSB_num = 32 - MSB_num;
		LSB_vec = readl_relaxed(base + GIC_DIST_PENDING_SET + reg*4)
					>>MSB_num;
		MSB_vec = readl_relaxed(base + GIC_DIST_PENDING_SET + (reg+1)*4)
					<<LSB_num;
		pending_vec = MSB_vec | LSB_vec;
	} else {
		pending_vec = readl_relaxed
				(base + GIC_DIST_PENDING_SET + reg*4);
	}

	return pending_vec;
}

void mt_irq_set_pending_hw(unsigned int hwirq)
{
	void __iomem *base;
	u32 bit = 1 << (hwirq % 32);

	if (hwirq >= 32) {
		base = GIC_DIST_BASE;
	} else {
		gic_populate_rdist(&base);
		base += SZ_64K;
	}

	writel(bit, base + GIC_DIST_PENDING_SET + (hwirq/32)*4);
}

#ifdef CONFIG_FAST_CIRQ_CLONE_FLUSH
u32 mt_irq_get_en_hw(unsigned int hwirq)
{
	void __iomem *base;
	u32 bit = 1 << (hwirq % 32);

	if (hwirq >= 32) {
		base = GIC_DIST_BASE;
	} else {
		gic_populate_rdist(&base);
		base += SZ_64K;
	}

	return (readl_relaxed(base + GIC_DIST_ENABLE_SET
			+ (hwirq/32)*4) & bit) ? 1 : 0;
}
#endif

void mt_irq_set_pending(unsigned int irq)
{
	unsigned int hwirq = virq_to_hwirq(irq);

	mt_irq_set_pending_hw(hwirq);
}

void mt_irq_unmask_for_sleep_ex(unsigned int virq)
{
	void __iomem *dist_base;
	u32 mask;
	unsigned int hwirq;
#ifdef CONFIG_MTK_INDIRECT_ACCESS
	unsigned int value;
#endif

	hwirq = virq_to_hwirq(virq);
	dist_base = GIC_DIST_BASE;
	mask = 1 << (hwirq % 32);

	if (hwirq < 16) {
		pr_notice("Fail to enable interrupt %d\n", hwirq);
		return;
	}

	writel(mask, dist_base + GIC_DIST_ENABLE_SET + hwirq / 32 * 4);


#ifndef CONFIG_MTK_INDIRECT_ACCESS
	if (irq_mask_mode_support())
		if (INT_MSK_CTL0 && hwirq >= 32)
			writel(~mask, INT_MSK_CTL0 + hwirq / 32 * 4);
#else
	if (INDIRECT_ACCESS_BASE && hwirq >= 32) {
		/* set unmask */
		value = 0;
		/* select spi id */
		value |= (hwirq << 16);
		/* select mask control */
		value |= (1 << 30);
		writel(value, INDIRECT_ACCESS_BASE);
	}
#endif

	/* make the writes prior to the writes behind */
	mb();
}

/*
 * mt_irq_unmask_for_sleep: enable an interrupt for the sleep manager's use
 * @irq: interrupt id
 * (THIS IS ONLY FOR SLEEP FUNCTION USE. DO NOT USE IT YOURSELF!)
 */
void mt_irq_unmask_for_sleep(unsigned int hwirq)
{
	void __iomem *dist_base;
	u32 mask, value;

	mask = 1 << (hwirq % 32);
	dist_base = GIC_DIST_BASE;

	if (hwirq < 16) {
		pr_notice("Fail to enable interrupt %d\n", hwirq);
		return;
	}

	writel(mask, dist_base + GIC_DIST_ENABLE_SET + hwirq / 32 * 4);

#ifndef CONFIG_MTK_INDIRECT_ACCESS
	if (irq_mask_mode_support()) {
		if (INT_MSK_CTL0 && hwirq >= 32) {
			value = ~mask & readl(INT_MSK_CTL0 + hwirq / 32 * 4);
			writel(value, INT_MSK_CTL0 + hwirq / 32 * 4);
		}
	}
#else
	if (INDIRECT_ACCESS_BASE && hwirq >= 32) {
		/* set unmask */
		value = 0;
		/* select spi id */
		value |= (hwirq << 16);
		/* select mask control */
		value |= (1 << 30);
		writel(value, INDIRECT_ACCESS_BASE);
	}
#endif
	/* make the writes prior to the writes behind */
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
	u32 mask, value;

	irq = virq_to_hwirq(irq);
	mask = 1 << (irq % 32);
	dist_base = GIC_DIST_BASE;

	if (irq < 16) {
		pr_notice("Fail to enable interrupt %d\n", irq);
		return;
	}

	writel(mask, dist_base + GIC_DIST_ENABLE_CLEAR + irq / 32 * 4);

#ifndef CONFIG_MTK_INDIRECT_ACCESS
	if (irq_mask_mode_support()) {
		if (INT_MSK_CTL0 && irq >= 32) {
			value = mask | readl(INT_MSK_CTL0 + irq / 32 * 4);
			writel(value, INT_MSK_CTL0 + irq / 32 * 4);
		}
	}
#else
	if (INDIRECT_ACCESS_BASE && irq >= 32) {
		/* set unmask */
		value = 1;
		/* select spi id */
		value |= (irq << 16);
		/* select mask control */
		value |= (1 << 30);
		writel(value, INDIRECT_ACCESS_BASE);
	}
#endif
	/* make the writes prior to the writes behind */
	mb();
}

char *mt_irq_dump_status_buf(int irq, char *buf)
{
	unsigned long rc;
	unsigned int result;
	char *ptr = buf;
	unsigned int is_gic600;

	irq = virq_to_hwirq(irq);

	if (!ptr)
		return NULL;

	ptr += sprintf(ptr, "[mt gic dump] irq = %d\n", irq);

	rc = mt_secure_call(MTK_SIP_KERNEL_GIC_DUMP, irq, 0, 0, 0);


	result = gicd_read_iidr(GIC_DIST_BASE);
	is_gic600 =
		((result >> GICD_V3_IIDR_PROD_ID) == GICD_V3_IIDR_GIC600) ? 1:0;

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

	/* get irq mask from mcusys */
	if (is_gic600 == 1) {
		result = (rc >> 14) & 0x1;
		ptr += sprintf(ptr,
			"[mt gic dump] irq_mask = %x (0x0: unmask, 0x1:mask)\n",
			result);
	}

	/* get target cpu mask */
	if (is_gic600 == 1)
		result = (rc >> 15) & 0xffff;
	else
		result = (rc >> 14) & 0xffff;

	ptr += sprintf(ptr, "[mt gic dump] tartget cpu mask = 0x%x\n", result);

	return ptr;
}

int mt_irq_dump_cpu(int irq)
{
	unsigned long rc;
	unsigned long result;

	irq = virq_to_hwirq(irq);

	rc = mt_secure_call(MTK_SIP_KERNEL_GIC_DUMP, irq, 0, 0, 0);

	/* get target cpu mask */
	result = (rc >> 14) & 0xffff;

	return (int)(find_first_bit((unsigned long *)&result, 16));
}

void mt_irq_dump_status(int irq)
{
	char *buf = kmalloc(2048, GFP_ATOMIC);

	if (!buf)
		return;

	if (mt_irq_dump_status_buf(irq, buf))
		pr_notice("%s", buf);

	kfree(buf);
}
EXPORT_SYMBOL(mt_irq_dump_status);

#ifndef CONFIG_MTK_SYSIRQ
static void _mt_set_pol_reg(void __iomem *add, u32 val)
{
	writel_relaxed(val, add);
}
void _mt_irq_set_polarity(unsigned int hwirq, unsigned int polarity)
{
	u32 offset, reg, value;
	void __iomem *base = INT_POL_CTL0;

	if (hwirq < 32) {
		pr_notice("Fail to set polarity of interrupt %d\n", hwirq);
		return;
	}

	offset = hwirq%32;
	reg = ((hwirq - 32)/32);

	/* if reg_len_pol0 != 0, means there is 2nd POL reg base,
	 * compute the correct offset for polarity reg in 2nd POL reg
	 */
	if ((reg_len_pol0 != 0) && (reg >= reg_len_pol0)) {
		if (!INT_POL_CTL1) {
			pr_notice("MUST have 2nd INT_POL_CTRL\n");
			/* is a bug */
			WARN_ON(1);
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
#endif

#define GIC_INT_MASK (MCUSYS_BASE + 0x5e8)
#define GIC500_ACTIVE_SEL_SHIFT 3
#define GIC500_ACTIVE_SEL_MASK (0x7 << GIC500_ACTIVE_SEL_SHIFT)
#define GIC500_ACTIVE_CPU_SHIFT 16
#define GIC500_ACTIVE_CPU_MASK (0xff << GIC500_ACTIVE_CPU_SHIFT)
static spinlock_t domain_lock;

int add_cpu_to_prefer_schedule_domain(unsigned long cpu)
{
	unsigned long domain;
	unsigned long flag;

	spin_lock_irqsave(&domain_lock, flag);
	domain = ioread32(GIC_INT_MASK);
	domain = domain | (1 << (cpu + GIC500_ACTIVE_CPU_SHIFT));
	iowrite32(domain, GIC_INT_MASK);
	spin_unlock_irqrestore(&domain_lock, flag);
	return 0;
}

int remove_cpu_from_prefer_schedule_domain(unsigned long cpu)
{
	unsigned long domain;
	unsigned long flag;

	spin_lock_irqsave(&domain_lock, flag);
	domain = ioread32(GIC_INT_MASK);
	domain = domain & ~(1 << (cpu + GIC500_ACTIVE_CPU_SHIFT));
	iowrite32(domain, GIC_INT_MASK);
	spin_unlock_irqrestore(&domain_lock, flag);
	return 0;
}

#ifdef CONFIG_CPU_PM
static int gic_sched_pm_notifier(struct notifier_block *self,
			       unsigned long cmd, void *v)
{
	unsigned int cur_cpu = smp_processor_id();

	if (cmd == CPU_PM_EXIT)
		remove_cpu_from_prefer_schedule_domain(cur_cpu);
	else if (cmd == CPU_PM_ENTER)
		add_cpu_to_prefer_schedule_domain(cur_cpu);

	return NOTIFY_OK;
}

static struct notifier_block gic_sched_pm_notifier_block = {
	.notifier_call = gic_sched_pm_notifier,
};

static void gic_sched_pm_init(void)
{
	cpu_pm_register_notifier(&gic_sched_pm_notifier_block);
}

#else
static inline void gic_cpu_pm_init(void) { }
#endif /* CONFIG_CPU_PM */

#ifdef CONFIG_HOTPLUG_CPU
static int gic_sched_hotplug_callback(struct notifier_block *nfb,
				      unsigned long action, void *hcpu)
{
	switch (action) {
	case CPU_ONLINE:
		add_cpu_to_prefer_schedule_domain((unsigned long)hcpu);
		break;
	case CPU_DOWN_PREPARE:
		remove_cpu_from_prefer_schedule_domain((unsigned long)hcpu);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

struct notifier_block gic_sched_nfb = {
	.notifier_call = gic_sched_hotplug_callback
};

static void gic_sched_hoplug_init(void)
{
	register_cpu_notifier(&gic_sched_nfb);
}
#else
static void gic_sched_hoplug_init(void){};
#endif

int __init mt_gic_ext_init(void)
{
	struct device_node *node;
#ifndef CONFIG_MTK_INDIRECT_ACCESS
	const char *need_unmask_str = NULL;
#else
	const char *enable_indirect_str = NULL;
#endif

	node = of_find_compatible_node(NULL, NULL, "arm,gic-v3");
	if (!node) {
		pr_notice("[gic_ext] find arm,gic-v3 node failed\n");
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
	 * INT_POL_CTL0 is enough
	 */
	INT_POL_CTL1 = of_iomap(node, 3);

	if (of_property_read_u32(node, "mediatek,reg_len_pol0",
				&reg_len_pol0))
		reg_len_pol0 = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mcucfg");
	MCUSYS_BASE = of_iomap(node, 0);


	spin_lock_init(&domain_lock);
	gic_sched_pm_init();
	gic_sched_hoplug_init();

	/* XXX */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6577-sysirq");
	if (!node)
		return -EINVAL;

#ifndef CONFIG_MTK_INDIRECT_ACCESS
	INT_MSK_CTL0 = NULL;
	if (!of_property_read_string(node,
				"need_unmask", &need_unmask_str)) {
		if (need_unmask_str && !strncmp(need_unmask_str, "yes", 3)) {
			unsigned int tmp_base, offset;

			if (of_property_read_u32(node,
						"mask_base", &tmp_base))
				return -EINVAL;
			if (of_property_read_u32(node,
						"mask_offset", &offset))
				return -EINVAL;

			INT_MSK_CTL0 = ioremap(tmp_base, offset);
			if (!INT_MSK_CTL0)
				return -EINVAL;
		}
	}
#else
	INDIRECT_ACCESS_BASE = NULL;
	if (!of_property_read_string(node,
				"enable_indirect", &enable_indirect_str)) {
		if (enable_indirect_str &&
				!strncmp(enable_indirect_str, "yes", 3)) {
			unsigned int indirect_base, en_base;

			if (of_property_read_u32(node,
					"indirect_base", &indirect_base))
				return -EINVAL;
			if (of_property_read_u32(node,
					"indirect_en_base", &en_base))
				return -EINVAL;

			pr_debug("[%s] indirect : 0x%x, indirect_en : 0x%x\n",
					__func__, indirect_base, en_base);

			INDIRECT_ACCESS_BASE = ioremap(indirect_base, 0x4);
			INDIRECT_ACCESS_EN_BASE = ioremap(en_base, 0x4);

			if (!INDIRECT_ACCESS_BASE || !INDIRECT_ACCESS_EN_BASE)
				return -EINVAL;
		}

	} else {
		pr_notice("[%s]: indirect access not specified\n", __func__);
		return -EINVAL;
	}

#endif
	pr_notice("### gic-v3 init done. ###\n");

	return 0;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek gicv3 extend Driver");
