/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include "tpd.h"
/* #ifdef TPD_HAVE_CALIBRATION */

/* #ifndef TPD_CUSTOM_CALIBRATION */

/* #if (defined(TPD_WARP_START) && defined(TPD_WARP_END)) */
/* #define TPD_DO_WARP */
int TPD_DO_WARP;
int tpd_wb_start[TPD_WARP_CNT] = { 0 };
int tpd_wb_end[TPD_WARP_CNT] = { 0 };

void tpd_warp_calibrate(int *x, int *y)
{
	int wx = *x, wy = *y;

	if (wx < tpd_wb_start[0] && tpd_wb_start[0] > 0) {
		wx = tpd_wb_start[0] -
		    ((tpd_wb_start[0] - wx) *
			(tpd_wb_start[0] - tpd_wb_end[0]) /
			(tpd_wb_start[0]));
	} else if (wx > tpd_wb_start[2])
		wx = (wx - tpd_wb_start[2]) *
			(tpd_wb_end[2] - tpd_wb_start[2]) /
			(TPD_RES_X - tpd_wb_start[2]) +
			tpd_wb_start[2];

	if (wy < tpd_wb_start[1] && tpd_wb_start[1] > 0)
		wy = tpd_wb_start[1] -
		    ((tpd_wb_start[1] - wy) *
			(tpd_wb_start[1] - tpd_wb_end[1]) /
			tpd_wb_start[1]);
	else if (wy > tpd_wb_start[3] && wy <= TPD_RES_Y)
		wy = (wy - tpd_wb_start[3]) *
			(tpd_wb_end[3] - tpd_wb_start[3]) /
			(TPD_RES_Y - tpd_wb_start[3]) +
			tpd_wb_start[3];
	if (wy < 0)
		wy = 0;
	if (wx < 0)
		wx = 0;
	*x = wx, *y = wy;
}

/* #else */
/* #define tpd_warp_calibrate(x,y) */
/* #endif */

void tpd_calibrate(int *x, int *y)
{
	int tx, i;

	if (tpd_calmat[0] == 0)
		for (i = 0; i < 6; i++)
			tpd_calmat[i] = tpd_def_calmat[i];
	/* ================ calibrating ================ */
	tx = ((tpd_calmat[0] * (*x)) +
		(tpd_calmat[1] * (*y)) + (tpd_calmat[2])) >> 12;
	*y = ((tpd_calmat[3] * (*x)) +
		(tpd_calmat[4] * (*y)) + (tpd_calmat[5])) >> 12;
	*x = tx;

	if (TPD_DO_WARP == 1)
		tpd_warp_calibrate(x, y);
	*x = (*x) + ((*y) * (*x) * tpd_calmat[6] / TPD_RES_X) / TPD_RES_Y;
	*y = (*y) + ((*y) * (*x) * tpd_calmat[7] / TPD_RES_X) / TPD_RES_Y;
}

/* #endif */

/* #endif */
