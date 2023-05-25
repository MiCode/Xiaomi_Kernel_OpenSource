/*
 * Copyright (C) 2020 Zhao, Inc.
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

/*****************************xinli HBM start**************************/
struct LCM_setting_table xinli_hbm0_on[1] = {
	{0x51, 2, {0x05,0xD6} },
};
struct LCM_setting_table xinli_hbm1_on[1] = {
	{0x51, 2, {0x06,0x8E} },
};
struct LCM_setting_table xinli_hbm2_on[1] = {
	{0x51, 2, {0x07,0x32} },
};
/*****************************xinli HBM end**************************/

/*****************************helitai HBM start***********************/
struct LCM_setting_table helitai_hbm0_on[1] = {
	{0x51, 2, {0xBA,0x0C} },
};
struct LCM_setting_table helitai_hbm1_on[1] = {
	{0x51, 2, {0xD1,0x0C} },
};
struct LCM_setting_table helitai_hbm2_on[1] = {
	{0x51, 2, {0xE6,0x04} },
};
/*****************************helitai HBM end**************************/

/*****************************tianma HBM start***********************/
struct LCM_setting_table tianma_hbm0_on[1] = {
	{0x51, 2, {0x04,0xF5} },
};
struct LCM_setting_table tianma_hbm1_on[1] = {
	{0x51, 2, {0x05,0x9c} },
};
struct LCM_setting_table tianma_hbm2_on[1] = {
	{0x51, 2, {0x06,0x3c} },
};
/*****************************tianma HBM end**************************/

