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
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/regulator/consumer.h>

#include <asm/cpu.h>

#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/socinfo.h>
#include <mach/rpm-regulator.h>

#include "acpuclock.h"
#include "avs.h"

/* Frequency switch modes. */
#define SHOT_SWITCH		4
#define HOP_SWITCH		5
#define SIMPLE_SLEW		6
#define COMPLEX_SLEW		7

/* PLL calibration limits.
 * The PLL hardware is capable of 384MHz to 1536MHz. The L_VALs
 * used for calibration should respect these limits. */
#define L_VAL_SCPLL_CAL_MIN	0x08 /* =  432 MHz with 27MHz source */
#define L_VAL_SCPLL_CAL_MAX	0x1C /* = 1512 MHz with 27MHz source */

#define MAX_VDD_SC		1250000 /* uV */
#define MAX_VDD_MEM		1250000 /* uV */
#define MAX_VDD_DIG		1200000 /* uV */
#define MAX_AXI			 310500 /* KHz */
#define SCPLL_LOW_VDD_FMAX	 594000 /* KHz */
#define SCPLL_LOW_VDD		1000000 /* uV */
#define SCPLL_NOMINAL_VDD	1100000 /* uV */

/* SCPLL Modes. */
#define SCPLL_POWER_DOWN	0
#define SCPLL_BYPASS		1
#define SCPLL_STANDBY		2
#define SCPLL_FULL_CAL		4
#define SCPLL_HALF_CAL		5
#define SCPLL_STEP_CAL		6
#define SCPLL_NORMAL		7

#define SCPLL_DEBUG_NONE	0
#define SCPLL_DEBUG_FULL	3

/* SCPLL registers offsets. */
#define SCPLL_DEBUG_OFFSET		0x0
#define SCPLL_CTL_OFFSET		0x4
#define SCPLL_CAL_OFFSET		0x8
#define SCPLL_STATUS_OFFSET		0x10
#define SCPLL_CFG_OFFSET		0x1C
#define SCPLL_FSM_CTL_EXT_OFFSET	0x24
#define SCPLL_LUT_A_HW_MAX		(0x38 + ((L_VAL_SCPLL_CAL_MAX / 4) * 4))

/* Clock registers. */
#define SPSS0_CLK_CTL_ADDR		(MSM_ACC0_BASE + 0x04)
#define SPSS0_CLK_SEL_ADDR		(MSM_ACC0_BASE + 0x08)
#define SPSS1_CLK_CTL_ADDR		(MSM_ACC1_BASE + 0x04)
#define SPSS1_CLK_SEL_ADDR		(MSM_ACC1_BASE + 0x08)
#define SPSS_L2_CLK_SEL_ADDR		(MSM_GCC_BASE  + 0x38)

/* PTE EFUSE register. */
#define QFPROM_PTE_EFUSE_ADDR		(MSM_QFPROM_BASE + 0x00C0)

static const void * const clk_ctl_addr[] = {SPSS0_CLK_CTL_ADDR,
			SPSS1_CLK_CTL_ADDR};
static const void * const clk_sel_addr[] = {SPSS0_CLK_SEL_ADDR,
			SPSS1_CLK_SEL_ADDR, SPSS_L2_CLK_SEL_ADDR};

static const int rpm_vreg_voter[] = { RPM_VREG_VOTER1, RPM_VREG_VOTER2 };
static struct regulator *regulator_sc[NR_CPUS];

enum scplls {
	CPU0 = 0,
	CPU1,
	L2,
};

static const void * const sc_pll_base[] = {
	[CPU0]	= MSM_SCPLL_BASE + 0x200,
	[CPU1]	= MSM_SCPLL_BASE + 0x300,
	[L2]	= MSM_SCPLL_BASE + 0x400,
};

enum sc_src {
	ACPU_AFAB,
	ACPU_PLL_8,
	ACPU_SCPLL,
};

static struct clock_state {
	struct clkctl_acpu_speed	*current_speed[NR_CPUS];
	struct clkctl_l2_speed		*current_l2_speed;
	spinlock_t			l2_lock;
	struct mutex			lock;
} drv_state;

struct clkctl_l2_speed {
	unsigned int     khz;
	unsigned int     src_sel;
	unsigned int     l_val;
	unsigned int     vdd_dig;
	unsigned int     vdd_mem;
	unsigned int     bw_level;
};

static struct clkctl_l2_speed *l2_vote[NR_CPUS];

struct clkctl_acpu_speed {
	unsigned int     use_for_scaling[2]; /* One for each CPU. */
	unsigned int     acpuclk_khz;
	int              pll;
	unsigned int     acpuclk_src_sel;
	unsigned int     acpuclk_src_div;
	unsigned int     core_src_sel;
	unsigned int     l_val;
	struct clkctl_l2_speed *l2_level;
	unsigned int     vdd_sc;
	unsigned int     avsdscr_setting;
};

/* Instantaneous bandwidth requests in MB/s. */
#define BW_MBPS(_bw) \
	{ \
		.vectors = &(struct msm_bus_vectors){ \
			.src = MSM_BUS_MASTER_AMPSS_M0, \
			.dst = MSM_BUS_SLAVE_EBI_CH0, \
			.ib = (_bw) * 1000000UL, \
			.ab = 0, \
		}, \
		.num_paths = 1, \
	}
static struct msm_bus_paths bw_level_tbl[] = {
	[0] =  BW_MBPS(824), /* At least 103 MHz on bus. */
	[1] = BW_MBPS(1336), /* At least 167 MHz on bus. */
	[2] = BW_MBPS(2008), /* At least 251 MHz on bus. */
	[3] = BW_MBPS(2480), /* At least 310 MHz on bus. */
};

static struct msm_bus_scale_pdata bus_client_pdata = {
	.usecase = bw_level_tbl,
	.num_usecases = ARRAY_SIZE(bw_level_tbl),
	.active_only = 1,
	.name = "acpuclock",
};

static uint32_t bus_perf_client;

/* L2 frequencies = 2 * 27 MHz * L_VAL */
static struct clkctl_l2_speed l2_freq_tbl_v2[] = {
	[0]  = { MAX_AXI, 0, 0,    1000000, 1100000, 0},
	[1]  = { 432000,  1, 0x08, 1000000, 1100000, 0},
	[2]  = { 486000,  1, 0x09, 1000000, 1100000, 0},
	[3]  = { 540000,  1, 0x0A, 1000000, 1100000, 0},
	[4]  = { 594000,  1, 0x0B, 1000000, 1100000, 0},
	[5]  = { 648000,  1, 0x0C, 1000000, 1100000, 1},
	[6]  = { 702000,  1, 0x0D, 1100000, 1100000, 1},
	[7]  = { 756000,  1, 0x0E, 1100000, 1100000, 1},
	[8]  = { 810000,  1, 0x0F, 1100000, 1100000, 1},
	[9]  = { 864000,  1, 0x10, 1100000, 1100000, 1},
	[10] = { 918000,  1, 0x11, 1100000, 1100000, 2},
	[11] = { 972000,  1, 0x12, 1100000, 1100000, 2},
	[12] = {1026000,  1, 0x13, 1100000, 1100000, 2},
	[13] = {1080000,  1, 0x14, 1100000, 1200000, 2},
	[14] = {1134000,  1, 0x15, 1100000, 1200000, 2},
	[15] = {1188000,  1, 0x16, 1200000, 1200000, 3},
	[16] = {1242000,  1, 0x17, 1200000, 1212500, 3},
	[17] = {1296000,  1, 0x18, 1200000, 1225000, 3},
	[18] = {1350000,  1, 0x19, 1200000, 1225000, 3},
	[19] = {1404000,  1, 0x1A, 1200000, 1250000, 3},
};

#define L2(x) (&l2_freq_tbl_v2[(x)])
/* SCPLL frequencies = 2 * 27 MHz * L_VAL */
static struct clkctl_acpu_speed acpu_freq_tbl_1188mhz[] = {
  { {1, 1},  192000,  ACPU_PLL_8, 3, 1, 0, 0,    L2(1),   812500, 0x03006000},
  /* MAX_AXI row is used to source CPU cores and L2 from the AFAB clock. */
  { {0, 0},  MAX_AXI, ACPU_AFAB,  1, 0, 0, 0,    L2(0),   875000, 0x03006000},
  { {1, 1},  384000,  ACPU_PLL_8, 3, 0, 0, 0,    L2(1),   875000, 0x03006000},
  { {1, 1},  432000,  ACPU_SCPLL, 0, 0, 1, 0x08, L2(1),   887500, 0x03006000},
  { {1, 1},  486000,  ACPU_SCPLL, 0, 0, 1, 0x09, L2(2),   912500, 0x03006000},
  { {1, 1},  540000,  ACPU_SCPLL, 0, 0, 1, 0x0A, L2(3),   925000, 0x03006000},
  { {1, 1},  594000,  ACPU_SCPLL, 0, 0, 1, 0x0B, L2(4),   937500, 0x03006000},
  { {1, 1},  648000,  ACPU_SCPLL, 0, 0, 1, 0x0C, L2(5),   950000, 0x03006000},
  { {1, 1},  702000,  ACPU_SCPLL, 0, 0, 1, 0x0D, L2(6),   975000, 0x03006000},
  { {1, 1},  756000,  ACPU_SCPLL, 0, 0, 1, 0x0E, L2(7),  1000000, 0x03006000},
  { {1, 1},  810000,  ACPU_SCPLL, 0, 0, 1, 0x0F, L2(8),  1012500, 0x03006000},
  { {1, 1},  864000,  ACPU_SCPLL, 0, 0, 1, 0x10, L2(9),  1037500, 0x03006000},
  { {1, 1},  918000,  ACPU_SCPLL, 0, 0, 1, 0x11, L2(10), 1062500, 0x03006000},
  { {1, 1},  972000,  ACPU_SCPLL, 0, 0, 1, 0x12, L2(11), 1087500, 0x03006000},
  { {1, 1}, 1026000,  ACPU_SCPLL, 0, 0, 1, 0x13, L2(12), 1125000, 0x03006000},
  { {1, 1}, 1080000,  ACPU_SCPLL, 0, 0, 1, 0x14, L2(13), 1137500, 0x03006000},
  { {1, 1}, 1134000,  ACPU_SCPLL, 0, 0, 1, 0x15, L2(14), 1162500, 0x03006000},
  { {1, 1}, 1188000,  ACPU_SCPLL, 0, 0, 1, 0x16, L2(15), 1187500, 0x03006000},
  { {0, 0}, 0 },
};

/* SCPLL frequencies = 2 * 27 MHz * L_VAL */
static struct clkctl_acpu_speed acpu_freq_tbl_slow[] = {
  { {1, 1},  192000,  ACPU_PLL_8, 3, 1, 0, 0,    L2(1),   800000, 0x03006000},
  /* MAX_AXI row is used to source CPU cores and L2 from the AFAB clock. */
  { {0, 0},  MAX_AXI, ACPU_AFAB,  1, 0, 0, 0,    L2(0),   825000, 0x03006000},
  { {1, 1},  384000,  ACPU_PLL_8, 3, 0, 0, 0,    L2(1),   825000, 0x03006000},
  { {1, 1},  432000,  ACPU_SCPLL, 0, 0, 1, 0x08, L2(1),   850000, 0x03006000},
  { {1, 1},  486000,  ACPU_SCPLL, 0, 0, 1, 0x09, L2(2),   850000, 0x03006000},
  { {1, 1},  540000,  ACPU_SCPLL, 0, 0, 1, 0x0A, L2(3),   875000, 0x03006000},
  { {1, 1},  594000,  ACPU_SCPLL, 0, 0, 1, 0x0B, L2(4),   875000, 0x03006000},
  { {1, 1},  648000,  ACPU_SCPLL, 0, 0, 1, 0x0C, L2(5),   900000, 0x03006000},
  { {1, 1},  702000,  ACPU_SCPLL, 0, 0, 1, 0x0D, L2(6),   900000, 0x03006000},
  { {1, 1},  756000,  ACPU_SCPLL, 0, 0, 1, 0x0E, L2(7),   925000, 0x03006000},
  { {1, 1},  810000,  ACPU_SCPLL, 0, 0, 1, 0x0F, L2(8),   975000, 0x03006000},
  { {1, 1},  864000,  ACPU_SCPLL, 0, 0, 1, 0x10, L2(9),   975000, 0x03006000},
  { {1, 1},  918000,  ACPU_SCPLL, 0, 0, 1, 0x11, L2(10), 1000000, 0x03006000},
  { {1, 1},  972000,  ACPU_SCPLL, 0, 0, 1, 0x12, L2(11), 1025000, 0x03006000},
  { {1, 1}, 1026000,  ACPU_SCPLL, 0, 0, 1, 0x13, L2(12), 1025000, 0x03006000},
  { {1, 1}, 1080000,  ACPU_SCPLL, 0, 0, 1, 0x14, L2(13), 1050000, 0x03006000},
  { {1, 1}, 1134000,  ACPU_SCPLL, 0, 0, 1, 0x15, L2(14), 1075000, 0x03006000},
  { {1, 1}, 1188000,  ACPU_SCPLL, 0, 0, 1, 0x16, L2(15), 1100000, 0x03006000},
  { {1, 1}, 1242000,  ACPU_SCPLL, 0, 0, 1, 0x17, L2(16), 1125000, 0x03006000},
  { {1, 1}, 1296000,  ACPU_SCPLL, 0, 0, 1, 0x18, L2(17), 1150000, 0x03006000},
  { {1, 1}, 1350000,  ACPU_SCPLL, 0, 0, 1, 0x19, L2(18), 1150000, 0x03006000},
  { {1, 1}, 1404000,  ACPU_SCPLL, 0, 0, 1, 0x1A, L2(19), 1175000, 0x03006000},
  { {1, 1}, 1458000,  ACPU_SCPLL, 0, 0, 1, 0x1B, L2(19), 1200000, 0x03006000},
  { {1, 1}, 1512000,  ACPU_SCPLL, 0, 0, 1, 0x1C, L2(19), 1225000, 0x03006000},
  { {0, 0}, 0 },
};

/* SCPLL frequencies = 2 * 27 MHz * L_VAL */
static struct clkctl_acpu_speed acpu_freq_tbl_nom[] = {
  { {1, 1},  192000,  ACPU_PLL_8, 3, 1, 0, 0,    L2(1),   800000, 0x03006000},
  /* MAX_AXI row is used to source CPU cores and L2 from the AFAB clock. */
  { {0, 0},  MAX_AXI, ACPU_AFAB,  1, 0, 0, 0,    L2(0),   825000, 0x03006000},
  { {1, 1},  384000,  ACPU_PLL_8, 3, 0, 0, 0,    L2(1),   825000, 0x03006000},
  { {1, 1},  432000,  ACPU_SCPLL, 0, 0, 1, 0x08, L2(1),   850000, 0x03006000},
  { {1, 1},  486000,  ACPU_SCPLL, 0, 0, 1, 0x09, L2(2),   850000, 0x03006000},
  { {1, 1},  540000,  ACPU_SCPLL, 0, 0, 1, 0x0A, L2(3),   875000, 0x03006000},
  { {1, 1},  594000,  ACPU_SCPLL, 0, 0, 1, 0x0B, L2(4),   875000, 0x03006000},
  { {1, 1},  648000,  ACPU_SCPLL, 0, 0, 1, 0x0C, L2(5),   900000, 0x03006000},
  { {1, 1},  702000,  ACPU_SCPLL, 0, 0, 1, 0x0D, L2(6),   900000, 0x03006000},
  { {1, 1},  756000,  ACPU_SCPLL, 0, 0, 1, 0x0E, L2(7),   925000, 0x03006000},
  { {1, 1},  810000,  ACPU_SCPLL, 0, 0, 1, 0x0F, L2(8),   950000, 0x03006000},
  { {1, 1},  864000,  ACPU_SCPLL, 0, 0, 1, 0x10, L2(9),   975000, 0x03006000},
  { {1, 1},  918000,  ACPU_SCPLL, 0, 0, 1, 0x11, L2(10),  975000, 0x03006000},
  { {1, 1},  972000,  ACPU_SCPLL, 0, 0, 1, 0x12, L2(11), 1000000, 0x03006000},
  { {1, 1}, 1026000,  ACPU_SCPLL, 0, 0, 1, 0x13, L2(12), 1000000, 0x03006000},
  { {1, 1}, 1080000,  ACPU_SCPLL, 0, 0, 1, 0x14, L2(13), 1025000, 0x03006000},
  { {1, 1}, 1134000,  ACPU_SCPLL, 0, 0, 1, 0x15, L2(14), 1025000, 0x03006000},
  { {1, 1}, 1188000,  ACPU_SCPLL, 0, 0, 1, 0x16, L2(15), 1050000, 0x03006000},
  { {1, 1}, 1242000,  ACPU_SCPLL, 0, 0, 1, 0x17, L2(16), 1075000, 0x03006000},
  { {1, 1}, 1296000,  ACPU_SCPLL, 0, 0, 1, 0x18, L2(17), 1100000, 0x03006000},
  { {1, 1}, 1350000,  ACPU_SCPLL, 0, 0, 1, 0x19, L2(18), 1125000, 0x03006000},
  { {1, 1}, 1404000,  ACPU_SCPLL, 0, 0, 1, 0x1A, L2(19), 1150000, 0x03006000},
  { {1, 1}, 1458000,  ACPU_SCPLL, 0, 0, 1, 0x1B, L2(19), 1150000, 0x03006000},
  { {1, 1}, 1512000,  ACPU_SCPLL, 0, 0, 1, 0x1C, L2(19), 1175000, 0x03006000},
  { {0, 0}, 0 },
};

/* SCPLL frequencies = 2 * 27 MHz * L_VAL */
static struct clkctl_acpu_speed acpu_freq_tbl_fast[] = {
  { {1, 1},  192000,  ACPU_PLL_8, 3, 1, 0, 0,    L2(1),   800000, 0x03006000},
  /* MAX_AXI row is used to source CPU cores and L2 from the AFAB clock. */
  { {0, 0},  MAX_AXI, ACPU_AFAB,  1, 0, 0, 0,    L2(0),   825000, 0x03006000},
  { {1, 1},  384000,  ACPU_PLL_8, 3, 0, 0, 0,    L2(1),   825000, 0x03006000},
  { {1, 1},  432000,  ACPU_SCPLL, 0, 0, 1, 0x08, L2(1),   850000, 0x03006000},
  { {1, 1},  486000,  ACPU_SCPLL, 0, 0, 1, 0x09, L2(2),   850000, 0x03006000},
  { {1, 1},  540000,  ACPU_SCPLL, 0, 0, 1, 0x0A, L2(3),   875000, 0x03006000},
  { {1, 1},  594000,  ACPU_SCPLL, 0, 0, 1, 0x0B, L2(4),   875000, 0x03006000},
  { {1, 1},  648000,  ACPU_SCPLL, 0, 0, 1, 0x0C, L2(5),   900000, 0x03006000},
  { {1, 1},  702000,  ACPU_SCPLL, 0, 0, 1, 0x0D, L2(6),   900000, 0x03006000},
  { {1, 1},  756000,  ACPU_SCPLL, 0, 0, 1, 0x0E, L2(7),   925000, 0x03006000},
  { {1, 1},  810000,  ACPU_SCPLL, 0, 0, 1, 0x0F, L2(8),   925000, 0x03006000},
  { {1, 1},  864000,  ACPU_SCPLL, 0, 0, 1, 0x10, L2(9),   950000, 0x03006000},
  { {1, 1},  918000,  ACPU_SCPLL, 0, 0, 1, 0x11, L2(10),  950000, 0x03006000},
  { {1, 1},  972000,  ACPU_SCPLL, 0, 0, 1, 0x12, L2(11),  950000, 0x03006000},
  { {1, 1}, 1026000,  ACPU_SCPLL, 0, 0, 1, 0x13, L2(12),  975000, 0x03006000},
  { {1, 1}, 1080000,  ACPU_SCPLL, 0, 0, 1, 0x14, L2(13), 1000000, 0x03006000},
  { {1, 1}, 1134000,  ACPU_SCPLL, 0, 0, 1, 0x15, L2(14), 1000000, 0x03006000},
  { {1, 1}, 1188000,  ACPU_SCPLL, 0, 0, 1, 0x16, L2(15), 1025000, 0x03006000},
  { {1, 1}, 1242000,  ACPU_SCPLL, 0, 0, 1, 0x17, L2(16), 1050000, 0x03006000},
  { {1, 1}, 1296000,  ACPU_SCPLL, 0, 0, 1, 0x18, L2(17), 1075000, 0x03006000},
  { {1, 1}, 1350000,  ACPU_SCPLL, 0, 0, 1, 0x19, L2(18), 1100000, 0x03006000},
  { {1, 1}, 1404000,  ACPU_SCPLL, 0, 0, 1, 0x1A, L2(19), 1100000, 0x03006000},
  { {1, 1}, 1458000,  ACPU_SCPLL, 0, 0, 1, 0x1B, L2(19), 1100000, 0x03006000},
  { {1, 1}, 1512000,  ACPU_SCPLL, 0, 0, 1, 0x1C, L2(19), 1125000, 0x03006000},
  { {0, 0}, 0 },
};


/* acpu_freq_tbl row to use when reconfiguring SC/L2 PLLs. */
#define CAL_IDX 1

static struct clkctl_acpu_speed *acpu_freq_tbl;
static struct clkctl_l2_speed *l2_freq_tbl = l2_freq_tbl_v2;
static unsigned int l2_freq_tbl_size = ARRAY_SIZE(l2_freq_tbl_v2);

static unsigned long acpuclk_8x60_get_rate(int cpu)
{
	return drv_state.current_speed[cpu]->acpuclk_khz;
}

static void select_core_source(unsigned int id, unsigned int src)
{
	uint32_t regval;
	int shift;

	shift = (id == L2) ? 0 : 1;
	regval = readl_relaxed(clk_sel_addr[id]);
	regval &= ~(0x3 << shift);
	regval |= (src << shift);
	writel_relaxed(regval, clk_sel_addr[id]);
}

static void select_clk_source_div(unsigned int id, struct clkctl_acpu_speed *s)
{
	uint32_t reg_clksel, reg_clkctl, src_sel;

	/* Configure the PLL divider mux if we plan to use it. */
	if (s->core_src_sel == 0) {

		reg_clksel = readl_relaxed(clk_sel_addr[id]);

		/* CLK_SEL_SRC1N0 (bank) bit. */
		src_sel = reg_clksel & 1;

		/* Program clock source and divider. */
		reg_clkctl = readl_relaxed(clk_ctl_addr[id]);
		reg_clkctl &= ~(0xFF << (8 * src_sel));
		reg_clkctl |= s->acpuclk_src_sel << (4 + 8 * src_sel);
		reg_clkctl |= s->acpuclk_src_div << (0 + 8 * src_sel);
		writel_relaxed(reg_clkctl, clk_ctl_addr[id]);

		/* Toggle clock source. */
		reg_clksel ^= 1;

		/* Program clock source selection. */
		writel_relaxed(reg_clksel, clk_sel_addr[id]);
	}
}

static void scpll_enable(int sc_pll, uint32_t l_val)
{
	uint32_t regval;

	/* Power-up SCPLL into standby mode. */
	writel_relaxed(SCPLL_STANDBY, sc_pll_base[sc_pll] + SCPLL_CTL_OFFSET);
	mb();
	udelay(10);

	/* Shot-switch to target frequency. */
	regval = (l_val << 3) | SHOT_SWITCH;
	writel_relaxed(regval, sc_pll_base[sc_pll] + SCPLL_FSM_CTL_EXT_OFFSET);
	writel_relaxed(SCPLL_NORMAL, sc_pll_base[sc_pll] + SCPLL_CTL_OFFSET);
	mb();
	udelay(20);
}

static void scpll_disable(int sc_pll)
{
	/* Power down SCPLL. */
	writel_relaxed(SCPLL_POWER_DOWN,
		       sc_pll_base[sc_pll] + SCPLL_CTL_OFFSET);
}

static void scpll_change_freq(int sc_pll, uint32_t l_val)
{
	uint32_t regval;
	const void *base_addr = sc_pll_base[sc_pll];

	/* Complex-slew switch to target frequency. */
	regval = (l_val << 3) | COMPLEX_SLEW;
	writel_relaxed(regval, base_addr + SCPLL_FSM_CTL_EXT_OFFSET);
	writel_relaxed(SCPLL_NORMAL, base_addr + SCPLL_CTL_OFFSET);

	/* Wait for frequency switch to start. */
	while (((readl_relaxed(base_addr + SCPLL_CTL_OFFSET) >> 3) & 0x3F)
			!= l_val)
		cpu_relax();
	/* Wait for frequency switch to finish. */
	while (readl_relaxed(base_addr + SCPLL_STATUS_OFFSET) & 0x1)
		cpu_relax();
}

/* Vote for the L2 speed and return the speed that should be applied. */
static struct clkctl_l2_speed *compute_l2_speed(unsigned int voting_cpu,
						struct clkctl_l2_speed *tgt_s)
{
	struct clkctl_l2_speed *new_s;
	int cpu;

	/* Bounds check. */
	BUG_ON(tgt_s >= (l2_freq_tbl + l2_freq_tbl_size));

	/* Find max L2 speed vote. */
	l2_vote[voting_cpu] = tgt_s;
	new_s = l2_freq_tbl;
	for_each_present_cpu(cpu)
		new_s = max(new_s, l2_vote[cpu]);

	return new_s;
}

/* Set the L2's clock speed. */
static void set_l2_speed(struct clkctl_l2_speed *tgt_s)
{
	if (tgt_s == drv_state.current_l2_speed)
		return;

	if (drv_state.current_l2_speed->src_sel == 1
				&& tgt_s->src_sel == 1)
		scpll_change_freq(L2, tgt_s->l_val);
	else {
		if (tgt_s->src_sel == 1) {
			scpll_enable(L2, tgt_s->l_val);
			mb();
			select_core_source(L2, tgt_s->src_sel);
		} else {
			select_core_source(L2, tgt_s->src_sel);
			mb();
			scpll_disable(L2);
		}
	}
	drv_state.current_l2_speed = tgt_s;
}

/* Update the bus bandwidth request. */
static void set_bus_bw(unsigned int bw)
{
	int ret;

	/* Bounds check. */
	if (bw >= ARRAY_SIZE(bw_level_tbl)) {
		pr_err("%s: invalid bandwidth request (%d)\n", __func__, bw);
		return;
	}

	/* Update bandwidth if requst has changed. This may sleep. */
	ret = msm_bus_scale_client_update_request(bus_perf_client, bw);
	if (ret)
		pr_err("%s: bandwidth request failed (%d)\n", __func__, ret);

	return;
}

/* Apply any per-cpu voltage increases. */
static int increase_vdd(int cpu, unsigned int vdd_sc, unsigned int vdd_mem,
			unsigned int vdd_dig, enum setrate_reason reason)
{
	int rc = 0;

	/* Increase vdd_mem active-set before vdd_dig and vdd_sc.
	 * vdd_mem should be >= both vdd_sc and vdd_dig. */
	rc = rpm_vreg_set_voltage(RPM_VREG_ID_PM8058_S0, rpm_vreg_voter[cpu],
				  vdd_mem, MAX_VDD_MEM, 0);
	if (rc) {
		pr_err("%s: vdd_mem (cpu%d) increase failed (%d)\n",
			__func__, cpu, rc);
		return rc;
	}

	/* Increase vdd_dig active-set vote. */
	rc = rpm_vreg_set_voltage(RPM_VREG_ID_PM8058_S1, rpm_vreg_voter[cpu],
				  vdd_dig, MAX_VDD_DIG, 0);
	if (rc) {
		pr_err("%s: vdd_dig (cpu%d) increase failed (%d)\n",
			__func__, cpu, rc);
		return rc;
	}

	 /* Don't update the Scorpion voltage in the hotplug path.  It should
	  * already be correct.  Attempting to set it is bad because we don't
	  * know what CPU we are running on at this point, but the Scorpion
	  * regulator API requires we call it from the affected CPU.  */
	if (reason == SETRATE_HOTPLUG)
		return rc;

	/* Update per-core Scorpion voltage. */
	rc = regulator_set_voltage(regulator_sc[cpu], vdd_sc, MAX_VDD_SC);
	if (rc) {
		pr_err("%s: vdd_sc (cpu%d) increase failed (%d)\n",
			__func__, cpu, rc);
		return rc;
	}

	return rc;
}

/* Apply any per-cpu voltage decreases. */
static void decrease_vdd(int cpu, unsigned int vdd_sc, unsigned int vdd_mem,
			 unsigned int vdd_dig, enum setrate_reason reason)
{
	int ret;

	/* Update per-core Scorpion voltage. This must be called on the CPU
	 * that's being affected. Don't do this in the hotplug remove path,
	 * where the rail is off and we're executing on the other CPU. */
	if (reason != SETRATE_HOTPLUG) {
		ret = regulator_set_voltage(regulator_sc[cpu], vdd_sc,
					    MAX_VDD_SC);
		if (ret) {
			pr_err("%s: vdd_sc (cpu%d) decrease failed (%d)\n",
				__func__, cpu, ret);
			return;
		}
	}

	/* Decrease vdd_dig active-set vote. */
	ret = rpm_vreg_set_voltage(RPM_VREG_ID_PM8058_S1, rpm_vreg_voter[cpu],
				   vdd_dig, MAX_VDD_DIG, 0);
	if (ret) {
		pr_err("%s: vdd_dig (cpu%d) decrease failed (%d)\n",
			__func__, cpu, ret);
		return;
	}

	/* Decrease vdd_mem active-set after vdd_dig and vdd_sc.
	 * vdd_mem should be >= both vdd_sc and vdd_dig. */
	ret = rpm_vreg_set_voltage(RPM_VREG_ID_PM8058_S0, rpm_vreg_voter[cpu],
				   vdd_mem, MAX_VDD_MEM, 0);
	if (ret) {
		pr_err("%s: vdd_mem (cpu%d) decrease failed (%d)\n",
			__func__, cpu, ret);
		return;
	}
}

static void switch_sc_speed(int cpu, struct clkctl_acpu_speed *tgt_s)
{
	struct clkctl_acpu_speed *strt_s = drv_state.current_speed[cpu];

	if (strt_s->pll != ACPU_SCPLL && tgt_s->pll != ACPU_SCPLL) {
		select_clk_source_div(cpu, tgt_s);
		/* Select core source because target may be AFAB. */
		select_core_source(cpu, tgt_s->core_src_sel);
	} else if (strt_s->pll != ACPU_SCPLL && tgt_s->pll == ACPU_SCPLL) {
		scpll_enable(cpu, tgt_s->l_val);
		mb();
		select_core_source(cpu, tgt_s->core_src_sel);
	} else if (strt_s->pll == ACPU_SCPLL && tgt_s->pll != ACPU_SCPLL) {
		select_clk_source_div(cpu, tgt_s);
		select_core_source(cpu, tgt_s->core_src_sel);
		/* Core source switch must complete before disabling SCPLL. */
		mb();
		udelay(1);
		scpll_disable(cpu);
	} else
		scpll_change_freq(cpu, tgt_s->l_val);

	/* Update the driver state with the new clock freq */
	drv_state.current_speed[cpu] = tgt_s;
}

static int acpuclk_8x60_set_rate(int cpu, unsigned long rate,
				 enum setrate_reason reason)
{
	struct clkctl_acpu_speed *tgt_s, *strt_s;
	struct clkctl_l2_speed *tgt_l2;
	unsigned int vdd_mem, vdd_dig, pll_vdd_dig;
	unsigned long flags;
	int rc = 0;

	if (cpu > num_possible_cpus()) {
		rc = -EINVAL;
		goto out;
	}

	if (reason == SETRATE_CPUFREQ || reason == SETRATE_HOTPLUG)
		mutex_lock(&drv_state.lock);

	strt_s = drv_state.current_speed[cpu];

	/* Return early if rate didn't change. */
	if (rate == strt_s->acpuclk_khz)
		goto out;

	/* Find target frequency. */
	for (tgt_s = acpu_freq_tbl; tgt_s->acpuclk_khz != 0; tgt_s++)
		if (tgt_s->acpuclk_khz == rate)
			break;
	if (tgt_s->acpuclk_khz == 0) {
		rc = -EINVAL;
		goto out;
	}

	/* AVS needs SAW_VCTL to be intitialized correctly, before enable,
	 * and is not initialized at acpuclk_init().
	 */
	if (reason == SETRATE_CPUFREQ)
		AVS_DISABLE(cpu);

	/* Calculate vdd_mem and vdd_dig requirements.
	 * vdd_mem must be >= vdd_sc */
	vdd_mem = max(tgt_s->vdd_sc, tgt_s->l2_level->vdd_mem);
	/* Factor-in PLL vdd_dig requirements. */
	if ((tgt_s->l2_level->khz > SCPLL_LOW_VDD_FMAX) ||
	    (tgt_s->pll == ACPU_SCPLL
	     && tgt_s->acpuclk_khz > SCPLL_LOW_VDD_FMAX))
		pll_vdd_dig = SCPLL_NOMINAL_VDD;
	else
		pll_vdd_dig = SCPLL_LOW_VDD;
	vdd_dig = max(tgt_s->l2_level->vdd_dig, pll_vdd_dig);

	/* Increase VDD levels if needed. */
	if ((reason == SETRATE_CPUFREQ || reason == SETRATE_HOTPLUG
	  || reason == SETRATE_INIT)
			&& (tgt_s->acpuclk_khz > strt_s->acpuclk_khz)) {
		rc = increase_vdd(cpu, tgt_s->vdd_sc, vdd_mem, vdd_dig, reason);
		if (rc)
			goto out;
	}

	pr_debug("Switching from ACPU%d rate %u KHz -> %u KHz\n",
		cpu, strt_s->acpuclk_khz, tgt_s->acpuclk_khz);

	/* Switch CPU speed. */
	switch_sc_speed(cpu, tgt_s);

	/* Update the L2 vote and apply the rate change. */
	spin_lock_irqsave(&drv_state.l2_lock, flags);
	tgt_l2 = compute_l2_speed(cpu, tgt_s->l2_level);
	set_l2_speed(tgt_l2);
	spin_unlock_irqrestore(&drv_state.l2_lock, flags);

	/* Nothing else to do for SWFI. */
	if (reason == SETRATE_SWFI)
		goto out;

	/* Nothing else to do for power collapse. */
	if (reason == SETRATE_PC)
		goto out;

	/* Update bus bandwith request. */
	set_bus_bw(tgt_l2->bw_level);

	/* Drop VDD levels if we can. */
	if (tgt_s->acpuclk_khz < strt_s->acpuclk_khz)
		decrease_vdd(cpu, tgt_s->vdd_sc, vdd_mem, vdd_dig, reason);

	pr_debug("ACPU%d speed change complete\n", cpu);

	/* Re-enable AVS */
	if (reason == SETRATE_CPUFREQ)
		AVS_ENABLE(cpu, tgt_s->avsdscr_setting);

out:
	if (reason == SETRATE_CPUFREQ || reason == SETRATE_HOTPLUG)
		mutex_unlock(&drv_state.lock);
	return rc;
}

static void __init scpll_init(int sc_pll)
{
	uint32_t regval;

	pr_debug("Initializing SCPLL%d\n", sc_pll);

	/* Clear calibration LUT registers containing max frequency entry.
	 * LUT registers are only writeable in debug mode. */
	writel_relaxed(SCPLL_DEBUG_FULL,
		       sc_pll_base[sc_pll] + SCPLL_DEBUG_OFFSET);
	writel_relaxed(0x0, sc_pll_base[sc_pll] + SCPLL_LUT_A_HW_MAX);
	writel_relaxed(SCPLL_DEBUG_NONE,
		       sc_pll_base[sc_pll] + SCPLL_DEBUG_OFFSET);

	/* Power-up SCPLL into standby mode. */
	writel_relaxed(SCPLL_STANDBY, sc_pll_base[sc_pll] + SCPLL_CTL_OFFSET);
	mb();
	udelay(10);

	/* Calibrate the SCPLL to the maximum range supported by the h/w. We
	 * might not use the full range of calibrated frequencies, but this
	 * simplifies changes required for future increases in max CPU freq.
	 */
	regval = (L_VAL_SCPLL_CAL_MAX << 24) | (L_VAL_SCPLL_CAL_MIN << 16);
	writel_relaxed(regval, sc_pll_base[sc_pll] + SCPLL_CAL_OFFSET);

	/* Start calibration */
	writel_relaxed(SCPLL_FULL_CAL, sc_pll_base[sc_pll] + SCPLL_CTL_OFFSET);

	/* Wait for proof that calibration has started before checking the
	 * 'calibration done' bit in the status register. Waiting for the
	 * LUT register we cleared to contain data accomplishes this.
	 * This is required since the 'calibration done' bit takes time to
	 * transition from 'done' to 'not done' when starting a calibration.
	 */
	while (readl_relaxed(sc_pll_base[sc_pll] + SCPLL_LUT_A_HW_MAX) == 0)
		cpu_relax();

	/* Wait for calibration to complete. */
	while (readl_relaxed(sc_pll_base[sc_pll] + SCPLL_STATUS_OFFSET) & 0x2)
		cpu_relax();

	/* Power-down SCPLL. */
	scpll_disable(sc_pll);
}

/* Force ACPU core and L2 cache clocks to rates that don't require SCPLLs. */
static void __init unselect_scplls(void)
{
	int cpu;

	/* Ensure CAL_IDX frequency uses AFAB sources for CPU cores and L2. */
	BUG_ON(acpu_freq_tbl[CAL_IDX].core_src_sel != 0);
	BUG_ON(acpu_freq_tbl[CAL_IDX].l2_level->src_sel != 0);

	for_each_possible_cpu(cpu) {
		select_clk_source_div(cpu, &acpu_freq_tbl[CAL_IDX]);
		select_core_source(cpu, acpu_freq_tbl[CAL_IDX].core_src_sel);
		drv_state.current_speed[cpu] = &acpu_freq_tbl[CAL_IDX];
		l2_vote[cpu] = acpu_freq_tbl[CAL_IDX].l2_level;
	}

	select_core_source(L2, acpu_freq_tbl[CAL_IDX].l2_level->src_sel);
	drv_state.current_l2_speed = acpu_freq_tbl[CAL_IDX].l2_level;
}

/* Ensure SCPLLs use the 27MHz PXO. */
static void __init scpll_set_refs(void)
{
	int cpu;
	uint32_t regval;

	/* Bit 4 = 0:PXO, 1:MXO. */
	for_each_possible_cpu(cpu) {
		regval = readl_relaxed(sc_pll_base[cpu] + SCPLL_CFG_OFFSET);
		regval &= ~BIT(4);
		writel_relaxed(regval, sc_pll_base[cpu] + SCPLL_CFG_OFFSET);
	}
	regval = readl_relaxed(sc_pll_base[L2] + SCPLL_CFG_OFFSET);
	regval &= ~BIT(4);
	writel_relaxed(regval, sc_pll_base[L2] + SCPLL_CFG_OFFSET);
}

/* Voltage regulator initialization. */
static void __init regulator_init(void)
{
	struct clkctl_acpu_speed **freq = drv_state.current_speed;
	const char *regulator_sc_name[] = {"8901_s0", "8901_s1"};
	int cpu, ret;

	for_each_possible_cpu(cpu) {
		/* VDD_SC0, VDD_SC1 */
		regulator_sc[cpu] = regulator_get(NULL, regulator_sc_name[cpu]);
		if (IS_ERR(regulator_sc[cpu]))
			goto err;
		ret = regulator_set_voltage(regulator_sc[cpu],
				freq[cpu]->vdd_sc, MAX_VDD_SC);
		if (ret)
			goto err;
		ret = regulator_enable(regulator_sc[cpu]);
		if (ret)
			goto err;
	}

	return;

err:
	pr_err("%s: Failed to initialize voltage regulators\n", __func__);
	BUG();
}

/* Register with bus driver. */
static void __init bus_init(void)
{
	bus_perf_client = msm_bus_scale_register_client(&bus_client_pdata);
	if (!bus_perf_client) {
		pr_err("%s: unable register bus client\n", __func__);
		BUG();
	}
}

#ifdef CONFIG_CPU_FREQ_MSM
static struct cpufreq_frequency_table freq_table[NR_CPUS][30];

static void __init cpufreq_table_init(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		int i, freq_cnt = 0;
		/* Construct the freq_table tables from acpu_freq_tbl. */
		for (i = 0; acpu_freq_tbl[i].acpuclk_khz != 0
				&& freq_cnt < ARRAY_SIZE(*freq_table); i++) {
			if (acpu_freq_tbl[i].use_for_scaling[cpu]) {
				freq_table[cpu][freq_cnt].index = freq_cnt;
				freq_table[cpu][freq_cnt].frequency
					= acpu_freq_tbl[i].acpuclk_khz;
				freq_cnt++;
			}
		}
		/* freq_table not big enough to store all usable freqs. */
		BUG_ON(acpu_freq_tbl[i].acpuclk_khz != 0);

		freq_table[cpu][freq_cnt].index = freq_cnt;
		freq_table[cpu][freq_cnt].frequency = CPUFREQ_TABLE_END;

		pr_info("CPU%d: %d scaling frequencies supported.\n",
			cpu, freq_cnt);

		/* Register table with CPUFreq. */
		cpufreq_frequency_table_get_attr(freq_table[cpu], cpu);
	}
}
#else
static void __init cpufreq_table_init(void) {}
#endif

#define HOT_UNPLUG_KHZ MAX_AXI
static int __cpuinit acpuclock_cpu_callback(struct notifier_block *nfb,
					    unsigned long action, void *hcpu)
{
	static int prev_khz[NR_CPUS];
	int cpu = (int)hcpu;

	switch (action) {
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		prev_khz[cpu] = acpuclk_8x60_get_rate(cpu);
		/* Fall through. */
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
		acpuclk_8x60_set_rate(cpu, HOT_UNPLUG_KHZ, SETRATE_HOTPLUG);
		break;
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		if (WARN_ON(!prev_khz[cpu]))
			return NOTIFY_BAD;
		acpuclk_8x60_set_rate(cpu, prev_khz[cpu], SETRATE_HOTPLUG);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata acpuclock_cpu_notifier = {
	.notifier_call = acpuclock_cpu_callback,
};

static unsigned int __init select_freq_plan(void)
{
	uint32_t pte_efuse, speed_bin, pvs, max_khz;
	struct clkctl_acpu_speed *f;

	pte_efuse = readl_relaxed(QFPROM_PTE_EFUSE_ADDR);

	speed_bin = pte_efuse & 0xF;
	if (speed_bin == 0xF)
		speed_bin = (pte_efuse >> 4) & 0xF;

	if (speed_bin == 0x1) {
		max_khz = 1512000;
		pvs = (pte_efuse >> 10) & 0x7;
		if (pvs == 0x7)
			pvs = (pte_efuse >> 13) & 0x7;

		switch (pvs) {
		case 0x0:
		case 0x7:
			acpu_freq_tbl = acpu_freq_tbl_slow;
			pr_info("ACPU PVS: Slow\n");
			break;
		case 0x1:
			acpu_freq_tbl = acpu_freq_tbl_nom;
			pr_info("ACPU PVS: Nominal\n");
			break;
		case 0x3:
			acpu_freq_tbl = acpu_freq_tbl_fast;
			pr_info("ACPU PVS: Fast\n");
			break;
		default:
			acpu_freq_tbl = acpu_freq_tbl_slow;
			pr_warn("ACPU PVS: Unknown. Defaulting to slow.\n");
			break;
		}
	} else {
		max_khz = 1188000;
		acpu_freq_tbl = acpu_freq_tbl_1188mhz;
	}

	/* Truncate the table based to max_khz. */
	for (f = acpu_freq_tbl; f->acpuclk_khz != 0; f++) {
		if (f->acpuclk_khz > max_khz) {
			f->acpuclk_khz = 0;
			break;
		}
	}
	f--;
	pr_info("Max ACPU freq: %u KHz\n", f->acpuclk_khz);

	return f->acpuclk_khz;
}

static struct acpuclk_data acpuclk_8x60_data = {
	.set_rate = acpuclk_8x60_set_rate,
	.get_rate = acpuclk_8x60_get_rate,
	.power_collapse_khz = MAX_AXI,
	.wait_for_irq_khz = MAX_AXI,
};

static int __init acpuclk_8x60_init(struct acpuclk_soc_data *soc_data)
{
	unsigned int max_cpu_khz;
	int cpu;

	mutex_init(&drv_state.lock);
	spin_lock_init(&drv_state.l2_lock);

	/* Configure hardware. */
	max_cpu_khz = select_freq_plan();
	unselect_scplls();
	scpll_set_refs();
	for_each_possible_cpu(cpu)
		scpll_init(cpu);
	scpll_init(L2);
	regulator_init();
	bus_init();

	/* Improve boot time by ramping up CPUs immediately. */
	for_each_online_cpu(cpu)
		acpuclk_8x60_set_rate(cpu, max_cpu_khz, SETRATE_INIT);

	acpuclk_register(&acpuclk_8x60_data);
	cpufreq_table_init();
	register_hotcpu_notifier(&acpuclock_cpu_notifier);

	return 0;
}

struct acpuclk_soc_data acpuclk_8x60_soc_data __initdata = {
	.init = acpuclk_8x60_init,
};
