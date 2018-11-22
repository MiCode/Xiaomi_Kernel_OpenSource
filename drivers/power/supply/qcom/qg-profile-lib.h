/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
 */

#ifndef __QG_PROFILE_LIB_H__
#define __QG_PROFILE_LIB_H__

struct profile_table_data {
	char		*name;
	int		rows;
	int		cols;
	int		*row_entries;
	int		*col_entries;
	int		**data;
};

int interpolate_single_row_lut(struct profile_table_data *lut,
						int x, int scale);
int interpolate_soc(struct profile_table_data *lut,
				int batt_temp, int ocv);
int interpolate_var(struct profile_table_data *lut,
				int batt_temp, int soc);
int interpolate_slope(struct profile_table_data *lut,
				int batt_temp, int soc);

#endif /*__QG_PROFILE_LIB_H__ */
