/*
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *  Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *  Copyright (c) 2014 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <linux/qcom_scm.h>

#include <asm/smp_plat.h>
#include <asm/fixmap.h>
#include "platsmp.h"
#ifdef CONFIG_MSM_PM_LEGACY
#include <soc/qcom/pm-legacy.h>
#endif
#include <soc/qcom/scm-boot.h>
#define MSM_APCS_IDR 0x0B011030

/* Base Address of APC IPC block */
#define APCS_ALIAS0_APC_SECURE 0x0B088000

#define VDD_SC1_ARRAY_CLAMP_GFS_CTL	0x35a0
#define SCSS_CPU1CORE_RESET		0x2d80
#define SCSS_DBG_STATUS_CORE_PWRDUP	0x2e64

#define APCS_CPU_PWR_CTL	0x04
#define PLL_CLAMP		BIT(8)
#define CORE_PWRD_UP		BIT(7)
#define GATE_CLK		BIT(6)
#define COREPOR_RST		BIT(5)
#define CORE_RST		BIT(4)
#define L2DT_SLP		BIT(3)
#define L1_RST_DIS		BIT(2)
#define CORE_MEM_CLAMP		BIT(1)
#define CLAMP			BIT(0)

#define APC_PWR_GATE_CTL	0x14
#define BHS_CNT_SHIFT		24
#define LDO_PWR_DWN_SHIFT	16
#define LDO_BYP_SHIFT		8
#define BHS_SEG_SHIFT		1
#define BHS_EN			BIT(0)

#define APCS_SAW2_VCTL		0x14
#define APCS_SAW2_2_VCTL	0x1c

extern void secondary_startup_arm(void);

static DEFINE_SPINLOCK(boot_lock);

#ifdef CONFIG_HOTPLUG_CPU
static void qcom_cpu_die(unsigned int cpu)
{
	wfi();
}

static bool qcom_cpu_can_disable(unsigned int cpu)
{
	return true; /*Hotplug of any CPU is supported */
}
#endif

static void qcom_secondary_init(unsigned int cpu)
{
#ifdef CONFIG_MSM_PM_LEGACY
	WARN_ON(msm_platform_secondary_init(cpu));
#endif
	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

static int scss_release_secondary(unsigned int cpu)
{
	struct device_node *node;
	void __iomem *base;

	node = of_find_compatible_node(NULL, NULL, "qcom,gcc-msm8660");
	if (!node) {
		pr_err("%s: can't find node\n", __func__);
		return -ENXIO;
	}

	base = of_iomap(node, 0);
	of_node_put(node);
	if (!base)
		return -ENOMEM;

	writel_relaxed(0, base + VDD_SC1_ARRAY_CLAMP_GFS_CTL);
	writel_relaxed(0, base + SCSS_CPU1CORE_RESET);
	writel_relaxed(3, base + SCSS_DBG_STATUS_CORE_PWRDUP);
	mb();
	iounmap(base);

	return 0;
}

static int kpssv1_release_secondary(unsigned int cpu)
{
	int ret = 0;
	void __iomem *reg, *saw_reg;
	struct device_node *cpu_node, *acc_node, *saw_node;
	u32 val;

	cpu_node = of_get_cpu_node(cpu, NULL);
	if (!cpu_node)
		return -ENODEV;

	acc_node = of_parse_phandle(cpu_node, "qcom,acc", 0);
	if (!acc_node) {
		ret = -ENODEV;
		goto out_acc;
	}

	saw_node = of_parse_phandle(cpu_node, "qcom,saw", 0);
	if (!saw_node) {
		ret = -ENODEV;
		goto out_saw;
	}

	reg = of_iomap(acc_node, 0);
	if (!reg) {
		ret = -ENOMEM;
		goto out_acc_map;
	}

	saw_reg = of_iomap(saw_node, 0);
	if (!saw_reg) {
		ret = -ENOMEM;
		goto out_saw_map;
	}

	/* Turn on CPU rail */
	writel_relaxed(0xA4, saw_reg + APCS_SAW2_VCTL);
	mb();
	udelay(512);

	/* Krait bring-up sequence */
	val = PLL_CLAMP | L2DT_SLP | CLAMP;
	writel_relaxed(val, reg + APCS_CPU_PWR_CTL);
	val &= ~L2DT_SLP;
	writel_relaxed(val, reg + APCS_CPU_PWR_CTL);
	mb();
	ndelay(300);

	val |= COREPOR_RST;
	writel_relaxed(val, reg + APCS_CPU_PWR_CTL);
	mb();
	udelay(2);

	val &= ~CLAMP;
	writel_relaxed(val, reg + APCS_CPU_PWR_CTL);
	mb();
	udelay(2);

	val &= ~COREPOR_RST;
	writel_relaxed(val, reg + APCS_CPU_PWR_CTL);
	mb();
	udelay(100);

	val |= CORE_PWRD_UP;
	writel_relaxed(val, reg + APCS_CPU_PWR_CTL);
	mb();

	iounmap(saw_reg);
out_saw_map:
	iounmap(reg);
out_acc_map:
	of_node_put(saw_node);
out_saw:
	of_node_put(acc_node);
out_acc:
	of_node_put(cpu_node);
	return ret;
}

static int kpssv2_release_secondary(unsigned int cpu)
{
	void __iomem *reg;
	struct device_node *cpu_node, *l2_node, *acc_node, *saw_node;
	void __iomem *l2_saw_base;
	unsigned reg_val;
	int ret;

	cpu_node = of_get_cpu_node(cpu, NULL);
	if (!cpu_node)
		return -ENODEV;

	acc_node = of_parse_phandle(cpu_node, "qcom,acc", 0);
	if (!acc_node) {
		ret = -ENODEV;
		goto out_acc;
	}

	l2_node = of_parse_phandle(cpu_node, "next-level-cache", 0);
	if (!l2_node) {
		ret = -ENODEV;
		goto out_l2;
	}

	saw_node = of_parse_phandle(l2_node, "qcom,saw", 0);
	if (!saw_node) {
		ret = -ENODEV;
		goto out_saw;
	}

	reg = of_iomap(acc_node, 0);
	if (!reg) {
		ret = -ENOMEM;
		goto out_map;
	}

	l2_saw_base = of_iomap(saw_node, 0);
	if (!l2_saw_base) {
		ret = -ENOMEM;
		goto out_saw_map;
	}

	/* Turn on the BHS, turn off LDO Bypass and power down LDO */
	reg_val = (64 << BHS_CNT_SHIFT) | (0x3f << LDO_PWR_DWN_SHIFT) | BHS_EN;
	writel_relaxed(reg_val, reg + APC_PWR_GATE_CTL);
	mb();
	/* wait for the BHS to settle */
	udelay(1);

	/* Turn on BHS segments */
	reg_val |= 0x3f << BHS_SEG_SHIFT;
	writel_relaxed(reg_val, reg + APC_PWR_GATE_CTL);
	mb();
	 /* wait for the BHS to settle */
	udelay(1);

	/* Finally turn on the bypass so that BHS supplies power */
	reg_val |= 0x3f << LDO_BYP_SHIFT;
	writel_relaxed(reg_val, reg + APC_PWR_GATE_CTL);

	/* enable max phases */
	writel_relaxed(0x10003, l2_saw_base + APCS_SAW2_2_VCTL);
	mb();
	udelay(50);

	reg_val = COREPOR_RST | CLAMP;
	writel_relaxed(reg_val, reg + APCS_CPU_PWR_CTL);
	mb();
	udelay(2);

	reg_val &= ~CLAMP;
	writel_relaxed(reg_val, reg + APCS_CPU_PWR_CTL);
	mb();
	udelay(2);

	reg_val &= ~COREPOR_RST;
	writel_relaxed(reg_val, reg + APCS_CPU_PWR_CTL);
	mb();

	reg_val |= CORE_PWRD_UP;
	writel_relaxed(reg_val, reg + APCS_CPU_PWR_CTL);
	mb();

	ret = 0;

	iounmap(l2_saw_base);
out_saw_map:
	iounmap(reg);
out_map:
	of_node_put(saw_node);
out_saw:
	of_node_put(l2_node);
out_l2:
	of_node_put(acc_node);
out_acc:
	of_node_put(cpu_node);

	return ret;
}

static DEFINE_PER_CPU(int, cold_boot_done);

/*
 * writing to physical address:
 * 0xb088000 + (cpu * 0x10000) + 0x04  and
 * 0xb088000 + (cpu * 0x10000) + 0x014
 * For each secondary cpu
 */

static int arm_release_secondary(unsigned int cpu)
{
	phys_addr_t base = APCS_ALIAS0_APC_SECURE;
	void __iomem *base_ptr;
	unsigned int reg_val;

	base_ptr = ioremap_nocache(base + (cpu * 0x10000), SZ_4K);

	if (!base_ptr)
		return -ENODEV;

	reg_val = COREPOR_RST | CORE_RST | CORE_MEM_CLAMP | CLAMP;
	writel_relaxed(reg_val, base_ptr + APCS_CPU_PWR_CTL);
	/* memory barrier */
	mb();

	/* Turn on the BHS, set the BHS_CNT value with 16 */
	reg_val = (0x10 << BHS_CNT_SHIFT) | BHS_EN;
	writel_relaxed(reg_val, base_ptr + APC_PWR_GATE_CTL);
	/* memory barrier */
	mb();
	udelay(2);

	reg_val = COREPOR_RST | CORE_RST | CLAMP;
	writel_relaxed(reg_val, base_ptr + APCS_CPU_PWR_CTL);
	/* memory barrier */
	mb();

	reg_val = COREPOR_RST | CORE_RST | L2DT_SLP | CLAMP;
	writel_relaxed(reg_val, base_ptr + APCS_CPU_PWR_CTL);
	/* memory barrier */
	mb();
	udelay(2);

	reg_val = COREPOR_RST | CORE_RST | L2DT_SLP;
	writel_relaxed(reg_val, base_ptr + APCS_CPU_PWR_CTL);
	/* memory barrier */
	mb();
	udelay(2);

	reg_val = L2DT_SLP;
	writel_relaxed(reg_val, base_ptr + APCS_CPU_PWR_CTL);
	/* memory barrier */
	mb();

	reg_val = CORE_PWRD_UP | L2DT_SLP;
	writel_relaxed(reg_val, base_ptr + APCS_CPU_PWR_CTL);
	/* memory barrier */
	mb();

	iounmap(base_ptr);
	return 0;
}

static int qcom_boot_secondary(unsigned int cpu, int (*func)(unsigned int))
{
	int ret = 0;

	if (!per_cpu(cold_boot_done, cpu)) {
		ret = func(cpu);
		if (!ret)
			per_cpu(cold_boot_done, cpu) = true;
	}

	/*
	 * set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

	/*
	 * Send the secondary CPU a soft interrupt, thereby causing
	 * the boot monitor to read the system wide flags register,
	 * and branch to the address found there.
	 */
	arch_send_wakeup_ipi_mask(cpumask_of(cpu));

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

	return ret;
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */

static void __init arm_smp_init_cpus(void)
{
	unsigned int i, ncores;
	void __iomem *base;

	set_fixmap_io(FIX_SMP_MEM_BASE, MSM_APCS_IDR & PAGE_MASK);
	base = (void __iomem *)__fix_to_virt(FIX_SMP_MEM_BASE);

	if (!base)
		return;

	base += MSM_APCS_IDR & ~PAGE_MASK;

	ncores = (__raw_readl(base)) & 0xF;
	if (ncores > nr_cpu_ids) {
		pr_warn("SMP: %u cores greater than maximum (%u), clipping\n",
					ncores, nr_cpu_ids);
		ncores = nr_cpu_ids;
	}
	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);
}

static int msm8909_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	pr_debug("Starting secondary CPU %d\n", cpu);
	return qcom_boot_secondary(cpu, arm_release_secondary);
}

static int msm8660_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	return qcom_boot_secondary(cpu, scss_release_secondary);
}

static int kpssv1_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	return qcom_boot_secondary(cpu, kpssv1_release_secondary);
}

static int kpssv2_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	return qcom_boot_secondary(cpu, kpssv2_release_secondary);
}

static void __init qcom_smp_prepare_cpus(unsigned int max_cpus)
{
	int cpu, map;
	u32 aff0_mask = 0;
	u32 aff1_mask = 0;
	u32 aff2_mask = 0;

	for_each_present_cpu(cpu) {
		map = cpu_logical_map(cpu);
		aff0_mask |= BIT(MPIDR_AFFINITY_LEVEL(map, 0));
		aff1_mask |= BIT(MPIDR_AFFINITY_LEVEL(map, 1));
		aff2_mask |= BIT(MPIDR_AFFINITY_LEVEL(map, 2));
	}
	if (scm_set_boot_addr_mc(virt_to_phys(secondary_startup_arm),
		aff0_mask, aff1_mask, aff2_mask, SCM_FLAG_COLDBOOT_MC)) {
		for_each_present_cpu(cpu) {
			if (cpu == smp_processor_id())
				continue;
			set_cpu_present(cpu, false);
		}
		pr_warn("Failed to set CPU boot address, disabling SMP\n");
	}
}

static const struct smp_operations smp_msm8660_ops __initconst = {
	.smp_prepare_cpus	= qcom_smp_prepare_cpus,
	.smp_secondary_init	= qcom_secondary_init,
	.smp_boot_secondary	= msm8660_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die		= qcom_cpu_die,
#endif
};
CPU_METHOD_OF_DECLARE(qcom_smp, "qcom,gcc-msm8660", &smp_msm8660_ops);

static const struct smp_operations qcom_smp_kpssv1_ops __initconst = {
	.smp_prepare_cpus	= qcom_smp_prepare_cpus,
	.smp_secondary_init	= qcom_secondary_init,
	.smp_boot_secondary	= kpssv1_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die		= qcom_cpu_die,
#endif
};
CPU_METHOD_OF_DECLARE(qcom_smp_kpssv1, "qcom,kpss-acc-v1", &qcom_smp_kpssv1_ops);

static const struct smp_operations qcom_smp_kpssv2_ops __initconst = {
	.smp_prepare_cpus	= qcom_smp_prepare_cpus,
	.smp_secondary_init	= qcom_secondary_init,
	.smp_boot_secondary	= kpssv2_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die		= qcom_cpu_die,
#endif
};
CPU_METHOD_OF_DECLARE(qcom_smp_kpssv2, "qcom,kpss-acc-v2", &qcom_smp_kpssv2_ops);

struct smp_operations msm8909_smp_ops __initdata = {
	.smp_init_cpus = arm_smp_init_cpus,
	.smp_prepare_cpus = qcom_smp_prepare_cpus,
	.smp_secondary_init = qcom_secondary_init,
	.smp_boot_secondary = msm8909_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
#ifdef CONFIG_MSM_PM_LEGACY
	.cpu_die		= qcom_cpu_die_legacy,
	.cpu_kill		= qcom_cpu_kill_legacy,
#else
	.cpu_die		= qcom_cpu_die,
#endif
	.cpu_can_disable	= qcom_cpu_can_disable,
#endif
};

CPU_METHOD_OF_DECLARE(qcom_smp_8909, "qcom,apss-8909", &msm8909_smp_ops);
