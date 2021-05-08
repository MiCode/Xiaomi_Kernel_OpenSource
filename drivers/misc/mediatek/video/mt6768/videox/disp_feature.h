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

/*****************************dijing HBM start*********************/
struct LCM_setting_table dijing_hbm0_on[1] = {
	{0x51, 2, {0x05,0xC2} },
};
struct LCM_setting_table dijing_hbm1_on[1] = {
	{0x51, 2, {0x06,0x21} },
};
struct LCM_setting_table dijing_hbm2_on[1] = {
	{0x51, 2, {0x06,0xA9} },
};
/*****************************dijing HBM end**************************/

/*****************************helitai HBM start***********************/
struct LCM_setting_table helitai_hbm0_on[1] = {
	{0x51, 2, {0xB8,0x04} },
};
struct LCM_setting_table helitai_hbm1_on[1] = {
	{0x51, 2, {0xC4,0x02} },
};
struct LCM_setting_table helitai_hbm2_on[1] = {
	{0x51, 2, {0xD5,0x02} },
};
/*****************************helitai HBM end**************************/


/*****************************xinli HBM start**************************/
struct LCM_setting_table xinli_hbm0_on[1] = {
	{0x51, 2, {0x0B,0x84} },
};
struct LCM_setting_table xinli_hbm1_on[1] = {
	{0x51, 2, {0x0C,0x42} },
};
struct LCM_setting_table xinli_hbm2_on[1] = {
	{0x51, 2, {0x0D,0x52} },
};
/*****************************xinli HBM end**************************/

/* 2020.12.15 longcheer baiyihan add for tianma display feature start */
/*****************************tianma HBM start**************************/
struct LCM_setting_table tianma_hbm0_on[1] = {
	{0x51, 2, {0xB8,0x04} },
};
struct LCM_setting_table tianma_hbm1_on[1] = {
	{0x51, 2, {0xC4,0x02} },
};
struct LCM_setting_table tianma_hbm2_on[1] = {
	{0x51, 2, {0xD5,0x02} },
};
/*****************************tianma HBM end**************************/
/* 2020.12.15 longcheer baiyihan add for tianma display feature end*/
