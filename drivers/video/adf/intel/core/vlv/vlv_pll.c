/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <intel_adf_device.h>
#include <core/intel_dc_config.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_pll.h>
#include <drm/drmP.h>

#define DIV_ROUND_CLOSEST_ULL(ll, d)    \
	({ u64 _tmp = (ll)+(d)/2; do_div(_tmp, d); _tmp; })

#define intel_pll_invalid(s)   do { pr_debug(s);  return false; } while (0)

static struct intel_limit intel_limits_chv = {
	/*
	 * These are the data rate limits (measured in fast clocks)
	 * since those are the strictest limits we have.  The fast
	 * clock and actual rate limits are more relaxed, so checking
	 * them would make no difference.
	 */
	.dot = { .min = 25000 * 5, .max = 540000 * 5},
	.vco = { .min = 4860000, .max = 6700000 },
	.n = { .min = 1, .max = 1 },
	.m1 = { .min = 2, .max = 2 },
	.m2 = { .min = 24 << 22, .max = 175 << 22 },
	.p1 = { .min = 2, .max = 4 },
	.p2 = { .p2_slow = 1, .p2_fast = 14 },
};

/**
 * Returns whether the given set of divisors are valid for a given refclk with
 * the given connectors.
 */

static bool intel_pll_is_valid(struct intel_limit *limit,
			struct intel_clock *clock)
{
	if (!clock || !limit)
		return false;

	if (clock->n   < limit->n.min   || limit->n.max   < clock->n)
		intel_pll_invalid("n out of range\n");
	if (clock->p1  < limit->p1.min  || limit->p1.max  < clock->p1)
		intel_pll_invalid("p1 out of range\n");
	if (clock->m2  < limit->m2.min  || limit->m2.max  < clock->m2)
		intel_pll_invalid("m2 out of range\n");
	if (clock->m1  < limit->m1.min  || limit->m1.max  < clock->m1)
		intel_pll_invalid("m1 out of range\n");

	if (clock->vco < limit->vco.min || limit->vco.max < clock->vco)
		intel_pll_invalid("vco out of range\n");

	/*
	 * XXX: We may need to be checking "Dot clock" depending on
	 * the multiplier, connector, etc., rather than just a single range.
	 */
	if (clock->dot < limit->dot.min || limit->dot.max < clock->dot)
		intel_pll_invalid("dot out of range\n");

	return true;
}


static void chv_clock(int refclk, struct intel_clock *clock)
{
	if (!clock)
		return;

	clock->m = clock->m1 * clock->m2;
	clock->p = clock->p1 * clock->p2;

	if (WARN_ON(clock->n == 0 || clock->p == 0))
		return;

	clock->vco = DIV_ROUND_CLOSEST_ULL((uint64_t)refclk * clock->m,
			clock->n << 22);
	clock->dot = DIV_ROUND_CLOSEST(clock->vco, clock->p);
}

static bool calc_clock_timings(u32 target, struct intel_clock *best_clock)
{
	struct intel_limit *limit = &intel_limits_chv;
	struct intel_clock clock = {0};
	uint64_t m2;
	int refclk = 100000;
	int found = false;

	memset(best_clock, 0, sizeof(*best_clock));

	/*
	 * Based on hardware doc, the n always set to 1, and m1 always
	 * set to 2.  If requires to support 200Mhz refclk, we need to
	 * revisit this because n may not 1 anymore.
	 */
	clock.n = 1, clock.m1 = 2;
	target *= 5;    /* fast clock */

	for (clock.p1 = limit->p1.max; clock.p1 >= limit->p1.min; clock.p1--) {
		for (clock.p2 = limit->p2.p2_fast;
			clock.p2 >= limit->p2.p2_slow;
			clock.p2 -= clock.p2 > 10 ? 2 : 1) {

			clock.p = clock.p1 * clock.p2;

			m2 = DIV_ROUND_CLOSEST_ULL(((uint64_t)target * clock.p *
					clock.n) << 22, refclk * clock.m1);

			if (m2 > INT_MAX/clock.m1)
				continue;

			clock.m2 = m2;

			chv_clock(refclk, &clock);

			if (!intel_pll_is_valid(limit, &clock))
				continue;

			/* based on hardware requirement, prefer bigger p
			 */
			if (clock.p > best_clock->p) {
				*best_clock = clock;
				found = true;
			}
		}
	}

	/* FIXME: program the calc values through DPIO */
	return found;
}

u32 vlv_pll_program_timings(struct vlv_pll *pll,
		struct drm_mode_modeinfo *mode,
		struct intel_clock *clock)
{
	u32 val = 0;
	bool ret = false;
	/* FIXME: get this from encoder */
	/* int pixel_multiplier = 1; */

	val = DPLL_SSC_REF_CLOCK_CHV | DPLL_REFA_CLK_ENABLE_VLV
		| DPLL_VGA_MODE_DIS;

	if (pll->pll_id != PLL_A)
		val = DPLL_INTEGRATED_CRI_CLK_VLV;

	REG_WRITE(pll->offset, val);

	ret = calc_clock_timings(mode->clock, clock);
	if (!ret) {
		pr_err("%s: unable to find clock values\n", __func__);
		return -EINVAL;
	}

	return 0;
}

u32 vlv_pll_wait_for_port_ready(enum port port_id)
{
	u32 port_mask;
	int dpll_reg;

	switch (port_id) {
	case PORT_B:
		port_mask = DPLL_PORTB_READY_MASK;
		dpll_reg = DPLL(0);
		break;
	case PORT_C:
		port_mask = DPLL_PORTC_READY_MASK;
		dpll_reg = DPLL(0);
		break;
	case PORT_D:
		port_mask = DPLL_PORTD_READY_MASK;
		dpll_reg = DPIO_PHY_STATUS;
		break;
	default:
		BUG();
	}

#define port_name(p) ((p) + 'A')
	if (wait_for((REG_READ(dpll_reg) & port_mask) == 0, 1000)) {
	WARN(1, "timed out waiting for port %c ready: 0x%08x\n",
		     port_name(port_id), REG_READ(dpll_reg));
		return -ETIMEDOUT;
	}

	return 0;
}

u32 vlv_pll_enable(struct vlv_pll *pll,
		struct drm_mode_modeinfo *mode)
{
	/* program the register values */
	u32 val = 0;

	/* program the register values i9xx_crtc_mode_set */

	val = REG_READ(pll->offset);
	val |= DPLL_VCO_ENABLE;
	REG_WRITE(pll->offset, val);

	if (wait_for(((REG_READ(pll->offset) & DPLL_LOCK_VLV) ==
						DPLL_LOCK_VLV), 1)) {
		pr_err("PLL %d failed to lock\n", pll->pll_id);
		return -ETIMEDOUT;
	}

	return 0;
}

u32 vlv_pll_disable(struct vlv_pll *pll)
{
	/* program the register values */
	u32 val = 0;

	val = REG_READ(pll->offset);
	val &= ~DPLL_VCO_ENABLE;
	if (pll->pll_id == _DPLL_A)
		val &= ~DPLL_INTEGRATED_CRI_CLK_VLV;

	REG_WRITE(pll->offset, val);
	udelay(1);
	return 0;
}

bool vlv_pll_init(struct vlv_pll *pll, enum intel_pipe_type type,
		enum pipe pipe_id, enum port port_id)
{
	/* do any init needed for each pll */
	if (type == INTEL_PIPE_DSI)
		return vlv_dsi_pll_init(pll, pipe_id, port_id);

	/* FIXME: convert to proper pll */
	pll->pll_id = (enum pll) pipe_id;
	pll->offset = DPLL(pipe_id);
	pll->port_id = port_id;

	return true;
}

bool vlv_pll_destroy(struct vlv_pll *pll)
{
	return false;
}
