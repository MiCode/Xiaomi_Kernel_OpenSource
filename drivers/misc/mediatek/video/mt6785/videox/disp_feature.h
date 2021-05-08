/*
 * Copyright (C) 2020 Zhao, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

/*****************************sanxing HBM start*********************/
struct LCM_setting_table sanxing_hbm0_off[1] = {
	{0x51, 2, {0x07,0xFF} },
};
struct LCM_setting_table sanxing_hbm1_on[1] = {
	{0x51, 2, {0x07,0xFF} },
};
struct LCM_setting_table sanxing_hbm2_on[1] = {
	{0x51, 2, {0x07,0xFF} },
};
/*****************************sanxing HBM end**************************/

