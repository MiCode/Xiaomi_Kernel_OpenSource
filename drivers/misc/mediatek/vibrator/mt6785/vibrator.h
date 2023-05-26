/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
