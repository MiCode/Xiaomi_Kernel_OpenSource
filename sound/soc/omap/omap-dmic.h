/*
 * omap-dmic.h  --  OMAP Digital Microphone Controller
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _OMAP_DMIC_H
#define _OMAP_DMIC_H

enum omap_dmic_clk {
	OMAP_DMIC_SYSCLK_PAD_CLKS,		/* PAD_CLKS */
	OMAP_DMIC_SYSCLK_SLIMBLUS_CLKS,		/* SLIMBUS_CLK */
	OMAP_DMIC_SYSCLK_SYNC_MUX_CLKS,		/* DMIC_SYNC_MUX_CLK */
};

/* DMIC dividers */
enum omap_dmic_div {
	OMAP_DMIC_CLKDIV,
};

#endif
