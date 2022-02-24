// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _VPU_DVFS_H_
#define _VPU_DVFS_H_


/* ++++++++++++++++++++++++++++++++++*/
/* |opp_index  |   vpu frequency  |        power             */
/* ------------------------------------------*/
/* |      0         |   700 MHz          |        336 mA           */
/* ------------------------------------------*/
/* |      1         |   624 MHz          |        250 mA           */
/* ------------------------------------------*/
/* |      2         |   606 MHz          |        221 mA           */
/* ------------------------------------------*/
/* |      3         |   594 MHz          |        208 mA           */
/* ------------------------------------------*/
/* |      4         |   560 MHz          |        140 mA           */
/* ------------------------------------------*/
/* |      5         |   525 MHz          |        120 mA           */
/* ------------------------------------------*/
/* |      6         |   450 MHz          |        114 mA           */
/* ------------------------------------------*/
/* |      7         |   416 MHz          |         84 mA           */
/* ------------------------------------------*/
/* |      8         |   364 MHz          |        336 mA           */
/* ------------------------------------------*/
/* |      9         |   312 MHz          |        250 mA           */
/* ------------------------------------------*/
/* |      10         |   273 MHz          |        221 mA           */
/* ------------------------------------------*/
/* |      11         |   208 MHz          |        208 mA           */
/* ------------------------------------------*/
/* |      12         |   137 MHz          |        140 mA           */
/* ------------------------------------------*/
/* |      13         |   104 MHz          |        120 mA           */
/* ------------------------------------------*/
/* |      14         |   52 MHz          |        114 mA           */
/* ------------------------------------------*/
/* |      15         |   26 MHz          |        114 mA           */
/* ------------------------------------------*/
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
	VPU_OPP_8 = 8,
	VPU_OPP_9 = 9,
	VPU_OPP_10 = 10,
	VPU_OPP_11 = 11,
	VPU_OPP_12 = 12,
	VPU_OPP_NUM
};

struct VPU_OPP_INFO {
	enum VPU_OPP_INDEX opp_index;
	int power;	/*mW*/
};

static const int g_vpu_opp_table[VPU_OPP_NUM] = {
	[VPU_OPP_0] = 700000,
	[VPU_OPP_1] = 624000,
	[VPU_OPP_2] = 606000,
	[VPU_OPP_3] = 594000,
	[VPU_OPP_4] = 560000,
	[VPU_OPP_5] = 525000,
	[VPU_OPP_6] = 450000,
	[VPU_OPP_7] = 416000,
	[VPU_OPP_8] = 364000,
	[VPU_OPP_9] = 312000,
	[VPU_OPP_10] = 273000,
	[VPU_OPP_11] = 208000,
	[VPU_OPP_12] = 137000,
};


extern struct VPU_OPP_INFO vpu_power_table[VPU_OPP_NUM];
extern int32_t vpu_thermal_en_throttle_cb(uint8_t vcore_opp, uint8_t vpu_opp);
extern int32_t vpu_thermal_dis_throttle_cb(void);
extern int get_vpu_opp(void);
extern int get_vpu_dspcore_opp(int core);
extern int get_vpu_platform_floor_opp(void);
extern int get_vpu_ceiling_opp(int core);
extern int get_vpu_opp_to_freq(uint8_t step);
void vpu_enable_mtcmos(void);
void vpu_disable_mtcmos(void);
int get_vpu_init_done(void);


#endif
