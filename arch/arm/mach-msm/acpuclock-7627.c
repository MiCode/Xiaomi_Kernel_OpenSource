/*
 * MSM architecture clock driver
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2012, Code Aurora Forum. All rights reserved.
 * Author: San Mehat <san@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/sort.h>
#include <linux/platform_device.h>

#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/socinfo.h>
#include <asm/mach-types.h>
#include <asm/cpu.h>

#include "smd_private.h"
#include "acpuclock.h"

#define A11S_CLK_CNTL_ADDR (MSM_CSR_BASE + 0x100)
#define A11S_CLK_SEL_ADDR (MSM_CSR_BASE + 0x104)
#define A11S_VDD_SVS_PLEVEL_ADDR (MSM_CSR_BASE + 0x124)


#define POWER_COLLAPSE_KHZ 19200

/* Max CPU frequency allowed by hardware while in standby waiting for an irq. */
#define MAX_WAIT_FOR_IRQ_KHZ 128000

/**
 * enum - For acpuclock PLL IDs
 */
enum {
	ACPU_PLL_0	= 0,
	ACPU_PLL_1,
	ACPU_PLL_2,
	ACPU_PLL_3,
	ACPU_PLL_4,
	ACPU_PLL_TCXO,
	ACPU_PLL_END,
};

struct acpu_clk_src {
	struct clk *clk;
	const char *name;
};

static struct acpu_clk_src pll_clk[ACPU_PLL_END] = {
	[ACPU_PLL_0] = { .name = "pll0_clk" },
	[ACPU_PLL_1] = { .name = "pll1_clk" },
	[ACPU_PLL_2] = { .name = "pll2_clk" },
	[ACPU_PLL_4] = { .name = "pll4_clk" },
};

struct clock_state {
	struct clkctl_acpu_speed	*current_speed;
	struct mutex			lock;
	uint32_t			max_speed_delta_khz;
	struct clk			*ebi1_clk;
};

struct clkctl_acpu_speed {
	unsigned int	use_for_scaling;
	unsigned int	a11clk_khz;
	int		pll;
	unsigned int	a11clk_src_sel;
	unsigned int	a11clk_src_div;
	unsigned int	ahbclk_khz;
	unsigned int	ahbclk_div;
	int		vdd;
	unsigned int	axiclk_khz;
	unsigned long   lpj; /* loops_per_jiffy */
	/* Pointers in acpu_freq_tbl[] for max up/down steppings. */
	struct clkctl_acpu_speed *down[ACPU_PLL_END];
	struct clkctl_acpu_speed *up[ACPU_PLL_END];
};

static struct clock_state drv_state = { 0 };
static struct clkctl_acpu_speed *acpu_freq_tbl;

/*
 * ACPU freq tables used for different PLLs frequency combinations. The
 * correct table is selected during init.
 *
 * Table stepping up/down entries are calculated during boot to choose the
 * largest frequency jump that's less than max_speed_delta_khz on each PLL.
 */

/* 7627 with GSM capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_245_pll2_1200_pll4_0[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 19200, 0, 0, 30720 },
	{ 0, 120000, ACPU_PLL_0, 4, 7,  60000, 1, 3,  61440 },
	{ 1, 122880, ACPU_PLL_1, 1, 1,  61440, 1, 3,  61440 },
	{ 0, 200000, ACPU_PLL_2, 2, 5,  66667, 2, 4,  61440 },
	{ 1, 245760, ACPU_PLL_1, 1, 0, 122880, 1, 4,  61440 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 160000, 1, 5, 160000 },
	{ 0, 400000, ACPU_PLL_2, 2, 2, 133333, 2, 5, 160000 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 160000, 2, 6, 160000 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 200000, 2, 7, 200000 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7627 with CDMA capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_196_pll2_1200_pll4_0[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 19200, 0, 0, 24576 },
	{ 1,  98304, ACPU_PLL_1, 1, 1,  98304, 0, 3,  49152 },
	{ 0, 120000, ACPU_PLL_0, 4, 7,  60000, 1, 3,  49152 },
	{ 1, 196608, ACPU_PLL_1, 1, 0,  65536, 2, 4,  98304 },
	{ 0, 200000, ACPU_PLL_2, 2, 5,  66667, 2, 4,  98304 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 160000, 1, 5, 160000 },
	{ 0, 400000, ACPU_PLL_2, 2, 2, 133333, 2, 5, 160000 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 160000, 2, 6, 160000 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 200000, 2, 7, 200000 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7627 with GSM capable modem - PLL2 @ 800 */
static struct clkctl_acpu_speed pll0_960_pll1_245_pll2_800_pll4_0[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 19200, 0, 0, 30720 },
	{ 0, 120000, ACPU_PLL_0, 4, 7,  60000, 1, 3,  61440 },
	{ 1, 122880, ACPU_PLL_1, 1, 1,  61440, 1, 3,  61440 },
	{ 0, 200000, ACPU_PLL_2, 2, 3,  66667, 2, 4,  61440 },
	{ 1, 245760, ACPU_PLL_1, 1, 0, 122880, 1, 4,  61440 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 160000, 1, 5, 160000 },
	{ 0, 400000, ACPU_PLL_2, 2, 1, 133333, 2, 5, 160000 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 160000, 2, 6, 160000 },
	{ 1, 800000, ACPU_PLL_2, 2, 0, 200000, 3, 7, 200000 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7627 with CDMA capable modem - PLL2 @ 800 */
static struct clkctl_acpu_speed pll0_960_pll1_196_pll2_800_pll4_0[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 19200, 0, 0, 24576 },
	{ 1,  98304, ACPU_PLL_1, 1, 1,  98304, 0, 3,  49152 },
	{ 0, 120000, ACPU_PLL_0, 4, 7,  60000, 1, 3,  49152 },
	{ 1, 196608, ACPU_PLL_1, 1, 0,  65536, 2, 4,  98304 },
	{ 0, 200000, ACPU_PLL_2, 2, 3,  66667, 2, 4,  98304 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 160000, 1, 5, 160000 },
	{ 0, 400000, ACPU_PLL_2, 2, 1, 133333, 2, 5, 160000 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 160000, 2, 6, 160000 },
	{ 1, 800000, ACPU_PLL_2, 2, 0, 200000, 3, 7, 200000 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7627a PLL2 @ 1200MHz with GSM capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_245_pll2_1200_pll4_800[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 30720 },
	{ 0, 61440, ACPU_PLL_1, 1, 3,  7680, 3, 1,  61440 },
	{ 1, 122880, ACPU_PLL_1, 1, 1,  15360, 3, 2,  61440 },
	{ 1, 245760, ACPU_PLL_1, 1, 0, 30720, 3, 3,  61440 },
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 122880 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 0, 400000, ACPU_PLL_4, 6, 1, 50000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 160000 },
	{ 1, 800000, ACPU_PLL_4, 6, 0, 100000, 3, 7, 200000 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7627a PLL2 @ 1200MHz with CDMA capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_196_pll2_1200_pll4_800[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 24576 },
	{ 0, 65536, ACPU_PLL_1, 1, 3,  8192, 3, 1,  49152 },
	{ 1, 98304, ACPU_PLL_1, 1, 1,  12288, 3, 2,  49152 },
	{ 1, 196608, ACPU_PLL_1, 1, 0, 24576, 3, 3,  98304 },
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 120000 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 120000 },
	{ 0, 400000, ACPU_PLL_4, 6, 1, 50000, 3, 4, 120000 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 120000 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 160000 },
	{ 1, 800000, ACPU_PLL_4, 6, 0, 100000, 3, 7, 200000 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7627aa PLL4 @ 1008MHz with GSM capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_245_pll2_1200_pll4_1008[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 30720 },
	{ 0, 61440, ACPU_PLL_1, 1, 3,  7680, 3, 1, 61440 },
	{ 1, 122880, ACPU_PLL_1, 1, 1,  15360, 3, 2, 61440 },
	{ 1, 245760, ACPU_PLL_1, 1, 0, 30720, 3, 3, 61440 },
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 122880 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 0, 504000, ACPU_PLL_4, 6, 1, 63000, 3, 6, 160000 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 160000 },
	{ 1, 1008000, ACPU_PLL_4, 6, 0, 126000, 3, 7, 200000},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7627aa PLL4 @ 1008MHz with CDMA capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_196_pll2_1200_pll4_1008[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 24576 },
	{ 0, 65536, ACPU_PLL_1, 1, 3,  8192, 3, 1, 49152 },
	{ 1, 98304, ACPU_PLL_1, 1, 1,  12288, 3, 2, 49152 },
	{ 1, 196608, ACPU_PLL_1, 1, 0, 24576, 3, 3, 98304 },
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 122880 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 0, 504000, ACPU_PLL_4, 6, 1, 63000, 3, 6, 160000 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 160000 },
	{ 1, 1008000, ACPU_PLL_4, 6, 0, 126000, 3, 7, 200000},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 8625 PLL4 @ 1209MHz with GSM capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_245_pll2_1200_pll4_1209[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 30720 },
	{ 0, 61440, ACPU_PLL_1, 1, 3,  7680, 3, 1, 61440 },
	{ 1, 122880, ACPU_PLL_1, 1, 1,  15360, 3, 2, 61440 },
	{ 1, 245760, ACPU_PLL_1, 1, 0, 30720, 3, 3, 61440 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 160000 },
	{ 0, 604800, ACPU_PLL_4, 6, 1, 75600, 3, 6, 160000 },
	{ 1, 1209600, ACPU_PLL_4, 6, 0, 151200, 3, 7, 200000},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 8625 PLL4 @ 1209MHz with CDMA capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_196_pll2_1200_pll4_1209[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 24576 },
	{ 0, 65536, ACPU_PLL_1, 1, 3,  8192, 3, 1, 49152 },
	{ 1, 98304, ACPU_PLL_1, 1, 1,  12288, 3, 2, 49152 },
	{ 1, 196608, ACPU_PLL_1, 1, 0, 24576, 3, 3, 98304 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 160000 },
	{ 0, 604800, ACPU_PLL_4, 6, 1, 75600, 3, 6, 160000 },
	{ 1, 1209600, ACPU_PLL_4, 6, 0, 151200, 3, 7, 200000},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 8625 PLL4 @ 1152MHz with GSM capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_245_pll2_1200_pll4_1152[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 30720 },
	{ 0, 61440, ACPU_PLL_1, 1, 3,  7680, 3, 1, 61440 },
	{ 1, 122880, ACPU_PLL_1, 1, 1,  15360, 3, 2, 61440 },
	{ 1, 245760, ACPU_PLL_1, 1, 0, 30720, 3, 3, 61440 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 0, 576000, ACPU_PLL_4, 6, 1, 72000, 3, 6, 160000 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 160000 },
	{ 1, 1152000, ACPU_PLL_4, 6, 0, 144000, 3, 7, 200000},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 8625 PLL4 @ 1115MHz with CDMA capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_196_pll2_1200_pll4_1152[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 24576 },
	{ 0, 65536, ACPU_PLL_1, 1, 3,  8192, 3, 1, 49152 },
	{ 1, 98304, ACPU_PLL_1, 1, 1,  12288, 3, 2, 49152 },
	{ 1, 196608, ACPU_PLL_1, 1, 0, 24576, 3, 3, 98304 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 0, 576000, ACPU_PLL_4, 6, 1, 72000, 3, 6, 160000 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 160000 },
	{ 1, 1152000, ACPU_PLL_4, 6, 0, 144000, 3, 7, 200000},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};


/* 7625a PLL2 @ 1200MHz with GSM capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_245_pll2_1200_25a[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 30720 },
	{ 0, 61440, ACPU_PLL_1, 1, 3,  7680, 3, 1,  61440 },
	{ 1, 122880, ACPU_PLL_1, 1, 1,  15360, 3, 2,  61440 },
	{ 1, 245760, ACPU_PLL_1, 1, 0, 30720, 3, 3,  61440 },
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 122880 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 0, 400000, ACPU_PLL_2, 2, 2, 50000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 200000 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7627a PLL2 @ 1200MHz with GSM capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_737_pll2_1200_pll4_800[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 30720 },
	{ 0, 61440, ACPU_PLL_1, 1, 11,  7680, 3, 1,  61440 },
	{ 1, 122880, ACPU_PLL_1, 1, 5,  15360, 3, 2,  61440 },
	{ 1, 245760, ACPU_PLL_1, 1, 2, 30720, 3, 3,  61440 },
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 122880 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 0, 400000, ACPU_PLL_4, 6, 1, 50000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 160000 },
	{ 1, 800000, ACPU_PLL_4, 6, 0, 100000, 3, 7, 200000 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7627a PLL2 @ 1200MHz with CDMA capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_589_pll2_1200_pll4_800[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 24576 },
	{ 0, 65536, ACPU_PLL_1, 1, 8,  8192, 3, 1,  49152 },
	{ 1, 98304, ACPU_PLL_1, 1, 5,  12288, 3, 2,  49152 },
	{ 1, 196608, ACPU_PLL_1, 1, 2, 24576, 3, 3,  98304 },
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 120000 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 120000 },
	{ 0, 400000, ACPU_PLL_4, 6, 1, 50000, 3, 4, 120000 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 120000 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 160000 },
	{ 1, 800000, ACPU_PLL_4, 6, 0, 100000, 3, 7, 200000 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7627aa PLL4 @ 1008MHz with GSM capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_737_pll2_1200_pll4_1008[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 30720 },
	{ 0, 61440, ACPU_PLL_1, 1, 11,  7680, 3, 1, 61440 },
	{ 1, 122880, ACPU_PLL_1, 1, 5,  15360, 3, 2, 61440 },
	{ 1, 245760, ACPU_PLL_1, 1, 2, 30720, 3, 3, 61440 },
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 122880 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 0, 504000, ACPU_PLL_4, 6, 1, 63000, 3, 6, 160000 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 160000 },
	{ 1, 1008000, ACPU_PLL_4, 6, 0, 126000, 3, 7, 200000},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7627aa PLL4 @ 1008MHz with CDMA capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_589_pll2_1200_pll4_1008[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 24576 },
	{ 0, 65536, ACPU_PLL_1, 1, 8,  8192, 3, 1, 49152 },
	{ 1, 98304, ACPU_PLL_1, 1, 5,  12288, 3, 2, 49152 },
	{ 1, 196608, ACPU_PLL_1, 1, 2, 24576, 3, 3, 98304 },
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 122880 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 0, 504000, ACPU_PLL_4, 6, 1, 63000, 3, 6, 160000 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 160000 },
	{ 1, 1008000, ACPU_PLL_4, 6, 0, 126000, 3, 7, 200000},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7625a PLL2 @ 1200MHz with GSM capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_737_pll2_1200_25a[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 30720 },
	{ 0, 61440, ACPU_PLL_1, 1, 11,  7680, 3, 1,  61440 },
	{ 1, 122880, ACPU_PLL_1, 1, 5,  15360, 3, 2,  61440 },
	{ 1, 245760, ACPU_PLL_1, 1, 2, 30720, 3, 3,  61440 },
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 122880 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 0, 400000, ACPU_PLL_2, 2, 2, 50000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 200000 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

#define PLL_CONFIG(m0, m1, m2, m4) { \
	m0, m1, m2, m4, \
	pll0_##m0##_pll1_##m1##_pll2_##m2##_pll4_##m4 \
}

struct pll_freq_tbl_map {
	unsigned int	pll0_rate;
	unsigned int	pll1_rate;
	unsigned int	pll2_rate;
	unsigned int	pll4_rate;
	struct clkctl_acpu_speed *tbl;
};

static struct pll_freq_tbl_map acpu_freq_tbl_list[] = {
	PLL_CONFIG(960, 196, 1200, 0),
	PLL_CONFIG(960, 245, 1200, 0),
	PLL_CONFIG(960, 196, 800, 0),
	PLL_CONFIG(960, 245, 800, 0),
	PLL_CONFIG(960, 245, 1200, 800),
	PLL_CONFIG(960, 196, 1200, 800),
	PLL_CONFIG(960, 245, 1200, 1008),
	PLL_CONFIG(960, 196, 1200, 1008),
	PLL_CONFIG(960, 737, 1200, 800),
	PLL_CONFIG(960, 589, 1200, 800),
	PLL_CONFIG(960, 737, 1200, 1008),
	PLL_CONFIG(960, 589, 1200, 1008),
	PLL_CONFIG(960, 245, 1200, 1209),
	PLL_CONFIG(960, 196, 1200, 1209),
	PLL_CONFIG(960, 245, 1200, 1152),
	PLL_CONFIG(960, 196, 1200, 1152),
	{ 0, 0, 0, 0, 0 }
};

#ifdef CONFIG_CPU_FREQ_MSM
static struct cpufreq_frequency_table freq_table[NR_CPUS][20];

static void __devinit cpufreq_table_init(void)
{
	int cpu;
	for_each_possible_cpu(cpu) {
		unsigned int i, freq_cnt = 0;

		/* Construct the freq_table table from acpu_freq_tbl since
		 * the freq_table values need to match frequencies specified
		 * in acpu_freq_tbl and acpu_freq_tbl needs to be fixed up
		 * during init.
		 */
		for (i = 0; acpu_freq_tbl[i].a11clk_khz != 0
				&& freq_cnt < ARRAY_SIZE(*freq_table)-1; i++) {
			if (acpu_freq_tbl[i].use_for_scaling) {
				freq_table[cpu][freq_cnt].index = freq_cnt;
				freq_table[cpu][freq_cnt].frequency
					= acpu_freq_tbl[i].a11clk_khz;
				freq_cnt++;
			}
		}

		/* freq_table not big enough to store all usable freqs. */
		BUG_ON(acpu_freq_tbl[i].a11clk_khz != 0);

		freq_table[cpu][freq_cnt].index = freq_cnt;
		freq_table[cpu][freq_cnt].frequency = CPUFREQ_TABLE_END;
		/* Register table with CPUFreq. */
		cpufreq_frequency_table_get_attr(freq_table[cpu], cpu);
		pr_info("CPU%d: %d scaling frequencies supported.\n",
			cpu, freq_cnt);
	}
}
#endif

static int acpuclk_set_vdd_level(int vdd)
{
	uint32_t current_vdd;

	/*
	* NOTE: v1.0 of 7x27a/7x25a chip doesn't have working
	* VDD switching support.
	*/
	if ((cpu_is_msm7x27a() || cpu_is_msm7x25a()) &&
		(SOCINFO_VERSION_MINOR(socinfo_get_version()) < 1))
		return 0;

	current_vdd = readl_relaxed(A11S_VDD_SVS_PLEVEL_ADDR) & 0x07;

	pr_debug("Switching VDD from %u mV -> %d mV\n",
	       current_vdd, vdd);

	writel_relaxed((1 << 7) | (vdd << 3), A11S_VDD_SVS_PLEVEL_ADDR);
	mb();
	udelay(62);
	if ((readl_relaxed(A11S_VDD_SVS_PLEVEL_ADDR) & 0x7) != vdd) {
		pr_err("VDD set failed\n");
		return -EIO;
	}

	pr_debug("VDD switched\n");

	return 0;
}

/* Set proper dividers for the given clock speed. */
static void acpuclk_set_div(const struct clkctl_acpu_speed *hunt_s)
{
	uint32_t reg_clkctl, reg_clksel, clk_div, src_sel;

	reg_clksel = readl_relaxed(A11S_CLK_SEL_ADDR);

	/* AHB_CLK_DIV */
	clk_div = (reg_clksel >> 1) & 0x03;
	/* CLK_SEL_SRC1NO */
	src_sel = reg_clksel & 1;

	/*
	 * If the new clock divider is higher than the previous, then
	 * program the divider before switching the clock
	 */
	if (hunt_s->ahbclk_div > clk_div) {
		reg_clksel &= ~(0x3 << 1);
		reg_clksel |= (hunt_s->ahbclk_div << 1);
		writel_relaxed(reg_clksel, A11S_CLK_SEL_ADDR);
	}

	/* Program clock source and divider */
	reg_clkctl = readl_relaxed(A11S_CLK_CNTL_ADDR);
	reg_clkctl &= ~(0xFF << (8 * src_sel));
	reg_clkctl |= hunt_s->a11clk_src_sel << (4 + 8 * src_sel);
	reg_clkctl |= hunt_s->a11clk_src_div << (0 + 8 * src_sel);
	writel_relaxed(reg_clkctl, A11S_CLK_CNTL_ADDR);

	/* Program clock source selection */
	reg_clksel ^= 1;
	writel_relaxed(reg_clksel, A11S_CLK_SEL_ADDR);

	/* Wait for the clock switch to complete */
	mb();
	udelay(50);

	/*
	 * If the new clock divider is lower than the previous, then
	 * program the divider after switching the clock
	 */
	if (hunt_s->ahbclk_div < clk_div) {
		reg_clksel &= ~(0x3 << 1);
		reg_clksel |= (hunt_s->ahbclk_div << 1);
		writel_relaxed(reg_clksel, A11S_CLK_SEL_ADDR);
	}
}

static int acpuclk_7627_set_rate(int cpu, unsigned long rate,
				 enum setrate_reason reason)
{
	uint32_t reg_clkctl;
	struct clkctl_acpu_speed *cur_s, *tgt_s, *strt_s;
	int res, rc = 0;
	unsigned int plls_enabled = 0, pll;

	if (reason == SETRATE_CPUFREQ)
		mutex_lock(&drv_state.lock);

	strt_s = cur_s = drv_state.current_speed;

	WARN_ONCE(cur_s == NULL, "%s: not initialized\n", __func__);
	if (cur_s == NULL) {
		rc = -ENOENT;
		goto out;
	}

	if (rate == cur_s->a11clk_khz)
		goto out;

	for (tgt_s = acpu_freq_tbl; tgt_s->a11clk_khz != 0; tgt_s++) {
		if (tgt_s->a11clk_khz == rate)
			break;
	}

	if (tgt_s->a11clk_khz == 0) {
		rc = -EINVAL;
		goto out;
	}

	/* Choose the highest speed at or below 'rate' with same PLL. */
	if (reason != SETRATE_CPUFREQ
	    && tgt_s->a11clk_khz < cur_s->a11clk_khz) {
		while (tgt_s->pll != ACPU_PLL_TCXO && tgt_s->pll != cur_s->pll)
			tgt_s--;
	}

	if (strt_s->pll != ACPU_PLL_TCXO)
		plls_enabled |= 1 << strt_s->pll;

	if (reason == SETRATE_CPUFREQ) {
		if (strt_s->pll != tgt_s->pll && tgt_s->pll != ACPU_PLL_TCXO) {
			rc = clk_enable(pll_clk[tgt_s->pll].clk);
			if (rc < 0) {
				pr_err("PLL%d enable failed (%d)\n",
					tgt_s->pll, rc);
				goto out;
			}
			plls_enabled |= 1 << tgt_s->pll;
		}
	}
	/* Need to do this when coming out of power collapse since some modem
	 * firmwares reset the VDD when the application processor enters power
	 * collapse. */
	if (reason == SETRATE_CPUFREQ || reason == SETRATE_PC) {
		/* Increase VDD if needed. */
		if (tgt_s->vdd > cur_s->vdd) {
			rc = acpuclk_set_vdd_level(tgt_s->vdd);
			if (rc < 0) {
				pr_err("Unable to switch ACPU vdd (%d)\n", rc);
				goto out;
			}
		}
	}

	/* Set wait states for CPU inbetween frequency changes */
	reg_clkctl = readl_relaxed(A11S_CLK_CNTL_ADDR);
	reg_clkctl |= (100 << 16); /* set WT_ST_CNT */
	writel_relaxed(reg_clkctl, A11S_CLK_CNTL_ADDR);

	pr_debug("Switching from ACPU rate %u KHz -> %u KHz\n",
		       strt_s->a11clk_khz, tgt_s->a11clk_khz);

	while (cur_s != tgt_s) {
		/*
		 * Always jump to target freq if within max_speed_delta_khz,
		 * regardless of PLL. If differnece is greater, use the
		 * predefined steppings in the table.
		 */
		int d = abs((int)(cur_s->a11clk_khz - tgt_s->a11clk_khz));
		if (d > drv_state.max_speed_delta_khz) {

			if (tgt_s->a11clk_khz > cur_s->a11clk_khz) {
				/* Step up: jump to target PLL as early as
				 * possible so indexing using TCXO (up[-1])
				 * never occurs. */
				if (likely(cur_s->up[tgt_s->pll]))
					cur_s = cur_s->up[tgt_s->pll];
				else
					cur_s = cur_s->up[cur_s->pll];
			} else {
				/* Step down: stay on current PLL as long as
				 * possible so indexing using TCXO (down[-1])
				 * never occurs. */
				if (likely(cur_s->down[cur_s->pll]))
					cur_s = cur_s->down[cur_s->pll];
				else
					cur_s = cur_s->down[tgt_s->pll];
			}

			if (cur_s == NULL) { /* This should not happen. */
				pr_err("No stepping frequencies found. "
					"strt_s:%u tgt_s:%u\n",
					strt_s->a11clk_khz, tgt_s->a11clk_khz);
				rc = -EINVAL;
				goto out;
			}

		} else {
			cur_s = tgt_s;
		}

		pr_debug("STEP khz = %u, pll = %d\n",
				cur_s->a11clk_khz, cur_s->pll);

		if (cur_s->pll != ACPU_PLL_TCXO
		    && !(plls_enabled & (1 << cur_s->pll))) {
			rc = clk_enable(pll_clk[cur_s->pll].clk);
			if (rc < 0) {
				pr_err("PLL%d enable failed (%d)\n",
					cur_s->pll, rc);
				goto out;
			}
			plls_enabled |= 1 << cur_s->pll;
		}

		acpuclk_set_div(cur_s);
		drv_state.current_speed = cur_s;
		/* Re-adjust lpj for the new clock speed. */
#ifdef CONFIG_SMP
		for_each_possible_cpu(cpu) {
			per_cpu(cpu_data, cpu).loops_per_jiffy =
							cur_s->lpj;
		}
#endif
		/* Adjust the global one */
		loops_per_jiffy = cur_s->lpj;

	}

	/* Nothing else to do for SWFI. */
	if (reason == SETRATE_SWFI)
		goto out;

	/* Change the AXI bus frequency if we can. */
	if (strt_s->axiclk_khz != tgt_s->axiclk_khz) {
		res = clk_set_rate(drv_state.ebi1_clk,
				tgt_s->axiclk_khz * 1000);
		if (res < 0)
			pr_warning("Setting AXI min rate failed (%d)\n", res);
	}

	/* Disable PLLs we are not using anymore. */
	if (tgt_s->pll != ACPU_PLL_TCXO)
		plls_enabled &= ~(1 << tgt_s->pll);
	for (pll = ACPU_PLL_0; pll < ACPU_PLL_END; pll++)
		if (plls_enabled & (1 << pll))
			clk_disable(pll_clk[pll].clk);

	/* Nothing else to do for power collapse. */
	if (reason == SETRATE_PC)
		goto out;

	/* Drop VDD level if we can. */
	if (tgt_s->vdd < strt_s->vdd) {
		res = acpuclk_set_vdd_level(tgt_s->vdd);
		if (res < 0)
			pr_warning("Unable to drop ACPU vdd (%d)\n", res);
	}

	pr_debug("ACPU speed change complete\n");
out:
	if (reason == SETRATE_CPUFREQ)
		mutex_unlock(&drv_state.lock);
	return rc;
}

static void __devinit acpuclk_hw_init(void)
{
	struct clkctl_acpu_speed *speed;
	uint32_t div, sel, reg_clksel;
	int res;

	/*
	 * Prepare all the PLLs because we enable/disable them
	 * from atomic context and can't always ensure they're
	 * all prepared in non-atomic context. Same goes for
	 * ebi1_acpu_clk.
	 */
	BUG_ON(clk_prepare(pll_clk[ACPU_PLL_0].clk));
	BUG_ON(clk_prepare(pll_clk[ACPU_PLL_1].clk));
	BUG_ON(clk_prepare(pll_clk[ACPU_PLL_2].clk));
	BUG_ON(clk_prepare(pll_clk[ACPU_PLL_4].clk));
	BUG_ON(clk_prepare(drv_state.ebi1_clk));

	/*
	 * Determine the rate of ACPU clock
	 */

	if (!(readl_relaxed(A11S_CLK_SEL_ADDR) & 0x01)) { /* CLK_SEL_SRC1N0 */
		/* CLK_SRC0_SEL */
		sel = (readl_relaxed(A11S_CLK_CNTL_ADDR) >> 12) & 0x7;
		/* CLK_SRC0_DIV */
		div = (readl_relaxed(A11S_CLK_CNTL_ADDR) >> 8) & 0x0f;
	} else {
		/* CLK_SRC1_SEL */
		sel = (readl_relaxed(A11S_CLK_CNTL_ADDR) >> 4) & 0x07;
		/* CLK_SRC1_DIV */
		div = readl_relaxed(A11S_CLK_CNTL_ADDR) & 0x0f;
	}

	for (speed = acpu_freq_tbl; speed->a11clk_khz != 0; speed++) {
		if (speed->a11clk_src_sel == sel
		 && (speed->a11clk_src_div == div))
			break;
	}
	if (speed->a11clk_khz == 0) {
		pr_err("Error - ACPU clock reports invalid speed\n");
		return;
	}

	drv_state.current_speed = speed;
	if (speed->pll != ACPU_PLL_TCXO) {
		if (clk_enable(pll_clk[speed->pll].clk))
			pr_warning("Failed to vote for boot PLL\n");
	}

	/* Fix div2 to 2 for 7x27/5a(aa) targets */
	if (!cpu_is_msm7x27()) {
		reg_clksel = readl_relaxed(A11S_CLK_SEL_ADDR);
		reg_clksel &= ~(0x3 << 14);
		reg_clksel |= (0x1 << 14);
		writel_relaxed(reg_clksel, A11S_CLK_SEL_ADDR);
	}

	res = clk_set_rate(drv_state.ebi1_clk, speed->axiclk_khz * 1000);
	if (res < 0)
		pr_warning("Setting AXI min rate failed (%d)\n", res);
	res = clk_enable(drv_state.ebi1_clk);
	if (res < 0)
		pr_warning("Enabling AXI clock failed (%d)\n", res);

	pr_info("ACPU running at %d KHz\n", speed->a11clk_khz);
}

static unsigned long acpuclk_7627_get_rate(int cpu)
{
	WARN_ONCE(drv_state.current_speed == NULL,
		  "%s: not initialized\n", __func__);
	if (drv_state.current_speed)
		return drv_state.current_speed->a11clk_khz;
	else
		return 0;
}

/*----------------------------------------------------------------------------
 * Clock driver initialization
 *---------------------------------------------------------------------------*/
#define MHZ 1000000
static void __devinit select_freq_plan(void)
{
	unsigned long pll_mhz[ACPU_PLL_END];
	struct pll_freq_tbl_map *t;
	int i;

	/* Get PLL clocks */
	for (i = 0; i < ACPU_PLL_END; i++) {
		if (pll_clk[i].name) {
			pll_clk[i].clk = clk_get_sys("acpu", pll_clk[i].name);
			if (IS_ERR(pll_clk[i].clk)) {
				pll_mhz[i] = 0;
				continue;
			}
			/* Get PLL's Rate */
			pll_mhz[i] = clk_get_rate(pll_clk[i].clk)/MHZ;
		}
	}

	/*
	 * For the pll configuration used in acpuclock table e.g.
	 * pll0_960_pll1_245_pll2_1200" is same for 7627 and
	 * 7625a (as pll0,pll1,pll2) having same rates, but frequency
	 * table is different for both targets.
	 *
	 * Hence below for loop will not be able to select correct
	 * table based on PLL rates as rates are same. Hence we need
	 * to add this cpu check for selecting the correct acpuclock table.
	 */
	if (cpu_is_msm7x25a()) {
		if (pll_mhz[ACPU_PLL_1] == 245) {
			acpu_freq_tbl =
				pll0_960_pll1_245_pll2_1200_25a;
		} else if (pll_mhz[ACPU_PLL_1] == 737) {
			acpu_freq_tbl =
				pll0_960_pll1_737_pll2_1200_25a;
		}
	} else {
		/* Select the right table to use. */
		for (t = acpu_freq_tbl_list; t->tbl != 0; t++) {
			if (t->pll0_rate == pll_mhz[ACPU_PLL_0]
				&& t->pll1_rate == pll_mhz[ACPU_PLL_1]
				&& t->pll2_rate == pll_mhz[ACPU_PLL_2]
				&& t->pll4_rate == pll_mhz[ACPU_PLL_4]) {
				acpu_freq_tbl = t->tbl;
				break;
			}
		}
	}

	if (acpu_freq_tbl == NULL) {
		pr_crit("Unknown PLL configuration!\n");
		BUG();
	}
}

/*
 * Hardware requires the CPU to be dropped to less than MAX_WAIT_FOR_IRQ_KHZ
 * before entering a wait for irq low-power mode. Find a suitable rate.
 */
static unsigned long __devinit find_wait_for_irq_khz(void)
{
	unsigned long found_khz = 0;
	int i;

	for (i = 0; acpu_freq_tbl[i].a11clk_khz &&
		    acpu_freq_tbl[i].a11clk_khz <= MAX_WAIT_FOR_IRQ_KHZ; i++)
		found_khz = acpu_freq_tbl[i].a11clk_khz;

	return found_khz;
}

static void __devinit lpj_init(void)
{
	int i = 0, cpu;
	const struct clkctl_acpu_speed *base_clk = drv_state.current_speed;
	unsigned long loops;

	for_each_possible_cpu(cpu) {
#ifdef CONFIG_SMP
		loops = per_cpu(cpu_data, cpu).loops_per_jiffy;
#else
		loops = loops_per_jiffy;
#endif
		for (i = 0; acpu_freq_tbl[i].a11clk_khz; i++) {
			acpu_freq_tbl[i].lpj = cpufreq_scale(
				loops,
				base_clk->a11clk_khz,
				acpu_freq_tbl[i].a11clk_khz);
		}
	}
}

static void __devinit precompute_stepping(void)
{
	int i, step_idx;

#define cur_freq acpu_freq_tbl[i].a11clk_khz
#define step_freq acpu_freq_tbl[step_idx].a11clk_khz
#define cur_pll acpu_freq_tbl[i].pll
#define step_pll acpu_freq_tbl[step_idx].pll

	for (i = 0; acpu_freq_tbl[i].a11clk_khz; i++) {

		/* Calculate max "up" step for each destination PLL */
		step_idx = i + 1;
		while (step_freq && (step_freq - cur_freq)
					<= drv_state.max_speed_delta_khz) {
			acpu_freq_tbl[i].up[step_pll] =
						&acpu_freq_tbl[step_idx];
			step_idx++;
		}
		if (step_idx == (i + 1) && step_freq) {
			pr_crit("Delta between freqs %u KHz and %u KHz is"
				" too high!\n", cur_freq, step_freq);
			BUG();
		}

		/* Calculate max "down" step for each destination PLL */
		step_idx = i - 1;
		while (step_idx >= 0 && (cur_freq - step_freq)
					<= drv_state.max_speed_delta_khz) {
			acpu_freq_tbl[i].down[step_pll] =
						&acpu_freq_tbl[step_idx];
			step_idx--;
		}
		if (step_idx == (i - 1) && i > 0) {
			pr_crit("Delta between freqs %u KHz and %u KHz is"
				" too high!\n", cur_freq, step_freq);
			BUG();
		}
	}
}

static void __devinit print_acpu_freq_tbl(void)
{
	struct clkctl_acpu_speed *t;
	short down_idx[ACPU_PLL_END];
	short up_idx[ACPU_PLL_END];
	int i, j;

#define FREQ_IDX(freq_ptr) (freq_ptr - acpu_freq_tbl)
	pr_info("Id CPU-KHz PLL DIV AHB-KHz ADIV AXI-KHz "
		"D0 D1 D2 D4 U0 U1 U2 U4\n");

	t = &acpu_freq_tbl[0];
	for (i = 0; t->a11clk_khz != 0; i++) {

		for (j = 0; j < ACPU_PLL_END; j++) {
			down_idx[j] = t->down[j] ? FREQ_IDX(t->down[j]) : -1;
			up_idx[j] = t->up[j] ? FREQ_IDX(t->up[j]) : -1;
		}

		pr_info("%2d %7d %3d %3d %7d %4d %7d "
			"%2d %2d %2d %2d %2d %2d %2d %2d\n",
			i, t->a11clk_khz, t->pll, t->a11clk_src_div + 1,
			t->ahbclk_khz, t->ahbclk_div + 1, t->axiclk_khz,
			down_idx[0], down_idx[1], down_idx[2], down_idx[4],
			up_idx[0], up_idx[1], up_idx[2], up_idx[4]);

		t++;
	}
}


static struct acpuclk_data acpuclk_7627_data = {
	.set_rate = acpuclk_7627_set_rate,
	.get_rate = acpuclk_7627_get_rate,
	.power_collapse_khz = POWER_COLLAPSE_KHZ,
	.switch_time_us = 50,
};

static int __devinit acpuclk_7627_probe(struct platform_device *pdev)
{
	const struct acpuclk_pdata *pdata = pdev->dev.platform_data;

	pr_info("%s()\n", __func__);

	drv_state.ebi1_clk = clk_get(NULL, "ebi1_acpu_clk");
	BUG_ON(IS_ERR(drv_state.ebi1_clk));

	mutex_init(&drv_state.lock);
	drv_state.max_speed_delta_khz = pdata->max_speed_delta_khz;
	select_freq_plan();
	acpuclk_7627_data.wait_for_irq_khz = find_wait_for_irq_khz();
	precompute_stepping();
	acpuclk_hw_init();
	lpj_init();
	print_acpu_freq_tbl();
	acpuclk_register(&acpuclk_7627_data);

#ifdef CONFIG_CPU_FREQ_MSM
	cpufreq_table_init();
#endif
	return 0;
}

static struct platform_driver acpuclk_7627_driver = {
	.probe = acpuclk_7627_probe,
	.driver = {
		.name = "acpuclk-7627",
		.owner = THIS_MODULE,
	},
};

static int __init acpuclk_7627_init(void)
{
	return platform_driver_register(&acpuclk_7627_driver);
}
postcore_initcall(acpuclk_7627_init);
