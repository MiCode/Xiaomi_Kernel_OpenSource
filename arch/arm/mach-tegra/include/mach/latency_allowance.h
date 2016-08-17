/*
 * arch/arm/mach-tegra/include/mach/latency_allowance.h
 *
 * Copyright (C) 2011-2012, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MACH_TEGRA_LATENCY_ALLOWANCE_H_
#define _MACH_TEGRA_LATENCY_ALLOWANCE_H_

enum tegra_la_id {
	TEGRA_LA_AFIR = 0,			/* T30 specific */
	TEGRA_LA_AFIW,				/* T30 specific */
	TEGRA_LA_AVPC_ARM7R,
	TEGRA_LA_AVPC_ARM7W,
	TEGRA_LA_DISPLAY_0A,
	TEGRA_LA_DISPLAY_0B,
	TEGRA_LA_DISPLAY_0C,
	TEGRA_LA_DISPLAY_1B,			/* T30 specific */
	TEGRA_LA_DISPLAY_HC,
	TEGRA_LA_DISPLAY_0AB,
	TEGRA_LA_DISPLAY_0BB,
	TEGRA_LA_DISPLAY_0CB,
	TEGRA_LA_DISPLAY_1BB,			/* T30 specific */
	TEGRA_LA_DISPLAY_HCB,
	TEGRA_LA_EPPUP,
	TEGRA_LA_EPPU,
	TEGRA_LA_EPPV,
	TEGRA_LA_EPPY,
	TEGRA_LA_G2PR,
	TEGRA_LA_G2SR,
	TEGRA_LA_G2DR,
	TEGRA_LA_G2DW,
	TEGRA_LA_HOST1X_DMAR,
	TEGRA_LA_HOST1XR,
	TEGRA_LA_HOST1XW,
	TEGRA_LA_HDAR,
	TEGRA_LA_HDAW,
	TEGRA_LA_ISPW,
	TEGRA_LA_MPCORER,
	TEGRA_LA_MPCOREW,
	TEGRA_LA_MPCORE_LPR,
	TEGRA_LA_MPCORE_LPW,
	TEGRA_LA_MPE_UNIFBR,			/* T30 specific */
	TEGRA_LA_MPE_IPRED,			/* T30 specific */
	TEGRA_LA_MPE_AMEMRD,			/* T30 specific */
	TEGRA_LA_MPE_CSRD,			/* T30 specific */
	TEGRA_LA_MPE_UNIFBW,			/* T30 specific */
	TEGRA_LA_MPE_CSWR,			/* T30 specific */
	TEGRA_LA_FDCDRD,
	TEGRA_LA_IDXSRD,
	TEGRA_LA_TEXSRD,
	TEGRA_LA_TEXL2SRD = TEGRA_LA_TEXSRD,	/* T11x specific */
	TEGRA_LA_FDCDWR,
	TEGRA_LA_FDCDRD2,
	TEGRA_LA_IDXSRD2,			/* T30 specific */
	TEGRA_LA_TEXSRD2,			/* T30 specific */
	TEGRA_LA_FDCDWR2,
	TEGRA_LA_PPCS_AHBDMAR,
	TEGRA_LA_PPCS_AHBSLVR,
	TEGRA_LA_PPCS_AHBDMAW,
	TEGRA_LA_PPCS_AHBSLVW,
	TEGRA_LA_PTCR,
	TEGRA_LA_SATAR,				/* T30 specific */
	TEGRA_LA_SATAW,				/* T30 specific */
	TEGRA_LA_VDE_BSEVR,
	TEGRA_LA_VDE_MBER,
	TEGRA_LA_VDE_MCER,
	TEGRA_LA_VDE_TPER,
	TEGRA_LA_VDE_BSEVW,
	TEGRA_LA_VDE_DBGW,
	TEGRA_LA_VDE_MBEW,
	TEGRA_LA_VDE_TPMW,
	TEGRA_LA_VI_RUV,			/* T30 specific */
	TEGRA_LA_VI_WSB,
	TEGRA_LA_VI_WU,
	TEGRA_LA_VI_WV,
	TEGRA_LA_VI_WY,

	TEGRA_LA_MSENCSRD,			/* T11x specific */
	TEGRA_LA_MSENCSWR,			/* T11x specific */
	TEGRA_LA_XUSB_HOSTR,			/* T11x specific */
	TEGRA_LA_XUSB_HOSTW,			/* T11x specific */
	TEGRA_LA_XUSB_DEVR,			/* T11x specific */
	TEGRA_LA_XUSB_DEVW,			/* T11x specific */
	TEGRA_LA_FDCDRD3,			/* T11x specific */
	TEGRA_LA_FDCDRD4,			/* T11x specific */
	TEGRA_LA_FDCDWR3,			/* T11x specific */
	TEGRA_LA_FDCDWR4,			/* T11x specific */
	TEGRA_LA_EMUCIFR,			/* T11x specific */
	TEGRA_LA_EMUCIFW,			/* T11x specific */
	TEGRA_LA_TSECSRD,			/* T11x specific */
	TEGRA_LA_TSECSWR,			/* T11x specific */

	TEGRA_LA_MAX_ID
};

#if !defined(CONFIG_TEGRA_LATENCY_ALLOWANCE)
static inline int tegra_set_latency_allowance(enum tegra_la_id id,
						unsigned int bandwidth_in_mbps)
{
	return 0;
}

static inline void tegra_latency_allowance_update_tick_length(
						unsigned int new_ns_per_tick)
{
	return;
}
#else
int tegra_set_latency_allowance(enum tegra_la_id id,
				unsigned int bandwidth_in_mbps);

void tegra_latency_allowance_update_tick_length(unsigned int new_ns_per_tick);
#endif

#if !defined(CONFIG_TEGRA_LATENCY_ALLOWANCE_SCALING)
static inline int tegra_enable_latency_scaling(enum tegra_la_id id,
						unsigned int threshold_low,
						unsigned int threshold_mid,
						unsigned int threshold_high)
{
	return 0;
}

static inline void tegra_disable_latency_scaling(enum tegra_la_id id)
{
}
#else
int tegra_enable_latency_scaling(enum tegra_la_id id,
				    unsigned int threshold_low,
				    unsigned int threshold_mid,
				    unsigned int threshold_high);

void tegra_disable_latency_scaling(enum tegra_la_id id);
#endif

#endif /* _MACH_TEGRA_LATENCY_ALLOWANCE_H_ */
