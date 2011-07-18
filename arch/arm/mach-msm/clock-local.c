/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <mach/msm_iomap.h>
#include <mach/clk.h>
#include <mach/scm-io.h>

#include "clock.h"
#include "clock-local.h"

#ifdef CONFIG_MSM_SECURE_IO
#undef readl_relaxed
#undef writel_relaxed
#define readl_relaxed secure_readl
#define writel_relaxed secure_writel
#endif

/*
 * When enabling/disabling a clock, check the halt bit up to this number
 * number of times (with a 1 us delay in between) before continuing.
 */
#define HALT_CHECK_MAX_LOOPS	100
/* For clock without halt checking, wait this long after enables/disables. */
#define HALT_CHECK_DELAY_US	10

DEFINE_SPINLOCK(local_clock_reg_lock);
struct clk_freq_tbl local_dummy_freq = F_END;

unsigned local_sys_vdd_votes[NUM_SYS_VDD_LEVELS];
static DEFINE_SPINLOCK(sys_vdd_vote_lock);

/*
 * Common Set-Rate Functions
 */

/* For clocks with MND dividers. */
void set_rate_mnd(struct rcg_clk *clk, struct clk_freq_tbl *nf)
{
	uint32_t ns_reg_val, ctl_reg_val;

	/* Assert MND reset. */
	ns_reg_val = readl_relaxed(clk->ns_reg);
	ns_reg_val |= BIT(7);
	writel_relaxed(ns_reg_val, clk->ns_reg);

	/* Program M and D values. */
	writel_relaxed(nf->md_val, clk->md_reg);

	/* If the clock has a separate CC register, program it. */
	if (clk->ns_reg != clk->b.ctl_reg) {
		ctl_reg_val = readl_relaxed(clk->b.ctl_reg);
		ctl_reg_val &= ~(clk->ctl_mask);
		ctl_reg_val |= nf->ctl_val;
		writel_relaxed(ctl_reg_val, clk->b.ctl_reg);
	}

	/* Deassert MND reset. */
	ns_reg_val &= ~BIT(7);
	writel_relaxed(ns_reg_val, clk->ns_reg);
}

void set_rate_nop(struct rcg_clk *clk, struct clk_freq_tbl *nf)
{
	/*
	 * Nothing to do for fixed-rate or integer-divider clocks. Any settings
	 * in NS registers are applied in the enable path, since power can be
	 * saved by leaving an un-clocked or slowly-clocked source selected
	 * until the clock is enabled.
	 */
}

void set_rate_mnd_8(struct rcg_clk *clk, struct clk_freq_tbl *nf)
{
	uint32_t ctl_reg_val;

	/* Assert MND reset. */
	ctl_reg_val = readl_relaxed(clk->b.ctl_reg);
	ctl_reg_val |= BIT(8);
	writel_relaxed(ctl_reg_val, clk->b.ctl_reg);

	/* Program M and D values. */
	writel_relaxed(nf->md_val, clk->md_reg);

	/* Program MN counter Enable and Mode. */
	ctl_reg_val &= ~(clk->ctl_mask);
	ctl_reg_val |= nf->ctl_val;
	writel_relaxed(ctl_reg_val, clk->b.ctl_reg);

	/* Deassert MND reset. */
	ctl_reg_val &= ~BIT(8);
	writel_relaxed(ctl_reg_val, clk->b.ctl_reg);
}

void set_rate_mnd_banked(struct rcg_clk *clk, struct clk_freq_tbl *nf)
{
	struct bank_masks *banks = clk->bank_masks;
	const struct bank_mask_info *new_bank_masks;
	const struct bank_mask_info *old_bank_masks;
	uint32_t ns_reg_val, ctl_reg_val;
	uint32_t bank_sel;

	/*
	 * Determine active bank and program the other one. If the clock is
	 * off, program the active bank since bank switching won't work if
	 * both banks aren't running.
	 */
	ctl_reg_val = readl_relaxed(clk->b.ctl_reg);
	bank_sel = !!(ctl_reg_val & banks->bank_sel_mask);
	 /* If clock isn't running, don't switch banks. */
	bank_sel ^= (!clk->enabled || clk->current_freq->freq_hz == 0);
	if (bank_sel == 0) {
		new_bank_masks = &banks->bank1_mask;
		old_bank_masks = &banks->bank0_mask;
	} else {
		new_bank_masks = &banks->bank0_mask;
		old_bank_masks = &banks->bank1_mask;
	}

	ns_reg_val = readl_relaxed(clk->ns_reg);

	/* Assert bank MND reset. */
	ns_reg_val |= new_bank_masks->rst_mask;
	writel_relaxed(ns_reg_val, clk->ns_reg);

	/*
	 * Program NS only if the clock is enabled, since the NS will be set
	 * as part of the enable procedure and should remain with a low-power
	 * MUX input selected until then.
	 */
	if (clk->enabled) {
		ns_reg_val &= ~(new_bank_masks->ns_mask);
		ns_reg_val |= (nf->ns_val & new_bank_masks->ns_mask);
		writel_relaxed(ns_reg_val, clk->ns_reg);
	}

	writel_relaxed(nf->md_val, new_bank_masks->md_reg);

	/* Enable counter only if clock is enabled. */
	if (clk->enabled)
		ctl_reg_val |= new_bank_masks->mnd_en_mask;
	else
		ctl_reg_val &= ~(new_bank_masks->mnd_en_mask);

	ctl_reg_val &= ~(new_bank_masks->mode_mask);
	ctl_reg_val |= (nf->ctl_val & new_bank_masks->mode_mask);
	writel_relaxed(ctl_reg_val, clk->b.ctl_reg);

	/* Deassert bank MND reset. */
	ns_reg_val &= ~(new_bank_masks->rst_mask);
	writel_relaxed(ns_reg_val, clk->ns_reg);

	/*
	 * Switch to the new bank if clock is running.  If it isn't, then
	 * no switch is necessary since we programmed the active bank.
	 */
	if (clk->enabled && clk->current_freq->freq_hz) {
		ctl_reg_val ^= banks->bank_sel_mask;
		writel_relaxed(ctl_reg_val, clk->b.ctl_reg);
		/*
		 * Wait at least 6 cycles of slowest bank's clock
		 * for the glitch-free MUX to fully switch sources.
		 */
		mb();
		udelay(1);

		/* Disable old bank's MN counter. */
		ctl_reg_val &= ~(old_bank_masks->mnd_en_mask);
		writel_relaxed(ctl_reg_val, clk->b.ctl_reg);

		/* Program old bank to a low-power source and divider. */
		ns_reg_val &= ~(old_bank_masks->ns_mask);
		ns_reg_val |= (clk->freq_tbl->ns_val & old_bank_masks->ns_mask);
		writel_relaxed(ns_reg_val, clk->ns_reg);
	}

	/*
	 * If this freq requires the MN counter to be enabled,
	 * update the enable mask to match the current bank.
	 */
	if (nf->mnd_en_mask)
		nf->mnd_en_mask = new_bank_masks->mnd_en_mask;
	/* Update the NS mask to match the current bank. */
	clk->ns_mask = new_bank_masks->ns_mask;
}

void set_rate_div_banked(struct rcg_clk *clk, struct clk_freq_tbl *nf)
{
	struct bank_masks *banks = clk->bank_masks;
	const struct bank_mask_info *new_bank_masks;
	const struct bank_mask_info *old_bank_masks;
	uint32_t ns_reg_val, bank_sel;

	/*
	 * Determine active bank and program the other one. If the clock is
	 * off, program the active bank since bank switching won't work if
	 * both banks aren't running.
	 */
	ns_reg_val = readl_relaxed(clk->ns_reg);
	bank_sel = !!(ns_reg_val & banks->bank_sel_mask);
	 /* If clock isn't running, don't switch banks. */
	bank_sel ^= (!clk->enabled || clk->current_freq->freq_hz == 0);
	if (bank_sel == 0) {
		new_bank_masks = &banks->bank1_mask;
		old_bank_masks = &banks->bank0_mask;
	} else {
		new_bank_masks = &banks->bank0_mask;
		old_bank_masks = &banks->bank1_mask;
	}

	/*
	 * Program NS only if the clock is enabled, since the NS will be set
	 * as part of the enable procedure and should remain with a low-power
	 * MUX input selected until then.
	 */
	if (clk->enabled) {
		ns_reg_val &= ~(new_bank_masks->ns_mask);
		ns_reg_val |= (nf->ns_val & new_bank_masks->ns_mask);
		writel_relaxed(ns_reg_val, clk->ns_reg);
	}

	/*
	 * Switch to the new bank if clock is running.  If it isn't, then
	 * no switch is necessary since we programmed the active bank.
	 */
	if (clk->enabled && clk->current_freq->freq_hz) {
		ns_reg_val ^= banks->bank_sel_mask;
		writel_relaxed(ns_reg_val, clk->ns_reg);
		/*
		 * Wait at least 6 cycles of slowest bank's clock
		 * for the glitch-free MUX to fully switch sources.
		 */
		mb();
		udelay(1);

		/* Program old bank to a low-power source and divider. */
		ns_reg_val &= ~(old_bank_masks->ns_mask);
		ns_reg_val |= (clk->freq_tbl->ns_val & old_bank_masks->ns_mask);
		writel_relaxed(ns_reg_val, clk->ns_reg);
	}

	/* Update the NS mask to match the current bank. */
	clk->ns_mask = new_bank_masks->ns_mask;
}

int (*soc_update_sys_vdd)(enum sys_vdd_level level);

/*
 * SYS_VDD voting functions
 */

/* Update system voltage level given the current votes. */
static int local_update_sys_vdd(void)
{
	static int cur_level = NUM_SYS_VDD_LEVELS;
	int level, rc = 0;

	if (local_sys_vdd_votes[HIGH])
		level = HIGH;
	else if (local_sys_vdd_votes[NOMINAL])
		level = NOMINAL;
	else if (local_sys_vdd_votes[LOW])
		level = LOW;
	else
		level = NONE;

	if (level == cur_level)
		return rc;

	rc = soc_update_sys_vdd(level);
	if (!rc)
		cur_level = level;

	return rc;
}

/* Vote for a system voltage level. */
int local_vote_sys_vdd(unsigned level)
{
	int rc = 0;
	unsigned long flags;

	/* Bounds checking. */
	if (level >= ARRAY_SIZE(local_sys_vdd_votes))
		return -EINVAL;

	spin_lock_irqsave(&sys_vdd_vote_lock, flags);
	local_sys_vdd_votes[level]++;
	rc = local_update_sys_vdd();
	if (rc)
		local_sys_vdd_votes[level]--;
	spin_unlock_irqrestore(&sys_vdd_vote_lock, flags);

	return rc;
}

/* Remove vote for a system voltage level. */
int local_unvote_sys_vdd(unsigned level)
{
	int rc = 0;
	unsigned long flags;

	/* Bounds checking. */
	if (level >= ARRAY_SIZE(local_sys_vdd_votes))
		return -EINVAL;

	spin_lock_irqsave(&sys_vdd_vote_lock, flags);

	if (WARN(!local_sys_vdd_votes[level],
		"Reference counts are incorrect for level %d!\n", level))
		goto out;

	local_sys_vdd_votes[level]--;
	rc = local_update_sys_vdd();
	if (rc)
		local_sys_vdd_votes[level]++;
out:
	spin_unlock_irqrestore(&sys_vdd_vote_lock, flags);
	return rc;
}
/*
 * Clock enable/disable functions
 */

/* Return non-zero if a clock status registers shows the clock is halted. */
static int branch_clk_is_halted(const struct branch *clk)
{
	int invert = (clk->halt_check == ENABLE);
	int status_bit = readl_relaxed(clk->halt_reg) & BIT(clk->halt_bit);
	return invert ? !status_bit : status_bit;
}

static void __branch_clk_enable_reg(const struct branch *clk, const char *name)
{
	u32 reg_val;

	if (clk->en_mask) {
		reg_val = readl_relaxed(clk->ctl_reg);
		reg_val |= clk->en_mask;
		writel_relaxed(reg_val, clk->ctl_reg);
	}

	/*
	 * Use a memory barrier since some halt status registers are
	 * not within the same 1K segment as the branch/root enable
	 * registers.  It's also needed in the udelay() case to ensure
	 * the delay starts after the branch enable.
	 */
	mb();

	/* Wait for clock to enable before returning. */
	if (clk->halt_check == DELAY)
		udelay(HALT_CHECK_DELAY_US);
	else if (clk->halt_check == ENABLE || clk->halt_check == HALT
			|| clk->halt_check == ENABLE_VOTED
			|| clk->halt_check == HALT_VOTED) {
		int count;

		/* Wait up to HALT_CHECK_MAX_LOOPS for clock to enable. */
		for (count = HALT_CHECK_MAX_LOOPS; branch_clk_is_halted(clk)
					&& count > 0; count--)
			udelay(1);
		WARN(count == 0, "%s status stuck at 'off'", name);
	}
}

/* Perform any register operations required to enable the clock. */
static void __rcg_clk_enable_reg(struct rcg_clk *clk)
{
	u32 reg_val;
	void __iomem *const reg = clk->b.ctl_reg;

	WARN(clk->current_freq == &local_dummy_freq,
		"Attempting to enable %s before setting its rate. "
		"Set the rate first!\n", clk->c.dbg_name);

	/*
	 * Program the NS register, if applicable. NS registers are not
	 * set in the set_rate path because power can be saved by deferring
	 * the selection of a clocked source until the clock is enabled.
	 */
	if (clk->ns_mask) {
		reg_val = readl_relaxed(clk->ns_reg);
		reg_val &= ~(clk->ns_mask);
		reg_val |= (clk->current_freq->ns_val & clk->ns_mask);
		writel_relaxed(reg_val, clk->ns_reg);
	}

	/* Enable MN counter, if applicable. */
	reg_val = readl_relaxed(reg);
	if (clk->current_freq->mnd_en_mask) {
		reg_val |= clk->current_freq->mnd_en_mask;
		writel_relaxed(reg_val, reg);
	}
	/* Enable root. */
	if (clk->root_en_mask) {
		reg_val |= clk->root_en_mask;
		writel_relaxed(reg_val, reg);
	}
	__branch_clk_enable_reg(&clk->b, clk->c.dbg_name);
}

/* Perform any register operations required to disable the branch. */
static u32 __branch_clk_disable_reg(const struct branch *clk, const char *name)
{
	u32 reg_val;

	reg_val = readl_relaxed(clk->ctl_reg);
	if (clk->en_mask) {
		reg_val &= ~(clk->en_mask);
		writel_relaxed(reg_val, clk->ctl_reg);
	}

	/*
	 * Use a memory barrier since some halt status registers are
	 * not within the same K segment as the branch/root enable
	 * registers.  It's also needed in the udelay() case to ensure
	 * the delay starts after the branch disable.
	 */
	mb();

	/* Wait for clock to disable before continuing. */
	if (clk->halt_check == DELAY || clk->halt_check == ENABLE_VOTED
				     || clk->halt_check == HALT_VOTED)
		udelay(HALT_CHECK_DELAY_US);
	else if (clk->halt_check == ENABLE || clk->halt_check == HALT) {
		int count;

		/* Wait up to HALT_CHECK_MAX_LOOPS for clock to disable. */
		for (count = HALT_CHECK_MAX_LOOPS; !branch_clk_is_halted(clk)
					&& count > 0; count--)
			udelay(1);
		WARN(count == 0, "%s status stuck at 'on'", name);
	}

	return reg_val;
}

/* Perform any register operations required to disable the generator. */
static void __rcg_clk_disable_reg(struct rcg_clk *clk)
{
	void __iomem *const reg = clk->b.ctl_reg;
	uint32_t reg_val;

	reg_val = __branch_clk_disable_reg(&clk->b, clk->c.dbg_name);
	/* Disable root. */
	if (clk->root_en_mask) {
		reg_val &= ~(clk->root_en_mask);
		writel_relaxed(reg_val, reg);
	}
	/* Disable MN counter, if applicable. */
	if (clk->current_freq->mnd_en_mask) {
		reg_val &= ~(clk->current_freq->mnd_en_mask);
		writel_relaxed(reg_val, reg);
	}
	/*
	 * Program NS register to low-power value with an un-clocked or
	 * slowly-clocked source selected.
	 */
	if (clk->ns_mask) {
		reg_val = readl_relaxed(clk->ns_reg);
		reg_val &= ~(clk->ns_mask);
		reg_val |= (clk->freq_tbl->ns_val & clk->ns_mask);
		writel_relaxed(reg_val, clk->ns_reg);
	}
}

static void _rcg_clk_enable(struct rcg_clk *clk)
{
	unsigned long flags;

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	__rcg_clk_enable_reg(clk);
	clk->enabled = true;
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
}

static void _rcg_clk_disable(struct rcg_clk *clk)
{
	unsigned long flags;

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	__rcg_clk_disable_reg(clk);
	clk->enabled = false;
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
}

/* Enable a clock and any related power rail. */
int rcg_clk_enable(struct clk *c)
{
	int rc;
	struct rcg_clk *clk = to_rcg_clk(c);

	rc = local_vote_sys_vdd(clk->current_freq->sys_vdd);
	if (rc)
		goto err_vdd;
	rc = clk_enable(clk->depends);
	if (rc)
		goto err_dep;
	_rcg_clk_enable(clk);
	return rc;

err_dep:
	local_unvote_sys_vdd(clk->current_freq->sys_vdd);
err_vdd:
	return rc;
}

/* Disable a clock and any related power rail. */
void rcg_clk_disable(struct clk *c)
{
	struct rcg_clk *clk = to_rcg_clk(c);

	_rcg_clk_disable(clk);
	clk_disable(clk->depends);
	local_unvote_sys_vdd(clk->current_freq->sys_vdd);
}

/* Turn off a clock at boot, without checking refcounts or disabling depends. */
void rcg_clk_auto_off(struct clk *c)
{
	_rcg_clk_disable(to_rcg_clk(c));
}

/*
 * Frequency-related functions
 */

/* Set a clock's frequency. */
static int _rcg_clk_set_rate(struct rcg_clk *clk, struct clk_freq_tbl *nf)
{
	struct clk_freq_tbl *cf;
	int rc = 0;
	struct clk *chld;
	unsigned long flags;

	spin_lock_irqsave(&clk->c.lock, flags);

	/* Check if frequency is actually changed. */
	cf = clk->current_freq;
	if (nf == cf)
		goto unlock;

	if (clk->enabled) {
		/* Vote for voltage and source for new freq. */
		rc = local_vote_sys_vdd(nf->sys_vdd);
		if (rc)
			goto unlock;
		rc = clk_enable(nf->src_clk);
		if (rc) {
			local_unvote_sys_vdd(nf->sys_vdd);
			goto unlock;
		}
	}

	spin_lock(&local_clock_reg_lock);

	/* Disable branch if clock isn't dual-banked with a glitch-free MUX. */
	if (clk->bank_masks == NULL) {
		/* Disable all branches to prevent glitches. */
		list_for_each_entry(chld, &clk->c.children, siblings) {
			struct branch_clk *x = to_branch_clk(chld);
			/*
			 * We don't need to grab the child's lock because
			 * we hold the local_clock_reg_lock and 'enabled' is
			 * only modified within lock.
			 */
			if (x->enabled)
				__branch_clk_disable_reg(&x->b, x->c.dbg_name);
		}
		if (clk->enabled)
			__rcg_clk_disable_reg(clk);
	}

	/* Perform clock-specific frequency switch operations. */
	BUG_ON(!clk->set_rate);
	clk->set_rate(clk, nf);

	/*
	 * Current freq must be updated before __rcg_clk_enable_reg()
	 * is called to make sure the MNCNTR_EN bit is set correctly.
	 */
	clk->current_freq = nf;

	/* Enable any clocks that were disabled. */
	if (clk->bank_masks == NULL) {
		if (clk->enabled)
			__rcg_clk_enable_reg(clk);
		/* Enable only branches that were ON before. */
		list_for_each_entry(chld, &clk->c.children, siblings) {
			struct branch_clk *x = to_branch_clk(chld);
			if (x->enabled)
				__branch_clk_enable_reg(&x->b, x->c.dbg_name);
		}
	}

	spin_unlock(&local_clock_reg_lock);

	/* Release requirements of the old freq. */
	if (clk->enabled) {
		clk_disable(cf->src_clk);
		local_unvote_sys_vdd(cf->sys_vdd);
	}
unlock:
	spin_unlock_irqrestore(&clk->c.lock, flags);

	return rc;
}

/* Set a clock to an exact rate. */
int rcg_clk_set_rate(struct clk *c, unsigned rate)
{
	struct rcg_clk *clk = to_rcg_clk(c);
	struct clk_freq_tbl *nf;

	for (nf = clk->freq_tbl; nf->freq_hz != FREQ_END
			&& nf->freq_hz != rate; nf++)
		;

	if (nf->freq_hz == FREQ_END)
		return -EINVAL;

	return _rcg_clk_set_rate(clk, nf);
}

/* Set a clock to a rate greater than some minimum. */
int rcg_clk_set_min_rate(struct clk *c, unsigned rate)
{
	struct rcg_clk *clk = to_rcg_clk(c);
	struct clk_freq_tbl *nf;

	for (nf = clk->freq_tbl; nf->freq_hz != FREQ_END
			&& nf->freq_hz < rate; nf++)
		;

	if (nf->freq_hz == FREQ_END)
		return -EINVAL;

	return _rcg_clk_set_rate(clk, nf);
}

/* Get the currently-set rate of a clock in Hz. */
unsigned rcg_clk_get_rate(struct clk *c)
{
	struct rcg_clk *clk = to_rcg_clk(c);
	unsigned long flags;
	unsigned ret = 0;

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	ret = clk->current_freq->freq_hz;
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	/*
	 * Return 0 if the rate has never been set. Might not be correct,
	 * but it's good enough.
	 */
	if (ret == FREQ_END)
		ret = 0;

	return ret;
}

/* Check if a clock is currently enabled. */
int rcg_clk_is_enabled(struct clk *clk)
{
	return to_rcg_clk(clk)->enabled;
}

/* Return a supported rate that's at least the specified rate. */
long rcg_clk_round_rate(struct clk *c, unsigned rate)
{
	struct rcg_clk *clk = to_rcg_clk(c);
	struct clk_freq_tbl *f;

	for (f = clk->freq_tbl; f->freq_hz != FREQ_END; f++)
		if (f->freq_hz >= rate)
			return f->freq_hz;

	return -EPERM;
}

bool local_clk_is_local(struct clk *clk)
{
	return true;
}

/* Return the nth supported frequency for a given clock. */
int rcg_clk_list_rate(struct clk *c, unsigned n)
{
	struct rcg_clk *clk = to_rcg_clk(c);

	if (!clk->freq_tbl || clk->freq_tbl->freq_hz == FREQ_END)
		return -ENXIO;

	return (clk->freq_tbl + n)->freq_hz;
}

struct clk *rcg_clk_get_parent(struct clk *clk)
{
	return to_rcg_clk(clk)->current_freq->src_clk;
}

static int pll_vote_clk_enable(struct clk *clk)
{
	u32 ena;
	unsigned long flags;
	struct pll_vote_clk *pll = to_pll_vote_clk(clk);

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	ena = readl_relaxed(pll->en_reg);
	ena |= pll->en_mask;
	writel_relaxed(ena, pll->en_reg);
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	/* Wait until PLL is enabled */
	while ((readl_relaxed(pll->status_reg) & BIT(16)) == 0)
		cpu_relax();

	return 0;
}

static void pll_vote_clk_disable(struct clk *clk)
{
	u32 ena;
	unsigned long flags;
	struct pll_vote_clk *pll = to_pll_vote_clk(clk);

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	ena = readl_relaxed(pll->en_reg);
	ena &= ~(pll->en_mask);
	writel_relaxed(ena, pll->en_reg);
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
}

static unsigned pll_vote_clk_get_rate(struct clk *clk)
{
	struct pll_vote_clk *pll = to_pll_vote_clk(clk);
	return pll->rate;
}

static struct clk *pll_vote_clk_get_parent(struct clk *clk)
{
	struct pll_vote_clk *pll = to_pll_vote_clk(clk);
	return pll->parent;
}

static int pll_vote_clk_is_enabled(struct clk *clk)
{
	struct pll_vote_clk *pll = to_pll_vote_clk(clk);
	return !!(readl_relaxed(pll->status_reg) & BIT(16));
}

struct clk_ops clk_ops_pll_vote = {
	.enable = pll_vote_clk_enable,
	.disable = pll_vote_clk_disable,
	.is_enabled = pll_vote_clk_is_enabled,
	.get_rate = pll_vote_clk_get_rate,
	.get_parent = pll_vote_clk_get_parent,
	.is_local = local_clk_is_local,
};

static int pll_clk_enable(struct clk *clk)
{
	u32 mode;
	unsigned long flags;
	struct pll_clk *pll = to_pll_clk(clk);

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	mode = readl_relaxed(pll->mode_reg);
	/* Disable PLL bypass mode. */
	mode |= BIT(1);
	writel_relaxed(mode, pll->mode_reg);

	/*
	 * H/W requires a 5us delay between disabling the bypass and
	 * de-asserting the reset. Delay 10us just to be safe.
	 */
	mb();
	udelay(10);

	/* De-assert active-low PLL reset. */
	mode |= BIT(2);
	writel_relaxed(mode, pll->mode_reg);

	/* Wait until PLL is locked. */
	mb();
	udelay(50);

	/* Enable PLL output. */
	mode |= BIT(0);
	writel_relaxed(mode, pll->mode_reg);

	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
	return 0;
}

static void pll_clk_disable(struct clk *clk)
{
	u32 mode;
	unsigned long flags;
	struct pll_clk *pll = to_pll_clk(clk);

	/*
	 * Disable the PLL output, disable test mode, enable
	 * the bypass mode, and assert the reset.
	 */
	spin_lock_irqsave(&local_clock_reg_lock, flags);
	mode = readl_relaxed(pll->mode_reg);
	mode &= ~BM(3, 0);
	writel_relaxed(mode, pll->mode_reg);
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
}

static unsigned pll_clk_get_rate(struct clk *clk)
{
	struct pll_clk *pll = to_pll_clk(clk);
	return pll->rate;
}

static struct clk *pll_clk_get_parent(struct clk *clk)
{
	struct pll_clk *pll = to_pll_clk(clk);
	return pll->parent;
}

struct clk_ops clk_ops_pll = {
	.enable = pll_clk_enable,
	.disable = pll_clk_disable,
	.get_rate = pll_clk_get_rate,
	.get_parent = pll_clk_get_parent,
	.is_local = local_clk_is_local,
};

struct clk_ops clk_ops_gnd = {
	.get_rate = fixed_clk_get_rate,
	.is_local = local_clk_is_local,
};

struct fixed_clk gnd_clk = {
	.c = {
		.dbg_name = "ground_clk",
		.ops = &clk_ops_gnd,
		CLK_INIT(gnd_clk.c),
	},
};

struct clk_ops clk_ops_measure = {
	.is_local = local_clk_is_local,
};

int branch_clk_enable(struct clk *clk)
{
	int rc;
	unsigned long flags;
	struct branch_clk *branch = to_branch_clk(clk);

	rc = clk_enable(branch->depends);
	if (rc)
		return rc;

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	__branch_clk_enable_reg(&branch->b, branch->c.dbg_name);
	branch->enabled = true;
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	return 0;
}

void branch_clk_disable(struct clk *clk)
{
	unsigned long flags;
	struct branch_clk *branch = to_branch_clk(clk);

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	__branch_clk_disable_reg(&branch->b, branch->c.dbg_name);
	branch->enabled = false;
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	clk_disable(branch->depends);
}

struct clk *branch_clk_get_parent(struct clk *clk)
{
	struct branch_clk *branch = to_branch_clk(clk);
	return branch->parent;
}

int branch_clk_set_parent(struct clk *clk, struct clk *parent)
{
	/*
	 * We setup the parent pointer at init time in msm_clock_init().
	 * This check is to make sure drivers can't change the parent.
	 */
	if (parent && list_empty(&clk->siblings)) {
		list_add(&clk->siblings, &parent->children);
		return 0;
	}
	return -EINVAL;
}

int branch_clk_is_enabled(struct clk *clk)
{
	struct branch_clk *branch = to_branch_clk(clk);
	return branch->enabled;
}

void branch_clk_auto_off(struct clk *clk)
{
	struct branch_clk *branch = to_branch_clk(clk);
	unsigned long flags;

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	__branch_clk_disable_reg(&branch->b, branch->c.dbg_name);
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
}

int branch_reset(struct branch *clk, enum clk_reset_action action)
{
	int ret = 0;
	u32 reg_val;
	unsigned long flags;

	if (!clk->reset_reg)
		return -EPERM;

	spin_lock_irqsave(&local_clock_reg_lock, flags);

	reg_val = readl_relaxed(clk->reset_reg);
	switch (action) {
	case CLK_RESET_ASSERT:
		reg_val |= clk->reset_mask;
		break;
	case CLK_RESET_DEASSERT:
		reg_val &= ~(clk->reset_mask);
		break;
	default:
		ret = -EINVAL;
	}
	writel_relaxed(reg_val, clk->reset_reg);

	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	/* Make sure write is issued before returning. */
	mb();

	return ret;
}

int branch_clk_reset(struct clk *clk, enum clk_reset_action action)
{
	return branch_reset(&to_branch_clk(clk)->b, action);
}
