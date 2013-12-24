/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#include <linux/batterydata-lib.h>

static struct single_row_lut desay_5200_fcc_temp = {
	.x		= {-20, 0, 25, 40},
	.y		= {5690, 5722, 5722, 5727},
	.cols	= 4
};

static struct single_row_lut desay_5200_fcc_sf = {
	.x		= {0},
	.y		= {100},
	.cols	= 1
};

static struct pc_temp_ocv_lut desay_5200_pc_temp_ocv = {
	.rows		= 29,
	.cols		= 4,
	.temp		= {-20, 0, 25, 40},
	.percent	= {100, 95, 90, 85, 80, 75, 70, 65, 60, 55,
				50, 45, 40, 35, 30, 25, 20, 15, 10, 9, 8,
				7, 6, 5, 4, 3, 2, 1, 0
	},
	.ocv		= {
				{4185, 4184, 4181, 4178},
				{4103, 4117, 4120, 4119},
				{4044, 4067, 4074, 4073},
				{3987, 4019, 4031, 4030},
				{3941, 3974, 3992, 3992},
				{3902, 3936, 3958, 3957},
				{3866, 3901, 3926, 3926},
				{3835, 3870, 3891, 3896},
				{3811, 3842, 3855, 3858},
				{3792, 3818, 3827, 3827},
				{3776, 3795, 3806, 3806},
				{3762, 3778, 3789, 3790},
				{3748, 3765, 3777, 3777},
				{3735, 3752, 3767, 3765},
				{3720, 3739, 3756, 3754},
				{3704, 3726, 3743, 3736},
				{3685, 3712, 3723, 3716},
				{3664, 3697, 3695, 3689},
				{3623, 3672, 3669, 3664},
				{3611, 3666, 3666, 3661},
				{3597, 3659, 3662, 3658},
				{3579, 3648, 3657, 3653},
				{3559, 3630, 3644, 3639},
				{3532, 3600, 3612, 3606},
				{3497, 3558, 3565, 3559},
				{3450, 3500, 3504, 3498},
				{3380, 3417, 3421, 3416},
				{3265, 3287, 3296, 3293},
				{3000, 3000, 3000, 3000}
	},
};

static struct sf_lut desay_5200_pc_sf = {
	.rows		= 1,
	.cols		= 1,
	/* row_entries are cycles here */
	.row_entries		= {0},
	.percent	= {100},
	.sf			= {
				{100}
	},
};

struct bms_battery_data desay_5200_data = {
	.fcc			= 5200,
	.fcc_temp_lut		= &desay_5200_fcc_temp,
	.fcc_sf_lut		= &desay_5200_fcc_sf,
	.pc_temp_ocv_lut	= &desay_5200_pc_temp_ocv,
	.pc_sf_lut		= &desay_5200_pc_sf,
	.default_rbatt_mohm	= 156,
	.rbatt_capacitive_mohm	= 50,
	.flat_ocv_threshold_uv	= 3800000,
	.battery_type		= "desay_5200mah",
};
