/*
 * arch/arm/mach-tegra/pm-t3.c
 *
 * Tegra3 SOC-specific power and cluster management
 *
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/clockchips.h>
#include <linux/cpu_pm.h>

#include <mach/gpio.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/io_dpd.h>
#include <mach/edp.h>

#include <asm/smp_plat.h>
#include <asm/cputype.h>
#include <asm/hardware/gic.h>

#include <trace/events/power.h>

#include "clock.h"
#include "cpuidle.h"
#include "pm.h"
#include "sleep.h"
#include "tegra3_emc.h"
#include "dvfs.h"

#ifdef CONFIG_TEGRA_CLUSTER_CONTROL
#define CAR_CCLK_BURST_POLICY \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x20)

#define CAR_SUPER_CCLK_DIVIDER \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x24)

#define CAR_CCLKG_BURST_POLICY \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x368)

#define CAR_SUPER_CCLKG_DIVIDER \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x36C)

#define CAR_CCLKLP_BURST_POLICY \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x370)
#define PLLX_DIV2_BYPASS_LP	(1<<16)

#define CAR_SUPER_CCLKLP_DIVIDER \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x374)

#define CAR_BOND_OUT_V \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x390)
#define CAR_BOND_OUT_V_CPU_G	(1<<0)
#define CAR_BOND_OUT_V_CPU_LP	(1<<1)

#define CAR_CLK_ENB_V_SET \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x440)
#define CAR_CLK_ENB_V_CPU_G	(1<<0)
#define CAR_CLK_ENB_V_CPU_LP	(1<<1)

#define CAR_RST_CPUG_CMPLX_SET \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x450)

#define CAR_RST_CPUG_CMPLX_CLR \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x454)

#define CAR_RST_CPULP_CMPLX_SET \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x458)

#define CAR_RST_CPULP_CMPLX_CLR \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x45C)

#define CAR_CLK_CPUG_CMPLX_SET \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x460)

#define CAR_CLK_CPUG_CMPLX_CLR \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x464)

#define CAR_CLK_CPULP_CMPLX_SET \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x468)

#define CAR_CLK_CPULP_CMPLX_CLR \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x46C)

#define CPU_CLOCK(cpu)	(0x1<<(8+cpu))
#define CPU_RESET(cpu)	(0x1111ul<<(cpu))

#define PLLX_FO_G (1<<28)
#define PLLX_FO_LP (1<<29)

#define CLK_RST_CONTROLLER_PLLX_MISC_0 \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0xE4)

static struct clk *cclk_lp;

static int cluster_switch_prolog_clock(unsigned int flags)
{
	u32 reg;
	u32 CclkBurstPolicy;
	u32 SuperCclkDivier;

	/* Read the bond out register containing the G and LP CPUs. */
	reg = readl(CAR_BOND_OUT_V);

	/* Sync G-PLLX divider bypass with LP (no effect on G, just to prevent
	   LP settings overwrite by save/restore code */
	CclkBurstPolicy = ~PLLX_DIV2_BYPASS_LP & readl(CAR_CCLKG_BURST_POLICY);
	CclkBurstPolicy |= PLLX_DIV2_BYPASS_LP & readl(CAR_CCLKLP_BURST_POLICY);
	writel(CclkBurstPolicy, CAR_CCLKG_BURST_POLICY);

	/* Switching to G? */
	if (flags & TEGRA_POWER_CLUSTER_G) {
		/* Do the G CPUs exist? */
		if (reg & CAR_BOND_OUT_V_CPU_G)
			return -ENXIO;

		/* Keep G CPU clock policy set by upper laayer, with the
		   exception of the transition via LP1 */
		if (flags & TEGRA_POWER_SDRAM_SELFREFRESH) {
			/* In LP1 power mode come up on CLKM (oscillator) */
			CclkBurstPolicy = readl(CAR_CCLKG_BURST_POLICY);
			CclkBurstPolicy &= ~0xF;
			SuperCclkDivier = 0;

			writel(CclkBurstPolicy, CAR_CCLKG_BURST_POLICY);
			writel(SuperCclkDivier, CAR_SUPER_CCLKG_DIVIDER);
		}

#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
		/* Hold G CPUs 1-3 in reset after the switch */
		reg = CPU_RESET(1) | CPU_RESET(2) | CPU_RESET(3);
		writel(reg, CAR_RST_CPUG_CMPLX_SET);

		/* Take G CPU 0 out of reset after the switch */
		reg = CPU_RESET(0);
		writel(reg, CAR_RST_CPUG_CMPLX_CLR);

		/* Disable the clocks on G CPUs 1-3 after the switch */
		reg = CPU_CLOCK(1) | CPU_CLOCK(2) | CPU_CLOCK(3);
		writel(reg, CAR_CLK_CPUG_CMPLX_SET);

		/* Enable the clock on G CPU 0 after the switch */
		reg = CPU_CLOCK(0);
		writel(reg, CAR_CLK_CPUG_CMPLX_CLR);

		/* Enable the G CPU complex clock after the switch */
		reg = CAR_CLK_ENB_V_CPU_G;
		writel(reg, CAR_CLK_ENB_V_SET);
#endif
	}
	/* Switching to LP? */
	else if (flags & TEGRA_POWER_CLUSTER_LP) {
		/* Does the LP CPU exist? */
		if (reg & CAR_BOND_OUT_V_CPU_LP)
			return -ENXIO;

		/* Keep LP CPU clock policy set by upper layer, with the
		   exception of the transition via LP1 */
		if (flags & TEGRA_POWER_SDRAM_SELFREFRESH) {
			/* In LP1 power mode come up on CLKM (oscillator) */
			CclkBurstPolicy = readl(CAR_CCLKLP_BURST_POLICY);
			CclkBurstPolicy &= ~0xF;
			SuperCclkDivier = 0;

			writel(CclkBurstPolicy, CAR_CCLKLP_BURST_POLICY);
			writel(SuperCclkDivier, CAR_SUPER_CCLKLP_DIVIDER);
		}

#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
		/* Take the LP CPU ut of reset after the switch */
		reg = CPU_RESET(0);
		writel(reg, CAR_RST_CPULP_CMPLX_CLR);

		/* Enable the clock on the LP CPU after the switch */
		reg = CPU_CLOCK(0);
		writel(reg, CAR_CLK_CPULP_CMPLX_CLR);

		/* Enable the LP CPU complex clock after the switch */
		reg = CAR_CLK_ENB_V_CPU_LP;
		writel(reg, CAR_CLK_ENB_V_SET);
#endif
	}

	return 0;
}

static inline void enable_pllx_cluster_port(void)
{
	u32 val = readl(CLK_RST_CONTROLLER_PLLX_MISC_0);
	val &= (is_lp_cluster()?(~PLLX_FO_G):(~PLLX_FO_LP));
	writel(val, CLK_RST_CONTROLLER_PLLX_MISC_0);
}

static inline void disable_pllx_cluster_port(void)
{
	u32 val = readl(CLK_RST_CONTROLLER_PLLX_MISC_0);
	val |= (is_lp_cluster()?PLLX_FO_G:PLLX_FO_LP);
	writel(val, CLK_RST_CONTROLLER_PLLX_MISC_0);
}

void tegra_cluster_switch_prolog(unsigned int flags)
{
	unsigned int target_cluster = flags & TEGRA_POWER_CLUSTER_MASK;
	unsigned int current_cluster = is_lp_cluster()
					? TEGRA_POWER_CLUSTER_LP
					: TEGRA_POWER_CLUSTER_G;
	u32 reg;
	u32 cpu;

	cpu = cpu_logical_map(smp_processor_id());

	/* Read the flow controler CSR register and clear the CPU switch
	   and immediate flags. If an actual CPU switch is to be performed,
	   re-write the CSR register with the desired values. */
	reg = readl(FLOW_CTRL_CPU_CSR(cpu));
	reg &= ~(FLOW_CTRL_CSR_IMMEDIATE_WAKE |
		 FLOW_CTRL_CSR_SWITCH_CLUSTER);

	/* Program flow controller for immediate wake if requested */
	if (flags & TEGRA_POWER_CLUSTER_IMMEDIATE)
		reg |= FLOW_CTRL_CSR_IMMEDIATE_WAKE;

	/* Do nothing if no switch actions requested */
	if (!target_cluster)
		goto done;

#if defined(CONFIG_ARCH_TEGRA_HAS_SYMMETRIC_CPU_PWR_GATE)
	reg &= ~FLOW_CTRL_CSR_ENABLE_EXT_MASK;
	if ((flags & TEGRA_POWER_CLUSTER_PART_CRAIL) &&
	    ((flags & TEGRA_POWER_CLUSTER_PART_NONCPU) == 0) &&
	    (current_cluster == TEGRA_POWER_CLUSTER_LP))
		reg |= FLOW_CTRL_CSR_ENABLE_EXT_NCPU;
	else if (flags & TEGRA_POWER_CLUSTER_PART_CRAIL)
		reg |= FLOW_CTRL_CSR_ENABLE_EXT_CRAIL;

	if (flags & TEGRA_POWER_CLUSTER_PART_NONCPU)
		reg |= FLOW_CTRL_CSR_ENABLE_EXT_NCPU;
#endif

	if ((current_cluster != target_cluster) ||
		(flags & TEGRA_POWER_CLUSTER_FORCE)) {
		if (current_cluster != target_cluster) {
			// Set up the clocks for the target CPU.
			if (cluster_switch_prolog_clock(flags)) {
				/* The target CPU does not exist */
				goto done;
			}

			/* Set up the flow controller to switch CPUs. */
			reg |= FLOW_CTRL_CSR_SWITCH_CLUSTER;

			/* Enable target port of PLL_X */
			enable_pllx_cluster_port();
		}
	}

done:
	writel(reg, FLOW_CTRL_CPU_CSR(cpu));
}


static void cluster_switch_epilog_actlr(void)
{
	u32 actlr;

	/*
	 * This is only needed for Cortex-A9, for Cortex-A15, do nothing!
	 *
	 * TLB maintenance broadcast bit (FW) is stubbed out on LP CPU (reads
	 * as zero, writes ignored). Hence, it is not preserved across G=>LP=>G
	 * switch by CPU save/restore code, but SMP bit is restored correctly.
	 * Synchronize these two bits here after LP=>G transition. Note that
	 * only CPU0 core is powered on before and after the switch. See also
	 * bug 807595.
	*/
	if (((read_cpuid_id() >> 4) & 0xFFF) == 0xC0F)
		return;

	__asm__("mrc p15, 0, %0, c1, c0, 1\n" : "=r" (actlr));

	if (actlr & (0x1 << 6)) {
		actlr |= 0x1;
		__asm__("mcr p15, 0, %0, c1, c0, 1\n" : : "r" (actlr));
	}
}

static void cluster_switch_epilog_gic(void)
{
	unsigned int max_irq, i;
	void __iomem *gic_base = IO_ADDRESS(TEGRA_ARM_INT_DIST_BASE);

	/* Reprogram the interrupt affinity because the on the LP CPU,
	   the interrupt distributor affinity regsiters are stubbed out
	   by ARM (reads as zero, writes ignored). So when the LP CPU
	   context save code runs, the affinity registers will read
	   as all zero. This causes all interrupts to be effectively
	   disabled when back on the G CPU because they aren't routable
	   to any CPU. See bug 667720 for details. */

	max_irq = readl(gic_base + GIC_DIST_CTR) & 0x1f;
	max_irq = (max_irq + 1) * 32;

	for (i = 32; i < max_irq; i += 4) {
		u32 val = 0x01010101;
#ifdef CONFIG_GIC_SET_MULTIPLE_CPUS
		unsigned int irq;
		for (irq = i; irq < (i + 4); irq++) {
			struct cpumask mask;
			struct irq_desc *desc = irq_to_desc(irq);

			if (desc && desc->affinity_hint) {
				if (cpumask_and(&mask, desc->affinity_hint,
						desc->irq_data.affinity))
					val |= (*cpumask_bits(&mask) & 0xff) <<
						((irq & 3) * 8);
			}
		}
#endif
		writel(val, gic_base + GIC_DIST_TARGET + i * 4 / 4);
	}
}

void tegra_cluster_switch_epilog(unsigned int flags)
{
	u32 reg;
	u32 cpu;

	cpu = cpu_logical_map(smp_processor_id());

	/* Make sure the switch and immediate flags are cleared in
	   the flow controller to prevent undesirable side-effects
	   for future users of the flow controller. */
	reg = readl(FLOW_CTRL_CPU_CSR(cpu));
	reg &= ~(FLOW_CTRL_CSR_IMMEDIATE_WAKE |
		 FLOW_CTRL_CSR_SWITCH_CLUSTER);
#if defined(CONFIG_ARCH_TEGRA_HAS_SYMMETRIC_CPU_PWR_GATE)
	reg &= ~FLOW_CTRL_CSR_ENABLE_EXT_MASK;
#endif
	writel(reg, FLOW_CTRL_CPU_CSR(cpu));

	/* Perform post-switch LP=>G clean-up */
	if (!is_lp_cluster()) {
		cluster_switch_epilog_actlr();
		cluster_switch_epilog_gic();
	}

	/* Disable unused port of PLL_X */
	disable_pllx_cluster_port();

	#if DEBUG_CLUSTER_SWITCH
	{
		/* FIXME: clock functions below are taking mutex */
		struct clk *c = tegra_get_clock_by_name(
			is_lp_cluster() ? "cpu_lp" : "cpu_g");
		DEBUG_CLUSTER(("%s: %s freq %lu\r\n", __func__,
			is_lp_cluster() ? "LP" : "G", clk_get_rate(c)));
	}
	#endif
}

int tegra_cluster_control(unsigned int us, unsigned int flags)
{
	static ktime_t last_g2lp;

	unsigned int target_cluster = flags & TEGRA_POWER_CLUSTER_MASK;
	unsigned int current_cluster = is_lp_cluster()
					? TEGRA_POWER_CLUSTER_LP
					: TEGRA_POWER_CLUSTER_G;
	unsigned long irq_flags;

	if ((target_cluster == TEGRA_POWER_CLUSTER_MASK) || !target_cluster)
		return -EINVAL;

	if (num_online_cpus() > 1)
		return -EBUSY;

	if ((current_cluster == target_cluster)
	&& !(flags & TEGRA_POWER_CLUSTER_FORCE))
		return -EEXIST;

	if (target_cluster == TEGRA_POWER_CLUSTER_G)
		if (!is_g_cluster_present())
			return -EPERM;

	trace_power_start(POWER_PSTATE, target_cluster, 0);

	if (flags & TEGRA_POWER_CLUSTER_IMMEDIATE)
		us = 0;

	DEBUG_CLUSTER(("%s(LP%d): %s->%s %s %s %d\r\n", __func__,
		(flags & TEGRA_POWER_SDRAM_SELFREFRESH) ? 1 : 2,
		is_lp_cluster() ? "LP" : "G",
		(target_cluster == TEGRA_POWER_CLUSTER_G) ? "G" : "LP",
		(flags & TEGRA_POWER_CLUSTER_IMMEDIATE) ? "immediate" : "",
		(flags & TEGRA_POWER_CLUSTER_FORCE) ? "force" : "",
		us));

	local_irq_save(irq_flags);

	if (current_cluster != target_cluster && !timekeeping_suspended) {
		ktime_t now = ktime_get();
		if (target_cluster == TEGRA_POWER_CLUSTER_G) {
			s64 t = ktime_to_us(ktime_sub(now, last_g2lp));
			s64 t_off = tegra_cpu_power_off_time();

			if (t_off > t)
				udelay((unsigned int)(t_off - t));

			tegra_dvfs_rail_on(tegra_cpu_rail, now);
		} else {
#ifdef CONFIG_TEGRA_VIRTUAL_CPUID
			u32 cpu;

			cpu = cpu_logical_map(smp_processor_id());
			writel(cpu, FLOW_CTRL_MPID);
#endif
			last_g2lp = now;
			tegra_dvfs_rail_off(tegra_cpu_rail, now);
		}
	}

	if (flags & TEGRA_POWER_SDRAM_SELFREFRESH) {
		if (us)
			tegra_pd_set_trigger(us);

		tegra_cluster_switch_prolog(flags);
		tegra_suspend_dram(TEGRA_SUSPEND_LP1, flags);
		tegra_cluster_switch_epilog(flags);

		if (us)
			tegra_pd_set_trigger(0);
	} else {
		int cpu;

		cpu = cpu_logical_map(smp_processor_id());

		tegra_set_cpu_in_pd(cpu);
		cpu_pm_enter();
		if (!timekeeping_suspended)
			clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER,
					   &cpu);
		tegra_idle_power_down_last(0, flags);
		if (!timekeeping_suspended)
			clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT,
					   &cpu);
		cpu_pm_exit();
		tegra_clear_cpu_in_pd(cpu);
	}
	local_irq_restore(irq_flags);

	DEBUG_CLUSTER(("%s: %s\r\n", __func__, is_lp_cluster() ? "LP" : "G"));

	return 0;
}

int tegra_switch_to_lp_cluster()
{
	struct clk *cpu_clk = tegra_get_clock_by_name("cpu");
	struct clk *cpu_lp_clk = tegra_get_clock_by_name("cpu_lp");
	int rate = clk_get_rate(cpu_clk);
	int e;

	if (is_lp_cluster())
		return 0;

	/* Change the Clock Rate to desired LP CPU's clock rate */

	if (rate > cpu_lp_clk->max_rate) {
		e = clk_set_rate(cpu_clk, cpu_lp_clk->max_rate);
		if (e) {
			pr_err("cluster_swtich: Failed to set clock %d", e);
			return e;
		}
	}

	e = clk_set_parent(cpu_clk, cpu_lp_clk);
	if (e) {
		pr_err("cluster switching request failed (%d)\n", e);
		return e;
	}
	return e;
}

int tegra_switch_to_g_cluster()
{
	struct clk *cpu_clk = tegra_get_clock_by_name("cpu");
	struct clk *cpu_g_clk = tegra_get_clock_by_name("cpu_g");
	int e;

	if (!is_lp_cluster())
		return 0;

	e = clk_set_parent(cpu_clk, cpu_g_clk);
	if (e) {
		pr_err("cluster switching request failed (%d)\n", e);
		return e;
	}

	/* Switch back to G Cluster Cpu Max Clock rate */

	e = clk_set_rate(cpu_clk, cpu_g_clk->max_rate);
	if (e) {
		pr_err("cluster_swtich: Failed to increase the clock %d\n", e);
		return e;
	}
	return e;
}

int tegra_cluster_switch(struct clk *cpu_clk, struct clk *new_cluster_clk)
{
	int ret;
	bool is_target_lp = is_lp_cluster() ^
		(clk_get_parent(cpu_clk) != new_cluster_clk);

	/* Update core edp limits before switch to LP cluster; abort on error */
	if (is_target_lp) {
		ret = tegra_core_edp_cpu_state_update(is_target_lp);
		if (ret)
			return ret;
	}

	ret = clk_set_parent(cpu_clk, new_cluster_clk);
	if (ret)
		return ret;

	/* Update core edp limits after switch to G cluster; ignore error */
	if (!is_target_lp)
		tegra_core_edp_cpu_state_update(is_target_lp);

	return 0;
}
#endif

#ifdef CONFIG_PM_SLEEP

void tegra_lp0_suspend_mc(void)
{
	/* Since memory frequency after LP0 is restored to boot rate
	   mc timing is saved during init, not on entry to LP0. Keep
	   this hook just in case, anyway */
}

void tegra_lp0_resume_mc(void)
{
	tegra_mc_timing_restore();
}

static int __init get_clock_cclk_lp(void)
{
	if (!cclk_lp)
		cclk_lp = tegra_get_clock_by_name("cclk_lp");
	return 0;
}

subsys_initcall(get_clock_cclk_lp);

void tegra_lp0_cpu_mode(bool enter)
{
	static bool entered_on_g = false;
	unsigned int flags;

	if (enter)
		entered_on_g = !is_lp_cluster();

	if (entered_on_g) {
		if (enter)
			tegra_clk_prepare_enable(cclk_lp);

		flags = enter ? TEGRA_POWER_CLUSTER_LP : TEGRA_POWER_CLUSTER_G;
		flags |= TEGRA_POWER_CLUSTER_IMMEDIATE;
#if defined(CONFIG_ARCH_TEGRA_HAS_SYMMETRIC_CPU_PWR_GATE)
		flags |= TEGRA_POWER_CLUSTER_PART_DEFAULT;
#endif
		if (!tegra_cluster_control(0, flags)) {
			if (!enter)
				tegra_clk_disable_unprepare(cclk_lp);
			pr_info("Tegra: switched to %s cluster\n",
				enter ? "LP" : "G");
		}
	}
}

#define IO_DPD_INFO(_name, _index, _bit) \
	{ \
		.name = _name, \
		.io_dpd_reg_index = _index, \
		.io_dpd_bit = _bit, \
	}

/* PMC IO DPD register offsets */
#define APBDEV_PMC_IO_DPD_REQ_0		0x1b8
#define APBDEV_PMC_IO_DPD_STATUS_0	0x1bc
#define APBDEV_PMC_SEL_DPD_TIM_0	0x1c8
#define APBDEV_DPD_ENABLE_LSB		30
#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
#define APBDEV_DPD2_ENABLE_LSB		5
#else
#define APBDEV_DPD2_ENABLE_LSB		30
#endif
#define PMC_DPD_SAMPLE			0x20

static struct tegra_io_dpd tegra_list_io_dpd[] = {
};
#endif

/* we want to cleanup bootloader io dpd setting in kernel */
static void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);

#ifdef CONFIG_PM_SLEEP
struct tegra_io_dpd *tegra_io_dpd_get(struct device *dev)
{
#ifdef CONFIG_TEGRA_IO_DPD
	int i;
	const char *name = dev ? dev_name(dev) : NULL;
	if (name) {
		for (i = 0; i < ARRAY_SIZE(tegra_list_io_dpd); i++) {
			if (!(strncmp(tegra_list_io_dpd[i].name, name,
				strlen(name)))) {
				return &tegra_list_io_dpd[i];
			}
		}
	}
	dev_info(dev, "Error: tegra3 io dpd not supported for %s\n",
		((name) ? name : "NULL"));
#endif
	return NULL;
}

static DEFINE_SPINLOCK(tegra_io_dpd_lock);

void tegra_io_dpd_enable(struct tegra_io_dpd *hnd)
{
	unsigned int enable_mask;
	unsigned int dpd_status;
	unsigned int dpd_enable_lsb;

	if (!hnd)
		return;

	spin_lock(&tegra_io_dpd_lock);
	dpd_enable_lsb = (hnd->io_dpd_reg_index) ? APBDEV_DPD2_ENABLE_LSB :
						APBDEV_DPD_ENABLE_LSB;
	writel(0x1, pmc + PMC_DPD_SAMPLE);
	writel(0x10, pmc + APBDEV_PMC_SEL_DPD_TIM_0);
	enable_mask = ((1 << hnd->io_dpd_bit) | (2 << dpd_enable_lsb));
	writel(enable_mask, pmc + (APBDEV_PMC_IO_DPD_REQ_0 +
					hnd->io_dpd_reg_index * 8));
	udelay(1);
	dpd_status = readl(pmc + (APBDEV_PMC_IO_DPD_STATUS_0 +
					hnd->io_dpd_reg_index * 8));
	if (!(dpd_status & (1 << hnd->io_dpd_bit))) {
#if !defined(CONFIG_TEGRA_FPGA_PLATFORM)
		pr_info("Error: dpd%d enable failed, status=%#x\n",
		(hnd->io_dpd_reg_index + 1), dpd_status);
#endif
	}
	/* Sample register must be reset before next sample operation */
	writel(0x0, pmc + PMC_DPD_SAMPLE);
	spin_unlock(&tegra_io_dpd_lock);
	return;
}

void tegra_io_dpd_disable(struct tegra_io_dpd *hnd)
{
	unsigned int enable_mask;
	unsigned int dpd_status;
	unsigned int dpd_enable_lsb;

	if (!hnd)
		return;

	spin_lock(&tegra_io_dpd_lock);
	dpd_enable_lsb = (hnd->io_dpd_reg_index) ? APBDEV_DPD2_ENABLE_LSB :
						APBDEV_DPD_ENABLE_LSB;
	enable_mask = ((1 << hnd->io_dpd_bit) | (1 << dpd_enable_lsb));
	writel(enable_mask, pmc + (APBDEV_PMC_IO_DPD_REQ_0 +
					hnd->io_dpd_reg_index * 8));
	dpd_status = readl(pmc + (APBDEV_PMC_IO_DPD_STATUS_0 +
					hnd->io_dpd_reg_index * 8));
	if (dpd_status & (1 << hnd->io_dpd_bit)) {
#if !defined(CONFIG_TEGRA_FPGA_PLATFORM)
		pr_info("Error: dpd%d disable failed, status=%#x\n",
		(hnd->io_dpd_reg_index + 1), dpd_status);
#endif
	}
	spin_unlock(&tegra_io_dpd_lock);
	return;
}

static void tegra_io_dpd_delayed_disable(struct work_struct *work)
{
	struct tegra_io_dpd *hnd = container_of(
		to_delayed_work(work), struct tegra_io_dpd, delay_dpd);
	tegra_io_dpd_disable(hnd);
	hnd->need_delay_dpd = 0;
}

int tegra_io_dpd_init(void)
{
	int i;
	for (i = 0;
		i < (sizeof(tegra_list_io_dpd) / sizeof(struct tegra_io_dpd));
		i++) {
			INIT_DELAYED_WORK(&(tegra_list_io_dpd[i].delay_dpd),
				tegra_io_dpd_delayed_disable);
			mutex_init(&(tegra_list_io_dpd[i].delay_lock));
			tegra_list_io_dpd[i].need_delay_dpd = 0;
	}
	return 0;
}

#else

int tegra_io_dpd_init(void)
{
	return 0;
}

void tegra_io_dpd_enable(struct tegra_io_dpd *hnd)
{
}

void tegra_io_dpd_disable(struct tegra_io_dpd *hnd)
{
}

struct tegra_io_dpd *tegra_io_dpd_get(struct device *dev)
{
	return NULL;
}

#endif

EXPORT_SYMBOL(tegra_io_dpd_get);
EXPORT_SYMBOL(tegra_io_dpd_enable);
EXPORT_SYMBOL(tegra_io_dpd_disable);
EXPORT_SYMBOL(tegra_io_dpd_init);

struct io_dpd_reg_info {
	u32 req_reg_off;
	u8 dpd_code_lsb;
};

static struct io_dpd_reg_info t3_io_dpd_req_regs[] = {
	{0x1b8, 30},
	{0x1c0, 5},
};

/* io dpd off request code */
#define IO_DPD_CODE_OFF		1

/* cleans io dpd settings from bootloader during kernel init */
void tegra_bl_io_dpd_cleanup()
{
	int i;
	unsigned int dpd_mask;
	unsigned int dpd_status;

	pr_info("Clear bootloader IO dpd settings\n");
	/* clear all dpd requests from bootloader */
	for (i = 0; i < ARRAY_SIZE(t3_io_dpd_req_regs); i++) {
		dpd_mask = ((1 << t3_io_dpd_req_regs[i].dpd_code_lsb) - 1);
		dpd_mask |= (IO_DPD_CODE_OFF <<
			t3_io_dpd_req_regs[i].dpd_code_lsb);
		writel(dpd_mask, pmc + t3_io_dpd_req_regs[i].req_reg_off);
		/* dpd status register is next to req reg in tegra3 */
		dpd_status = readl(pmc +
			(t3_io_dpd_req_regs[i].req_reg_off + 4));
	}
	return;
}
EXPORT_SYMBOL(tegra_bl_io_dpd_cleanup);

