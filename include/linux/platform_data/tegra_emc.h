/*
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2012-2014 NVIDIA Corporation. All rights reserved.
 *
 * Author:
 *	Colin Cross <ccross@android.com>
 *	Olof Johansson <olof@lixom.net>
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

#ifndef __TEGRA_EMC_H_
#define __TEGRA_EMC_H_

#define TEGRA14_MAX_TABLE_ID_LEN	16
#define TEGRA12_MAX_TABLE_ID_LEN	50

#define TEGRA_EMC_MAX_FREQS 20

#define TEGRA_EMC_NUM_REGS 46
#define TEGRA30_EMC_NUM_REGS 110

struct tegra_emc_table {
	unsigned long rate;
	u32 regs[TEGRA_EMC_NUM_REGS];
};

struct tegra_emc_pdata {
	const char *description;
	int mem_manufacturer_id; /* LPDDR2 MR5 or -1 to ignore */
	int mem_revision_id1;    /* LPDDR2 MR6 or -1 to ignore */
	int mem_revision_id2;    /* LPDDR2 MR7 or -1 to ignore */
	int mem_pid;             /* LPDDR2 MR8 or -1 to ignore */
	int num_tables;
	struct tegra_emc_table *tables;
};

struct tegra30_emc_table {
	u8 rev;
	unsigned long rate;

	/* unconditionally updated in one burst shot */
	u32 burst_regs[TEGRA30_EMC_NUM_REGS];

	/* updated separately under some conditions */
	u32 emc_zcal_cnt_long;
	u32 emc_acal_interval;
	u32 emc_periodic_qrst;
	u32 emc_mode_reset;
	u32 emc_mode_1;
	u32 emc_mode_2;
	u32 emc_dsr;
	int emc_min_mv;
};

struct tegra30_emc_pdata {
	const char *description;
	int num_tables;
	struct tegra30_emc_table *tables;
};

/* !!!FIXME!!! Need actual Tegra11x values */
#define TEGRA11_EMC_MAX_NUM_REGS	120

struct tegra11_emc_table {
	u8 rev;
	unsigned long rate;
	int emc_min_mv;
	const char *src_name;
	u32 src_sel_reg;

	int burst_regs_num;
	int emc_trimmers_num;
	int burst_up_down_regs_num;

	/* unconditionally updated in one burst shot */
	u32 burst_regs[TEGRA11_EMC_MAX_NUM_REGS];

	/* unconditionally updated in one burst shot to particular channel */
	u32 emc_trimmers_0[TEGRA11_EMC_MAX_NUM_REGS];
	u32 emc_trimmers_1[TEGRA11_EMC_MAX_NUM_REGS];

	/* one burst shot, but update time depends on rate change direction */
	u32 burst_up_down_regs[TEGRA11_EMC_MAX_NUM_REGS];

	/* updated separately under some conditions */
	u32 emc_zcal_cnt_long;
	u32 emc_acal_interval;
	u32 emc_cfg;
	u32 emc_mode_reset;
	u32 emc_mode_1;
	u32 emc_mode_2;
	u32 emc_mode_4;
	u32 clock_change_latency;
};

struct tegra11_emc_pdata {
	const char *description;
	int num_tables;
	struct tegra11_emc_table *tables;
};

/* !!!FIXME!!! Need actual Tegra14x values */
#define TEGRA14_EMC_MAX_NUM_REGS	200
#define TEGRA14_EMC_MAX_UP_DOWN_REGS	20

struct tegra14_emc_table {
	u8 rev;
	char table_id[TEGRA14_MAX_TABLE_ID_LEN];
	unsigned long rate;
	int emc_min_mv;
	const char *src_name;
	u32 src_sel_reg;

	int burst_regs_num;
	int burst_up_down_regs_num;

	/* unconditionally updated in one burst shot */
	u32 burst_regs[TEGRA14_EMC_MAX_NUM_REGS];

	/* one burst shot, but update time depends on rate change direction */
	u32 burst_up_down_regs[TEGRA14_EMC_MAX_UP_DOWN_REGS];

	/* updated separately under some conditions */
	u32 emc_zcal_cnt_long;
	u32 emc_acal_interval;
	u32 emc_cfg;
	u32 emc_cfg_dig_dll;
	u32 emc_mode_reset;
	u32 emc_mode_1;
	u32 emc_mode_2;
	u32 emc_mode_4;
	u32 clock_change_latency;
};

struct tegra14_emc_pdata {
	const char *description;
	int num_tables;
	struct tegra14_emc_table *tables;
	struct tegra14_emc_table *tables_derated;
};

#define TEGRA12_EMC_MAX_NUM_REGS 200
#define TEGRA12_EMC_MAX_NUM_BURST_REGS 175
#define TEGRA12_EMC_MAX_UP_DOWN_REGS 40

struct tegra12_emc_table {
	u8 rev;
	char table_id[TEGRA12_MAX_TABLE_ID_LEN];
	unsigned long rate;
	int emc_min_mv;
	int gk20a_min_mv;
	char src_name[16];
	u32 src_sel_reg;

	int burst_regs_num;
	int burst_up_down_regs_num;

	/* unconditionally updated in one burst shot */
	u32 burst_regs[TEGRA12_EMC_MAX_NUM_BURST_REGS];

	/* one burst shot, but update time depends on rate change direction */
	u32 burst_up_down_regs[TEGRA12_EMC_MAX_UP_DOWN_REGS];

	/* updated separately under some conditions */
	u32 emc_zcal_cnt_long;
	u32 emc_acal_interval;
	u32 emc_ctt_term_ctrl;
	u32 emc_cfg;
	u32 emc_cfg_2;
	u32 emc_sel_dpd_ctrl;
	u32 emc_cfg_dig_dll;
	u32 emc_bgbias_ctl0;
	u32 emc_auto_cal_config2;
	u32 emc_auto_cal_config3;
	u32 emc_auto_cal_config;
	u32 emc_mode_reset;
	u32 emc_mode_1;
	u32 emc_mode_2;
	u32 emc_mode_4;
	u32 clock_change_latency;
};

struct tegra12_emc_pdata {
	const char *description;
	int num_tables;
	struct tegra12_emc_table *tables;
	struct tegra12_emc_table *tables_derated;
};

#endif
