/*
 * arch/arm/mach-tegra/pm.c
 *
 * CPU complex suspend & resume functions for Tegra SoCs
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/slab.h>
#include <linux/serial_reg.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/syscore_ops.h>
#include <linux/cpu_pm.h>
#include <linux/vmalloc.h>
#include <linux/memblock.h>
#include <linux/console.h>
#include <linux/pm_qos.h>
#include <linux/export.h>
#include <linux/tegra_audio.h>

#include <trace/events/power.h>
#include <trace/events/nvsecurity.h>

#include <asm/cacheflush.h>
#include <asm/hardware/gic.h>
#include <asm/idmap.h>
#include <asm/localtimer.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>
#include <asm/suspend.h>
#include <asm/smp_plat.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/powergate.h>
#include <mach/hardware.h>

#include "board.h"
#include "clock.h"
#include "common.h"
#include "cpuidle.h"
#include "fuse.h"
#include "gic.h"
#include "pm.h"
#include "pm-irq.h"
#include "reset.h"
#include "sleep.h"
#include "timer.h"
#include "dvfs.h"
#include "cpu-tegra.h"

#define CREATE_TRACE_POINTS
#include <trace/events/nvpower.h>

struct suspend_context {
	/*
	 * The next 7 values are referenced by offset in __restart_plls
	 * in headsmp-t2.S, and should not be moved
	 */
	u32 pllx_misc;
	u32 pllx_base;
	u32 pllp_misc;
	u32 pllp_base;
	u32 pllp_outa;
	u32 pllp_outb;
	u32 pll_timeout;

	u32 cpu_burst;
	u32 clk_csite_src;
	u32 cclk_divider;

	u32 mc[3];
	u8 uart[5];

	struct tegra_twd_context twd;
};

#ifdef CONFIG_PM_SLEEP
phys_addr_t tegra_pgd_phys;	/* pgd used by hotplug & LP2 bootup */
static pgd_t *tegra_pgd;
static DEFINE_SPINLOCK(tegra_lp2_lock);
static cpumask_t tegra_in_lp2;
static cpumask_t *iram_cpu_lp2_mask;
static unsigned long *iram_cpu_lp1_mask;
static u8 *iram_save;
static unsigned long iram_save_size;
static void __iomem *iram_code = IO_ADDRESS(TEGRA_IRAM_CODE_AREA);
static void __iomem *clk_rst = IO_ADDRESS(TEGRA_CLK_RESET_BASE);
static void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
static void __iomem *tmrus_reg_base = IO_ADDRESS(TEGRA_TMR1_BASE);
static int tegra_last_pclk;
static u64 resume_time;
static u64 resume_entry_time;
static u64 suspend_time;
static u64 suspend_entry_time;
static u64 suspend_end_time;
#endif

struct suspend_context tegra_sctx;

#define TEGRA_POWER_PWRREQ_POLARITY	(1 << 8)   /* core power request polarity */
#define TEGRA_POWER_PWRREQ_OE		(1 << 9)   /* core power request enable */
#define TEGRA_POWER_SYSCLK_POLARITY	(1 << 10)  /* sys clk polarity */
#define TEGRA_POWER_SYSCLK_OE		(1 << 11)  /* system clock enable */
#define TEGRA_POWER_PWRGATE_DIS		(1 << 12)  /* power gate disabled */
#define TEGRA_POWER_EFFECT_LP0		(1 << 14)  /* enter LP0 when CPU pwr gated */
#define TEGRA_POWER_CPU_PWRREQ_POLARITY (1 << 15)  /* CPU power request polarity */
#define TEGRA_POWER_CPU_PWRREQ_OE	(1 << 16)  /* CPU power request enable */
#define TEGRA_POWER_CPUPWRGOOD_EN	(1 << 19)  /* CPU power good enable */

#define TEGRA_DPAD_ORIDE_SYS_CLK_REQ	(1 << 21)

#define PMC_CTRL		0x0
#define PMC_CTRL_LATCH_WAKEUPS	(1 << 5)
#define PMC_WAKE_MASK		0xc
#define PMC_WAKE_LEVEL		0x10
#define PMC_DPAD_ORIDE		0x1C
#define PMC_WAKE_DELAY		0xe0
#define PMC_DPD_SAMPLE		0x20

#define PMC_WAKE_STATUS		0x14
#define PMC_SW_WAKE_STATUS	0x18
#define PMC_COREPWRGOOD_TIMER	0x3c
#define PMC_CPUPWRGOOD_TIMER	0xc8
#define PMC_CPUPWROFF_TIMER	0xcc
#define PMC_COREPWROFF_TIMER	PMC_WAKE_DELAY

#define PMC_PWRGATE_TOGGLE	0x30
#define PWRGATE_TOGGLE_START	(1 << 8)
#define UN_PWRGATE_CPU		\
	(PWRGATE_TOGGLE_START | TEGRA_CPU_POWERGATE_ID(TEGRA_POWERGATE_CPU))

#ifdef CONFIG_TEGRA_CLUSTER_CONTROL
#define PMC_SCRATCH4_WAKE_CLUSTER_MASK	(1<<31)
#endif

#define CLK_RESET_CCLK_BURST	0x20
#define CLK_RESET_CCLK_DIVIDER  0x24
#define CLK_RESET_PLLC_BASE	0x80
#define CLK_RESET_PLLM_BASE	0x90
#define CLK_RESET_PLLX_BASE	0xe0
#define CLK_RESET_PLLX_MISC	0xe4
#define CLK_RESET_PLLP_BASE	0xa0
#define CLK_RESET_PLLP_OUTA	0xa4
#define CLK_RESET_PLLP_OUTB	0xa8
#define CLK_RESET_PLLP_MISC	0xac

#define CLK_RESET_SOURCE_CSITE	0x1d4

#define CLK_RESET_CCLK_BURST_POLICY_SHIFT 28
#define CLK_RESET_CCLK_RUN_POLICY_SHIFT    4
#define CLK_RESET_CCLK_IDLE_POLICY_SHIFT   0
#define CLK_RESET_CCLK_IDLE_POLICY	   1
#define CLK_RESET_CCLK_RUN_POLICY	   2
#define CLK_RESET_CCLK_BURST_POLICY_PLLM   3
#define CLK_RESET_CCLK_BURST_POLICY_PLLX   8

#define EMC_MRW_0		0x0e8
#define EMC_MRW_DEV_SELECTN     30
#define EMC_MRW_DEV_NONE	(3 << EMC_MRW_DEV_SELECTN)

#define MC_SECURITY_START	0x6c
#define MC_SECURITY_SIZE	0x70
#define MC_SECURITY_CFG2	0x7c

#define AWAKE_CPU_FREQ_MIN	51000
static struct pm_qos_request awake_cpu_freq_req;

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
static struct clk *tegra_dfll;
#endif
static struct clk *tegra_pclk;
static const struct tegra_suspend_platform_data *pdata;
static enum tegra_suspend_mode current_suspend_mode = TEGRA_SUSPEND_NONE;

#if defined(CONFIG_TEGRA_CLUSTER_CONTROL) && INSTRUMENT_CLUSTER_SWITCH
enum tegra_cluster_switch_time_id {
	tegra_cluster_switch_time_id_start = 0,
	tegra_cluster_switch_time_id_prolog,
	tegra_cluster_switch_time_id_switch,
	tegra_cluster_switch_time_id_epilog,
	tegra_cluster_switch_time_id_max
};

static unsigned long
		tegra_cluster_switch_times[tegra_cluster_switch_time_id_max];
#define tegra_cluster_switch_time(flags, id) \
	do { \
		barrier(); \
		if (flags & TEGRA_POWER_CLUSTER_MASK) { \
			void __iomem *timer_us = \
						IO_ADDRESS(TEGRA_TMRUS_BASE); \
			if (id < tegra_cluster_switch_time_id_max) \
				tegra_cluster_switch_times[id] = \
							readl(timer_us); \
				wmb(); \
		} \
		barrier(); \
	} while(0)
#else
#define tegra_cluster_switch_time(flags, id) do {} while(0)
#endif

#ifdef CONFIG_PM_SLEEP
static const char *tegra_suspend_name[TEGRA_MAX_SUSPEND_MODE] = {
	[TEGRA_SUSPEND_NONE]	= "none",
	[TEGRA_SUSPEND_LP2]	= "lp2",
	[TEGRA_SUSPEND_LP1]	= "lp1",
	[TEGRA_SUSPEND_LP0]	= "lp0",
};

void tegra_log_resume_time(void)
{
	u64 resume_end_time = readl(tmrus_reg_base + TIMERUS_CNTR_1US);

	if (resume_entry_time > resume_end_time)
		resume_end_time |= 1ull<<32;
	resume_time = resume_end_time - resume_entry_time;
}

void tegra_log_suspend_time(void)
{
	suspend_entry_time = readl(tmrus_reg_base + TIMERUS_CNTR_1US);
	suspend_end_time = 0;
}

static void tegra_get_suspend_time(void)
{
	if (suspend_end_time)
		return;
	suspend_end_time = readl(tmrus_reg_base + TIMERUS_CNTR_1US);

	if (suspend_entry_time > suspend_end_time)
		suspend_end_time |= 1ull<<32;
	suspend_time = suspend_end_time - suspend_entry_time;
}

unsigned long tegra_cpu_power_good_time(void)
{
	if (WARN_ON_ONCE(!pdata))
		return 5000;

	return pdata->cpu_timer;
}

unsigned long tegra_cpu_power_off_time(void)
{
	if (WARN_ON_ONCE(!pdata))
		return 5000;

	return pdata->cpu_off_timer;
}

unsigned long tegra_cpu_lp2_min_residency(void)
{
	if (WARN_ON_ONCE(!pdata))
		return 2000;

	return pdata->cpu_lp2_min_residency;
}

#ifdef CONFIG_ARCH_TEGRA_HAS_SYMMETRIC_CPU_PWR_GATE
#define TEGRA_MIN_RESIDENCY_VMIN_FMIN	2000
#define TEGRA_MIN_RESIDENCY_NCPU_SLOW	2000
#define TEGRA_MIN_RESIDENCY_NCPU_FAST	13000
#define TEGRA_MIN_RESIDENCY_CRAIL	20000

unsigned long tegra_min_residency_vmin_fmin(void)
{
	return pdata && pdata->min_residency_vmin_fmin
			? pdata->min_residency_vmin_fmin
			: TEGRA_MIN_RESIDENCY_VMIN_FMIN;
}

unsigned long tegra_min_residency_ncpu()
{
	if (is_lp_cluster()) {
		return pdata && pdata->min_residency_ncpu_slow
			? pdata->min_residency_ncpu_slow
			: TEGRA_MIN_RESIDENCY_NCPU_SLOW;
	} else
		return pdata && pdata->min_residency_ncpu_fast
			? pdata->min_residency_ncpu_fast
			: TEGRA_MIN_RESIDENCY_NCPU_FAST;
}

unsigned long tegra_min_residency_crail(void)
{
	return pdata && pdata->min_residency_crail
			? pdata->min_residency_crail
			: TEGRA_MIN_RESIDENCY_CRAIL;
}
#endif

static void suspend_cpu_dfll_mode(void)
{
#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	/* If DFLL is used as CPU clock source go to open loop mode */
	if (!is_lp_cluster() && tegra_dfll &&
	    tegra_dvfs_rail_is_dfll_mode(tegra_cpu_rail))
		tegra_clk_cfg_ex(tegra_dfll, TEGRA_CLK_DFLL_LOCK, 0);
#endif
}

static void resume_cpu_dfll_mode(void)
{
#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	/* If DFLL is used as CPU clock source restore closed loop mode */
	if (!is_lp_cluster() && tegra_dfll &&
	    tegra_dvfs_rail_is_dfll_mode(tegra_cpu_rail))
		tegra_clk_cfg_ex(tegra_dfll, TEGRA_CLK_DFLL_LOCK, 1);
#endif
}

/*
 * create_suspend_pgtable
 *
 * Creates a page table with identity mappings of physical memory and IRAM
 * for use when the MMU is off, in addition to all the regular kernel mappings.
 */
static __init int create_suspend_pgtable(void)
{
	tegra_pgd = pgd_alloc(&init_mm);
	if (!tegra_pgd)
		return -ENOMEM;

	/* Only identity-map size of lowmem (high_memory - PAGE_OFFSET) */
	identity_mapping_add(tegra_pgd, PHYS_OFFSET,
		PHYS_OFFSET + (unsigned long)high_memory - PAGE_OFFSET);
	identity_mapping_add(tegra_pgd, IO_IRAM_PHYS,
		IO_IRAM_PHYS + SECTION_SIZE);

	/* inner/outer write-back/write-allocate, sharable */
	tegra_pgd_phys = (virt_to_phys(tegra_pgd) & PAGE_MASK) | 0x4A;

	return 0;
}

/* ensures that sufficient time is passed for a register write to
 * serialize into the 32KHz domain */
static void pmc_32kwritel(u32 val, unsigned long offs)
{
	writel(val, pmc + offs);
	udelay(130);
}

static void set_power_timers(unsigned long us_on, unsigned long us_off,
			     long rate)
{
	static unsigned long last_us_off = 0;
	unsigned long long ticks;
	unsigned long long pclk;

	if (WARN_ON_ONCE(rate <= 0))
		pclk = 100000000;
	else
		pclk = rate;

	if ((rate != tegra_last_pclk) || (us_off != last_us_off)) {
		ticks = (us_on * pclk) + 999999ull;
		do_div(ticks, 1000000);
		writel((unsigned long)ticks, pmc + PMC_CPUPWRGOOD_TIMER);

		ticks = (us_off * pclk) + 999999ull;
		do_div(ticks, 1000000);
		writel((unsigned long)ticks, pmc + PMC_CPUPWROFF_TIMER);
		wmb();
	}
	tegra_last_pclk = pclk;
	last_us_off = us_off;
}

/*
 * restore_cpu_complex
 *
 * restores cpu clock setting, clears flow controller
 *
 * Always called on CPU 0.
 */
static void restore_cpu_complex(u32 mode)
{
	int cpu = cpu_logical_map(smp_processor_id());
	unsigned int reg;
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
	unsigned int policy;
#endif

/*
 * On Tegra11x PLLX and CPU burst policy is either preserved across LP2,
 * or restored by common clock suspend/resume procedures. Hence, we don't
 * need it here.
 */
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
	/* Is CPU complex already running on PLLX? */
	reg = readl(clk_rst + CLK_RESET_CCLK_BURST);
	policy = (reg >> CLK_RESET_CCLK_BURST_POLICY_SHIFT) & 0xF;

	if (policy == CLK_RESET_CCLK_IDLE_POLICY)
		reg = (reg >> CLK_RESET_CCLK_IDLE_POLICY_SHIFT) & 0xF;
	else if (policy == CLK_RESET_CCLK_RUN_POLICY)
		reg = (reg >> CLK_RESET_CCLK_RUN_POLICY_SHIFT) & 0xF;
	else
		BUG();

	if (reg != CLK_RESET_CCLK_BURST_POLICY_PLLX) {
		/* restore PLLX settings if CPU is on different PLL */
		writel(tegra_sctx.pllx_misc, clk_rst + CLK_RESET_PLLX_MISC);
		writel(tegra_sctx.pllx_base, clk_rst + CLK_RESET_PLLX_BASE);

		/* wait for PLL stabilization if PLLX was enabled */
		if (tegra_sctx.pllx_base & (1<<30)) {
#if USE_PLL_LOCK_BITS
			/* Enable lock detector */
			reg = readl(clk_rst + CLK_RESET_PLLX_MISC);
			reg |= 1<<18;
			writel(reg, clk_rst + CLK_RESET_PLLX_MISC);
			while (!(readl(clk_rst + CLK_RESET_PLLX_BASE) &
				 (1<<27)))
				cpu_relax();

			udelay(PLL_POST_LOCK_DELAY);
#else
			udelay(300);
#endif
		}
	}

	/* Restore original burst policy setting for calls resulting from CPU
	   LP2 in idle or system suspend; keep cluster switch prolog setting
	   intact. */
	if (!(mode & TEGRA_POWER_CLUSTER_MASK)) {
		writel(tegra_sctx.cclk_divider, clk_rst +
		       CLK_RESET_CCLK_DIVIDER);
		writel(tegra_sctx.cpu_burst, clk_rst +
		       CLK_RESET_CCLK_BURST);
	}
#endif
	writel(tegra_sctx.clk_csite_src, clk_rst + CLK_RESET_SOURCE_CSITE);

	/* Do not power-gate CPU 0 when flow controlled */
	reg = readl(FLOW_CTRL_CPU_CSR(cpu));
	reg &= ~FLOW_CTRL_CSR_WFE_BITMAP;	/* clear wfe bitmap */
	reg &= ~FLOW_CTRL_CSR_WFI_BITMAP;	/* clear wfi bitmap */
	reg &= ~FLOW_CTRL_CSR_ENABLE;		/* clear enable */
	reg |= FLOW_CTRL_CSR_INTR_FLAG;		/* clear intr */
	reg |= FLOW_CTRL_CSR_EVENT_FLAG;	/* clear event */
	flowctrl_writel(reg, FLOW_CTRL_CPU_CSR(cpu));

	/* If an immedidate cluster switch is being perfomed, restore the
	   local timer registers. For calls resulting from CPU LP2 in
	   idle or system suspend, the local timer was shut down and
	   timekeeping switched over to the global system timer. In this
	   case keep local timer disabled, and restore only periodic load. */
#ifdef CONFIG_HAVE_ARM_TWD
	if (!(mode & (TEGRA_POWER_CLUSTER_MASK |
		      TEGRA_POWER_CLUSTER_IMMEDIATE))) {
		tegra_sctx.twd.twd_ctrl = 0;
	}
	tegra_twd_resume(&tegra_sctx.twd);
#endif
}

/*
 * suspend_cpu_complex
 *
 * saves pll state for use by restart_plls, prepares flow controller for
 * transition to suspend state
 *
 * Must always be called on cpu 0.
 */
static void suspend_cpu_complex(u32 mode)
{
	int cpu = cpu_logical_map(smp_processor_id());
	unsigned int reg;
	int i;

	BUG_ON(cpu != 0);

	/* switch coresite to clk_m, save off original source */
	tegra_sctx.clk_csite_src = readl(clk_rst + CLK_RESET_SOURCE_CSITE);
	writel(3<<30, clk_rst + CLK_RESET_SOURCE_CSITE);

	tegra_sctx.cpu_burst = readl(clk_rst + CLK_RESET_CCLK_BURST);
	tegra_sctx.pllx_base = readl(clk_rst + CLK_RESET_PLLX_BASE);
	tegra_sctx.pllx_misc = readl(clk_rst + CLK_RESET_PLLX_MISC);
	tegra_sctx.pllp_base = readl(clk_rst + CLK_RESET_PLLP_BASE);
	tegra_sctx.pllp_outa = readl(clk_rst + CLK_RESET_PLLP_OUTA);
	tegra_sctx.pllp_outb = readl(clk_rst + CLK_RESET_PLLP_OUTB);
	tegra_sctx.pllp_misc = readl(clk_rst + CLK_RESET_PLLP_MISC);
	tegra_sctx.cclk_divider = readl(clk_rst + CLK_RESET_CCLK_DIVIDER);

#ifdef CONFIG_HAVE_ARM_TWD
	tegra_twd_suspend(&tegra_sctx.twd);
#endif

	reg = readl(FLOW_CTRL_CPU_CSR(cpu));
	reg &= ~FLOW_CTRL_CSR_WFE_BITMAP;	/* clear wfe bitmap */
	reg &= ~FLOW_CTRL_CSR_WFI_BITMAP;	/* clear wfi bitmap */
	reg |= FLOW_CTRL_CSR_INTR_FLAG;		/* clear intr flag */
	reg |= FLOW_CTRL_CSR_EVENT_FLAG;	/* clear event flag */
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	reg |= FLOW_CTRL_CSR_WFE_CPU0 << cpu;	/* enable power gating on wfe */
#else
	reg |= FLOW_CTRL_CSR_WFI_CPU0 << cpu;	/* enable power gating on wfi */
#endif
	reg |= FLOW_CTRL_CSR_ENABLE;		/* enable power gating */
	flowctrl_writel(reg, FLOW_CTRL_CPU_CSR(cpu));

	for (i = 0; i < num_possible_cpus(); i++) {
		if (i == cpu)
			continue;
		reg = readl(FLOW_CTRL_CPU_CSR(i));
		reg |= FLOW_CTRL_CSR_EVENT_FLAG;
		reg |= FLOW_CTRL_CSR_INTR_FLAG;
		flowctrl_writel(reg, FLOW_CTRL_CPU_CSR(i));
	}

	tegra_gic_cpu_disable(true);
}

void tegra_clear_cpu_in_pd(int cpu)
{
	spin_lock(&tegra_lp2_lock);
	BUG_ON(!cpumask_test_cpu(cpu, &tegra_in_lp2));
	cpumask_clear_cpu(cpu, &tegra_in_lp2);

	/* Update the IRAM copy used by the reset handler. The IRAM copy
	   can't use used directly by cpumask_clear_cpu() because it uses
	   LDREX/STREX which requires the addressed location to be inner
	   cacheable and sharable which IRAM isn't. */
	writel(tegra_in_lp2.bits[0], iram_cpu_lp2_mask);
	dsb();

	spin_unlock(&tegra_lp2_lock);
}

bool tegra_set_cpu_in_pd(int cpu)
{
	bool last_cpu = false;

	spin_lock(&tegra_lp2_lock);
	BUG_ON(cpumask_test_cpu(cpu, &tegra_in_lp2));
	cpumask_set_cpu(cpu, &tegra_in_lp2);

	/* Update the IRAM copy used by the reset handler. The IRAM copy
	   can't use used directly by cpumask_set_cpu() because it uses
	   LDREX/STREX which requires the addressed location to be inner
	   cacheable and sharable which IRAM isn't. */
	writel(tegra_in_lp2.bits[0], iram_cpu_lp2_mask);
	dsb();

	if ((cpu == 0) && cpumask_equal(&tegra_in_lp2, cpu_online_mask))
		last_cpu = true;
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	else if (cpu == 1)
		tegra2_cpu_set_resettable_soon();
#endif

	spin_unlock(&tegra_lp2_lock);
	return last_cpu;
}

static void tegra_sleep_core(enum tegra_suspend_mode mode,
			     unsigned long v2p)
{
#ifdef CONFIG_TRUSTED_FOUNDATIONS
	outer_flush_range(__pa(&tegra_resume_timestamps_start),
			  __pa(&tegra_resume_timestamps_end));

	if (mode == TEGRA_SUSPEND_LP0) {
		trace_smc_sleep_core(NVSEC_SMC_START);

		tegra_generic_smc(0xFFFFFFFC, 0xFFFFFFE3,
				  virt_to_phys(tegra_resume));
	} else {
		trace_smc_sleep_core(NVSEC_SMC_START);

		tegra_generic_smc(0xFFFFFFFC, 0xFFFFFFE6,
				  (TEGRA_RESET_HANDLER_BASE +
				   tegra_cpu_reset_handler_offset));
	}

	trace_smc_sleep_core(NVSEC_SMC_DONE);
#endif
	tegra_get_suspend_time();
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	cpu_suspend(v2p, tegra2_sleep_core_finish);
#else
	cpu_suspend(v2p, tegra3_sleep_core_finish);
#endif
}

static inline void tegra_sleep_cpu(unsigned long v2p)
{
	cpu_suspend(v2p, tegra_sleep_cpu_finish);
}

unsigned int tegra_idle_power_down_last(unsigned int sleep_time,
					unsigned int flags)
{
	u32 reg;
	unsigned int remain;
#ifdef CONFIG_CACHE_L2X0
	pgd_t *pgd;
#endif

	/* Only the last cpu down does the final suspend steps */
	reg = readl(pmc + PMC_CTRL);
	reg |= TEGRA_POWER_CPU_PWRREQ_OE;
	if (pdata->combined_req)
		reg &= ~TEGRA_POWER_PWRREQ_OE;
	else
		reg |= TEGRA_POWER_PWRREQ_OE;

	reg &= ~TEGRA_POWER_EFFECT_LP0;
	writel(reg, pmc + PMC_CTRL);

	tegra_cluster_switch_time(flags, tegra_cluster_switch_time_id_start);

	/*
	 * We can use clk_get_rate_all_locked() here, because all other cpus
	 * are in LP2 state and irqs are disabled
	 */
	if (flags & TEGRA_POWER_CLUSTER_MASK) {
		if (is_idle_task(current))
			trace_nvcpu_cluster_rcuidle(NVPOWER_CPU_CLUSTER_START);
		else
			trace_nvcpu_cluster(NVPOWER_CPU_CLUSTER_START);
		set_power_timers(pdata->cpu_timer, 2,
			clk_get_rate_all_locked(tegra_pclk));
		if (flags & TEGRA_POWER_CLUSTER_G) {
			/*
			 * To reduce the vdd_cpu up latency when LP->G
			 * transition. Before the transition, enable
			 * the vdd_cpu rail.
			 */
			if (is_lp_cluster()) {
#if defined(CONFIG_ARCH_TEGRA_HAS_SYMMETRIC_CPU_PWR_GATE)
				reg = readl(FLOW_CTRL_CPU_PWR_CSR);
				reg |= FLOW_CTRL_CPU_PWR_CSR_RAIL_ENABLE;
				writel(reg, FLOW_CTRL_CPU_PWR_CSR);
#else
				writel(UN_PWRGATE_CPU,
				       pmc + PMC_PWRGATE_TOGGLE);
#endif
			}
		}
		tegra_cluster_switch_prolog(flags);
	} else {
		suspend_cpu_dfll_mode();
		set_power_timers(pdata->cpu_timer, pdata->cpu_off_timer,
			clk_get_rate_all_locked(tegra_pclk));
#if defined(CONFIG_ARCH_TEGRA_HAS_SYMMETRIC_CPU_PWR_GATE)
		reg = readl(FLOW_CTRL_CPU_CSR(0));
		reg &= ~FLOW_CTRL_CSR_ENABLE_EXT_MASK;
		if (is_lp_cluster()) {
			/* for LP cluster, there is no option for rail gating */
			if ((flags & TEGRA_POWER_CLUSTER_PART_MASK) ==
						TEGRA_POWER_CLUSTER_PART_MASK)
				reg |= FLOW_CTRL_CSR_ENABLE_EXT_EMU;
			else if (flags)
				reg |= FLOW_CTRL_CSR_ENABLE_EXT_NCPU;
		}
		else {
			if (flags & TEGRA_POWER_CLUSTER_PART_CRAIL)
				reg |= FLOW_CTRL_CSR_ENABLE_EXT_CRAIL;
			if (flags & TEGRA_POWER_CLUSTER_PART_NONCPU)
				reg |= FLOW_CTRL_CSR_ENABLE_EXT_NCPU;
		}
		writel(reg, FLOW_CTRL_CPU_CSR(0));
#endif
	}

	if (sleep_time)
		tegra_pd_set_trigger(sleep_time);

	cpu_cluster_pm_enter();
	suspend_cpu_complex(flags);
	tegra_cluster_switch_time(flags, tegra_cluster_switch_time_id_prolog);
#ifdef CONFIG_CACHE_L2X0
	flush_cache_all();
	/*
	 * No need to flush complete L2. Cleaning kernel and IO mappings
	 * is enough for the LP code sequence that has L2 disabled but
	 * MMU on.
	 */
	pgd = cpu_get_pgd();
	outer_clean_range(__pa(pgd + USER_PTRS_PER_PGD),
			  __pa(pgd + PTRS_PER_PGD));
	outer_disable();
#endif
	tegra_sleep_cpu(PHYS_OFFSET - PAGE_OFFSET);

	tegra_init_cache(false);

#ifdef CONFIG_TRUSTED_FOUNDATIONS
#ifndef CONFIG_ARCH_TEGRA_11x_SOC
	trace_smc_wake(tegra_resume_smc_entry_time, NVSEC_SMC_START);
	trace_smc_wake(tegra_resume_smc_exit_time, NVSEC_SMC_DONE);
#endif
#endif

	tegra_cluster_switch_time(flags, tegra_cluster_switch_time_id_switch);
	restore_cpu_complex(flags);
	cpu_cluster_pm_exit();

	remain = tegra_pd_timer_remain();
	if (sleep_time)
		tegra_pd_set_trigger(0);

	if (flags & TEGRA_POWER_CLUSTER_MASK) {
		tegra_cluster_switch_epilog(flags);
		if (is_idle_task(current))
			trace_nvcpu_cluster_rcuidle(NVPOWER_CPU_CLUSTER_DONE);
		else
			trace_nvcpu_cluster(NVPOWER_CPU_CLUSTER_DONE);
	} else {
		resume_cpu_dfll_mode();
	}
	tegra_cluster_switch_time(flags, tegra_cluster_switch_time_id_epilog);

#if INSTRUMENT_CLUSTER_SWITCH
	if (flags & TEGRA_POWER_CLUSTER_MASK) {
		pr_err("%s: prolog %lu us, switch %lu us, epilog %lu us, total %lu us\n",
			is_lp_cluster() ? "G=>LP" : "LP=>G",
			tegra_cluster_switch_times[tegra_cluster_switch_time_id_prolog] -
			tegra_cluster_switch_times[tegra_cluster_switch_time_id_start],
			tegra_cluster_switch_times[tegra_cluster_switch_time_id_switch] -
			tegra_cluster_switch_times[tegra_cluster_switch_time_id_prolog],
			tegra_cluster_switch_times[tegra_cluster_switch_time_id_epilog] -
			tegra_cluster_switch_times[tegra_cluster_switch_time_id_switch],
			tegra_cluster_switch_times[tegra_cluster_switch_time_id_epilog] -
			tegra_cluster_switch_times[tegra_cluster_switch_time_id_start]);
	}
#endif
	return remain;
}

#ifdef CONFIG_TEGRA_LP1_LOW_COREVOLTAGE
int tegra_is_lp1_suspend_mode(void)
{
	return (current_suspend_mode == TEGRA_SUSPEND_LP1);
}
#endif

static int tegra_common_suspend(void)
{
	void __iomem *mc = IO_ADDRESS(TEGRA_MC_BASE);

	tegra_sctx.mc[0] = readl(mc + MC_SECURITY_START);
	tegra_sctx.mc[1] = readl(mc + MC_SECURITY_SIZE);
	tegra_sctx.mc[2] = readl(mc + MC_SECURITY_CFG2);

#ifdef CONFIG_TEGRA_LP1_LOW_COREVOLTAGE
	if (pdata && pdata->lp1_lowvolt_support) {
		u32 lp1_core_lowvolt =
			(tegra_is_voice_call_active() ||
			tegra_dvfs_rail_get_thermal_floor(tegra_core_rail)) ?
			pdata->lp1_core_volt_low_cold << 8 :
			pdata->lp1_core_volt_low << 8;

		lp1_core_lowvolt |= pdata->core_reg_addr;
		memcpy(tegra_lp1_register_core_lowvolt(), &lp1_core_lowvolt, 4);
	}
#endif

	/* copy the reset vector and SDRAM shutdown code into IRAM */
	memcpy(iram_save, iram_code, iram_save_size);
	memcpy(iram_code, tegra_iram_start(), iram_save_size);

	return 0;
}

static void tegra_common_resume(void)
{
	void __iomem *mc = IO_ADDRESS(TEGRA_MC_BASE);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	void __iomem *emc = IO_ADDRESS(TEGRA_EMC_BASE);
#endif

	/* Clear DPD sample */
	writel(0x0, pmc + PMC_DPD_SAMPLE);

	writel(tegra_sctx.mc[0], mc + MC_SECURITY_START);
	writel(tegra_sctx.mc[1], mc + MC_SECURITY_SIZE);
	writel(tegra_sctx.mc[2], mc + MC_SECURITY_CFG2);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	/* trigger emc mode write */
	writel(EMC_MRW_DEV_NONE, emc + EMC_MRW_0);
#endif
	/* clear scratch registers shared by suspend and the reset pen */
	writel(0x0, pmc + PMC_SCRATCH39);
	writel(0x0, pmc + PMC_SCRATCH41);

	/* restore IRAM */
	memcpy(iram_code, iram_save, iram_save_size);
}

static int tegra_suspend_prepare_late(void)
{
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	disable_irq(INT_SYS_STATS_MON);
#endif
	return 0;
}

static void tegra_suspend_wake(void)
{
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	enable_irq(INT_SYS_STATS_MON);
#endif
}

static void tegra_pm_set(enum tegra_suspend_mode mode)
{
	u32 reg, boot_flag;
	unsigned long rate = 32768;

	reg = readl(pmc + PMC_CTRL);
	reg |= TEGRA_POWER_CPU_PWRREQ_OE;
	if (pdata->combined_req)
		reg &= ~TEGRA_POWER_PWRREQ_OE;
	else
		reg |= TEGRA_POWER_PWRREQ_OE;
	reg &= ~TEGRA_POWER_EFFECT_LP0;

	switch (mode) {
	case TEGRA_SUSPEND_LP0:
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
		rate = clk_get_rate_all_locked(tegra_pclk);
#endif
		if (pdata->combined_req) {
			reg |= TEGRA_POWER_PWRREQ_OE;
			reg &= ~TEGRA_POWER_CPU_PWRREQ_OE;
		}

		/*
		 * LP0 boots through the AVP, which then resumes the AVP to
		 * the address in scratch 39, and the cpu to the address in
		 * scratch 41 to tegra_resume
		 */
		writel(0x0, pmc + PMC_SCRATCH39);

		/* Enable DPD sample to trigger sampling pads data and direction
		 * in which pad will be driven during lp0 mode*/
		writel(0x1, pmc + PMC_DPD_SAMPLE);
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
		/* this is needed only for T11x, not for other chips */
		reg &= ~TEGRA_POWER_CPUPWRGOOD_EN;
#endif

		/* Set warmboot flag */
		boot_flag = readl(pmc + PMC_SCRATCH0);
		pmc_32kwritel(boot_flag | 1, PMC_SCRATCH0);

		pmc_32kwritel(tegra_lp0_vec_start, PMC_SCRATCH1);

		reg |= TEGRA_POWER_EFFECT_LP0;
		/* No break here. LP0 code falls through to write SCRATCH41 */
	case TEGRA_SUSPEND_LP1:
		__raw_writel(virt_to_phys(tegra_resume), pmc + PMC_SCRATCH41);
		wmb();
		break;
	case TEGRA_SUSPEND_LP2:
		rate = clk_get_rate(tegra_pclk);
		break;
	case TEGRA_SUSPEND_NONE:
		return;
	default:
		BUG();
	}

	set_power_timers(pdata->cpu_timer, pdata->cpu_off_timer, rate);

	pmc_32kwritel(reg, PMC_CTRL);
}

static const char *lp_state[TEGRA_MAX_SUSPEND_MODE] = {
	[TEGRA_SUSPEND_NONE] = "none",
	[TEGRA_SUSPEND_LP2] = "LP2",
	[TEGRA_SUSPEND_LP1] = "LP1",
	[TEGRA_SUSPEND_LP0] = "LP0",
};

static int tegra_suspend_enter(suspend_state_t state)
{
	int ret;
	ktime_t delta;
	struct timespec ts_entry, ts_exit;

	if (pdata && pdata->board_suspend)
		pdata->board_suspend(current_suspend_mode, TEGRA_SUSPEND_BEFORE_PERIPHERAL);

	read_persistent_clock(&ts_entry);

	ret = tegra_suspend_dram(current_suspend_mode, 0);
	if (ret) {
		pr_info("Aborting suspend, tegra_suspend_dram error=%d\n", ret);
		goto abort_suspend;
	}

	read_persistent_clock(&ts_exit);

	if (timespec_compare(&ts_exit, &ts_entry) > 0) {
		delta = timespec_to_ktime(timespec_sub(ts_exit, ts_entry));

		tegra_dvfs_rail_pause(tegra_cpu_rail, delta, false);
		if (current_suspend_mode == TEGRA_SUSPEND_LP0)
			tegra_dvfs_rail_pause(tegra_core_rail, delta, false);
		else
			tegra_dvfs_rail_pause(tegra_core_rail, delta, true);
	}

abort_suspend:
	if (pdata && pdata->board_resume)
		pdata->board_resume(current_suspend_mode, TEGRA_RESUME_AFTER_PERIPHERAL);

	return ret;
}

static void tegra_suspend_check_pwr_stats(void)
{
	/* cpus and l2 are powered off later */
	unsigned long pwrgate_partid_mask =
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
		(1 << TEGRA_POWERGATE_HEG)	|
		(1 << TEGRA_POWERGATE_SATA)	|
		(1 << TEGRA_POWERGATE_3D1)	|
#endif
		(1 << TEGRA_POWERGATE_3D)	|
		(1 << TEGRA_POWERGATE_VENC)	|
		(1 << TEGRA_POWERGATE_PCIE)	|
		(1 << TEGRA_POWERGATE_VDEC)	|
		(1 << TEGRA_POWERGATE_MPE);

	int partid;

	for (partid = 0; partid < TEGRA_NUM_POWERGATE; partid++)
		if ((1 << partid) & pwrgate_partid_mask)
			if (tegra_powergate_is_powered(partid))
				pr_warning("partition %s is left on before suspend\n",
					tegra_powergate_get_name(partid));

	return;
}

int tegra_suspend_dram(enum tegra_suspend_mode mode, unsigned int flags)
{
	int err = 0;
	u32 scratch37 = 0xDEADBEEF;
	u32 reg;

	if (WARN_ON(mode <= TEGRA_SUSPEND_NONE ||
		mode >= TEGRA_MAX_SUSPEND_MODE)) {
		err = -ENXIO;
		goto fail;
	}

	if (tegra_is_voice_call_active()) {
		/* backup the current value of scratch37 */
		scratch37 = readl(pmc + PMC_SCRATCH37);

		/* If voice call is active, set a flag in PMC_SCRATCH37 */
		reg = TEGRA_POWER_LP1_AUDIO;
		pmc_32kwritel(reg, PMC_SCRATCH37);
	}

	if ((mode == TEGRA_SUSPEND_LP0) && !tegra_pm_irq_lp0_allowed()) {
		pr_info("LP0 not used due to unsupported wakeup events\n");
		mode = TEGRA_SUSPEND_LP1;
	}

	if ((mode == TEGRA_SUSPEND_LP0) || (mode == TEGRA_SUSPEND_LP1))
		tegra_suspend_check_pwr_stats();

	tegra_common_suspend();

	tegra_pm_set(mode);

	if (pdata && pdata->board_suspend)
		pdata->board_suspend(mode, TEGRA_SUSPEND_BEFORE_CPU);

	local_fiq_disable();

	trace_cpu_suspend(CPU_SUSPEND_START, tegra_rtc_read_ms());

	if (mode == TEGRA_SUSPEND_LP0) {
#ifdef CONFIG_TEGRA_CLUSTER_CONTROL
		reg = readl(pmc + PMC_SCRATCH4);
		if (is_lp_cluster())
			reg |= PMC_SCRATCH4_WAKE_CLUSTER_MASK;
		else
			reg &= (~PMC_SCRATCH4_WAKE_CLUSTER_MASK);
		pmc_32kwritel(reg, PMC_SCRATCH4);
#endif
		tegra_tsc_suspend();
		tegra_lp0_suspend_mc();
		tegra_cpu_reset_handler_save();
		tegra_tsc_wait_for_suspend();
		tegra_smp_clear_power_mask();
	}
	else if (mode == TEGRA_SUSPEND_LP1)
		*iram_cpu_lp1_mask = 1;

	suspend_cpu_complex(flags);

#if defined(CONFIG_ARCH_TEGRA_HAS_SYMMETRIC_CPU_PWR_GATE)
	/* In case of LP0/1, program external power gating accordinly */
	if (mode == TEGRA_SUSPEND_LP0 || mode == TEGRA_SUSPEND_LP1) {
		reg = readl(FLOW_CTRL_CPU_CSR(0));
		if (is_lp_cluster())
			reg |= FLOW_CTRL_CSR_ENABLE_EXT_NCPU; /* Non CPU */
		else
			reg |= FLOW_CTRL_CSR_ENABLE_EXT_CRAIL;  /* CRAIL */
		flowctrl_writel(reg, FLOW_CTRL_CPU_CSR(0));
	}
#endif

	flush_cache_all();
	outer_flush_all();
	outer_disable();

	if (mode == TEGRA_SUSPEND_LP2)
		tegra_sleep_cpu(PHYS_OFFSET - PAGE_OFFSET);
	else
		tegra_sleep_core(mode, PHYS_OFFSET - PAGE_OFFSET);

	resume_entry_time = 0;
	if (mode != TEGRA_SUSPEND_LP0)
		resume_entry_time = readl(tmrus_reg_base + TIMERUS_CNTR_1US);

	tegra_init_cache(true);

#ifdef CONFIG_TRUSTED_FOUNDATIONS
#ifndef CONFIG_ARCH_TEGRA_11x_SOC
	trace_smc_wake(tegra_resume_smc_entry_time, NVSEC_SMC_START);
	trace_smc_wake(tegra_resume_smc_exit_time, NVSEC_SMC_DONE);
#endif

	if (mode == TEGRA_SUSPEND_LP0) {
		trace_secureos_init(tegra_resume_entry_time,
			NVSEC_SUSPEND_EXIT_DONE);
	}
#endif

	if (mode == TEGRA_SUSPEND_LP0) {

		/* CPUPWRGOOD_EN is not enabled in HW so disabling this, *
		* Otherwise it is creating issue in cluster switch after LP0 *
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
		reg = readl(pmc+PMC_CTRL);
		reg |= TEGRA_POWER_CPUPWRGOOD_EN;
		pmc_32kwritel(reg, PMC_CTRL);
#endif
		*/

		tegra_tsc_resume();
		tegra_cpu_reset_handler_restore();
		tegra_lp0_resume_mc();
		tegra_tsc_wait_for_resume();
	} else if (mode == TEGRA_SUSPEND_LP1)
		*iram_cpu_lp1_mask = 0;

	/* if scratch37 was clobbered during LP1, restore it */
	if (scratch37 != 0xDEADBEEF)
		pmc_32kwritel(scratch37, PMC_SCRATCH37);

	restore_cpu_complex(flags);

	/* for platforms where the core & CPU power requests are
	 * combined as a single request to the PMU, transition out
	 * of LP0 state by temporarily enabling both requests
	 */
	if (mode == TEGRA_SUSPEND_LP0 && pdata->combined_req) {
		reg = readl(pmc + PMC_CTRL);
		reg |= TEGRA_POWER_CPU_PWRREQ_OE;
		pmc_32kwritel(reg, PMC_CTRL);
		reg &= ~TEGRA_POWER_PWRREQ_OE;
		pmc_32kwritel(reg, PMC_CTRL);
	}

	if (pdata && pdata->board_resume)
		pdata->board_resume(mode, TEGRA_RESUME_AFTER_CPU);

	trace_cpu_suspend(CPU_SUSPEND_DONE, tegra_rtc_read_ms());

	local_fiq_enable();

	tegra_common_resume();

fail:
	return err;
}

/*
 * Function pointers to optional board specific function
 */
void (*tegra_deep_sleep)(int);
EXPORT_SYMBOL(tegra_deep_sleep);

static int tegra_suspend_prepare(void)
{
	if ((current_suspend_mode == TEGRA_SUSPEND_LP0) && tegra_deep_sleep)
		tegra_deep_sleep(1);
	return 0;
}

static void tegra_suspend_finish(void)
{
	if (pdata && pdata->cpu_resume_boost) {
		int ret = tegra_suspended_target(pdata->cpu_resume_boost);
		pr_info("Tegra: resume CPU boost to %u KHz: %s (%d)\n",
			pdata->cpu_resume_boost, ret ? "Failed" : "OK", ret);
	}

	if ((current_suspend_mode == TEGRA_SUSPEND_LP0) && tegra_deep_sleep)
		tegra_deep_sleep(0);
}

static const struct platform_suspend_ops tegra_suspend_ops = {
	.valid		= suspend_valid_only_mem,
	.prepare	= tegra_suspend_prepare,
	.finish		= tegra_suspend_finish,
	.prepare_late	= tegra_suspend_prepare_late,
	.wake		= tegra_suspend_wake,
	.enter		= tegra_suspend_enter,
};

static ssize_t suspend_mode_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	char *start = buf;
	char *end = buf + PAGE_SIZE;

	start += scnprintf(start, end - start, "%s ", \
				tegra_suspend_name[current_suspend_mode]);
	start += scnprintf(start, end - start, "\n");

	return start - buf;
}

static ssize_t suspend_mode_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t n)
{
	int len;
	const char *name_ptr;
	enum tegra_suspend_mode new_mode;

	name_ptr = buf;
	while (*name_ptr && !isspace(*name_ptr))
		name_ptr++;
	len = name_ptr - buf;
	if (!len)
		goto bad_name;
	/* TEGRA_SUSPEND_NONE not allowed as suspend state */
	if (!(strncmp(buf, tegra_suspend_name[TEGRA_SUSPEND_NONE], len))
		|| !(strncmp(buf, tegra_suspend_name[TEGRA_SUSPEND_LP2], len))) {
		pr_info("Illegal tegra suspend state: %s\n", buf);
		goto bad_name;
	}

	for (new_mode = TEGRA_SUSPEND_NONE; \
			new_mode < TEGRA_MAX_SUSPEND_MODE; ++new_mode) {
		if (!strncmp(buf, tegra_suspend_name[new_mode], len)) {
			current_suspend_mode = new_mode;
			break;
		}
	}

bad_name:
	return n;
}

static struct kobj_attribute suspend_mode_attribute =
	__ATTR(mode, 0644, suspend_mode_show, suspend_mode_store);

static ssize_t suspend_resume_time_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%ums\n", ((u32)resume_time / 1000));
}

static struct kobj_attribute suspend_resume_time_attribute =
	__ATTR(resume_time, 0444, suspend_resume_time_show, 0);

static ssize_t suspend_time_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%ums\n", ((u32)suspend_time / 1000));
}

static struct kobj_attribute suspend_time_attribute =
	__ATTR(suspend_time, 0444, suspend_time_show, 0);

static struct kobject *suspend_kobj;

static int tegra_pm_enter_suspend(void)
{
	pr_info("Entering suspend state %s\n", lp_state[current_suspend_mode]);
	suspend_cpu_dfll_mode();
	if (current_suspend_mode == TEGRA_SUSPEND_LP0)
		tegra_lp0_cpu_mode(true);
	return 0;
}

static void tegra_pm_enter_resume(void)
{
	if (current_suspend_mode == TEGRA_SUSPEND_LP0)
		tegra_lp0_cpu_mode(false);
	resume_cpu_dfll_mode();
	pr_info("Exited suspend state %s\n", lp_state[current_suspend_mode]);
}

static void tegra_pm_enter_shutdown(void)
{
	suspend_cpu_dfll_mode();
	pr_info("Shutting down tegra ...\n");
}

static struct syscore_ops tegra_pm_enter_syscore_ops = {
	.suspend = tegra_pm_enter_suspend,
	.resume = tegra_pm_enter_resume,
	.shutdown = tegra_pm_enter_shutdown,
};

static __init int tegra_pm_enter_syscore_init(void)
{
	register_syscore_ops(&tegra_pm_enter_syscore_ops);
	return 0;
}
subsys_initcall(tegra_pm_enter_syscore_init);
#endif

void __init tegra_init_suspend(struct tegra_suspend_platform_data *plat)
{
	u32 reg;
	u32 mode;

	pm_qos_add_request(&awake_cpu_freq_req, PM_QOS_CPU_FREQ_MIN,
			   AWAKE_CPU_FREQ_MIN);

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	tegra_dfll = clk_get_sys(NULL, "dfll_cpu");
	BUG_ON(IS_ERR(tegra_dfll));
#endif
	tegra_pclk = clk_get_sys(NULL, "pclk");
	BUG_ON(IS_ERR(tegra_pclk));
	pdata = plat;
	(void)reg;
	(void)mode;

	if (plat->suspend_mode == TEGRA_SUSPEND_LP2)
		plat->suspend_mode = TEGRA_SUSPEND_LP0;

#ifndef CONFIG_PM_SLEEP
	if (plat->suspend_mode != TEGRA_SUSPEND_NONE) {
		pr_warning("%s: Suspend requires CONFIG_PM_SLEEP -- "
			   "disabling suspend\n", __func__);
		plat->suspend_mode = TEGRA_SUSPEND_NONE;
	}
#else
	if (create_suspend_pgtable() < 0) {
		pr_err("%s: PGD memory alloc failed -- LP0/LP1/LP2 unavailable\n",
				__func__);
		plat->suspend_mode = TEGRA_SUSPEND_NONE;
		goto fail;
	}

	if ((tegra_get_chipid() == TEGRA_CHIPID_TEGRA3) &&
	    (tegra_revision == TEGRA_REVISION_A01) &&
	    (plat->suspend_mode == TEGRA_SUSPEND_LP0)) {
		/* Tegra 3 A01 supports only LP1 */
		pr_warning("%s: Suspend mode LP0 is not supported on A01 "
			   "-- disabling LP0\n", __func__);
		plat->suspend_mode = TEGRA_SUSPEND_LP1;
	}
	if (plat->suspend_mode == TEGRA_SUSPEND_LP0 && tegra_lp0_vec_size &&
		tegra_lp0_vec_relocate) {
		unsigned char *reloc_lp0;
		unsigned long tmp;
		void __iomem *orig;
		reloc_lp0 = kmalloc(tegra_lp0_vec_size + L1_CACHE_BYTES - 1,
					GFP_KERNEL);
		WARN_ON(!reloc_lp0);
		if (!reloc_lp0) {
			pr_err("%s: Failed to allocate reloc_lp0\n",
				__func__);
			goto out;
		}

		orig = ioremap(tegra_lp0_vec_start, tegra_lp0_vec_size);
		WARN_ON(!orig);
		if (!orig) {
			pr_err("%s: Failed to map tegra_lp0_vec_start %08lx\n",
				__func__, tegra_lp0_vec_start);
			kfree(reloc_lp0);
			goto out;
		}

		tmp = (unsigned long) reloc_lp0;
		tmp = (tmp + L1_CACHE_BYTES - 1) & ~(L1_CACHE_BYTES - 1);
		reloc_lp0 = (unsigned char *)tmp;
		memcpy(reloc_lp0, orig, tegra_lp0_vec_size);
		iounmap(orig);
		tegra_lp0_vec_start = virt_to_phys(reloc_lp0);
	}

out:
	if (plat->suspend_mode == TEGRA_SUSPEND_LP0 && !tegra_lp0_vec_size) {
		pr_warning("%s: Suspend mode LP0 requested, no lp0_vec "
			   "provided by bootlader -- disabling LP0\n",
			   __func__);
		plat->suspend_mode = TEGRA_SUSPEND_LP1;
	}

	iram_save_size = tegra_iram_end() - tegra_iram_start();

	iram_save = kmalloc(iram_save_size, GFP_KERNEL);
	if (!iram_save && (plat->suspend_mode >= TEGRA_SUSPEND_LP1)) {
		pr_err("%s: unable to allocate memory for SDRAM self-refresh "
		       "-- LP0/LP1 unavailable\n", __func__);
		plat->suspend_mode = TEGRA_SUSPEND_LP2;
	}

#ifdef CONFIG_TEGRA_LP1_LOW_COREVOLTAGE
	if (pdata->lp1_lowvolt_support) {
		u32 lp1_core_lowvolt, lp1_core_highvolt;
		memcpy(tegra_lp1_register_pmuslave_addr(), &pdata->pmuslave_addr, 4);
		memcpy(tegra_lp1_register_i2c_base_addr(), &pdata->i2c_base_addr, 4);

		lp1_core_lowvolt = 0;
		lp1_core_lowvolt = (pdata->lp1_core_volt_low << 8) | pdata->core_reg_addr;
		memcpy(tegra_lp1_register_core_lowvolt(), &lp1_core_lowvolt, 4);

		lp1_core_highvolt = 0;
		lp1_core_highvolt = (pdata->lp1_core_volt_high << 8) | pdata->core_reg_addr;
		memcpy(tegra_lp1_register_core_highvolt(), &lp1_core_highvolt, 4);
	}
#endif
	/* !!!FIXME!!! THIS IS TEGRA2 ONLY */
	/* Initialize scratch registers used for CPU LP2 synchronization */
	writel(0, pmc + PMC_SCRATCH37);
	writel(0, pmc + PMC_SCRATCH38);
	writel(0, pmc + PMC_SCRATCH39);
	writel(0, pmc + PMC_SCRATCH41);

	/* Always enable CPU power request; just normal polarity is supported */
	reg = readl(pmc + PMC_CTRL);
	BUG_ON(reg & TEGRA_POWER_CPU_PWRREQ_POLARITY);
	reg |= TEGRA_POWER_CPU_PWRREQ_OE;
	pmc_32kwritel(reg, PMC_CTRL);

	/* Configure core power request and system clock control if LP0
	   is supported */
	__raw_writel(pdata->core_timer, pmc + PMC_COREPWRGOOD_TIMER);
	__raw_writel(pdata->core_off_timer, pmc + PMC_COREPWROFF_TIMER);

	reg = readl(pmc + PMC_CTRL);

	if (!pdata->sysclkreq_high)
		reg |= TEGRA_POWER_SYSCLK_POLARITY;
	else
		reg &= ~TEGRA_POWER_SYSCLK_POLARITY;

	if (!pdata->corereq_high)
		reg |= TEGRA_POWER_PWRREQ_POLARITY;
	else
		reg &= ~TEGRA_POWER_PWRREQ_POLARITY;

	/* configure output inverters while the request is tristated */
	pmc_32kwritel(reg, PMC_CTRL);

	/* now enable requests */
	reg |= TEGRA_POWER_SYSCLK_OE;
	if (!pdata->combined_req)
		reg |= TEGRA_POWER_PWRREQ_OE;
	pmc_32kwritel(reg, PMC_CTRL);

	if (pdata->sysclkreq_gpio) {
		reg = readl(pmc + PMC_DPAD_ORIDE);
		reg &= ~TEGRA_DPAD_ORIDE_SYS_CLK_REQ;
		pmc_32kwritel(reg, PMC_DPAD_ORIDE);
	}

	if (pdata->suspend_mode == TEGRA_SUSPEND_LP0)
		tegra_lp0_suspend_init();

	suspend_set_ops(&tegra_suspend_ops);

	/* Create /sys/power/suspend/type */
	suspend_kobj = kobject_create_and_add("suspend", power_kobj);
	if (suspend_kobj) {
		if (sysfs_create_file(suspend_kobj, \
						&suspend_mode_attribute.attr))
			pr_err("%s: sysfs_create_file suspend type failed!\n",
								__func__);
		if (sysfs_create_file(suspend_kobj, \
					&suspend_resume_time_attribute.attr))
			pr_err("%s: sysfs_create_file resume_time failed!\n",
								__func__);
		if (sysfs_create_file(suspend_kobj, \
					&suspend_time_attribute.attr))
			pr_err("%s: sysfs_create_file suspend_time failed!\n",
								__func__);
	}

	iram_cpu_lp2_mask = tegra_cpu_lp2_mask;
	iram_cpu_lp1_mask = tegra_cpu_lp1_mask;

	/* clear io dpd settings before kernel */
	tegra_bl_io_dpd_cleanup();

fail:
#endif
	if (plat->suspend_mode == TEGRA_SUSPEND_NONE)
		tegra_pd_in_idle(false);

	current_suspend_mode = plat->suspend_mode;
}

unsigned long debug_uart_port_base = 0;
EXPORT_SYMBOL(debug_uart_port_base);

static int tegra_debug_uart_suspend(void)
{
	void __iomem *uart;
	u32 lcr;

	if (!debug_uart_port_base)
		return 0;

	uart = IO_ADDRESS(debug_uart_port_base);

	lcr = readb(uart + UART_LCR * 4);

	tegra_sctx.uart[0] = lcr;
	tegra_sctx.uart[1] = readb(uart + UART_MCR * 4);

	/* DLAB = 0 */
	writeb(lcr & ~UART_LCR_DLAB, uart + UART_LCR * 4);

	tegra_sctx.uart[2] = readb(uart + UART_IER * 4);

	/* DLAB = 1 */
	writeb(lcr | UART_LCR_DLAB, uart + UART_LCR * 4);

	tegra_sctx.uart[3] = readb(uart + UART_DLL * 4);
	tegra_sctx.uart[4] = readb(uart + UART_DLM * 4);

	writeb(lcr, uart + UART_LCR * 4);

	return 0;
}

static void tegra_debug_uart_resume(void)
{
	void __iomem *uart;
	u32 lcr;

	if (!debug_uart_port_base)
		return;

	uart = IO_ADDRESS(debug_uart_port_base);

	lcr = tegra_sctx.uart[0];

	writeb(tegra_sctx.uart[1], uart + UART_MCR * 4);

	/* DLAB = 0 */
	writeb(lcr & ~UART_LCR_DLAB, uart + UART_LCR * 4);

	writeb(UART_FCR_ENABLE_FIFO | UART_FCR_T_TRIG_01 | UART_FCR_R_TRIG_01,
			uart + UART_FCR * 4);

	writeb(tegra_sctx.uart[2], uart + UART_IER * 4);

	/* DLAB = 1 */
	writeb(lcr | UART_LCR_DLAB, uart + UART_LCR * 4);

	writeb(tegra_sctx.uart[3], uart + UART_DLL * 4);
	writeb(tegra_sctx.uart[4], uart + UART_DLM * 4);

	writeb(lcr, uart + UART_LCR * 4);
}

static struct syscore_ops tegra_debug_uart_syscore_ops = {
	.suspend = tegra_debug_uart_suspend,
	.resume = tegra_debug_uart_resume,
};

struct clk *debug_uart_clk = NULL;
EXPORT_SYMBOL(debug_uart_clk);

void tegra_console_uart_suspend(void)
{
	if (console_suspend_enabled && debug_uart_clk)
		tegra_clk_disable_unprepare(debug_uart_clk);
}

void tegra_console_uart_resume(void)
{
	if (console_suspend_enabled && debug_uart_clk)
		tegra_clk_prepare_enable(debug_uart_clk);
}

static int tegra_debug_uart_syscore_init(void)
{
	register_syscore_ops(&tegra_debug_uart_syscore_ops);
	return 0;
}
arch_initcall(tegra_debug_uart_syscore_init);
