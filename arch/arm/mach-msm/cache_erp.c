/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/cpu.h>
#include <mach/msm-krait-l2-accessors.h>

#define CESR_DCTPE		BIT(0)
#define CESR_DCDPE		BIT(1)
#define CESR_ICTPE		BIT(2)
#define CESR_ICDPE		BIT(3)
#define CESR_DCTE		(BIT(4) | BIT(5))
#define CESR_ICTE		(BIT(6) | BIT(7))
#define CESR_TLBMH		BIT(16)
#define CESR_I_MASK		0x000000CC

#define L2ESR_IND_ADDR		0x204
#define L2ESYNR0_IND_ADDR	0x208
#define L2ESYNR1_IND_ADDR	0x209
#define L2EAR0_IND_ADDR		0x20C
#define L2EAR1_IND_ADDR		0x20D

#define L2ESR_MPDCD		BIT(0)
#define L2ESR_MPSLV             BIT(1)
#define L2ESR_TSESB             BIT(2)
#define L2ESR_TSEDB             BIT(3)
#define L2ESR_DSESB             BIT(4)
#define L2ESR_DSEDB             BIT(5)
#define L2ESR_MSE		BIT(6)
#define L2ESR_MPLDREXNOK	BIT(8)

#define L2ESR_ACCESS_ERR_MASK	0xFFFC

#define L2ESR_CPU_MASK		0x0F
#define L2ESR_CPU_SHIFT		16

#ifdef CONFIG_MSM_L1_ERR_PANIC
#define ERP_L1_ERR(a) panic(a)
#else
#define ERP_L1_ERR(a) do { } while (0)
#endif

#ifdef CONFIG_MSM_L2_ERP_PORT_PANIC
#define ERP_PORT_ERR(a) panic(a)
#else
#define ERP_PORT_ERR(a) WARN(1, a)
#endif

#ifdef CONFIG_MSM_L2_ERP_1BIT_PANIC
#define ERP_1BIT_ERR(a) panic(a)
#else
#define ERP_1BIT_ERR(a) do { } while (0)
#endif

#ifdef CONFIG_MSM_L2_ERP_PRINT_ACCESS_ERRORS
#define print_access_errors()	1
#else
#define print_access_errors()	0
#endif

#ifdef CONFIG_MSM_L2_ERP_2BIT_PANIC
#define ERP_2BIT_ERR(a) panic(a)
#else
#define ERP_2BIT_ERR(a) do { } while (0)
#endif

#define MODULE_NAME "msm_cache_erp"

struct msm_l1_err_stats {
	unsigned int dctpe;
	unsigned int dcdpe;
	unsigned int ictpe;
	unsigned int icdpe;
	unsigned int dcte;
	unsigned int icte;
	unsigned int tlbmh;
};

struct msm_l2_err_stats {
	unsigned int mpdcd;
	unsigned int mpslv;
	unsigned int tsesb;
	unsigned int tsedb;
	unsigned int dsesb;
	unsigned int dsedb;
	unsigned int mse;
	unsigned int mplxrexnok;
};

static DEFINE_PER_CPU(struct msm_l1_err_stats, msm_l1_erp_stats);
static struct msm_l2_err_stats msm_l2_erp_stats;

static int l1_erp_irq, l2_erp_irq;
static struct proc_dir_entry *procfs_entry;

static inline unsigned int read_cesr(void)
{
	unsigned int cesr;
	asm volatile ("mrc p15, 7, %0, c15, c0, 1" : "=r" (cesr));
	return cesr;
}

static inline void write_cesr(unsigned int cesr)
{
	asm volatile ("mcr p15, 7, %[cesr], c15, c0, 1" : : [cesr]"r" (cesr));
}

static inline unsigned int read_cesynr(void)
{
	unsigned int cesynr;
	asm volatile ("mrc p15, 7, %0, c15, c0, 3" : "=r" (cesynr));
	return cesynr;
}

static int proc_read_status(char *page, char **start, off_t off, int count,
			    int *eof, void *data)
{
	struct msm_l1_err_stats *l1_stats;
	char *p = page;
	int len, cpu, ret, bytes_left = PAGE_SIZE;

	for_each_present_cpu(cpu) {
		l1_stats = &per_cpu(msm_l1_erp_stats, cpu);

		ret = snprintf(p, bytes_left,
			"CPU %d:\n"	\
			"\tD-cache tag parity errors:\t%u\n"	\
			"\tD-cache data parity errors:\t%u\n"	\
			"\tI-cache tag parity errors:\t%u\n"	\
			"\tI-cache data parity errors:\t%u\n"	\
			"\tD-cache timing errors:\t\t%u\n"	\
			"\tI-cache timing errors:\t\t%u\n"	\
			"\tTLB multi-hit errors:\t\t%u\n\n",	\
			cpu,
			l1_stats->dctpe,
			l1_stats->dcdpe,
			l1_stats->ictpe,
			l1_stats->icdpe,
			l1_stats->dcte,
			l1_stats->icte,
			l1_stats->tlbmh);
		p += ret;
		bytes_left -= ret;
	}

	p += snprintf(p, bytes_left,
			"L2 master port decode errors:\t\t%u\n"	\
			"L2 master port slave errors:\t\t%u\n"		\
			"L2 tag soft errors, single-bit:\t\t%u\n"	\
			"L2 tag soft errors, double-bit:\t\t%u\n"	\
			"L2 data soft errors, single-bit:\t%u\n"	\
			"L2 data soft errors, double-bit:\t%u\n"	\
			"L2 modified soft errors:\t\t%u\n"		\
			"L2 master port LDREX NOK errors:\t%u\n",
			msm_l2_erp_stats.mpdcd,
			msm_l2_erp_stats.mpslv,
			msm_l2_erp_stats.tsesb,
			msm_l2_erp_stats.tsedb,
			msm_l2_erp_stats.dsesb,
			msm_l2_erp_stats.dsedb,
			msm_l2_erp_stats.mse,
			msm_l2_erp_stats.mplxrexnok);

	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static irqreturn_t msm_l1_erp_irq(int irq, void *dev_id)
{
	struct msm_l1_err_stats *l1_stats = dev_id;
	unsigned int cesr = read_cesr();
	unsigned int i_cesynr, d_cesynr;

	pr_alert("L1 Error detected on CPU %d!\n", smp_processor_id());
	pr_alert("\tCESR    = 0x%08x\n", cesr);

	if (cesr & CESR_DCTPE) {
		pr_alert("D-cache tag parity error\n");
		l1_stats->dctpe++;
	}

	if (cesr & CESR_DCDPE) {
		pr_alert("D-cache data parity error\n");
		l1_stats->dcdpe++;
	}

	if (cesr & CESR_ICTPE) {
		pr_alert("I-cache tag parity error\n");
		l1_stats->ictpe++;
	}

	if (cesr & CESR_ICDPE) {
		pr_alert("I-cache data parity error\n");
		l1_stats->icdpe++;
	}

	if (cesr & CESR_DCTE) {
		pr_alert("D-cache timing error\n");
		l1_stats->dcte++;
	}

	if (cesr & CESR_ICTE) {
		pr_alert("I-cache timing error\n");
		l1_stats->icte++;
	}

	if (cesr & CESR_TLBMH) {
		pr_alert("TLB multi-hit error\n");
		l1_stats->tlbmh++;
	}

	if (cesr & (CESR_ICTPE | CESR_ICDPE | CESR_ICTE)) {
		i_cesynr = read_cesynr();
		pr_alert("I-side CESYNR = 0x%08x\n", i_cesynr);
		write_cesr(CESR_I_MASK);

		/*
		 * Clear the I-side bits from the captured CESR value so that we
		 * don't accidentally clear any new I-side errors when we do
		 * the CESR write-clear operation.
		 */
		cesr &= ~CESR_I_MASK;
	}

	if (cesr & (CESR_DCTPE | CESR_DCDPE | CESR_DCTE)) {
		d_cesynr = read_cesynr();
		pr_alert("D-side CESYNR = 0x%08x\n", d_cesynr);
	}

	/* Clear the interrupt bits we processed */
	write_cesr(cesr);

	ERP_L1_ERR("L1 cache / TLB error detected");

	return IRQ_HANDLED;
}

static irqreturn_t msm_l2_erp_irq(int irq, void *dev_id)
{
	unsigned int l2esr;
	unsigned int l2esynr0;
	unsigned int l2esynr1;
	unsigned int l2ear0;
	unsigned int l2ear1;
	int soft_error = 0;
	int port_error = 0;
	int unrecoverable = 0;
	int print_alert;

	l2esr = get_l2_indirect_reg(L2ESR_IND_ADDR);
	l2esynr0 = get_l2_indirect_reg(L2ESYNR0_IND_ADDR);
	l2esynr1 = get_l2_indirect_reg(L2ESYNR1_IND_ADDR);
	l2ear0 = get_l2_indirect_reg(L2EAR0_IND_ADDR);
	l2ear1 = get_l2_indirect_reg(L2EAR1_IND_ADDR);

	print_alert = print_access_errors() || (l2esr & L2ESR_ACCESS_ERR_MASK);

	if (print_alert) {
		pr_alert("L2 Error detected!\n");
		pr_alert("\tL2ESR    = 0x%08x\n", l2esr);
		pr_alert("\tL2ESYNR0 = 0x%08x\n", l2esynr0);
		pr_alert("\tL2ESYNR1 = 0x%08x\n", l2esynr1);
		pr_alert("\tL2EAR0   = 0x%08x\n", l2ear0);
		pr_alert("\tL2EAR1   = 0x%08x\n", l2ear1);
		pr_alert("\tCPU bitmap = 0x%x\n", (l2esr >> L2ESR_CPU_SHIFT) &
							L2ESR_CPU_MASK);
	}

	if (l2esr & L2ESR_MPDCD) {
		if (print_alert)
			pr_alert("L2 master port decode error\n");
		port_error++;
		msm_l2_erp_stats.mpdcd++;
	}

	if (l2esr & L2ESR_MPSLV) {
		if (print_alert)
			pr_alert("L2 master port slave error\n");
		port_error++;
		msm_l2_erp_stats.mpslv++;
	}

	if (l2esr & L2ESR_TSESB) {
		pr_alert("L2 tag soft error, single-bit\n");
		soft_error++;
		msm_l2_erp_stats.tsesb++;
	}

	if (l2esr & L2ESR_TSEDB) {
		pr_alert("L2 tag soft error, double-bit\n");
		soft_error++;
		unrecoverable++;
		msm_l2_erp_stats.tsedb++;
	}

	if (l2esr & L2ESR_DSESB) {
		pr_alert("L2 data soft error, single-bit\n");
		soft_error++;
		msm_l2_erp_stats.dsesb++;
	}

	if (l2esr & L2ESR_DSEDB) {
		pr_alert("L2 data soft error, double-bit\n");
		soft_error++;
		unrecoverable++;
		msm_l2_erp_stats.dsedb++;
	}

	if (l2esr & L2ESR_MSE) {
		pr_alert("L2 modified soft error\n");
		soft_error++;
		msm_l2_erp_stats.mse++;
	}

	if (l2esr & L2ESR_MPLDREXNOK) {
		pr_alert("L2 master port LDREX received Normal OK response\n");
		port_error++;
		msm_l2_erp_stats.mplxrexnok++;
	}

	if (port_error && print_alert)
		ERP_PORT_ERR("L2 master port error detected");

	if (soft_error && !unrecoverable)
		ERP_1BIT_ERR("L2 single-bit error detected");

	if (unrecoverable)
		ERP_2BIT_ERR("L2 double-bit error detected, trouble ahead");

	set_l2_indirect_reg(L2ESR_IND_ADDR, l2esr);
	return IRQ_HANDLED;
}

static void enable_erp_irq_callback(void *info)
{
	enable_percpu_irq(l1_erp_irq, IRQ_TYPE_LEVEL_HIGH);
}

static void disable_erp_irq_callback(void *info)
{
	disable_percpu_irq(l1_erp_irq);
}

static int cache_erp_cpu_callback(struct notifier_block *nfb,
					    unsigned long action, void *hcpu)
{
	switch (action & (~CPU_TASKS_FROZEN)) {
	case CPU_STARTING:
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
	struct resource *r;
	int ret, cpu;

	r = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "l1_irq");

	if (!r) {
		pr_err("Could not get L1 resource\n");
		ret = -ENODEV;
		goto fail;
	}

	l1_erp_irq = r->start;

	ret = request_percpu_irq(l1_erp_irq, msm_l1_erp_irq, "MSM_L1",
				 &msm_l1_erp_stats);

	if (ret) {
		pr_err("Failed to request the L1 cache error interrupt\n");
		goto fail;
	}

	r = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "l2_irq");

	if (!r) {
		pr_err("Could not get L2 resource\n");
		ret = -ENODEV;
		goto fail_l1;
	}

	l2_erp_irq = r->start;
	ret = request_irq(l2_erp_irq, msm_l2_erp_irq, 0, "MSM_L2", NULL);

	if (ret) {
		pr_err("Failed to request the L2 cache error interrupt\n");
		goto fail_l1;
	}

	procfs_entry = create_proc_entry("cpu/msm_cache_erp", S_IRUGO, NULL);

	if (!procfs_entry) {
		pr_err("Failed to create procfs node for cache error reporting\n");
		ret = -ENODEV;
		goto fail_l2;
	}

	get_online_cpus();
	register_hotcpu_notifier(&cache_erp_cpu_notifier);
	for_each_cpu(cpu, cpu_online_mask)
		smp_call_function_single(cpu, enable_erp_irq_callback, NULL, 1);
	put_online_cpus();

	procfs_entry->read_proc = proc_read_status;
	return 0;

fail_l2:
	free_irq(l2_erp_irq, NULL);
fail_l1:
	free_percpu_irq(l1_erp_irq, NULL);
fail:
	return  ret;
}

static int msm_cache_erp_remove(struct platform_device *pdev)
{
	int cpu;
	if (procfs_entry)
		remove_proc_entry("cpu/msm_cache_erp", NULL);

	get_online_cpus();
	unregister_hotcpu_notifier(&cache_erp_cpu_notifier);
	for_each_cpu(cpu, cpu_online_mask)
		smp_call_function_single(cpu, disable_erp_irq_callback, NULL,
					 1);
	put_online_cpus();

	free_percpu_irq(l1_erp_irq, NULL);

	disable_irq(l2_erp_irq);
	free_irq(l2_erp_irq, NULL);
	return 0;
}

static struct platform_driver msm_cache_erp_driver = {
	.probe = msm_cache_erp_probe,
	.remove = msm_cache_erp_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init msm_cache_erp_init(void)
{
	return platform_driver_register(&msm_cache_erp_driver);
}

static void __exit msm_cache_erp_exit(void)
{
	platform_driver_unregister(&msm_cache_erp_driver);
}


module_init(msm_cache_erp_init);
module_exit(msm_cache_erp_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM cache error reporting driver");
