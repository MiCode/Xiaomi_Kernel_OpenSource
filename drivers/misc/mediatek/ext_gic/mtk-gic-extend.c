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
#include <linux/irqchip/arm-gic.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/irqchip/mtk-gic-extend.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
#include <trustzone/kree/tz_irq.h>
#endif
#if defined(CONFIG_FIQ_GLUE)
#include <asm/fiq.h>
#include <asm/fiq_glue.h>
#endif

void __iomem *GIC_DIST_BASE;
void __iomem *GIC_CPU_BASE;
void __iomem *INT_POL_CTL0;

/**
 * get_hardware_irq - get a hardware interrupt num
 * @virq:	a linux irq
 *
 */
unsigned int get_hardware_irq(unsigned int virq)
{
	struct irq_desc *desc = irq_to_desc(virq);
	struct irq_data *data, *tdata = NULL;

	if (desc)
		tdata = data = &desc->irq_data;
	else
		return -EINVAL;

#ifdef	CONFIG_IRQ_DOMAIN_HIERARCHY
	for (data = data->parent_data; data; data = data->parent_data)
		tdata = data;
#endif
	if (tdata)
		return tdata->hwirq;
	else
		return -EINVAL;
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
		/*
		 * #if defined(CONFIG_FIQ_GLUE)
		 * local_fiq_disable();
		 * #endif
		 */

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

		/* add memory barrier */
		mb();

		/*
		 * #if defined(CONFIG_FIQ_GLUE)
		 * local_fiq_enable();
		 * #endif
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
	void __iomem *dist_base;

	dist_base = GIC_DIST_BASE;

	if (!mask)
		return -1;
	if (mask->header != IRQ_MASK_HEADER)
		return -1;
	if (mask->footer != IRQ_MASK_FOOTER)
		return -1;

	/*
	 * #if defined(CONFIG_FIQ_GLUE)
	 * local_fiq_disable();
	 * #endif
	 */

	writel(mask->mask0, (dist_base + GIC_DIST_ENABLE_SET));
	writel(mask->mask1, (dist_base + GIC_DIST_ENABLE_SET + 0x4));
	writel(mask->mask2, (dist_base + GIC_DIST_ENABLE_SET + 0x8));
	writel(mask->mask3, (dist_base + GIC_DIST_ENABLE_SET + 0xC));
	writel(mask->mask4, (dist_base + GIC_DIST_ENABLE_SET + 0x10));
	writel(mask->mask5, (dist_base + GIC_DIST_ENABLE_SET + 0x14));
	writel(mask->mask6, (dist_base + GIC_DIST_ENABLE_SET + 0x18));
	writel(mask->mask7, (dist_base + GIC_DIST_ENABLE_SET + 0x1C));
	writel(mask->mask8, (dist_base + GIC_DIST_ENABLE_SET + 0x20));

	/* add memory barrier */
	mb();


	/*
	 * #if defined(CONFIG_FIQ_GLUE)
	 * local_fiq_enable();
	 * #endif
	 */
	return 0;
}

/*
 * mt_irq_set_pending_for_sleep: pending an interrupt for the sleep
 * manager's use
 * @irq: interrupt id
 * (THIS IS ONLY FOR SLEEP FUNCTION USE. DO NOT USE IT YOURSELF!)
 */
void mt_irq_set_pending_for_sleep(unsigned int irq)
{
	void __iomem *dist_base;
	u32 mask = 1 << (irq % 32);

	dist_base = GIC_DIST_BASE;

	if (irq < 16) {
		pr_info("Fail to set a pending on interrupt %d\n", irq);
		return;
	}

	writel(mask, dist_base + GIC_DIST_PENDING_SET + irq / 32 * 4);
	pr_debug("irq:%d, 0x%p=0x%x\n", irq,
		  dist_base + GIC_DIST_PENDING_SET + irq / 32 * 4, mask);

	/* add memory barrier */
	mb();
}

u32 mt_irq_get_pending_hw(unsigned int irq)
{
	void __iomem *dist_base;
	u32 bit = 1 << (irq % 32);

	dist_base = GIC_DIST_BASE;

	return (readl_relaxed(dist_base + GIC_DIST_PENDING_SET + irq / 32 * 4)
				& bit) ? 1 : 0;
}

void mt_irq_set_pending_hw(unsigned int irq)
{
	void __iomem *dist_base;
	u32 bit = 1 << (irq % 32);

	dist_base = GIC_DIST_BASE;
	writel(bit, dist_base + GIC_DIST_PENDING_SET + irq / 32 * 4);
}

u32 mt_irq_get_pol_hw(u32 hwirq)
{
	u32 reg;
	void __iomem *base = INT_POL_CTL0;

	if (hwirq < 32) {
		pr_info("Fail to set polarity of interrupt %d\n", hwirq);
		return 0;
	}

	reg = ((hwirq - 32)/32);

	return readl_relaxed(base + reg*4);
}

u32 mt_irq_get_pending_vec(u32 start_irq)
{
	void __iomem *base = 0;
	u32 pending_vec = 0;
	u32 reg = start_irq/32;
	u32 LSB_num, MSB_num;
	u32 LSB_vec, MSB_vec;

	if (start_irq >= 32)
		base = GIC_DIST_BASE;
	else
		return -EINVAL;

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
					    reg*4);
	}

	return pending_vec;
}

/*
 * mt_irq_unmask_for_sleep: enable an interrupt for the sleep manager's use
 * @irq: interrupt id
 * (THIS IS ONLY FOR SLEEP FUNCTION USE. DO NOT USE IT YOURSELF!)
 */
void mt_irq_unmask_for_sleep(unsigned int virq)
{
	u32 mask;
	void __iomem *dist_base;
	unsigned int irq = get_hardware_irq(virq);

	if (irq < 0)
		return;

	mask = 1 << (irq % 32);
	dist_base = GIC_DIST_BASE;

	if (irq < 16) {
		pr_info("Fail to enable interrupt %d\n", irq);
		return;
	}

	writel(mask, dist_base + GIC_DIST_ENABLE_SET + irq / 32 * 4);

	/* add memory barrier */
	mb();
}

/*
 * mt_irq_mask_for_sleep: disable an interrupt for the sleep manager's use
 * @irq: interrupt id
 * (THIS IS ONLY FOR SLEEP FUNCTION USE. DO NOT USE IT YOURSELF!)
 */
void mt_irq_mask_for_sleep(unsigned int virq)
{
	u32 mask;
	void __iomem *dist_base;
	unsigned int irq = get_hardware_irq(virq);

	if (irq < 0)
		return;

	mask = 1 << (irq % 32);
	dist_base = GIC_DIST_BASE;

	if (irq < 16) {
		pr_info("Fail to enable interrupt %d\n", irq);
		return;
	}

	writel(mask, dist_base + GIC_DIST_ENABLE_CLEAR + irq / 32 * 4);

	/* add memory barrier */
	mb();
}
#if defined(CONFIG_FIQ_GLUE)

#ifdef NR_IRQS
#undef NR_IRQS
#define		NR_IRQS		224
#else
#define		NR_IRQS		224
#endif

#define CPU_BRINGUP_SGI		1
#define FIQ_DBG_SGI		14
#define NR_GIC_SGI		16
#define NR_GIC_PPI		16
#define GIC_PRIVATE_SIGNALS	32
#define MT_WDT_IRQ_ID		120

struct irq2fiq {
	int irq;
	int count;
	fiq_isr_handler handler;
	void *arg;
};

struct fiq_isr_log {
	int in_fiq_isr;
	int irq;
	int smp_call_cnt;
};

#define line_fiq(no)		(is_fiq_set[(no)/32] & (1<<((no)&31)))
#define line_is_fiq(no)		((no) < NR_IRQS && line_fiq(no))
#define line_set_fiq(no)	\
	do {			\
		if ((no) < NR_IRQS) \
			is_fiq_set[(no)/32] |= (1<<((no)&31)); \
	} while (0)

static struct irq2fiq irqs_to_fiq[] = {
	{.irq = MT_WDT_IRQ_ID,},
	{.irq = FIQ_SMP_CALL_SGI,},
};

static struct fiq_isr_log fiq_isr_logs[NR_CPUS];
static unsigned int is_fiq_set[(NR_IRQS + 31) / 32];

static int fiq_glued;
unsigned int irq_total_secondary_cpus;
static spinlock_t irq_lock;
/*
 * mt_irq_mask: disable an interrupt.
 * @data: irq_data
 */
static void mt_irq_mask(struct irq_data *data)
{
	const unsigned int irq = data->irq;
	u32 mask = 1 << (irq % 32);

	if (irq < NR_GIC_SGI) {
		/*Note: workaround for false alarm*/
		if (irq != FIQ_DBG_SGI)
			pr_info("Fail to disable interrupt %d\n", irq);
		return;
	}

	writel(mask, (void *)(GIC_DIST_BASE + GIC_DIST_ENABLE_CLEAR
	       + irq / 32 * 4));
	dsb();
}

/*
 * mt_irq_unmask: enable an interrupt.
 * @data: irq_data
 */
static void mt_irq_unmask(struct irq_data *data)
{
	const unsigned int irq = data->irq;
	u32 mask = 1 << (irq % 32);

	if (irq < NR_GIC_SGI) {
		/*Note: workaround for false alarm:*/
		if (irq != FIQ_DBG_SGI)
			pr_info("Fail to enable interrupt %d\n", irq);
		return;
	}

	writel(mask, (void *)(GIC_DIST_BASE + GIC_DIST_ENABLE_SET
	       + irq / 32 * 4));
	dsb();
}

/*
 * mt_irq_set_sens: set the interrupt sensitivity
 * @irq: interrupt id
 * @sens: sensitivity
 */
static void mt_irq_set_sens(unsigned int irq, unsigned int sens)
{
	unsigned long flags;
	u32 config;

	if (irq < (NR_GIC_SGI + NR_GIC_PPI)) {
		pr_info("Fail to set sensitivity of interrupt %d\n", irq);
		return;
	}

	spin_lock_irqsave(&irq_lock, flags);

	if (sens == MT_EDGE_SENSITIVE) {
		config = readl((void *)(GIC_DIST_BASE + GIC_DIST_CONFIG
				+ (irq / 16) * 4));
		config |= (0x2 << (irq % 16) * 2);
		writel(config, (void *)(GIC_DIST_BASE + GIC_DIST_CONFIG
			+ (irq / 16) * 4));
	} else {
		config = readl((void *)(GIC_DIST_BASE + GIC_DIST_CONFIG
				+ (irq / 16) * 4));
		config &= ~(0x2 << (irq % 16) * 2);
		writel(config, (void *)(GIC_DIST_BASE + GIC_DIST_CONFIG
			+ (irq / 16) * 4));
	}

	spin_unlock_irqrestore(&irq_lock, flags);

	dsb();
}

/*
 * mt_irq_set_polarity: set the interrupt polarity
 * @irq: interrupt id
 * @polarity: interrupt polarity
 */
static void mt_irq_set_polarity(unsigned int irq, unsigned int polarity)
{
	unsigned long flags;
	u32 offset, reg_index, value;

	if (irq < (NR_GIC_SGI + NR_GIC_PPI)) {
		pr_info("Fail to set polarity of interrupt %d\n", irq);
		return;
	}

	offset = (irq - GIC_PRIVATE_SIGNALS) & 0x1F;
	reg_index = (irq - GIC_PRIVATE_SIGNALS) >> 5;

	spin_lock_irqsave(&irq_lock, flags);

	if (polarity == 0) {
		/* active low */
		value = readl((void *)(INT_POL_CTL0 + (reg_index * 4)));
		value |= (1 << offset);
		writel(value, (INT_POL_CTL0 + (reg_index * 4)));
	} else {
		/* active high */
		value = readl((void *)(INT_POL_CTL0 + (reg_index * 4)));
		value &= ~(0x1 << offset);
		writel(value, INT_POL_CTL0 + (reg_index * 4));
	}
	dsb();
	spin_unlock_irqrestore(&irq_lock, flags);
}

/*
 * mt_irq_set_type: set interrupt type
 * @irq: interrupt id
 * @flow_type: interrupt type
 * Always return 0.
 */
static int mt_irq_set_type(struct irq_data *data, unsigned int flow_type)
{
	const unsigned int irq = data->irq;

	if (flow_type & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) {
		mt_irq_set_sens(irq, MT_EDGE_SENSITIVE);
		mt_irq_set_polarity(irq, (flow_type & IRQF_TRIGGER_FALLING)
					? 0 : 1);
		__irq_set_handler_locked(irq, handle_edge_irq);
	} else if (flow_type & (IRQF_TRIGGER_HIGH | IRQF_TRIGGER_LOW)) {
		mt_irq_set_sens(irq, MT_LEVEL_SENSITIVE);
		mt_irq_set_polarity(irq, (flow_type & IRQF_TRIGGER_LOW)
					? 0 : 1);
		__irq_set_handler_locked(irq, handle_level_irq);
	}

	return 0;
}

#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
static inline void __mt_enable_fiq(int irq)
{
	struct irq_data data;

	if (!line_is_fiq(irq)) {
		data.irq = irq;
		mt_irq_unmask(&data);
		return;
	}

	kree_enable_fiq(irq);
}

static inline void __mt_disable_fiq(int irq)
{
	struct irq_data data;

	if (!line_is_fiq(irq)) {
		data.irq = irq;
		mt_irq_mask(&data);
		return;
	}

	kree_disable_fiq(irq);
}

static int mt_irq_set_fiq(int irq, unsigned long irq_flags)
{
	struct irq_data data;
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&irq_lock, flags);
	ret = kree_set_fiq(irq, irq_flags);
	spin_unlock_irqrestore(&irq_lock, flags);
	if (!ret) {
		data.irq = irq;
		mt_irq_set_type(&data, irq_flags);
	}
	return ret;
}

static inline unsigned int mt_fiq_get_intack(void)
{
	return kree_fiq_get_intack();
}

static inline void mt_fiq_eoi(unsigned int iar)
{
	kree_fiq_eoi(iar);
}

static int irq_raise_softfiq(const struct cpumask *mask, unsigned int irq)
{
	if (!line_is_fiq(irq))
		return 0;

	kree_raise_softfiq(*cpus_addr(*mask), irq);
	return 1;
}

static int restore_for_fiq(struct notifier_block *nfb, unsigned long action,
				void *hcpu)
{
	/* Do nothing. TEE should recover GIC by itself */
	return NOTIFY_OK;
}
#else
static void __set_security(int irq);
static void __raise_priority(int irq);

static inline void __mt_enable_fiq(int irq)
{
	struct irq_data data;

	data.irq = irq;
	mt_irq_unmask(&data);
}

static inline void __mt_disable_fiq(int irq)
{
	struct irq_data data;

	data.irq = irq;
	mt_irq_mask(&data);
}

static int mt_irq_set_fiq(int irq, unsigned long irq_flags)
{
	struct irq_data data;

	__set_security(irq);
	__raise_priority(irq);
	data.irq = irq;
	mt_irq_set_type(&data, irq_flags);
	return 0;
}

static inline unsigned int mt_fiq_get_intack(void)
{
	return readl((void *)(GIC_CPU_BASE + GIC_CPU_INTACK));
}

static inline void mt_fiq_eoi(unsigned int iar)
{
	writel(iar, (void *)(GIC_CPU_BASE + GIC_CPU_EOI));
	dsb();
}

/* No TEE, just let kernel gic send it. */
static int irq_raise_softfiq(const struct cpumask *mask, unsigned int irq)
{
	return 0;
}

static void __set_security(int irq)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&irq_lock, flags);

	val = readl((void *)(GIC_ICDISR + 4 * (irq / 32)));
	val &= ~(1 << (irq % 32));
	writel(val, (void *)(GIC_ICDISR + 4 * (irq / 32)));

	spin_unlock_irqrestore(&irq_lock, flags);
}

static void __raise_priority(int irq)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&irq_lock, flags);

	val = readl((void *)(GIC_DIST_BASE + GIC_DIST_PRI + 4 * (irq / 4)));
	val &= ~(0xFF << ((irq % 4) * 8));
	val |= (0x50 << ((irq % 4) * 8));
	writel(val, (void *)(GIC_DIST_BASE + GIC_DIST_PRI + 4 * (irq / 4)));

	spin_unlock_irqrestore(&irq_lock, flags);
}

static int restore_for_fiq(struct notifier_block *nfb, unsigned long action,
				void *hcpu)
{
	int i;

	switch (action) {
	case CPU_STARTING:
		for (i = 0; i < ARRAY_SIZE(irqs_to_fiq); i++) {
			if ((irqs_to_fiq[i].irq < (NR_GIC_SGI + NR_GIC_PPI))
			    && (irqs_to_fiq[i].handler)) {
				__set_security(irqs_to_fiq[i].irq);
				__raise_priority(irqs_to_fiq[i].irq);
				dsb();
			}
		}
		break;

	default:
		break;
	}

	return NOTIFY_OK;
}
#endif				/* CONFIG_MTK_IN_HOUSE_TEE_SUPPORT */

/*
 * trigger_sw_irq: force to trigger a GIC SGI.
 * @irq: SGI number
 */
void trigger_sw_irq(int irq)
{
	if (irq < NR_GIC_SGI)
		writel((0x18000 | irq), (void *)(GIC_DIST_BASE
			+ GIC_DIST_SOFTINT));
}

/*
 * mt_disable_fiq: disable an interrupt which is directed to FIQ.
 * @irq: interrupt id
 * Return error code.
 * NoteXXX: Not allow to suspend due to FIQ context.
 */
int mt_disable_fiq(int irq)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(irqs_to_fiq); i++) {
		if (irqs_to_fiq[i].irq == irq) {
			__mt_disable_fiq(irq);
			return 0;
		}
	}

	return -1;
}

/*
 * mt_enable_fiq: enable an interrupt which is directed to FIQ.
 * @irq: interrupt id
 * Return error code.
 * NoteXXX: Not allow to suspend due to FIQ context.
 */
int mt_enable_fiq(int irq)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(irqs_to_fiq); i++) {
		if (irqs_to_fiq[i].irq == irq) {
			__mt_enable_fiq(irq);
			return 0;
		}
	}

	return -1;
}

/*
 * For gic_arch_extn, check and mask if it is FIQ.
 */
static void mt_fiq_mask_check(struct irq_data *d)
{
	if (!line_is_fiq(d->irq))
		return;

	__mt_disable_fiq(d->irq);
}

/*
 * For gic_arch_extn, check and unmask if it is FIQ.
 */
static void mt_fiq_unmask_check(struct irq_data *d)
{
	if (!line_is_fiq(d->irq))
		return;

	__mt_enable_fiq(d->irq);
}

/*
 * fiq_isr: FIQ handler.
 */
static void fiq_isr(struct fiq_glue_handler *h, void *regs, void *svc_sp)
{
	unsigned int iar, irq;
	int i;

	iar = mt_fiq_get_intack();
	irq = iar & 0x3FF;

	if (irq >= NR_IRQS)
		return;

	for (i = 0; i < ARRAY_SIZE(irqs_to_fiq); i++) {
		if (irqs_to_fiq[i].irq == irq) {
			if (irqs_to_fiq[i].handler) {
				irqs_to_fiq[i].count++;
				(irqs_to_fiq[i].handler) (irqs_to_fiq[i].arg,
								regs, svc_sp);
			} else {
				pr_info("Unexpected interrupt %d received!\n",
				       irq);
			}
			break;
		}
	}
	if (i == ARRAY_SIZE(irqs_to_fiq))
		pr_info("Interrupt %d triggers FIQ but it is not registered\n",
		       irq);

	mt_fiq_eoi(iar);
}

/*
 * get_fiq_isr_log: get fiq_isr_log for debugging.
 * @cpu: processor id
 * @log: pointer to the address of fiq_isr_log
 * @len: length of fiq_isr_log
 * Return 0 for success or error code for failure.
 */
int get_fiq_isr_log(int cpu, unsigned int *log, unsigned int *len)
{
	if (log)
		*log = (unsigned int)&(fiq_isr_logs[cpu]);

	if (len)
		*len = sizeof(struct fiq_isr_log);

	return 0;
}

static struct notifier_block fiq_notifier = {
	.notifier_call = restore_for_fiq,
};

static struct fiq_glue_handler fiq_handler = {
	.fiq = fiq_isr,
};

static int __init_fiq(void)
{
	int ret;

	spin_lock_init(&irq_lock);
	register_cpu_notifier(&fiq_notifier);
	irq_total_secondary_cpus = num_possible_cpus() - 1;

	/* Hook FIQ checker for mask/unmask. */
	/* So kernel enable_irq/disable_irq call will set FIQ correctly. */
	/* This is necessary for IRQ/FIQ suspend to work. */
	gic_arch_extn.irq_mask = mt_fiq_mask_check;
	gic_arch_extn.irq_unmask = mt_fiq_unmask_check;

	ret = fiq_glue_register_handler(&fiq_handler);
	if (ret)
		pr_info("fail to register fiq_glue_handler\n");
	else
		fiq_glued = 1;

	return ret;
}

/*
 * request_fiq: direct an interrupt to FIQ and register its handler.
 * @irq: interrupt id
 * @handler: FIQ handler
 * @irq_flags: IRQF_xxx
 * @arg: argument to the FIQ handler
 * Return error code.
 */
int request_fiq(int irq, fiq_isr_handler handler, unsigned long irq_flags,
		void *arg)
{
	int i;
	unsigned long flags;

	if (!fiq_glued)
		__init_fiq();

	for (i = 0; i < ARRAY_SIZE(irqs_to_fiq); i++) {
		spin_lock_irqsave(&irq_lock, flags);

		if (irqs_to_fiq[i].irq == irq) {
			irqs_to_fiq[i].handler = handler;
			irqs_to_fiq[i].arg = arg;

			line_set_fiq(irq);
			spin_unlock_irqrestore(&irq_lock, flags);
			if (mt_irq_set_fiq(irq, irq_flags))
				break;

			/* add memory barrier */
			mb();
			__mt_enable_fiq(irq);
			return 0;
		}
		spin_unlock_irqrestore(&irq_lock, flags);
	}
	return -1;
}


void irq_raise_softirq(const struct cpumask *mask, unsigned int irq)
{
	unsigned long map = *cpus_addr(*mask);
	int satt, cpu, cpu_bmask;
	u32 val;

	satt = 1 << 15;
	/*
	 * NoteXXX: CPU1 SGI is configured as secure as default.
	 *          Need to use the secure SGI 1 which is for waking up cpu1.
	 */
	if (irq == CPU_BRINGUP_SGI) {
		if (irq_total_secondary_cpus) {
			--irq_total_secondary_cpus;
			satt = 0;
		}
	}

	val = readl(IOMEM(GIC_DIST_BASE + GIC_DIST_IGROUP + 4 * (irq / 32)));
	if (!(val & (1 << (irq % 32)))) {   /*  secure interrupt? */
		satt = 0;
	}

	cpu = 0;
	cpu_bmask = 0;

#if defined(SPM_MCDI_FUNC)
	/*
	 * Processors cannot receive interrupts during power-down.
	 * Wait until the SPM checks status and returns.
	 */
	for_each_cpu(cpu, mask) {
		cpu_bmask |= 1 << cpu;
	}
	spm_check_core_status_before(cpu_bmask);
#endif

	if (irq_raise_softfiq(mask, irq) == 0) {
		/*
		 * Ensure that stores to Normal memory are visible to the
		 * other CPUs before issuing the IPI.
		 */
		dsb();
		writel(((map << 16) | satt | irq), (void *)(GIC_DIST_BASE
		       + GIC_DIST_SOFTINT));
		dsb();
	}

#if defined(SPM_MCDI_FUNC)
	spm_check_core_status_after(cpu_bmask);
#endif

}
#else
int get_fiq_isr_log(int cpu, unsigned int *log, unsigned int *len)
{
	return -ENODEV;
}

int request_fiq(int irq, fiq_isr_handler handler, unsigned long irq_flags,
		void *arg)
{
	return -ENODEV;
}

#endif				/* CONFIG_FIQ_GLUE */

static int __init mtk_gic_ext_init(void)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "arm,gic-400");
	if (!node) {
		node = of_find_compatible_node(NULL, NULL, "arm,cortex-a7-gic");
		if (!node) {
			pr_info("[gic_ext] find arm,gic node failed\n");
			return -EINVAL;
		}
	}

	GIC_DIST_BASE = of_iomap(node, 0);
	if (IS_ERR(GIC_DIST_BASE))
		return -EINVAL;

	GIC_CPU_BASE = of_iomap(node, 1);
	if (IS_ERR(GIC_CPU_BASE))
		return -EINVAL;
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6577-sysirq");
	if (!node) {
		pr_info("[gic_ext] find mediatek,mt6577-sysirq node failed\n");
		return -EINVAL;
	}

	INT_POL_CTL0 = of_iomap(node, 0);
	if (IS_ERR(INT_POL_CTL0))
		return -EINVAL;

	return 0;
}
arch_initcall(mtk_gic_ext_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek gic extend Driver");
MODULE_AUTHOR("Maoguang Meng <maoguang.meng@mediatek.com>");
