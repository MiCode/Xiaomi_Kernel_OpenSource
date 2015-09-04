/*
 * Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "msm_cache_erp64: " fmt

#include <linux/printk.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/cpu.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/cpu_pm.h>
#include <linux/smp.h>

#include <soc/qcom/kryo-l2-accessors.h>

/* Instruction cache */
#define ICECR_EL1			S3_1_c11_c1_0
#define	ICECR_IRQ_EN			(BIT(1) | BIT(3) | BIT(5) | BIT(7))
#define ICESR_EL1			S3_1_c11_c1_1
#define	ICESR_BIT_L1DPE			BIT(3)
#define	ICESR_BIT_L1TPE			BIT(2)
#define	ICESR_BIT_L0DPE			BIT(1)
#define	ICESR_BIT_L0TPE			BIT(0)
#define ICESYNR0_EL1			S3_1_c11_c1_3
#define ICESYNR1_EL1			S3_1_c11_c1_4
#define ICEAR0_EL1			S3_1_c11_c1_5
#define ICEAR1_EL1			S3_1_c11_c1_6
#define ICESRS_EL1			S3_1_c11_c1_2

/* Data cache */
#define DCECR_EL1			S3_1_c11_c5_0
#define	DCECR_IRQ_EN			(BIT(1) | BIT(3) | BIT(5) | BIT(7) | \
					 BIT(9))
#define DCESR_EL1			S3_1_c11_c5_1
#define	DCESR_BIT_S1FTLBDPE		BIT(4)
#define	DCESR_BIT_S1FTLBTPE		BIT(3)
#define	DCESR_BIT_L1DPE			BIT(2)
#define	DCESR_BIT_L1PTPE		BIT(1)
#define	DCESR_BIT_L1VTPE		BIT(0)
#define DCESYNR0_EL1			S3_1_c11_c5_3
#define DCESYNR1_EL1			S3_1_c11_c5_4
#define DCESRS_EL1			S3_1_c11_c5_2
#define DCEAR0_EL1			S3_1_c11_c5_5
#define DCEAR1_EL1			S3_1_c11_c5_6

/* L2 cache */
#define L2CPUSRSELR_EL1I		S3_3_c15_c0_6
#define L2CPUSRDR_EL1			S3_3_c15_c0_7
#define L2ECR0_IA			0x200
#define	L2ECR0_IRQ_EN			(BIT(1) | BIT(3) | BIT(6) | BIT(9) | \
					BIT(11) | BIT(13) | BIT(16) | \
					BIT(19) | BIT(21) | BIT(23) | \
					BIT(26) | BIT(29))

#define L2ECR1_IA			0x201
#define	L2ECR1_IRQ_EN			(BIT(1) | BIT(3) | BIT(6) | BIT(9) | \
					BIT(11) | BIT(13) | BIT(16) | \
					BIT(19) | BIT(21) | BIT(23) | BIT(29))
#define L2ECR2_IA			0x202
#define L2ECR2_IRQ_EN_MASK		0x3FFFFFF
#define L2ECR2_IRQ_EN			(BIT(1) | BIT(3) | BIT(6) | BIT(9) | \
					BIT(12) | BIT(15) | BIT(17) | \
					BIT(19) | BIT(22) | BIT(25))
#define L2ESR0_IA			0x204
#define L2ESR0_MASK			0x00FFFFFF
#define L2ESR0_CE			((BIT(0) | BIT(1) | BIT(2) | BIT(3) | \
					BIT(4) | BIT(5) | BIT(12) | BIT(13) | \
					BIT(14) | BIT(15) | BIT(16) | BIT(17)) \
					& L2ESR0_MASK)
#define L2ESR0_UE			(~L2ESR0_CE & L2ESR0_MASK)
#define L2ESRS0_IA			0x205
#define L2ESR1_IA			0x206
#define L2ESR1_MASK			0x80FFFBFF
#define L2ESRS1_IA			0x207
#define L2ESYNR0_IA			0x208
#define L2ESYNR1_IA			0x209
#define L2ESYNR2_IA			0x20A
#define L2ESYNR3_IA			0x20B
#define L2ESYNR4_IA			0x20C
#define L2EAR0_IA			0x20E
#define L2EAR1_IA			0x20F

#define L3_QLL_HML3_FIRA		0x3000
#define L3_QLL_HML3_FIRA_CE		(BIT(1) | BIT(3) | BIT(5))
#define L3_QLL_HML3_FIRA_UE		(BIT(2) | BIT(4) | BIT(6))
#define L3_QLL_HML3_FIRAC		0x3008
#define L3_QLL_HML3_FIRAS		0x3010
#define L3_QLL_HML3_FIRAT0C		0x3020
#define L3_QLL_HML3_FIRAT0C_IRQ_EN	0xFFFFFFFF
#define L3_QLL_HML3_FIRAT1C		0x3024
#define L3_QLL_HML3_FIRAT1S		0x302C
#define L3_QLL_HML3_FIRAT1S_IRQ_EN	0x01EFC8FE
#define L3_QLL_HML3_FIRSYNA		0x3100
#define L3_QLL_HML3_FIRSYNB		0x3104
#define L3_QLL_HML3_FIRSYNC		0x3108
#define L3_QLL_HML3_FIRSYND		0x310C

#define M4M_ERR_STATUS			0x10000
#define M4M_ERR_STATUS_MASK		0x1FF
#define M4M_ERR_Q22SIB_RET_DEC_ERR	(BIT(7))
#define M4M_ERR_Q22SIB_RET_SLV_ERR	(BIT(6))
#define M4M_ERR_CLR			0x10008
#define M4M_INT_CTRL			0x10010
#define M4M_INT_CTRL_IRQ_EN		0x1FF
#define M4M_ERR_CTRL			0x10018
#define M4M_ERR_INJ			0x10020
#define M4M_ERR_CAP_0			0x10030
#define M4M_ERR_CAP_1			0x10038
#define M4M_ERR_CAP_2			0x10040
#define M4M_ERR_CAP_3			0x10048

#define AFFINITY_LEVEL_L3		3

#ifdef CONFIG_MSM_CACHE_M4M_ERP64_PANIC_ON_CE
static bool __read_mostly panic_on_ce = true;
#else
static bool __read_mostly panic_on_ce;
#endif

#ifdef CONFIG_MSM_CACHE_M4M_ERP64_PANIC_ON_UE
static bool __read_mostly panic_on_ue = true;
#else
static bool __read_mostly panic_on_ue;
#endif

module_param(panic_on_ce, bool, false);
module_param(panic_on_ue, bool, false);

static void __iomem *hml3_base;
static void __iomem *m4m_base;

enum erp_irq_index { IRQ_L1, IRQ_L2_INFO0, IRQ_L2_INFO1, IRQ_L2_ERR0,
		     IRQ_L2_ERR1, IRQ_L3, IRQ_M4M, IRQ_MAX };
static const char * const erp_irq_names[] = {
	"l1_irq", "l2_irq_info_0", "l2_irq_info_1", "l2_irq_err_0",
	"l2_irq_err_1", "l3_irq", "m4m_irq"
};
static int erp_irqs[IRQ_MAX];

struct msm_l1_err_stats {
	/* nothing */
};

static DEFINE_PER_CPU(struct msm_l1_err_stats, msm_l1_erp_stats);
static DEFINE_PER_CPU(struct call_single_data, handler_csd);

#define erp_mrs(reg) ({							\
	u64 __val;							\
	asm volatile("mrs %0, " __stringify(reg) : "=r" (__val));	\
	__val;								\
})

#define erp_msr(reg, val) {					    \
	asm volatile("msr " __stringify(reg) ", %0" : : "r" (val)); \
}

static void msm_erp_show_icache_error(void)
{
	u64 icesr;
	int cpu = raw_smp_processor_id();

	icesr = erp_mrs(ICESR_EL1);
	if (!(icesr & (ICESR_BIT_L0TPE | ICESR_BIT_L0DPE | ICESR_BIT_L1TPE |
		       ICESR_BIT_L1DPE))) {
		pr_debug("CPU%d: No I-cache error detected ICESR 0x%llx\n",
			 cpu, icesr);
		goto clear_out;
	}

	pr_alert("CPU%d: I-cache error\n", cpu);
	pr_alert("CPU%d: ICESR_EL1 0x%llx ICESYNR0 0x%llx ICESYNR1 0x%llx ICEAR0 0x%llx IECAR1 0x%llx\n",
		 cpu, icesr, erp_mrs(ICESYNR0_EL1), erp_mrs(ICESYNR1_EL1),
		 erp_mrs(ICEAR0_EL1), erp_mrs(ICEAR1_EL1));

	/*
	 * all detectable I-cache erros are recoverable as
	 * corrupted lines are refetched
	 */
	if (panic_on_ce)
		BUG_ON(1);
	else
		WARN_ON(1);

clear_out:
	erp_msr(ICESR_EL1, icesr);
}

static void msm_erp_show_dcache_error(void)
{
	u64 dcesr;
	int cpu = raw_smp_processor_id();

	dcesr = erp_mrs(DCESR_EL1);
	if (!(dcesr & (DCESR_BIT_L1VTPE | DCESR_BIT_L1PTPE | DCESR_BIT_L1DPE |
		       DCESR_BIT_S1FTLBTPE | DCESR_BIT_S1FTLBDPE))) {
		pr_debug("CPU%d: No D-cache error detected DCESR 0x%llx\n",
			 cpu, dcesr);
		goto clear_out;
	}

	pr_alert("CPU%d: D-cache error detected\n", cpu);
	pr_alert("CPU%d: L1 DCESR 0x%llx, DCESYNR0 0x%llx, DCESYNR1 0x%llx, DCEAR0 0x%llx, DCEAR1 0x%llx\n",
		cpu, dcesr, erp_mrs(DCESYNR0_EL1), erp_mrs(DCESYNR1_EL1),
		erp_mrs(DCEAR0_EL1), erp_mrs(DCEAR1_EL1));

	/* all D-cache erros are correctable */
	if (panic_on_ce)
		BUG_ON(1);
	else
		WARN_ON(1);

clear_out:
	erp_msr(DCESR_EL1, dcesr);
}

static irqreturn_t msm_l1_erp_irq(int irq, void *dev_id)
{
	msm_erp_show_icache_error();
	msm_erp_show_dcache_error();
	return IRQ_HANDLED;
}

static DEFINE_SPINLOCK(local_handler_lock);
static void msm_l2_erp_local_handler(void *force)
{
	unsigned long flags;
	u64 esr0, esr1;
	bool parity_ue, parity_ce, misc_ue;
	int cpu;

	spin_lock_irqsave(&local_handler_lock, flags);

	esr0 = get_l2_indirect_reg(L2ESR0_IA);
	esr1 = get_l2_indirect_reg(L2ESR1_IA);
	parity_ue = esr0 & L2ESR0_UE;
	parity_ce = esr0 & L2ESR0_CE;
	misc_ue = esr1;
	cpu = raw_smp_processor_id();

	if (force || parity_ue || parity_ce || misc_ue) {
		if (parity_ue)
			pr_alert("CPU%d: L2 uncorrectable parity error\n", cpu);
		if (parity_ce)
			pr_alert("CPU%d: L2 correctable parity error\n", cpu);
		if (misc_ue)
			pr_alert("CPU%d: L2 (non-parity) error\n", cpu);
		pr_alert("CPU%d: L2ESR0 0x%llx, L2ESR1 0x%llx\n",
			cpu, esr0, esr1);
		pr_alert("CPU%d: L2ESYNR0 0x%llx, L2ESYNR1 0x%llx, L2ESYNR2 0x%llx\n",
			cpu, get_l2_indirect_reg(L2ESYNR0_IA),
			get_l2_indirect_reg(L2ESYNR1_IA),
			get_l2_indirect_reg(L2ESYNR2_IA));
		pr_alert("CPU%d: L2EAR0 0x%llx, L2EAR1 0x%llx\n", cpu,
			get_l2_indirect_reg(L2EAR0_IA),
			get_l2_indirect_reg(L2EAR1_IA));
	} else {
		pr_info("CPU%d: No L2 error detected in L2ESR0 0x%llx, L2ESR1 0x%llx)\n",
			cpu, esr0, esr1);
	}

	/* clear */
	set_l2_indirect_reg(L2ESR0_IA, esr0);
	set_l2_indirect_reg(L2ESR1_IA, esr1);

	if (panic_on_ue)
		BUG_ON(parity_ue || misc_ue);
	else
		WARN_ON(parity_ue || misc_ue);

	if (panic_on_ce)
		BUG_ON(parity_ce);
	else
		WARN_ON(parity_ce);

	spin_unlock_irqrestore(&local_handler_lock, flags);
}

static irqreturn_t msm_l2_erp_irq(int irq, void *dev_id)
{
	int cpu;
	struct call_single_data *csd;

	for_each_online_cpu(cpu) {
		csd = &per_cpu(handler_csd, cpu);
		csd->func = msm_l2_erp_local_handler;
		smp_call_function_single_async(cpu, csd);
	}

	return IRQ_HANDLED;
}

static irqreturn_t msm_l3_erp_irq(int irq, void *dev_id)
{
	u32 hml3_fira;
	bool parity_ue, parity_ce, misc_ue;

	hml3_fira = readl_relaxed(hml3_base + L3_QLL_HML3_FIRA);
	parity_ue = (hml3_fira & L3_QLL_HML3_FIRAT1S_IRQ_EN) &
			L3_QLL_HML3_FIRA_UE;
	parity_ce = (hml3_fira & L3_QLL_HML3_FIRAT1S_IRQ_EN) &
			L3_QLL_HML3_FIRA_CE;
	misc_ue = (hml3_fira & L3_QLL_HML3_FIRAT1S_IRQ_EN) &
			~(L3_QLL_HML3_FIRA_UE | L3_QLL_HML3_FIRA_CE);
	if (parity_ue)
		pr_alert("L3 uncorrectable parity error\n");
	if (parity_ce)
		pr_alert("L3 correctable parity error\n");
	if (misc_ue)
		pr_alert("L3 (non-parity) error\n");

	pr_alert("HML3_FIRA    0x%0x\n", hml3_fira);
	pr_alert("HML3_FIRSYNA 0x%0x, HML3_FIRSYNB 0x%0x\n",
		readl_relaxed(hml3_base + L3_QLL_HML3_FIRSYNA),
		readl_relaxed(hml3_base + L3_QLL_HML3_FIRSYNB));
	pr_alert("HML3_FIRSYNC 0x%0x, HML3_FIRSYND 0x%0x\n",
		readl_relaxed(hml3_base + L3_QLL_HML3_FIRSYNC),
		readl_relaxed(hml3_base + L3_QLL_HML3_FIRSYND));

	if (panic_on_ue)
		BUG_ON(parity_ue || misc_ue);
	else
		WARN_ON(parity_ue || misc_ue);

	if (panic_on_ce)
		BUG_ON(parity_ce);
	else
		WARN_ON(parity_ce);

	writel_relaxed(hml3_fira, hml3_base + L3_QLL_HML3_FIRAC);
	/* ensure of irq clear */
	wmb();
	return IRQ_HANDLED;
}

static irqreturn_t msm_m4m_erp_irq(int irq, void *dev_id)
{
	u32 m4m_status;

	pr_alert("CPU%d: M4M error detected\n", raw_smp_processor_id());
	m4m_status = readl_relaxed(m4m_base + M4M_ERR_STATUS);
	pr_alert("M4M_ERR_STATUS 0x%0x\n", m4m_status);
	if ((m4m_status & M4M_ERR_STATUS_MASK) &
	    ~(M4M_ERR_Q22SIB_RET_DEC_ERR | M4M_ERR_Q22SIB_RET_SLV_ERR)) {
		pr_alert("M4M_ERR_CAP_0  0x%0x, M4M_ERR_CAP_1 0x%x\n",
			 readl_relaxed(m4m_base + M4M_ERR_CAP_0),
			 readl_relaxed(m4m_base + M4M_ERR_CAP_1));
		pr_alert("M4M_ERR_CAP_2  0x%0x, M4M_ERR_CAP_3 0x%x\n",
			 readl_relaxed(m4m_base + M4M_ERR_CAP_2),
			 readl_relaxed(m4m_base + M4M_ERR_CAP_3));
	} else {
		/*
		 * M4M error-capture registers not valid when error detected
		 * due to DEC_ERR or SLV_ERR. L2E registers are still valid.
		 */
		pr_alert("Omit dumping M4M_ERR_CAP\n");
	}

	/*
	 * On QSB errors, the L2 captures the bad address and syndrome in
	 * L2E error registers.  Therefore dump L2E always whenever M4M error
	 * detected.
	 */
	on_each_cpu(msm_l2_erp_local_handler, (void *)1, 1);
	writel_relaxed(1, m4m_base + M4M_ERR_CLR);
	/* ensure of irq clear */
	wmb();

	if (panic_on_ue)
		BUG_ON(1);
	else
		WARN_ON(1);

	return IRQ_HANDLED;
}

static void enable_erp_irq_callback(void *info)
{
	enable_percpu_irq(erp_irqs[IRQ_L1], IRQ_TYPE_NONE);
}

static void disable_erp_irq_callback(void *info)
{
	disable_percpu_irq(erp_irqs[IRQ_L1]);
}

static void msm_cache_erp_irq_init(void *param)
{
	u64 v;
	/* Enable L0/L1 I/D cache error reporting. */
	erp_msr(ICECR_EL1, ICECR_IRQ_EN);
	erp_msr(DCECR_EL1, DCECR_IRQ_EN);
	/*
	 * Enable L2 data, tag, QSB and possion error reporting.
	 */
	set_l2_indirect_reg(L2ECR0_IA, L2ECR0_IRQ_EN);
	set_l2_indirect_reg(L2ECR1_IA, L2ECR1_IRQ_EN);
	v = (get_l2_indirect_reg(L2ECR2_IA) & ~L2ECR2_IRQ_EN_MASK)
		| L2ECR2_IRQ_EN;
	set_l2_indirect_reg(L2ECR2_IA, v);
}

static void msm_cache_erp_l3_init(void)
{
	writel_relaxed(L3_QLL_HML3_FIRAT0C_IRQ_EN,
		       hml3_base + L3_QLL_HML3_FIRAT0C);
	writel_relaxed(L3_QLL_HML3_FIRAT1S_IRQ_EN,
		       hml3_base + L3_QLL_HML3_FIRAT1S);
}

static int cache_erp_cpu_pm_callback(struct notifier_block *self,
				     unsigned long cmd, void *v)
{
	unsigned long aff_level = (unsigned long) v;

	switch (cmd) {
	case CPU_CLUSTER_PM_EXIT:
		msm_cache_erp_irq_init(NULL);

		if (aff_level >= AFFINITY_LEVEL_L3)
			msm_cache_erp_l3_init();
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block cache_erp_cpu_pm_notifier = {
	.notifier_call = cache_erp_cpu_pm_callback,
};

static int cache_erp_cpu_callback(struct notifier_block *nfb,
				  unsigned long action, void *hcpu)
{
	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_STARTING:
		msm_cache_erp_irq_init(NULL);
		enable_erp_irq_callback(NULL);
		break;
	case CPU_DYING:
		disable_erp_irq_callback(NULL);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block cache_erp_cpu_notifier = {
	.notifier_call = cache_erp_cpu_callback,
};

static int msm_cache_erp_probe(struct platform_device *pdev)
{
	int i, ret = 0;
	struct resource *r;

	dev_dbg(&pdev->dev, "enter\n");

	/* L3 */
	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hml3_base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(hml3_base)) {
		dev_err(&pdev->dev, "failed to ioremap (0x%p)\n", hml3_base);
		return PTR_ERR(hml3_base);
	}

	for (i = 0; i <= IRQ_L3; i++) {
		r = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
						 erp_irq_names[i]);
		if (!r) {
			dev_err(&pdev->dev, "failed to get %s\n",
				erp_irq_names[i]);
			return -ENODEV;
		}
		erp_irqs[i] = r->start;
	}

	msm_cache_erp_l3_init();

	/* L0/L1 erp irq per cpu */
	dev_info(&pdev->dev, "Registering for L1 error interrupts\n");
	ret = request_percpu_irq(erp_irqs[IRQ_L1], msm_l1_erp_irq,
				 erp_irq_names[IRQ_L1], &msm_l1_erp_stats);
	if (ret) {
		dev_err(&pdev->dev, "failed to request L0/L1 ERP irq %s (%d)\n",
			erp_irq_names[IRQ_L1], ret);
		return ret;
	} else {
		dev_dbg(&pdev->dev, "requested L0/L1 ERP irq %s\n",
			erp_irq_names[IRQ_L1]);
	}

	get_online_cpus();
	register_hotcpu_notifier(&cache_erp_cpu_notifier);
	cpu_pm_register_notifier(&cache_erp_cpu_pm_notifier);

	/* Perform L1/L2 cache error detection init on online cpus */
	on_each_cpu(msm_cache_erp_irq_init, NULL, 1);
	/* Enable irqs */
	on_each_cpu(enable_erp_irq_callback, NULL, 1);
	put_online_cpus();

	/* L2 erp irq per cluster */
	dev_info(&pdev->dev, "Registering for L2 error interrupts\n");
	for (i = IRQ_L2_INFO0; i <= IRQ_L2_ERR1; i++) {
		ret = devm_request_irq(&pdev->dev, erp_irqs[i],
						msm_l2_erp_irq,
						IRQF_ONESHOT |
						IRQF_TRIGGER_HIGH,
						erp_irq_names[i], NULL);
		if (ret) {
			dev_err(&pdev->dev, "failed to request irq %s (%d)\n",
				erp_irq_names[i], ret);
			goto cleanup;
		}
	}

	/* L3 erp irq */
	dev_info(&pdev->dev, "Registering for L3 error interrupts\n");
	ret = devm_request_irq(&pdev->dev, erp_irqs[IRQ_L3], msm_l3_erp_irq,
			       IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
			       erp_irq_names[IRQ_L3], NULL);
	if (ret) {
		dev_err(&pdev->dev, "failed to request L3 irq %s (%d)\n",
			erp_irq_names[IRQ_L3], ret);
		goto cleanup;
	}

	return 0;

cleanup:
	free_percpu_irq(erp_irqs[IRQ_L1], NULL);
	return ret;
}

static void msm_m4m_erp_irq_init(void)
{
	writel_relaxed(M4M_INT_CTRL_IRQ_EN, m4m_base + M4M_INT_CTRL);
	writel_relaxed(0, m4m_base + M4M_ERR_CTRL);
}

static int msm_m4m_erp_m4m_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *r;

	dev_dbg(&pdev->dev, "enter\n");

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	m4m_base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(m4m_base)) {
		dev_err(&pdev->dev, "failed to ioremap (0x%p)\n", m4m_base);
		return PTR_ERR(m4m_base);
	}

	r = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
					 erp_irq_names[IRQ_M4M]);
	if (!r) {
		dev_err(&pdev->dev, "failed to get %s\n",
			erp_irq_names[IRQ_M4M]);
		ret = -ENODEV;
		goto exit;
	}
	erp_irqs[IRQ_M4M] = r->start;

	dev_info(&pdev->dev, "Registering for M4M error interrupts\n");
	ret = devm_request_irq(&pdev->dev, erp_irqs[IRQ_M4M],
					msm_m4m_erp_irq,
					IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
					erp_irq_names[IRQ_M4M], NULL);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq %s (%d)\n",
			erp_irq_names[IRQ_M4M], ret);
		goto exit;
	}

	msm_m4m_erp_irq_init();

exit:
	return ret;
}

static struct of_device_id cache_erp_dt_ids[] = {
	{ .compatible = "qcom,kryo_cache_erp64", },
	{}
};
MODULE_DEVICE_TABLE(of, cache_erp_dt_ids);

static struct platform_driver msm_cache_erp_driver = {
	.probe = msm_cache_erp_probe,
	.driver = {
		.name = "msm_cache_erp64",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(cache_erp_dt_ids),
	},
};

static struct of_device_id m4m_erp_dt_ids[] = {
	{ .compatible = "qcom,m4m_erp", },
	{}
};
MODULE_DEVICE_TABLE(of, m4m_erp_dt_ids);
static struct platform_driver msm_m4m_erp_driver = {
	.probe = msm_m4m_erp_m4m_probe,
	.driver = {
		.name = "msm_m4m_erp",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(m4m_erp_dt_ids),
	},
};

static int __init msm_cache_erp_init(void)
{
	int r;

	r = platform_driver_register(&msm_cache_erp_driver);
	if (!r)
		r = platform_driver_register(&msm_m4m_erp_driver);
	if (r)
		pr_err("failed to register driver %d\n", r);
	return r;
}

arch_initcall(msm_cache_erp_init);
