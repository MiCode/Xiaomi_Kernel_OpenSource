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
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/sort.h>
#include <linux/remote_spinlock.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <asm/mach-types.h>
#include <mach/socinfo.h>

#include "smd_private.h"
#include "acpuclock.h"

#define A11S_CLK_CNTL_ADDR (MSM_CSR_BASE + 0x100)
#define A11S_CLK_SEL_ADDR (MSM_CSR_BASE + 0x104)
#define A11S_VDD_SVS_PLEVEL_ADDR (MSM_CSR_BASE + 0x124)
#define PLLn_MODE(n)	(MSM_CLK_CTL_BASE + 0x300 + 28 * (n))
#define PLLn_L_VAL(n)	(MSM_CLK_CTL_BASE + 0x304 + 28 * (n))

#define PLL4_MODE	(MSM_CLK_CTL_BASE + 0x374)
#define PLL4_L_VAL	(MSM_CLK_CTL_BASE + 0x378)

#define POWER_COLLAPSE_KHZ 19200

/* Max CPU frequency allowed by hardware while in standby waiting for an irq. */
#define MAX_WAIT_FOR_IRQ_KHZ 128000

enum {
	ACPU_PLL_TCXO	= -1,
	ACPU_PLL_0	= 0,
	ACPU_PLL_1,
	ACPU_PLL_2,
	ACPU_PLL_3,
	ACPU_PLL_4,
	ACPU_PLL_END,
};

static const struct pll {
	void __iomem *mod_reg;
	const uint32_t l_val_mask;
} soc_pll[ACPU_PLL_END] = {
	[ACPU_PLL_0] = {PLLn_MODE(ACPU_PLL_0), 0x3f},
	[ACPU_PLL_1] = {PLLn_MODE(ACPU_PLL_1), 0x3f},
	[ACPU_PLL_2] = {PLLn_MODE(ACPU_PLL_2), 0x3f},
	[ACPU_PLL_3] = {PLLn_MODE(ACPU_PLL_3), 0x3f},
	[ACPU_PLL_4] = {PLL4_MODE, 0x3ff},
};

struct clock_state {
	struct clkctl_acpu_speed	*current_speed;
	struct mutex			lock;
	uint32_t			max_speed_delta_khz;
	struct clk			*ebi1_clk;
};

#define PLL_BASE	7

struct shared_pll_control {
	uint32_t	version;
	struct {
		/* Denotes if the PLL is ON. Technically, this can be read
		 * directly from the PLL registers, but this feild is here,
		 * so let's use it.
		 */
		uint32_t	on;
		/* One bit for each processor core. The application processor
		 * is allocated bit position 1. All other bits should be
		 * considered as votes from other processors.
		 */
		uint32_t	votes;
	} pll[PLL_BASE + ACPU_PLL_END];
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
	unsigned long	lpj; /* loops_per_jiffy */
	/* Pointers in acpu_freq_tbl[] for max up/down steppings. */
	struct clkctl_acpu_speed *down[ACPU_PLL_END];
	struct clkctl_acpu_speed *up[ACPU_PLL_END];
};

static remote_spinlock_t pll_lock;
static struct shared_pll_control *pll_control;
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
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 150000 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 0, 400000, ACPU_PLL_4, 6, 1, 50000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 200000 },
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
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 200000 },
	{ 1, 800000, ACPU_PLL_4, 6, 0, 100000, 3, 7, 200000 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7627aa PLL4 @ 1008MHz with GSM capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_245_pll2_1200_pll4_1008[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 30720 },
	{ 0, 61440, ACPU_PLL_1, 1, 3,  7680, 3, 1, 61440 },
	{ 1, 122880, ACPU_PLL_1, 1, 1,  15360, 3, 2, 61440 },
	{ 1, 245760, ACPU_PLL_1, 1, 0, 30720, 3, 3, 61440 },
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 150000 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 0, 504000, ACPU_PLL_4, 6, 1, 63000, 3, 6, 200000 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 200000 },
	{ 1, 1008000, ACPU_PLL_4, 6, 0, 126000, 3, 7, 200000},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7627aa PLL4 @ 1008MHz with CDMA capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_196_pll2_1200_pll4_1008[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 24576 },
	{ 0, 65536, ACPU_PLL_1, 1, 3,  8192, 3, 1, 49152 },
	{ 1, 98304, ACPU_PLL_1, 1, 1,  12288, 3, 2, 49152 },
	{ 1, 196608, ACPU_PLL_1, 1, 0, 24576, 3, 3, 98304 },
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 150000 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 0, 504000, ACPU_PLL_4, 6, 1, 63000, 3, 6, 200000 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 200000 },
	{ 1, 1008000, ACPU_PLL_4, 6, 0, 126000, 3, 7, 200000},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7625a PLL2 @ 1200MHz with GSM capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_245_pll2_1200_25a[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 30720 },
	{ 0, 61440, ACPU_PLL_1, 1, 3,  7680, 3, 1,  61440 },
	{ 1, 122880, ACPU_PLL_1, 1, 1,  15360, 3, 2,  61440 },
	{ 1, 245760, ACPU_PLL_1, 1, 0, 30720, 3, 3,  61440 },
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 150000 },
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
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 150000 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 0, 400000, ACPU_PLL_4, 6, 1, 50000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 200000 },
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
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 200000 },
	{ 1, 800000, ACPU_PLL_4, 6, 0, 100000, 3, 7, 200000 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7627aa PLL4 @ 1008MHz with GSM capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_737_pll2_1200_pll4_1008[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 30720 },
	{ 0, 61440, ACPU_PLL_1, 1, 11,  7680, 3, 1, 61440 },
	{ 1, 122880, ACPU_PLL_1, 1, 5,  15360, 3, 2, 61440 },
	{ 1, 245760, ACPU_PLL_1, 1, 2, 30720, 3, 3, 61440 },
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 150000 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 0, 504000, ACPU_PLL_4, 6, 1, 63000, 3, 6, 200000 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 200000 },
	{ 1, 1008000, ACPU_PLL_4, 6, 0, 126000, 3, 7, 200000},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7x27aa PLL4 @ 1008MHz with CDMA capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_589_pll2_1200_pll4_1008[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 24576 },
	{ 0, 65536, ACPU_PLL_1, 1, 8,  8192, 3, 1, 49152 },
	{ 1, 98304, ACPU_PLL_1, 1, 5,  12288, 3, 2, 49152 },
	{ 1, 196608, ACPU_PLL_1, 1, 2, 24576, 3, 3, 98304 },
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 150000 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 0, 504000, ACPU_PLL_4, 6, 1, 63000, 3, 6, 200000 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 200000 },
	{ 1, 1008000, ACPU_PLL_4, 6, 0, 126000, 3, 7, 200000},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

/* 7x25a PLL2 @ 1200MHz with GSM capable modem */
static struct clkctl_acpu_speed pll0_960_pll1_737_pll2_1200_25a[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 30720 },
	{ 0, 61440, ACPU_PLL_1, 1, 11,  7680, 3, 1,  61440 },
	{ 1, 122880, ACPU_PLL_1, 1, 5,  15360, 3, 2,  61440 },
	{ 1, 245760, ACPU_PLL_1, 1, 2, 30720, 3, 3,  61440 },
	{ 0, 300000, ACPU_PLL_2, 2, 3, 37500, 3, 4, 150000 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, 4, 122880 },
	{ 0, 400000, ACPU_PLL_2, 2, 2, 50000, 3, 4, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, 5, 122880 },
	{ 1, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 6, 200000 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

#define PLL_0_MHZ	0
#define PLL_196_MHZ	10
#define PLL_245_MHZ	12
#define PLL_589_MHZ	30
#define PLL_737_MHZ	38
#define PLL_800_MHZ	41
#define PLL_960_MHZ	50
#define PLL_1008_MHZ	52
#define PLL_1200_MHZ	62

#define PLL_CONFIG(m0, m1, m2, m4) { \
	PLL_##m0##_MHZ, PLL_##m1##_MHZ, PLL_##m2##_MHZ, PLL_##m4##_MHZ, \
	pll0_##m0##_pll1_##m1##_pll2_##m2##_pll4_##m4 \
}

struct pll_freq_tbl_map {
	unsigned int	pll0_l;
	unsigned int	pll1_l;
	unsigned int	pll2_l;
	unsigned int	pll4_l;
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
	{ 0, 0, 0, 0, 0 }
};

#ifdef CONFIG_CPU_FREQ_MSM
static struct cpufreq_frequency_table freq_table[20];

static void __init cpufreq_table_init(void)
{
	unsigned int i;
	unsigned int freq_cnt = 0;

	/* Construct the freq_table table from acpu_freq_tbl since the
	 * freq_table values need to match frequencies specified in
	 * acpu_freq_tbl and acpu_freq_tbl needs to be fixed up during init.
	 */
	for (i = 0; acpu_freq_tbl[i].a11clk_khz != 0
			&& freq_cnt < ARRAY_SIZE(freq_table)-1; i++) {
		if (acpu_freq_tbl[i].use_for_scaling) {
			freq_table[freq_cnt].index = freq_cnt;
			freq_table[freq_cnt].frequency
				= acpu_freq_tbl[i].a11clk_khz;
			freq_cnt++;
		}
	}

	/* freq_table not big enough to store all usable freqs. */
	BUG_ON(acpu_freq_tbl[i].a11clk_khz != 0);

	freq_table[freq_cnt].index = freq_cnt;
	freq_table[freq_cnt].frequency = CPUFREQ_TABLE_END;

	pr_info("%d scaling frequencies supported.\n", freq_cnt);
}
#endif

static void pll_enable(void __iomem *addr, unsigned on)
{
	if (on) {
		writel_relaxed(2, addr);
		mb();
		udelay(5);
		writel_relaxed(6, addr);
		mb();
		udelay(50);
		writel_relaxed(7, addr);
	} else {
		writel_relaxed(0, addr);
	}
}

static int pc_pll_request(unsigned id, unsigned on)
{
	int res = 0;
	on = !!on;

	if (on)
		pr_debug("Enabling PLL %d\n", id);
	else
		pr_debug("Disabling PLL %d\n", id);

	if (id >= ACPU_PLL_END)
		return -EINVAL;

	remote_spin_lock(&pll_lock);
	if (on) {
		pll_control->pll[PLL_BASE + id].votes |= 2;
		if (!pll_control->pll[PLL_BASE + id].on) {
			pll_enable(soc_pll[id].mod_reg, 1);
			pll_control->pll[PLL_BASE + id].on = 1;
		}
	} else {
		pll_control->pll[PLL_BASE + id].votes &= ~2;
		if (pll_control->pll[PLL_BASE + id].on
		    && !pll_control->pll[PLL_BASE + id].votes) {
			pll_enable(soc_pll[id].mod_reg, 0);
			pll_control->pll[PLL_BASE + id].on = 0;
		}
	}
	remote_spin_unlock(&pll_lock);

	if (on)
		pr_debug("PLL enabled\n");
	else
		pr_debug("PLL disabled\n");

	return res;
}

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
			rc = pc_pll_request(tgt_s->pll, 1);
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
			rc = pc_pll_request(cur_s->pll, 1);
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
		loops_per_jiffy = cur_s->lpj;
		mb();
		udelay(50);
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
		if (plls_enabled & (1 << pll)) {
			res = pc_pll_request(pll, 0);
			if (res < 0)
				pr_warning("PLL%d disable failed (%d)\n",
						pll, res);
		}

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

static void __init acpuclk_hw_init(void)
{
	struct clkctl_acpu_speed *speed;
	uint32_t div, sel, reg_clksel;
	int res;

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
	if (speed->pll != ACPU_PLL_TCXO)
		if (pc_pll_request(speed->pll, 1))
			pr_warning("Failed to vote for boot PLL\n");

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

static void __init acpu_freq_tbl_fixup(void)
{
	unsigned long pll0_l, pll1_l, pll2_l, pll4_l;
	struct pll_freq_tbl_map *lst;

	/* Wait for the PLLs to be initialized and then read their frequency.
	 */
	do {
		pll0_l = readl_relaxed(PLLn_L_VAL(0)) &
				soc_pll[ACPU_PLL_0].l_val_mask;
		cpu_relax();
		udelay(50);
	} while (pll0_l == 0);
	do {
		pll1_l = readl_relaxed(PLLn_L_VAL(1)) &
				soc_pll[ACPU_PLL_1].l_val_mask;
		cpu_relax();
		udelay(50);
	} while (pll1_l == 0);
	do {
		pll2_l = readl_relaxed(PLLn_L_VAL(2)) &
				soc_pll[ACPU_PLL_2].l_val_mask;
		cpu_relax();
		udelay(50);
	} while (pll2_l == 0);

	pr_info("L val: PLL0: %d, PLL1: %d, PLL2: %d\n",
			(int)pll0_l, (int)pll1_l, (int)pll2_l);

	if (!cpu_is_msm7x27() && !cpu_is_msm7x25a()) {
		do {
			pll4_l = readl_relaxed(PLL4_L_VAL) &
				soc_pll[ACPU_PLL_4].l_val_mask;
			cpu_relax();
			udelay(50);
		} while (pll4_l == 0);
		pr_info("L val: PLL4: %d\n", (int)pll4_l);
	} else {
		pll4_l = 0;
	}

	/* Fix the tables for 7x25a variant to not conflict with 7x27 ones */
	if (cpu_is_msm7x25a()) {
		if (pll1_l == PLL_245_MHZ) {
			acpu_freq_tbl =
				pll0_960_pll1_245_pll2_1200_25a;
		} else if (pll1_l == PLL_737_MHZ) {
			acpu_freq_tbl =
				pll0_960_pll1_737_pll2_1200_25a;
		}
	} else {
		/* Select the right table to use. */
		for (lst = acpu_freq_tbl_list; lst->tbl != 0; lst++) {
			if (lst->pll0_l == pll0_l && lst->pll1_l == pll1_l
					&& lst->pll2_l == pll2_l
					&& lst->pll4_l == pll4_l) {
				acpu_freq_tbl = lst->tbl;
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
static unsigned long __init find_wait_for_irq_khz(void)
{
	unsigned long found_khz = 0;
	int i;

	for (i = 0; acpu_freq_tbl[i].a11clk_khz &&
		    acpu_freq_tbl[i].a11clk_khz <= MAX_WAIT_FOR_IRQ_KHZ; i++)
		found_khz = acpu_freq_tbl[i].a11clk_khz;

	return found_khz;
}

/* Initalize the lpj field in the acpu_freq_tbl. */
static void __init lpj_init(void)
{
	int i;
	const struct clkctl_acpu_speed *base_clk = drv_state.current_speed;
	for (i = 0; acpu_freq_tbl[i].a11clk_khz; i++) {
		acpu_freq_tbl[i].lpj = cpufreq_scale(loops_per_jiffy,
						base_clk->a11clk_khz,
						acpu_freq_tbl[i].a11clk_khz);
	}
}

static void __init precompute_stepping(void)
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

static void __init print_acpu_freq_tbl(void)
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

static void shared_pll_control_init(void)
{
#define PLL_REMOTE_SPINLOCK_ID "S:7"
	unsigned smem_size;

	remote_spin_lock_init(&pll_lock, PLL_REMOTE_SPINLOCK_ID);
	pll_control = smem_get_entry(SMEM_CLKREGIM_SOURCES, &smem_size);

	if (!pll_control) {
		pr_err("Can't find shared PLL control data structure!\n");
		BUG();
	/* There might be more PLLs than what the application processor knows
	 * about. But the index used for each PLL is guaranteed to remain the
	 * same. */
	} else if (smem_size < sizeof(struct shared_pll_control)) {
			pr_err("Shared PLL control data"
					"structure too small!\n");
			BUG();
	} else if (pll_control->version != 0xCCEE0001) {
			pr_err("Shared PLL control version mismatch!\n");
			BUG();
	} else {
		pr_info("Shared PLL control available.\n");
		return;
	}

}

static struct acpuclk_data acpuclk_7627_data = {
	.set_rate = acpuclk_7627_set_rate,
	.get_rate = acpuclk_7627_get_rate,
	.power_collapse_khz = POWER_COLLAPSE_KHZ,
	.switch_time_us = 50,
};

static int __init acpuclk_7627_init(struct acpuclk_soc_data *soc_data)
{
	pr_info("%s()\n", __func__);

	drv_state.ebi1_clk = clk_get(NULL, "ebi1_acpu_clk");
	BUG_ON(IS_ERR(drv_state.ebi1_clk));

	mutex_init(&drv_state.lock);
	shared_pll_control_init();
	drv_state.max_speed_delta_khz = soc_data->max_speed_delta_khz;
	acpu_freq_tbl_fixup();
	acpuclk_7627_data.wait_for_irq_khz = find_wait_for_irq_khz();
	precompute_stepping();
	acpuclk_hw_init();
	lpj_init();
	print_acpu_freq_tbl();
	acpuclk_register(&acpuclk_7627_data);

#ifdef CONFIG_CPU_FREQ_MSM
	cpufreq_table_init();
	cpufreq_frequency_table_get_attr(freq_table, smp_processor_id());
#endif
	return 0;
}

struct acpuclk_soc_data acpuclk_7x27_soc_data __initdata = {
	.max_speed_delta_khz = 400000,
	.init = acpuclk_7627_init,
};

struct acpuclk_soc_data acpuclk_7x27a_soc_data __initdata = {
	.max_speed_delta_khz = 400000,
	.init = acpuclk_7627_init,
};

struct acpuclk_soc_data acpuclk_7x27aa_soc_data __initdata = {
	.max_speed_delta_khz = 504000,
	.init = acpuclk_7627_init,
};
