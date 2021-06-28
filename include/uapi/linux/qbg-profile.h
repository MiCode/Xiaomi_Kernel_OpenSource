/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef __QBG_PROFILE_H__
#define __QBG_PROFILE_H__

#define MAX_BP_LUT_ROWS	35
#define MAX_BP_LUT_COLS	8
#define MAX_PROFILE_NAME_LENGTH	256

enum profile_table_type {
	CHARGE_TABLE = 0,
	DISCHARGE_TABLE,
};

struct battery_data_table {
	unsigned short int table[MAX_BP_LUT_ROWS][MAX_BP_LUT_COLS];
	int unit_conv_factor[MAX_BP_LUT_COLS];
	unsigned short int nrows;
	unsigned short int ncols;
};

struct battery_config {
	char bp_profile_name[MAX_PROFILE_NAME_LENGTH];
	int bp_batt_id;
	int capacity;
	int bp_checksum;
	int soh_range_high;
	int soh_range_low;
	int normal_impedance;
	int aged_impedance;
	int normal_capacity;
	int aged_capacity;
	int recharge_soc_delta;
	int recharge_vflt_delta;
	int recharge_iterm;
};

struct battery_profile_table {
	enum profile_table_type table_type;
	int table_index;
	struct battery_data_table *table;
};

/* IOCTLs to query battery profile data */
/* Battery configuration */
#define BPIOCXBP \
	_IOWR('B', 0x01, struct battery_config)
/* Battery profile table */
#define BPIOCXBPTABLE \
	_IOWR('B', 0x02, struct battery_profile_table)

#endif
