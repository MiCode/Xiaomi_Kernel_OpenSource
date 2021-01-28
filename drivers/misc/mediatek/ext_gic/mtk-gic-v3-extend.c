// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
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
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/sizes.h>
#include <linux/irqchip/arm-gic.h>
/* #include <linux/irqchip/arm-gic-v3.h> */
#include <linux/irqchip/mtk-gic-extend.h>
#include <linux/io.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/arm-smccc.h>
#ifdef CONFIG_CPU_PM
#include <linux/cpu_pm.h>
#endif
#ifdef CONFIG_PM_SLEEP
#include <linux/syscore_ops.h>
#endif

#define IOMEM(x)        ((void __force __iomem *)(x))
#define GICD_IROUTER                      0x6000
#define GICD_IROUTER_SPI_MODE_ANY	  (1U << 31)
/* for cirq use */
void __iomem *GIC_DIST_BASE;
void __iomem *INT_POL_CTL0;
void __iomem *INT_POL_CTL1;
void __iomem *MCUSYS_BASE_SWMODE;
static void __iomem *GIC_REDIST_BASE;
static u32 reg_len_pol0;

unsigned int __attribute__((weak)) irq_sw_mode_support(void)
{
	return 0;
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
		/* make sure distributor changes happen */
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
	/* make sure dist changes happen */
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
		pending_vec = readl_relaxed(base + GIC_DIST_PENDING_SET +
					    reg * 4);
	}

	return pending_vec;
}

#ifdef CONFIG_FAST_CIRQ_CLONE_FLUSH
u32 mt_irq_get_en_hw(unsigned int hwirq)
{
	void __iomem *base;
	u32 bit = 1 << (hwirq % 32);

	if (hwirq >= 32) {
		base = GIC_DIST_BASE + GIC_DIST_ENABLE_SET;
	} else {
		gic_populate_rdist(&base);
		base += SZ_64K;
		base = base + GIC_DIST_ENABLE_SET;
	}

	return (readl_relaxed(base + (hwirq/32)*4) & bit) ?
		1 : 0;
}
#endif

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

	hwirq = virq_to_hwirq(virq);
	dist_base = GIC_DIST_BASE;
	mask = 1 << (hwirq % 32);

	if (hwirq < 16) {
		pr_notice("Fail to enable interrupt %d\n", hwirq);
		return;
	}

	writel(mask, dist_base + GIC_DIST_ENABLE_SET + hwirq / 32 * 4);
	/* make sure enable happen */
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
	u32 mask;

	mask = 1 << (hwirq % 32);
	dist_base = GIC_DIST_BASE;

	if (hwirq < 16) {
		pr_notice("Fail to enable interrupt %d\n", hwirq);
		return;
	}

	writel(mask, dist_base + GIC_DIST_ENABLE_SET + hwirq / 32 * 4);
	/* make sure enable happen */
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
	u32 mask;

	irq = virq_to_hwirq(irq);
	mask = 1 << (irq % 32);
	dist_base = GIC_DIST_BASE;

	if (irq < 16) {
		pr_notice("Fail to enable interrupt %d\n", irq);
		return;
	}

	writel(mask, dist_base + GIC_DIST_ENABLE_CLEAR + irq / 32 * 4);
	/* make sure clr happen */
	mb();
}

char *mt_irq_dump_status_buf(int irq, char *buf)
{
	int rc;
	struct arm_smccc_res res;
	unsigned int result;
	char *ptr = buf;

	irq = virq_to_hwirq(irq);

	if (!ptr)
		return NULL;

	ptr += sprintf(ptr, "[mt gic dump] irq = %d\n", irq);

#if defined(CONFIG_ARM64) || defined(CONFIG_ARM_PSCI)
	arm_smccc_smc(MTK_SIP_KERNEL_GIC_DUMP, irq, 0, 0, 0, 0, 0, 0, &res);
#endif

	rc = res.a0;

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
		pr_notice("%s", buf);

	kfree(buf);
}
EXPORT_SYMBOL(mt_irq_dump_status);

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

#define GIC_INT_MASK (MCUSYS_BASE_SWMODE + 0x5e8)
#define GIC500_ACTIVE_SEL_SHIFT 3
#define GIC500_ACTIVE_SEL_MASK (0x7 << GIC500_ACTIVE_SEL_SHIFT)
#define GIC500_ACTIVE_CPU_SHIFT 16
#define GIC500_ACTIVE_CPU_MASK (0xff << GIC500_ACTIVE_CPU_SHIFT)
static spinlock_t domain_lock;
int print_en;

int add_cpu_to_prefer_schedule_domain(unsigned long cpu)
{
	unsigned long domain;

	if (irq_sw_mode_support() != 1)
		return 0;

	spin_lock(&domain_lock);
	domain = ioread32(GIC_INT_MASK);
	domain = domain | (1 << (cpu + GIC500_ACTIVE_CPU_SHIFT));
	iowrite32(domain, GIC_INT_MASK);
	spin_unlock(&domain_lock);
	return 0;
}

int remove_cpu_from_prefer_schedule_domain(unsigned long cpu)
{
	unsigned long domain;

	if (irq_sw_mode_support() != 1)
		return 0;

	spin_lock(&domain_lock);
	domain = ioread32(GIC_INT_MASK);
	domain = domain & ~(1 << (cpu + GIC500_ACTIVE_CPU_SHIFT));
	iowrite32(domain, GIC_INT_MASK);
	spin_unlock(&domain_lock);
	return 0;
}

#ifdef CONFIG_CPU_PM
static int gic_sched_pm_notifier(struct notifier_block *self,
			       unsigned long cmd, void *v)
{
	unsigned int cur_cpu = smp_processor_id();

	if (cmd == CPU_PM_EXIT)
		add_cpu_to_prefer_schedule_domain(cur_cpu);
	else if (cmd == CPU_PM_ENTER)
		remove_cpu_from_prefer_schedule_domain(cur_cpu);

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

void irq_sw_mode_init(void)
{
	struct device_node *node;

	if (irq_sw_mode_support() != 1) {
		pr_notice("### IRQ SW mode not support ###\n");
		return;
	}
	node = of_find_compatible_node(NULL, NULL, "mediatek,mcucfg");
	MCUSYS_BASE_SWMODE = of_iomap(node, 0);
	spin_lock_init(&domain_lock);
	gic_sched_pm_init();
}

int __init mt_gic_ext_init(void)
{
	struct device_node *node;

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

	irq_sw_mode_init();
	pr_notice("### gic-v3 init done. ###\n");

	return 0;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek gicv3 extend Driver");
