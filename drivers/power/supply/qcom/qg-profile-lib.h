/* Copyright (c) 2018 The Linux Foundation. All rights reserved.
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
