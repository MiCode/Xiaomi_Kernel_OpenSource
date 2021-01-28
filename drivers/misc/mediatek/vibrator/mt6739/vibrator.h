/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef __CUST_VIBRATOR_H__
#define __CUST_VIBRATOR_H__


#define CUST_VIBR_LIMIT
#define CUST_VIBR_VOL

enum vib_strength {
	VOL_1_2 = 0,
	VOL_1_3,
	VOL_1_5,
	VOL_1_8,
	VOL_2_5,
	VOL_2_8,
	VOL_3_0,
	VOL_3_3,
};

struct vibrator_hw {
	int	vib_timer;
#ifdef CUST_VIBR_LIMIT
	int	vib_limit;
#endif
#ifdef CUST_VIBR_VOL
	int	vib_vol;
#endif
};

void init_cust_vibrator_dtsi(struct platform_device *pdev);
void init_vibr_oc_handler(void (*vibr_oc_func)(void));

#endif
