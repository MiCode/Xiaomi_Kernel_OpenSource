/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _VPU_DVFS_H_
#define _VPU_DVFS_H_


/* ++++++++++++++++++++++++++++++++++*/
/* |opp_index  |   vpu frequency  |        power             */
/* ------------------------------------------*/
/* |      0         |   525 MHz          |        336 mA           */
/* ------------------------------------------*/
/* |      1         |   450 MHz          |        250 mA           */
/* ------------------------------------------*/
/* |      2         |   416 MHz          |        221 mA           */
/* ------------------------------------------*/
/* |      3         |   364 MHz          |        208 mA           */
/* ------------------------------------------*/
/* |      4         |   312 MHz          |        140 mA           */
/* ------------------------------------------*/
/* |      5         |   273 MHz          |        120 mA           */
/* ------------------------------------------*/
/* |      6         |   208 MHz          |        114 mA           */
/* ------------------------------------------*/
/* |      7         |   182 MHz          |         84 mA           */
/* ++++++++++++++++++++++++++++++++++*/

enum VPU_OPP_INDEX {
	VPU_OPP_0 = 0,
	VPU_OPP_1 = 1,
	VPU_OPP_2 = 2,
	VPU_OPP_3 = 3,
	VPU_OPP_4 = 4,
	VPU_OPP_5 = 5,
	VPU_OPP_6 = 6,
	VPU_OPP_7 = 7,
	VPU_OPP_NUM
};

struct VPU_OPP_INFO {
	enum VPU_OPP_INDEX opp_index;
	int power;	/*mW*/
};

extern struct VPU_OPP_INFO vpu_power_table[VPU_OPP_NUM];
extern int32_t vpu_thermal_en_throttle_cb(uint8_t vcore_opp,
	uint8_t vpu_opp);
extern int32_t vpu_thermal_dis_throttle_cb(void);

#endif
