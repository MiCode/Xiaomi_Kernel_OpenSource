/*
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/regulator/consumer.h>

#include <asm/mach-types.h>
#include <asm/cpu.h>

#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/socinfo.h>
#include <mach/msm-krait-l2-accessors.h>
#include <mach/rpm-regulator.h>
#include <mach/msm_bus.h>

#include "acpuclock.h"
#include "acpuclock-krait.h"

/* MUX source selects. */
#define PRI_SRC_SEL_SEC_SRC	0
#define PRI_SRC_SEL_HFPLL	1
#define PRI_SRC_SEL_HFPLL_DIV2	2
#define SEC_SRC_SEL_QSB		0
#define SEC_SRC_SEL_L2PLL	1
#define SEC_SRC_SEL_AUX		2

/* PTE EFUSE register offset. */
#define PTE_EFUSE		0xC0

static DEFINE_MUTEX(driver_lock);
static DEFINE_SPINLOCK(l2_lock);

static struct drv_data {
	const struct acpu_level *acpu_freq_tbl;
	const struct l2_level *l2_freq_tbl;
	struct scalable *scalable;
	u32 bus_perf_client;
	struct device *dev;
} drv;

static unsigned long acpuclk_krait_get_rate(int cpu)
{
	return drv.scalable[cpu].cur_speed->khz;
}

/* Select a source on the primary MUX. */
static void set_pri_clk_src(struct scalable *sc, u32 pri_src_sel)
{
	u32 regval;

	regval = get_l2_indirect_reg(sc->l2cpmr_iaddr);
	regval &= ~0x3;
	regval |= (pri_src_sel & 0x3);
	set_l2_indirect_reg(sc->l2cpmr_iaddr, regval);
	/* Wait for switch to complete. */
	mb();
	udelay(1);
}

/* Select a source on the secondary MUX. */
static void set_sec_clk_src(struct scalable *sc, u32 sec_src_sel)
{
	u32 regval;

	regval = get_l2_indirect_reg(sc->l2cpmr_iaddr);
	regval &= ~(0x3 << 2);
	regval |= ((sec_src_sel & 0x3) << 2);
	set_l2_indirect_reg(sc->l2cpmr_iaddr, regval);
	/* Wait for switch to complete. */
	mb();
	udelay(1);
}

/* Enable an already-configured HFPLL. */
static void hfpll_enable(struct scalable *sc, bool skip_regulators)
{
	int rc;

	if (!skip_regulators) {
		/* Enable regulators required by the HFPLL. */
		if (sc->vreg[VREG_HFPLL_A].rpm_vreg_id) {
			rc = rpm_vreg_set_voltage(
				sc->vreg[VREG_HFPLL_A].rpm_vreg_id,
				sc->vreg[VREG_HFPLL_A].rpm_vreg_voter,
				sc->vreg[VREG_HFPLL_A].cur_vdd,
				sc->vreg[VREG_HFPLL_A].max_vdd, 0);
			if (rc)
				dev_err(drv.dev,
					"%s regulator enable failed (%d)\n",
					sc->vreg[VREG_HFPLL_A].name, rc);
		}
		if (sc->vreg[VREG_HFPLL_B].rpm_vreg_id) {
			rc = rpm_vreg_set_voltage(
				sc->vreg[VREG_HFPLL_B].rpm_vreg_id,
				sc->vreg[VREG_HFPLL_B].rpm_vreg_voter,
				sc->vreg[VREG_HFPLL_B].cur_vdd,
				sc->vreg[VREG_HFPLL_B].max_vdd, 0);
			if (rc)
				dev_err(drv.dev,
					"%s regulator enable failed (%d)\n",
					sc->vreg[VREG_HFPLL_B].name, rc);
		}
	}

	/* Disable PLL bypass mode. */
	writel_relaxed(0x2, sc->hfpll_base + sc->hfpll_data->mode_offset);

	/*
	 * H/W requires a 5us delay between disabling the bypass and
	 * de-asserting the reset. Delay 10us just to be safe.
	 */
	mb();
	udelay(10);

	/* De-assert active-low PLL reset. */
	writel_relaxed(0x6, sc->hfpll_base + sc->hfpll_data->mode_offset);

	/* Wait for PLL to lock. */
	mb();
	udelay(60);

	/* Enable PLL output. */
	writel_relaxed(0x7, sc->hfpll_base + sc->hfpll_data->mode_offset);
}

/* Disable a HFPLL for power-savings or while it's being reprogrammed. */
static void hfpll_disable(struct scalable *sc, bool skip_regulators)
{
	int rc;

	/*
	 * Disable the PLL output, disable test mode, enable the bypass mode,
	 * and assert the reset.
	 */
	writel_relaxed(0, sc->hfpll_base + sc->hfpll_data->mode_offset);

	if (!skip_regulators) {
		/* Remove voltage votes required by the HFPLL. */
		if (sc->vreg[VREG_HFPLL_B].rpm_vreg_id) {
			rc = rpm_vreg_set_voltage(
				sc->vreg[VREG_HFPLL_B].rpm_vreg_id,
				sc->vreg[VREG_HFPLL_B].rpm_vreg_voter,
				0, 0, 0);
			if (rc)
				dev_err(drv.dev,
					"%s regulator enable failed (%d)\n",
					sc->vreg[VREG_HFPLL_B].name, rc);
		}
		if (sc->vreg[VREG_HFPLL_A].rpm_vreg_id) {
			rc = rpm_vreg_set_voltage(
				sc->vreg[VREG_HFPLL_A].rpm_vreg_id,
				sc->vreg[VREG_HFPLL_A].rpm_vreg_voter,
				0, 0, 0);
			if (rc)
				dev_err(drv.dev,
					"%s regulator enable failed (%d)\n",
					sc->vreg[VREG_HFPLL_A].name, rc);
		}
	}
}

/* Program the HFPLL rate. Assumes HFPLL is already disabled. */
static void hfpll_set_rate(struct scalable *sc, const struct core_speed *tgt_s)
{
	writel_relaxed(tgt_s->pll_l_val,
		sc->hfpll_base + sc->hfpll_data->l_offset);
}

/* Return the L2 speed that should be applied. */
static const struct l2_level *compute_l2_level(struct scalable *sc,
					       const struct l2_level *vote_l)
{
	const struct l2_level *new_l;
	int cpu;

	/* Find max L2 speed vote. */
	sc->l2_vote = vote_l;
	new_l = drv.l2_freq_tbl;
	for_each_present_cpu(cpu)
		new_l = max(new_l, drv.scalable[cpu].l2_vote);

	return new_l;
}

/* Update the bus bandwidth request. */
static void set_bus_bw(unsigned int bw)
{
	int ret;

	/* Update bandwidth if request has changed. This may sleep. */
	ret = msm_bus_scale_client_update_request(drv.bus_perf_client, bw);
	if (ret)
		dev_err(drv.dev, "bandwidth request failed (%d)\n", ret);
}

/* Set the CPU or L2 clock speed. */
static void set_speed(struct scalable *sc, const struct core_speed *tgt_s)
{
	const struct core_speed *strt_s = sc->cur_speed;

	if (strt_s->src == HFPLL && tgt_s->src == HFPLL) {
		/*
		 * Move to an always-on source running at a frequency
		 * that does not require an elevated CPU voltage.
		 */
		set_sec_clk_src(sc, SEC_SRC_SEL_AUX);
		set_pri_clk_src(sc, PRI_SRC_SEL_SEC_SRC);

		/* Re-program HFPLL. */
		hfpll_disable(sc, 1);
		hfpll_set_rate(sc, tgt_s);
		hfpll_enable(sc, 1);

		/* Move to HFPLL. */
		set_pri_clk_src(sc, tgt_s->pri_src_sel);
	} else if (strt_s->src == HFPLL && tgt_s->src != HFPLL) {
		set_sec_clk_src(sc, tgt_s->sec_src_sel);
		set_pri_clk_src(sc, tgt_s->pri_src_sel);
		hfpll_disable(sc, 0);
	} else if (strt_s->src != HFPLL && tgt_s->src == HFPLL) {
		hfpll_set_rate(sc, tgt_s);
		hfpll_enable(sc, 0);
		set_pri_clk_src(sc, tgt_s->pri_src_sel);
	} else {
		set_sec_clk_src(sc, tgt_s->sec_src_sel);
	}

	sc->cur_speed = tgt_s;
}

/* Apply any per-cpu voltage increases. */
static int increase_vdd(int cpu, int vdd_core, int vdd_mem, int vdd_dig,
			enum setrate_reason reason)
{
	struct scalable *sc = &drv.scalable[cpu];
	int rc = 0;

	/*
	 * Increase vdd_mem active-set before vdd_dig.
	 * vdd_mem should be >= vdd_dig.
	 */
	if (vdd_mem > sc->vreg[VREG_MEM].cur_vdd) {
		rc = rpm_vreg_set_voltage(sc->vreg[VREG_MEM].rpm_vreg_id,
				sc->vreg[VREG_MEM].rpm_vreg_voter, vdd_mem,
				sc->vreg[VREG_MEM].max_vdd, 0);
		if (rc) {
			dev_err(drv.dev,
				"vdd_mem (cpu%d) increase failed (%d)\n",
				cpu, rc);
			return rc;
		}
		 sc->vreg[VREG_MEM].cur_vdd = vdd_mem;
	}

	/* Increase vdd_dig active-set vote. */
	if (vdd_dig > sc->vreg[VREG_DIG].cur_vdd) {
		rc = rpm_vreg_set_voltage(sc->vreg[VREG_DIG].rpm_vreg_id,
				sc->vreg[VREG_DIG].rpm_vreg_voter, vdd_dig,
				sc->vreg[VREG_DIG].max_vdd, 0);
		if (rc) {
			dev_err(drv.dev,
				"vdd_dig (cpu%d) increase failed (%d)\n",
				cpu, rc);
			return rc;
		}
		sc->vreg[VREG_DIG].cur_vdd = vdd_dig;
	}

	/*
	 * Update per-CPU core voltage. Don't do this for the hotplug path for
	 * which it should already be correct. Attempting to set it is bad
	 * because we don't know what CPU we are running on at this point, but
	 * the CPU regulator API requires we call it from the affected CPU.
	 */
	if (vdd_core > sc->vreg[VREG_CORE].cur_vdd
			&& reason != SETRATE_HOTPLUG) {
		rc = regulator_set_voltage(sc->vreg[VREG_CORE].reg, vdd_core,
					   sc->vreg[VREG_CORE].max_vdd);
		if (rc) {
			dev_err(drv.dev,
				"vdd_core (cpu%d) increase failed (%d)\n",
				cpu, rc);
			return rc;
		}
		sc->vreg[VREG_CORE].cur_vdd = vdd_core;
	}

	return rc;
}

/* Apply any per-cpu voltage decreases. */
static void decrease_vdd(int cpu, int vdd_core, int vdd_mem, int vdd_dig,
			enum setrate_reason reason)
{
	struct scalable *sc = &drv.scalable[cpu];
	int ret;

	/*
	 * Update per-CPU core voltage. This must be called on the CPU
	 * that's being affected. Don't do this in the hotplug remove path,
	 * where the rail is off and we're executing on the other CPU.
	 */
	if (vdd_core < sc->vreg[VREG_CORE].cur_vdd
			&& reason != SETRATE_HOTPLUG) {
		ret = regulator_set_voltage(sc->vreg[VREG_CORE].reg, vdd_core,
					    sc->vreg[VREG_CORE].max_vdd);
		if (ret) {
			dev_err(drv.dev,
				"vdd_core (cpu%d) decrease failed (%d)\n",
				cpu, ret);
			return;
		}
		sc->vreg[VREG_CORE].cur_vdd = vdd_core;
	}

	/* Decrease vdd_dig active-set vote. */
	if (vdd_dig < sc->vreg[VREG_DIG].cur_vdd) {
		ret = rpm_vreg_set_voltage(sc->vreg[VREG_DIG].rpm_vreg_id,
				sc->vreg[VREG_DIG].rpm_vreg_voter, vdd_dig,
				sc->vreg[VREG_DIG].max_vdd, 0);
		if (ret) {
			dev_err(drv.dev,
				"vdd_dig (cpu%d) decrease failed (%d)\n",
				cpu, ret);
			return;
		}
		sc->vreg[VREG_DIG].cur_vdd = vdd_dig;
	}

	/*
	 * Decrease vdd_mem active-set after vdd_dig.
	 * vdd_mem should be >= vdd_dig.
	 */
	if (vdd_mem < sc->vreg[VREG_MEM].cur_vdd) {
		ret = rpm_vreg_set_voltage(sc->vreg[VREG_MEM].rpm_vreg_id,
				sc->vreg[VREG_MEM].rpm_vreg_voter, vdd_mem,
				sc->vreg[VREG_MEM].max_vdd, 0);
		if (ret) {
			dev_err(drv.dev,
				"vdd_mem (cpu%d) decrease failed (%d)\n",
				cpu, ret);
			return;
		}
		 sc->vreg[VREG_MEM].cur_vdd = vdd_mem;
	}
}

static int calculate_vdd_mem(const struct acpu_level *tgt)
{
	return tgt->l2_level->vdd_mem;
}

static int calculate_vdd_dig(const struct acpu_level *tgt)
{
	int pll_vdd_dig;
	const int *hfpll_vdd = drv.scalable[L2].hfpll_data->vdd;
	const u32 low_vdd_l_max = drv.scalable[L2].hfpll_data->low_vdd_l_max;

	if (tgt->l2_level->speed.src != HFPLL)
		pll_vdd_dig = hfpll_vdd[HFPLL_VDD_NONE];
	else if (tgt->l2_level->speed.pll_l_val > low_vdd_l_max)
		pll_vdd_dig = hfpll_vdd[HFPLL_VDD_NOM];
	else
		pll_vdd_dig = hfpll_vdd[HFPLL_VDD_LOW];

	return max(tgt->l2_level->vdd_dig, pll_vdd_dig);
}

static int calculate_vdd_core(const struct acpu_level *tgt)
{
	return tgt->vdd_core;
}

/* Set the CPU's clock rate and adjust the L2 rate, voltage and BW requests. */
static int acpuclk_krait_set_rate(int cpu, unsigned long rate,
				  enum setrate_reason reason)
{
	const struct core_speed *strt_acpu_s, *tgt_acpu_s;
	const struct l2_level *tgt_l2_l;
	const struct acpu_level *tgt;
	int vdd_mem, vdd_dig, vdd_core;
	unsigned long flags;
	int rc = 0;

	if (cpu > num_possible_cpus()) {
		rc = -EINVAL;
		goto out;
	}

	if (reason == SETRATE_CPUFREQ || reason == SETRATE_HOTPLUG)
		mutex_lock(&driver_lock);

	strt_acpu_s = drv.scalable[cpu].cur_speed;

	/* Return early if rate didn't change. */
	if (rate == strt_acpu_s->khz)
		goto out;

	/* Find target frequency. */
	for (tgt = drv.acpu_freq_tbl; tgt->speed.khz != 0; tgt++) {
		if (tgt->speed.khz == rate) {
			tgt_acpu_s = &tgt->speed;
			break;
		}
	}
	if (tgt->speed.khz == 0) {
		rc = -EINVAL;
		goto out;
	}

	/* Calculate voltage requirements for the current CPU. */
	vdd_mem  = calculate_vdd_mem(tgt);
	vdd_dig  = calculate_vdd_dig(tgt);
	vdd_core = calculate_vdd_core(tgt);

	/* Increase VDD levels if needed. */
	if (reason == SETRATE_CPUFREQ || reason == SETRATE_HOTPLUG) {
		rc = increase_vdd(cpu, vdd_core, vdd_mem, vdd_dig, reason);
		if (rc)
			goto out;
	}

	pr_debug("Switching from ACPU%d rate %lu KHz -> %lu KHz\n",
		 cpu, strt_acpu_s->khz, tgt_acpu_s->khz);

	/* Set the new CPU speed. */
	set_speed(&drv.scalable[cpu], tgt_acpu_s);

	/*
	 * Update the L2 vote and apply the rate change. A spinlock is
	 * necessary to ensure L2 rate is calculated and set atomically
	 * with the CPU frequency, even if acpuclk_krait_set_rate() is
	 * called from an atomic context and the driver_lock mutex is not
	 * acquired.
	 */
	spin_lock_irqsave(&l2_lock, flags);
	tgt_l2_l = compute_l2_level(&drv.scalable[cpu], tgt->l2_level);
	set_speed(&drv.scalable[L2], &tgt_l2_l->speed);
	spin_unlock_irqrestore(&l2_lock, flags);

	/* Nothing else to do for power collapse or SWFI. */
	if (reason == SETRATE_PC || reason == SETRATE_SWFI)
		goto out;

	/* Update bus bandwith request. */
	set_bus_bw(tgt_l2_l->bw_level);

	/* Drop VDD levels if we can. */
	decrease_vdd(cpu, vdd_core, vdd_mem, vdd_dig, reason);

	pr_debug("ACPU%d speed change complete\n", cpu);

out:
	if (reason == SETRATE_CPUFREQ || reason == SETRATE_HOTPLUG)
		mutex_unlock(&driver_lock);
	return rc;
}

/* Initialize a HFPLL at a given rate and enable it. */
static void __init hfpll_init(struct scalable *sc,
			      const struct core_speed *tgt_s)
{
	pr_debug("Initializing HFPLL%d\n", sc - drv.scalable);

	/* Disable the PLL for re-programming. */
	hfpll_disable(sc, 1);

	/* Configure PLL parameters for integer mode. */
	writel_relaxed(sc->hfpll_data->config_val,
		       sc->hfpll_base + sc->hfpll_data->config_offset);
	writel_relaxed(0, sc->hfpll_base + sc->hfpll_data->m_offset);
	writel_relaxed(1, sc->hfpll_base + sc->hfpll_data->n_offset);

	/* Set an initial rate and enable the PLL. */
	hfpll_set_rate(sc, tgt_s);
	hfpll_enable(sc, 0);
}

/* Voltage regulator initialization. */
static void __init regulator_init(const struct acpu_level *lvl)
{
	int cpu, ret;
	struct scalable *sc;
	int vdd_mem, vdd_dig, vdd_core;

	vdd_mem = calculate_vdd_mem(lvl);
	vdd_dig = calculate_vdd_dig(lvl);

	for_each_possible_cpu(cpu) {
		sc = &drv.scalable[cpu];

		/* Set initial vdd_mem vote. */
		ret = rpm_vreg_set_voltage(sc->vreg[VREG_MEM].rpm_vreg_id,
				sc->vreg[VREG_MEM].rpm_vreg_voter, vdd_mem,
				sc->vreg[VREG_MEM].max_vdd, 0);
		if (ret) {
			dev_err(drv.dev, "%s initialization failed (%d)\n",
				sc->vreg[VREG_MEM].name, ret);
			BUG();
		}
		sc->vreg[VREG_MEM].cur_vdd  = vdd_mem;

		/* Set initial vdd_dig vote. */
		ret = rpm_vreg_set_voltage(sc->vreg[VREG_DIG].rpm_vreg_id,
				sc->vreg[VREG_DIG].rpm_vreg_voter, vdd_dig,
				sc->vreg[VREG_DIG].max_vdd, 0);
		if (ret) {
			dev_err(drv.dev, "%s initialization failed (%d)\n",
				sc->vreg[VREG_DIG].name, ret);
			BUG();
		}
		sc->vreg[VREG_DIG].cur_vdd  = vdd_dig;

		/* Setup Krait CPU regulators and initial core voltage. */
		sc->vreg[VREG_CORE].reg = regulator_get(NULL,
					  sc->vreg[VREG_CORE].name);
		if (IS_ERR(sc->vreg[VREG_CORE].reg)) {
			dev_err(drv.dev, "regulator_get(%s) failed (%ld)\n",
				sc->vreg[VREG_CORE].name,
				PTR_ERR(sc->vreg[VREG_CORE].reg));
			BUG();
		}
		vdd_core = calculate_vdd_core(lvl);
		ret = regulator_set_voltage(sc->vreg[VREG_CORE].reg, vdd_core,
					    sc->vreg[VREG_CORE].max_vdd);
		if (ret) {
			dev_err(drv.dev, "regulator_set_voltage(%s) (%d)\n",
				sc->vreg[VREG_CORE].name, ret);
			BUG();
		}
		sc->vreg[VREG_CORE].cur_vdd = vdd_core;
		ret = regulator_set_optimum_mode(sc->vreg[VREG_CORE].reg,
						 sc->vreg[VREG_CORE].peak_ua);
		if (ret < 0) {
			dev_err(drv.dev, "regulator_set_optimum_mode(%s) failed"
				" (%d)\n", sc->vreg[VREG_CORE].name, ret);
			BUG();
		}
		ret = regulator_enable(sc->vreg[VREG_CORE].reg);
		if (ret) {
			dev_err(drv.dev, "regulator_enable(%s) failed (%d)\n",
				sc->vreg[VREG_CORE].name, ret);
			BUG();
		}
	}
}

/* Set initial rate for a given core. */
static void __init init_clock_sources(struct scalable *sc,
				      const struct core_speed *tgt_s)
{
	u32 regval;

	/* Program AUX source input to the secondary MUX. */
	if (sc->aux_clk_sel_addr)
		writel_relaxed(sc->aux_clk_sel, sc->aux_clk_sel_addr);

	/* Switch away from the HFPLL while it's re-initialized. */
	set_sec_clk_src(sc, SEC_SRC_SEL_AUX);
	set_pri_clk_src(sc, PRI_SRC_SEL_SEC_SRC);
	hfpll_init(sc, tgt_s);

	/* Set PRI_SRC_SEL_HFPLL_DIV2 divider to div-2. */
	regval = get_l2_indirect_reg(sc->l2cpmr_iaddr);
	regval &= ~(0x3 << 6);
	set_l2_indirect_reg(sc->l2cpmr_iaddr, regval);

	/* Switch to the target clock source. */
	set_sec_clk_src(sc, tgt_s->sec_src_sel);
	set_pri_clk_src(sc, tgt_s->pri_src_sel);
	sc->cur_speed = tgt_s;
}

static void __init per_cpu_init(int cpu, const struct acpu_level *max_level)
{
	drv.scalable[cpu].hfpll_base =
		ioremap(drv.scalable[cpu].hfpll_phys_base, SZ_32);
	BUG_ON(!drv.scalable[cpu].hfpll_base);

	init_clock_sources(&drv.scalable[cpu], &max_level->speed);
	drv.scalable[cpu].l2_vote = max_level->l2_level;
}

/* Register with bus driver. */
static void __init bus_init(struct msm_bus_scale_pdata *bus_scale_data,
			    unsigned int init_bw)
{
	int ret;

	drv.bus_perf_client = msm_bus_scale_register_client(bus_scale_data);
	if (!drv.bus_perf_client) {
		dev_err(drv.dev, "unable to register bus client\n");
		BUG();
	}

	ret = msm_bus_scale_client_update_request(drv.bus_perf_client, init_bw);
	if (ret)
		dev_err(drv.dev, "initial bandwidth req failed (%d)\n", ret);
}

#ifdef CONFIG_CPU_FREQ_MSM
static struct cpufreq_frequency_table freq_table[NR_CPUS][35];

static void __init cpufreq_table_init(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		int i, freq_cnt = 0;
		/* Construct the freq_table tables from acpu_freq_tbl. */
		for (i = 0; drv.acpu_freq_tbl[i].speed.khz != 0
				&& freq_cnt < ARRAY_SIZE(*freq_table); i++) {
			if (drv.acpu_freq_tbl[i].use_for_scaling) {
				freq_table[cpu][freq_cnt].index = freq_cnt;
				freq_table[cpu][freq_cnt].frequency
					= drv.acpu_freq_tbl[i].speed.khz;
				freq_cnt++;
			}
		}
		/* freq_table not big enough to store all usable freqs. */
		BUG_ON(drv.acpu_freq_tbl[i].speed.khz != 0);

		freq_table[cpu][freq_cnt].index = freq_cnt;
		freq_table[cpu][freq_cnt].frequency = CPUFREQ_TABLE_END;

		dev_info(drv.dev, "CPU%d: %d frequencies supported\n",
			cpu, freq_cnt);

		/* Register table with CPUFreq. */
		cpufreq_frequency_table_get_attr(freq_table[cpu], cpu);
	}
}
#else
static void __init cpufreq_table_init(void) {}
#endif

#define HOT_UNPLUG_KHZ STBY_KHZ
static int __cpuinit acpuclk_cpu_callback(struct notifier_block *nfb,
					    unsigned long action, void *hcpu)
{
	static int prev_khz[NR_CPUS];
	int rc, cpu = (int)hcpu;
	struct scalable *sc = &drv.scalable[cpu];

	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_DEAD:
		prev_khz[cpu] = acpuclk_krait_get_rate(cpu);
		/* Fall through. */
	case CPU_UP_CANCELED:
		acpuclk_krait_set_rate(cpu, HOT_UNPLUG_KHZ, SETRATE_HOTPLUG);
		regulator_set_optimum_mode(sc->vreg[VREG_CORE].reg, 0);
		break;
	case CPU_UP_PREPARE:
		if (WARN_ON(!prev_khz[cpu]))
			return NOTIFY_BAD;
		rc = regulator_set_optimum_mode(sc->vreg[VREG_CORE].reg,
						sc->vreg[VREG_CORE].peak_ua);
		if (rc < 0)
			return NOTIFY_BAD;
		acpuclk_krait_set_rate(cpu, prev_khz[cpu], SETRATE_HOTPLUG);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata acpuclk_cpu_notifier = {
	.notifier_call = acpuclk_cpu_callback,
};

static const struct acpu_level __init *select_freq_plan(
		const struct acpu_level *const *pvs_tbl, u32 qfprom_phys)
{
	const struct acpu_level *l, *max_acpu_level = NULL;
	void __iomem *qfprom_base;
	u32 pte_efuse, pvs, tbl_idx;
	char *pvs_names[] = { "Slow", "Nominal", "Fast", "Unknown" };

	qfprom_base = ioremap(qfprom_phys, SZ_256);
	/* Select frequency tables. */
	if (qfprom_base) {
		pte_efuse = readl_relaxed(qfprom_base + PTE_EFUSE);
		pvs = (pte_efuse >> 10) & 0x7;
		iounmap(qfprom_base);
		if (pvs == 0x7)
			pvs = (pte_efuse >> 13) & 0x7;

		switch (pvs) {
		case 0x0:
		case 0x7:
			tbl_idx = PVS_SLOW;
			break;
		case 0x1:
			tbl_idx = PVS_NOMINAL;
			break;
		case 0x3:
			tbl_idx = PVS_FAST;
			break;
		default:
			tbl_idx = PVS_UNKNOWN;
			break;
		}
	} else {
		tbl_idx = PVS_UNKNOWN;
		dev_err(drv.dev, "Unable to map QFPROM base\n");
	}
	dev_info(drv.dev, "ACPU PVS: %s\n", pvs_names[tbl_idx]);
	if (tbl_idx == PVS_UNKNOWN) {
		tbl_idx = PVS_SLOW;
		dev_warn(drv.dev, "ACPU PVS: Defaulting to %s\n",
			 pvs_names[tbl_idx]);
	}
	drv.acpu_freq_tbl = pvs_tbl[tbl_idx];

	/* Find the max supported scaling frequency. */
	for (l = drv.acpu_freq_tbl; l->speed.khz != 0; l++)
		if (l->use_for_scaling)
			max_acpu_level = l;
	BUG_ON(!max_acpu_level);
	dev_info(drv.dev, "Max ACPU freq: %lu KHz\n",
		 max_acpu_level->speed.khz);

	return max_acpu_level;
}

static struct acpuclk_data acpuclk_krait_data = {
	.set_rate = acpuclk_krait_set_rate,
	.get_rate = acpuclk_krait_get_rate,
	.power_collapse_khz = STBY_KHZ,
	.wait_for_irq_khz = STBY_KHZ,
};

int __init acpuclk_krait_init(struct device *dev,
			      const struct acpuclk_krait_params *params)
{
	const struct acpu_level *max_acpu_level;
	int cpu;

	drv.scalable = params->scalable;
	drv.l2_freq_tbl = params->l2_freq_tbl;
	drv.dev = dev;

	drv.scalable[L2].hfpll_base =
		ioremap(drv.scalable[L2].hfpll_phys_base, SZ_32);
	BUG_ON(!drv.scalable[L2].hfpll_base);

	max_acpu_level = select_freq_plan(params->pvs_acpu_freq_tbl,
					  params->qfprom_phys_base);
	regulator_init(max_acpu_level);
	bus_init(params->bus_scale_data, max_acpu_level->l2_level->bw_level);
	init_clock_sources(&drv.scalable[L2], &max_acpu_level->l2_level->speed);
	for_each_online_cpu(cpu)
		per_cpu_init(cpu, max_acpu_level);

	cpufreq_table_init();

	acpuclk_register(&acpuclk_krait_data);
	register_hotcpu_notifier(&acpuclk_cpu_notifier);

	return 0;
}
